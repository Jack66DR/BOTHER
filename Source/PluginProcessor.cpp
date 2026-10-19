#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Factory.h"

namespace
{
    juce::String pctText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

    juce::String dbText (float v, int)
    {
        if (v <= -59.9f) return "-inf dB";
        return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    }

    float dbToGain (float db) { return db <= -59.9f ? 0.0f : juce::Decibels::decibelsToGain (db); }

    juce::NormalisableRange<float> centred (float lo, float hi, float centre)
    {
        juce::NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    juce::String hzText (float v, int)
    {
        if (v < 1.0f) return juce::String (v, 2) + " Hz";
        if (v < 10.0f) return juce::String (v, 2) + " Hz";
        return juce::String (v, 1) + " Hz";
    }

    juce::StringArray shapeNames()
    {
        juce::StringArray s;
        for (auto* n : bother::kShapeNames) s.add (n);
        return s;
    }

    juce::StringArray divisionNames()
    {
        juce::StringArray s;
        for (auto& d : bother::kDivisions) s.add (d.name);
        return s;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout BotherProcessor::createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using B = AudioParameterBool;
    using C = AudioParameterChoice;
    using A = AudioParameterFloatAttributes;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = [] (const char* id, const char* name, float def)
    {
        return std::make_unique<F> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                    A().withStringFromValueFunction (pctText));
    };
    auto gain = [] (const char* id, const char* name)
    {
        return std::make_unique<F> (ParameterID { id, 1 }, name, NormalisableRange<float> (-60.0f, 12.0f, 0.0f, 2.0f), 0.0f,
                                    A().withStringFromValueFunction (dbText));
    };

    l.add (gain (ParamIDs::in, "In"));
    l.add (gain (ParamIDs::out, "Out"));
    l.add (pct (ParamIDs::mix, "Mix", 1.0f));

    // ---- the wobble
    l.add (std::make_unique<C> (ParameterID { ParamIDs::shape, 1 }, "Shape", shapeNames(), 0));

    l.add (std::make_unique<F> (ParameterID { ParamIDs::edge, 1 }, "Edge", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                A().withStringFromValueFunction ([] (float v, int)
                                {
                                    if (std::abs (v) < 0.03f) return juce::String ("pure");
                                    return juce::String (v < 0.0f ? "round " : "sharp ") + juce::String (juce::roundToInt (std::abs (v) * 100.0f));
                                })));

    l.add (std::make_unique<F> (ParameterID { ParamIDs::rate, 1 }, "Rate", centred (bother::kMinRate, bother::kMaxRate, 5.0f), 5.0f,
                                A().withStringFromValueFunction (hzText)));
    l.add (std::make_unique<C> (ParameterID { ParamIDs::beat, 1 }, "Beat", divisionNames(), 9));   // 1/8
    l.add (std::make_unique<B> (ParameterID { ParamIDs::sync, 1 }, "Sync", false));
    l.add (pct (ParamIDs::depth, "Depth", 0.6f));

    l.add (std::make_unique<F> (ParameterID { ParamIDs::spread, 1 }, "Spread", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                A().withStringFromValueFunction ([] (float v, int)
                                {
                                    return juce::String (juce::roundToInt (v * 180.0f)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
                                })));

    l.add (std::make_unique<B> (ParameterID { ParamIDs::harmonic, 1 }, "Harmonic", false));

    // ---- the drift
    l.add (std::make_unique<F> (ParameterID { ParamIDs::driftRate, 1 }, "Drift Rate",
                                centred (bother::kMinDrift, bother::kMaxDrift, 0.4f), 0.15f,
                                A().withStringFromValueFunction (hzText)));
    l.add (std::make_unique<F> (ParameterID { ParamIDs::driftDepth, 1 }, "Drift Depth", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                                A().withStringFromValueFunction ([] (float v, int)
                                {
                                    if (v < 0.005f) return juce::String ("off");
                                    return juce::String (juce::CharPointer_UTF8 ("\xc2\xb1 ")) + juce::String (v * bother::kDriftOctaves, 2) + " oct";
                                })));
    l.add (std::make_unique<C> (ParameterID { ParamIDs::driftShape, 1 }, "Drift Shape", shapeNames(), 6));   // WANDER
    l.add (pct (ParamIDs::sway, "Sway", 0.0f));

    // ---- the dirt
    l.add (pct (ParamIDs::drive, "Drive", 0.15f));
    l.add (pct (ParamIDs::grit, "Grit", 0.0f));
    l.add (std::make_unique<F> (ParameterID { ParamIDs::tone, 1 }, "Tone", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                A().withStringFromValueFunction ([] (float v, int)
                                {
                                    if (std::abs (v) < 0.03f) return juce::String ("flat");
                                    return juce::String (v < 0.0f ? "dark " : "bright ") + juce::String (juce::roundToInt (std::abs (v) * 100.0f));
                                })));
    return l;
}

BotherProcessor::BotherProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PARAMS", createLayout())
{
    const char* ids[18] = { ParamIDs::in, ParamIDs::out, ParamIDs::mix, ParamIDs::shape, ParamIDs::edge, ParamIDs::rate,
                            ParamIDs::beat, ParamIDs::sync, ParamIDs::depth, ParamIDs::spread, ParamIDs::harmonic,
                            ParamIDs::driftRate, ParamIDs::driftDepth, ParamIDs::driftShape, ParamIDs::sway,
                            ParamIDs::drive, ParamIDs::grit, ParamIDs::tone };
    for (int i = 0; i < 18; ++i)
        p[i] = apvts.getRawParameterValue (ids[i]);

    presets = std::make_unique<manners::Presets> (apvts, undo, bother::presetSetup());
}

bool BotherProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void BotherProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, (uint32_t) juce::Random::getSystemRandom().nextInt());
    dryBuf.setSize (2, juce::jmax (1, samplesPerBlock));
    dryBuf.clear();

