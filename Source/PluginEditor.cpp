#include "PluginEditor.h"

using namespace bother::ui;

namespace
{
    manners::Theme theme()
    {
        manners::Theme t;
        t.name = "BOTHER";
        t.ink = juce::Colour (238, 132, 104);      // sunburnt coral
        t.accent = juce::Colour (150, 236, 200);   // seasick mint
        t.base[0] = 1.24f; t.base[1] = 0.86f; t.base[2] = 0.80f;
        t.leak[0] = 1.00f; t.leak[1] = 0.58f; t.leak[2] = 0.42f;
        return t;
    }

    const juce::Identifier kScaleProp { "uiScale" };

    // the nuisance meter's window, in face coordinates
    const juce::Rectangle<float> kWindow { 420.0f, 128.0f, 591.0f, 350.0f };

    // inside the window
    const juce::Rectangle<float> kPictureBox { 26.0f, 76.0f, 168.0f, 180.0f };
    const juce::Rectangle<float> kTraceBox { 230.0f, 78.0f, 337.0f, 176.0f };
    constexpr float kDriftY = 312.0f, kDriftX0 = 128.0f, kDriftX1 = 567.0f;

    juce::String aboutLine()
    {
        const juce::String date (__DATE__);   // "Oct 19 2026"
        const auto parts = juce::StringArray::fromTokens (date, " ", "");
        const auto when = parts.size() == 3 ? parts[1] + " " + parts[0].toUpperCase() + " " + parts[2] : date;
        return "BOTHER " + juce::String (BOTHER_VERSION) + ", BUILT " + when;
    }

