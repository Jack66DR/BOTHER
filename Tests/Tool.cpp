// BOTHER_Tool
//   render <out.wav> [preset]   runs a few notes through the full plugin and writes a WAV
//   snapshot <out.png> [look]   renders the editor to a PNG for a layout check

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Factory.h"

static void setParam (BotherProcessor& p, const char* id, float value)
{
    if (auto* prm = p.apvts.getParameter (id))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

static int render (const juce::File& out, const juce::String& preset)
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    const double seconds = 12.0;

    BotherProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);

    bool factory = false;
    for (int i = 0; i < (int) proc.presets->factory().size(); ++i)
        if (proc.presets->factory()[(size_t) i].name.replaceCharacter (' ', '_').toLowerCase() == preset)
        {
            proc.presets->loadFactory (i);
            factory = true;
        }
    proc.prepareToPlay (sr, block);

    // a slow strum, every two seconds: E minor, then C, then G, then D, a held organ-ish pad underneath
    std::vector<std::pair<double, double>> hits;
    const double chords[4][4] = { { 82.41, 123.47, 164.81, 196.0 }, { 130.81, 164.81, 196.0, 261.63 },
                                  { 98.0, 146.83, 196.0, 246.94 }, { 146.83, 220.0, 293.66, 369.99 } };
    for (int c = 0; c < 6; ++c)
        for (int k = 0; k < 4; ++k)
            hits.push_back ({ 0.1 + 2.0 * c + 0.03 * k, chords[c % 4][k] });
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> all (2, total);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;

    for (int s = 0; s < total; s += block)
    {
        const int n = juce::jmin (block, total - s);
        buf.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) (s + i) / sr;
            double x = 0.0;
            for (auto [t0, hz] : hits)
                if (t >= t0 && t < t0 + 2.6)
                {
                    const double u = t - t0;
                    const double env = std::exp (-u * 0.9) * std::min (1.0, u * 150.0);
                    x += env * (std::sin (juce::MathConstants<double>::twoPi * hz * u)
                                + 0.25 * std::sin (juce::MathConstants<double>::twoPi * hz * 2.0 * u) * std::exp (-u * 4.0));
                }
            x += 0.4 * std::sin (juce::MathConstants<double>::twoPi * 110.0 * t) + 0.2 * std::sin (juce::MathConstants<double>::twoPi * 330.0 * t);
            buf.setSample (0, i, (float) (0.12 * x));
            buf.setSample (1, i, (float) (0.12 * x));
        }
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            all.copyFrom (ch, s, buf, ch, 0, n);
    }

    const float pk = all.getMagnitude (0, total);
    std::printf ("rendered %.0f s, peak %.3f (%.1f dBFS)\n", seconds, pk, juce::Decibels::gainToDecibels (pk));
    for (int sec = 0; sec < (int) seconds; sec += 2)
        std::printf ("  %2d-%2d s rms %.1f dBFS\n", sec, sec + 2,
                     juce::Decibels::gainToDecibels (all.getRMSLevel (0, (int) (sec * sr), (int) (2 * sr)), -120.0f));

    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (out.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.release(), sr, 2, 24, {}, 0));
    if (w == nullptr)
        return 1;
    w->writeFromAudioSampleBuffer (all, 0, total);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}

static int snapshot (const juce::File& out, const juce::String& look)
{
    juce::ScopedJuceInitialiser_GUI gui;
    BotherProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (manners::kBaseW, manners::kBaseH);
    const float pixelScale = look.endsWith ("2x") ? 2.0f : 1.0f;

    // play a little and let the meter fill up
    auto* editor = dynamic_cast<BotherEditor*> (ed.get());
    for (int i = 0; i < (int) proc.presets->factory().size(); ++i)
        if (look.startsWith (proc.presets->factory()[(size_t) i].name.replaceCharacter (' ', '_').toLowerCase()))
            proc.presets->loadFactory (i);
    const int blocks = 400;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (double) (b * 512 + i) / 48000.0;
            const double u = std::fmod (t, 1.3);
            const float v = (float) (0.3 * std::exp (-u * 2.0) * std::sin (juce::MathConstants<double>::twoPi * 261.63 * u));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        proc.processBlock (buf, midi);
        if (b % 3 == 0 && editor != nullptr)
            editor->pump();
    }

    if (editor != nullptr)
    {
        using Page = manners::MenuCard::Page;
        if (look.startsWith ("menu")) editor->openMenu (Page::main);
        if (look.startsWith ("load")) editor->openMenu (Page::load);
        if (look.startsWith ("save")) editor->openMenu (Page::save);
        if (look.startsWith ("size")) editor->openMenu (Page::size);
    }

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, pixelScale);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}

// presets: factory load, undo, save, load back, step, init, randomise
static int presetCheck()
{
    juce::ScopedJuceInitialiser_GUI gui;
    BotherProcessor proc;
    auto& ps = *proc.presets;
    auto real = [&proc] (const char* id) { auto* p = proc.apvts.getParameter (id); return p->convertFrom0to1 (p->getValue()); };
    auto settle = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (300); };
    int fails = 0;
    auto check = [&fails] (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", what); fails += ok ? 0 : 1; };

    settle();
    ps.loadFactory (1);
    settle();
    check (std::abs (real (ParamIDs::rate) - 6.5f) < 0.05f && real (ParamIDs::shape) == 1.0f && ps.currentName() == "SURF SHACK", "factory preset loads");
    ps.undo.undo();
    settle();
    check (std::abs (real (ParamIDs::rate) - 5.0f) < 0.05f && real (ParamIDs::shape) == 0.0f, "undo puts it back");
    ps.undo.redo();
    settle();
    check (std::abs (real (ParamIDs::rate) - 6.5f) < 0.05f, "redo");

    const juce::String name = "TOOL CHECK " + juce::String (juce::Random::getSystemRandom().nextInt (100000));
    check (ps.save (name), "save to the preset folder");
    ps.init();
    settle();
    check (std::abs (real (ParamIDs::rate) - 5.0f) < 0.05f && ps.currentName() == "INIT", "init");
    const auto file = ps.folder().getChildFile (name + ".manners");
    check (ps.loadFile (file) && std::abs (real (ParamIDs::rate) - 6.5f) < 0.05f, "load it back");
    ps.step (1);
    check (ps.currentName() != name, "next preset moves on");
    ps.randomise();
    check (ps.currentName() == "RANDOMISED", "randomise");
    file.deleteFile();
    std::printf (fails == 0 ? "ALL PASS\n" : "%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    if (argc >= 2 && juce::String (argv[1]) == "presets")
        return presetCheck();

    if (argc < 3)
    {
        std::printf ("usage: BOTHER_Tool render <out.wav> [default|<factory_preset_name>]\n"
                     "       BOTHER_Tool snapshot <out.png> [face|<factory_preset_name>|menu|load|save|size][2x]\n");
        return 2;
    }
    const juce::String mode (argv[1]);
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2])));
    if (mode == "render")
        return render (out, argc > 3 ? juce::String (argv[3]) : juce::String ("default"));
    if (mode == "snapshot")
        return snapshot (out, argc > 3 ? juce::String (argv[3]) : juce::String ("face"));
    return 2;
}