    inGainS.reset (sampleRate, 0.03);
    outGainS.reset (sampleRate, 0.03);
    mixS.reset (sampleRate, 0.03);
    inGainS.setCurrentAndTargetValue (dbToGain (get (pIn)));
    outGainS.setCurrentAndTargetValue (dbToGain (get (pOut)));
    mixS.setCurrentAndTargetValue (get (pMix));
    inPeak = 0.0f;
    wasPlaying = false;
}

void BotherProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0)
        return;

    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    if (dryBuf.getNumSamples() < n)   // host broke its block-size promise
        dryBuf.setSize (2, n, false, false, true);

    // ---- where we are in the song
    double bpm = 120.0, ppq = 0.0;
    bool playing = false, havePpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
            if (auto q = pos->getPpqPosition()) { ppq = *q; havePpq = true; }
            playing = pos->getIsPlaying();
        }

    bother::Params bp;
    bp.shape = (bother::Shape) juce::jlimit (0, bother::kNumShapes - 1, juce::roundToInt (get (pShape)));
    bp.edge = get (pEdge);
    bp.depth = get (pDepth);
    bp.spread = get (pSpread);
    bp.harmonic = get (pHarmonic) > 0.5f;
    bp.driftShape = (bother::Shape) juce::jlimit (0, bother::kNumShapes - 1, juce::roundToInt (get (pDriftShape)));
    bp.driftRateHz = get (pDriftRate);
    bp.driftDepth = get (pDriftDepth);
    bp.sway = get (pSway);
    bp.drive = get (pDrive);
    bp.grit = get (pGrit);
    bp.tone = get (pTone);

    const bool sync = get (pSync) > 0.5f;
    bool locked = false;
    if (sync)
    {
        const auto& div = bother::kDivisions[juce::jlimit (0, bother::kNumDivisions - 1, juce::roundToInt (get (pBeat)))];
        bp.rateHz = (float) (bpm / 60.0 / div.quarters);
        // Lock the wobble to the grid while the song plays, unless DRIFT is bending the rate
        // (then it follows the tempo but is free to wander off the grid, which is the point).
        if (playing && havePpq && bp.driftDepth < 0.001f)
        {
            engine.setCycles (ppq / div.quarters);
            locked = true;
        }
        else if (playing && ! wasPlaying && havePpq)
            engine.setCycles (ppq / div.quarters);   // at least start on the beat
    }
    else
        bp.rateHz = get (pRate);
    wasPlaying = playing;
    uiLocked.store (locked);

    inGainS.setTargetValue (dbToGain (get (pIn)));
    outGainS.setTargetValue (dbToGain (get (pOut)));
    mixS.setTargetValue (get (pMix));

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    auto* dL = dryBuf.getWritePointer (0);
    auto* dR = dryBuf.getWritePointer (1);

    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        dL[i] = L[i];
        dR[i] = R[i];
        const float g = inGainS.getNextValue();
        L[i] *= g;
        R[i] *= g;
        peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
    }

    engine.process (L, R, n, bp);

    for (int i = 0; i < n; ++i)
    {
        const float m = mixS.getNextValue();
        const float og = outGainS.getNextValue();
        L[i] = (dL[i] * (1.0f - m) + L[i] * m) * og;
        R[i] = (dR[i] * (1.0f - m) + R[i] * m) * og;
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    inPeak = juce::jmax (peak, inPeak * std::pow (0.9995f, (float) n));
    uiInLevel.store (inPeak);
}

void BotherProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BotherProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BotherProcessor::createEditor()
{
    return new BotherEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BotherProcessor();
}