    float raw (BotherProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
}

//==============================================================================
Monitor::Monitor (BotherProcessor& p) : proc (p)
{
    setTitle ("Nuisance meter: the shape, the wobble you are hearing, and the drift");
    setInterceptsMouseClicks (false, false);
}

void Monitor::tick (double)
{
    const int shape = juce::roundToInt (raw (proc, ParamIDs::shape));
    const float edge = raw (proc, ParamIDs::edge);
    if (shape != pictureShape || std::abs (edge - pictureEdge) > 0.004f)
    {
        pictureShape = shape;
        pictureEdge = edge;
        bother::picture ((bother::Shape) juce::jlimit (0, bother::kNumShapes - 1, shape), edge, kPicture, picture.data());
    }

    // the last 2.5 s of gain, oldest first
    auto& e = proc.engine;
    const int w = e.scopeWrite.load (std::memory_order_acquire);
    for (int k = 0; k < kTrace; ++k)
    {
        const int idx = w - kTrace + k;
        trace[(size_t) k] = idx < 0 ? 1.0f : e.scope[(size_t) (idx % bother::kScope)].load (std::memory_order_relaxed);
    }

    drift += 0.4f * (e.uiDrift.load() - drift);
    rate = e.uiRate.load();
    driftAmount = raw (proc, ParamIDs::driftDepth);
    sync = raw (proc, ParamIDs::sync) > 0.5f;
    locked = proc.uiLocked.load();
    harmonic = raw (proc, ParamIDs::harmonic) > 0.5f;
    beatName = bother::kDivisions[juce::jlimit (0, bother::kNumDivisions - 1, juce::roundToInt (raw (proc, ParamIDs::beat)))].name;
    const float in = juce::jlimit (0.0f, 1.0f, proc.uiInLevel.load() * 2.0f);
    inLevel += (in > inLevel ? 0.5f : 0.08f) * (in - inLevel);
    repaint();
}

void Monitor::paint (juce::Graphics& g)
{
    auto* face = findParentComponentOfClass<manners::Face>();
    if (face == nullptr)
        return;
    const auto ink = face->theme().ink, mint = face->theme().accent;
    const float w = (float) getWidth();

    auto rateText = [this]
    {
        if (rate >= 100.0f) return juce::String (juce::roundToInt (rate)) + " HZ";
        return juce::String (rate, rate < 10.0f ? 2 : 1) + " HZ";
    };

    // ---- the printed parts: labels, the picture of the shape, the scale, the drift rail
    manners::paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        lg.setColour (ink);
        manners::text (lg, "NUISANCE METER", 22.0f, 30.0f, 19.0f, 0.12f, juce::Justification::centredLeft, 300.0f);
        manners::text (lg, sync ? beatName + (locked ? "  LOCKED" : "  SYNC") : juce::String ("FREE"),
                       w - 22.0f, 30.0f, 19.0f, 0.10f, juce::Justification::centredRight, 300.0f);

        // two cycles of the shape, as EDGE has it
        const auto pb = kPictureBox;
        lg.setColour (ink.withAlpha (0.45f));
        lg.drawRect (pb, 1.4f);
        lg.fillRect (pb.getCentreX() - 0.6f, pb.getY() + 6.0f, 1.2f, pb.getHeight() - 12.0f);
        juce::Path pic;
        const float px0 = pb.getX() + 10.0f, pw = pb.getWidth() - 20.0f, py0 = pb.getY() + 16.0f, ph = pb.getHeight() - 32.0f;
        for (int i = 0; i < 2 * kPicture; ++i)
        {
            const float x = px0 + pw * (float) i / (float) (2 * kPicture - 1);
            const float y = py0 + ph * 0.5f * (1.0f - picture[(size_t) i]);
            if (i == 0) pic.startNewSubPath (x, y); else pic.lineTo (x, y);
        }
        lg.setColour (ink);
        lg.strokePath (pic, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const int sh = juce::jlimit (0, bother::kNumShapes - 1, pictureShape);
        manners::text (lg, bother::kShapeNames[sh], pb.getCentreX(), pb.getBottom() + 22.0f, 17.0f, 0.10f);

        // the trace's scale: full, half, and the floor
        const auto tb = kTraceBox;
        for (int k = 0; k <= 2; ++k)
        {
            const float y = tb.getY() + tb.getHeight() * (float) k * 0.5f;
            lg.setColour (ink.withAlpha (k == 1 ? 0.35f : 0.6f));
            for (float x = tb.getX(); x < tb.getRight(); x += 12.0f)
                lg.fillRect (x, y - 0.7f, 6.0f, 1.4f);
        }
        lg.setColour (ink.withAlpha (0.8f));
        manners::text (lg, "ON", tb.getX() - 4.0f, tb.getY(), 14.0f, 0.1f, juce::Justification::centredRight, 60.0f);
        manners::text (lg, "OFF", tb.getX() - 4.0f, tb.getBottom(), 14.0f, 0.1f, juce::Justification::centredRight, 60.0f);
        manners::text (lg, "LAST 2.5 S", tb.getRight(), tb.getBottom() + 22.0f, 15.0f, 0.10f, juce::Justification::centredRight, 300.0f);

        // the drift rail: a ruler with the slow end on the left, the fast end on the right
        lg.setColour (ink);
        manners::text (lg, "DRIFT", 22.0f, kDriftY, 19.0f, 0.12f, juce::Justification::centredLeft, 200.0f);
        lg.setColour (ink.withAlpha (0.7f));
        lg.fillRect (kDriftX0, kDriftY - 0.8f, kDriftX1 - kDriftX0, 1.6f);
        for (int k = 0; k <= 6; ++k)
        {
            const float x = kDriftX0 + (kDriftX1 - kDriftX0) * (float) k / 6.0f;
            const float hgt = k == 3 ? 14.0f : 8.0f;
            lg.fillRect (x - 0.8f, kDriftY - hgt * 0.5f, 1.6f, hgt);
        }
        manners::text (lg, "SLOWER", kDriftX0, kDriftY + 20.0f, 13.0f, 0.12f, juce::Justification::centredLeft, 200.0f);
        manners::text (lg, "FASTER", kDriftX1, kDriftY + 20.0f, 13.0f, 0.12f, juce::Justification::centredRight, 200.0f);
    });

    // ---- the light on the glass: the live gain trace, the rate, the drift needle
    manners::paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        auto glowStroke = [&lg] (const juce::Path& p, juce::Colour c, float width)
        {
            lg.setColour (c.withMultipliedAlpha (0.16f));
            lg.strokePath (p, juce::PathStrokeType (width * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            lg.setColour (c);
            lg.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };

        const auto tb = kTraceBox;
        juce::Path tr;
        for (int k = 0; k < kTrace; ++k)
        {
            const float x = tb.getX() + tb.getWidth() * (float) k / (float) (kTrace - 1);
            const float y = tb.getBottom() - tb.getHeight() * juce::jlimit (0.0f, 1.0f, trace[(size_t) k]);
            if (k == 0) tr.startNewSubPath (x, y); else tr.lineTo (x, y);
        }
        glowStroke (tr, mint.withAlpha (0.55f + 0.45f * std::sqrt (inLevel)), 2.2f);
        // the newest moment, a little square riding the right-hand edge
        const float yNow = tb.getBottom() - tb.getHeight() * juce::jlimit (0.0f, 1.0f, trace.back());
        lg.setColour (mint);
        lg.drawRect (juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ tb.getRight(), yNow }), 1.6f);

        lg.setColour (mint);
        manners::text (lg, rateText(), w - 22.0f, 56.0f - 4.0f, 22.0f, 0.06f, juce::Justification::centredRight, 300.0f);
        if (harmonic)
            manners::text (lg, "HARMONIC", tb.getX(), tb.getBottom() + 22.0f, 15.0f, 0.12f, juce::Justification::centredLeft, 300.0f);

        // the needle on the drift rail
        const float on = driftAmount > 0.005f ? 1.0f : 0.25f;
        const float nx = kDriftX0 + (kDriftX1 - kDriftX0) * 0.5f * (1.0f + juce::jlimit (-1.0f, 1.0f, drift * (driftAmount > 0.005f ? 1.0f : 0.0f)));
        juce::Path needle;
        needle.addTriangle (nx - 8.0f, kDriftY - 18.0f, nx + 8.0f, kDriftY - 18.0f, nx, kDriftY - 4.0f);
        lg.setColour (mint.withAlpha (on));
        lg.fillPath (needle);
    }, manners::Weather::screen);
}

//==============================================================================
BotherEditor::BotherEditor (BotherProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), face (theme()),
      menu (*p.presets, { [this] (float s) { setScale (s); }, [this] { return scale(); }, aboutLine() })
{
    addAndMakeVisible (face);
    face.setBounds (0, 0, manners::kBaseW, manners::kBaseH);

    face.addHeading ("TREMOLO", 60.0f, 370.0f, 146.0f);
    face.addHeading ("DRIFT", 1061.0f, 1371.0f, 146.0f);
    face.addHeading ("DIRT", 60.0f, 660.0f, 592.0f);
    face.addHeading ("OUTPUT", 771.0f, 1371.0f, 592.0f);
    face.addWindow (kWindow);

    auto dial = [this] (const char* id, const char* label, float cx, float cy, float r, float labelSize = 24.0f)
    {
        auto d = std::make_unique<manners::Dial> (proc.apvts, id, label, r, labelSize);
        face.addAndMakeVisible (*d);
        d->place (cx, cy);
        auto* raw = d.get();
        dials.push_back (std::move (d));
        return raw;
    };

    // TREMOLO, left of the meter
    dial (ParamIDs::shape, "SHAPE", 140.0f, 204.0f, 37.0f);
    dial (ParamIDs::edge,  "EDGE",  300.0f, 204.0f, 37.0f);
    rateDial = dial (ParamIDs::rate, "RATE", 140.0f, 334.0f, 37.0f);
    beatDial = dial (ParamIDs::beat, "BEAT", 140.0f, 334.0f, 37.0f);
    dial (ParamIDs::depth,  "DEPTH",  300.0f, 334.0f, 37.0f);
    dial (ParamIDs::spread, "SPREAD", 140.0f, 464.0f, 37.0f);

    // DRIFT, right of the meter: the LFO that leans on RATE
    dial (ParamIDs::driftRate,  "SPEED",  1131.0f, 204.0f, 37.0f);
    dial (ParamIDs::driftDepth, "AMOUNT", 1291.0f, 204.0f, 37.0f);
    dial (ParamIDs::driftShape, "CURVE",  1131.0f, 334.0f, 37.0f);
    dial (ParamIDs::sway,       "SWAY",   1291.0f, 334.0f, 37.0f);

    // DIRT and OUTPUT along the bottom, DRIVE and MIX as the bigger hands
    dial (ParamIDs::grit,  "GRIT",  150.0f, 672.0f, 44.0f);
    dial (ParamIDs::tone,  "TONE",  300.0f, 672.0f, 44.0f);
    dial (ParamIDs::drive, "DRIVE", 510.0f, 664.0f, 66.0f, 30.0f);
    dial (ParamIDs::mix,   "MIX",   921.0f, 664.0f, 66.0f, 30.0f);
    dial (ParamIDs::in,    "IN",   1131.0f, 672.0f, 44.0f);
    dial (ParamIDs::out,   "OUT",  1281.0f, 672.0f, 44.0f);

    face.addAndMakeVisible (monitor);
    monitor.setBounds (kWindow.toNearestInt());

    // lamps: SYNC in the tremolo's spare corner, HARMONIC under the meter
    syncLamp.setClickingTogglesState (true);
    harmonicLamp.setClickingTogglesState (true);
    syncLamp.place (262.0f, 464.0f);
    harmonicLamp.place (650.0f, 520.0f);
    for (auto* b : { &syncLamp, &harmonicLamp })
        face.addAndMakeVisible (*b);

    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    buttonAttachments.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::sync, syncLamp));
    buttonAttachments.push_back (std::make_unique<BA> (proc.apvts, ParamIDs::harmonic, harmonicLamp));
    syncLamp.onClick = [this] { proc.presets->beginGesture ("Sync"); };
    harmonicLamp.onClick = [this] { proc.presets->beginGesture ("Harmonic"); };

    // the name strip, the preset arrows and the menu
    face.addAndMakeVisible (strip);
    strip.setBounds (36, 798, manners::kBaseW - 72, 62);
    strip.onStep = [this] (int d) { proc.presets->step (d); };
    face.onReadout = [this] (const juce::String& s) { strip.show (s); };
    face.onGesture = [this] { proc.presets->beginGesture ("Turn knob"); };

    face.addAndMakeVisible (menuIcon);
    menuIcon.setBounds (1312, 44, 74, 52);
    menuIcon.onClick = [this] { if (menu.isOpen()) menu.close(); else menu.open(); };
    face.addChildComponent (menu);

    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) manners::kBaseW / (double) manners::kBaseH);
    setResizeLimits (manners::kBaseW / 2, manners::kBaseH / 2, (int) (manners::kBaseW * 1.6), (int) (manners::kBaseH * 1.6));
    const float s = juce::jlimit (0.5f, 1.6f, (float) (double) proc.apvts.state.getProperty (kScaleProp, 0.75));
    setSize (juce::roundToInt ((float) manners::kBaseW * s), juce::roundToInt ((float) manners::kBaseH * s));

    advance (0.0);
    startTimerHz (30);
}

BotherEditor::~BotherEditor()
{
    stopTimer();
}

void BotherEditor::setScale (float s)
{
    setSize (juce::roundToInt ((float) manners::kBaseW * s), juce::roundToInt ((float) manners::kBaseH * s));
}

void BotherEditor::resized()
{
    face.setTransform (juce::AffineTransform::scale (scale()));
    proc.apvts.state.setProperty (kScaleProp, scale(), nullptr);
}

void BotherEditor::openMenu (manners::MenuCard::Page page)
{
    menu.open();
    if (page != manners::MenuCard::Page::main)
        menu.show (page);
}

void BotherEditor::advance (double dt)
{
    t += dt;
    monitor.tick (t);

    // RATE and BEAT share a spot on the card; SYNC decides which one you're holding
    const bool sync = raw (proc, ParamIDs::sync) > 0.5f;
    rateDial->setVisible (! sync);
    beatDial->setVisible (sync);

    // the host can recall a song (and its preset name) at any time, so watch for it
    const auto name = proc.presets->currentName().toUpperCase();
    if (name != shownPreset)
    {
        shownPreset = name;
        strip.setPreset (name);
    }
}
