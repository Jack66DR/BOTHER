#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "manners/MannersPresets.h"

namespace ParamIDs
{
    inline constexpr const char* in         = "in";
    inline constexpr const char* out        = "out";
    inline constexpr const char* mix        = "mix";
    inline constexpr const char* shape      = "shape";
    inline constexpr const char* edge       = "edge";
    inline constexpr const char* rate       = "rate";
    inline constexpr const char* beat       = "beat";
    inline constexpr const char* sync       = "sync";
    inline constexpr const char* depth      = "depth";
    inline constexpr const char* spread     = "spread";
    inline constexpr const char* harmonic   = "harmonic";
    inline constexpr const char* driftRate  = "driftRate";
    inline constexpr const char* driftDepth = "driftDepth";
    inline constexpr const char* driftShape = "driftShape";
    inline constexpr const char* sway       = "sway";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* grit       = "grit";
    inline constexpr const char* tone       = "tone";
}

namespace bother
{
    // SYNC divisions, and how many quarter notes one wobble lasts at each.
    struct Division { const char* name; double quarters; };
    inline constexpr Division kDivisions[] = {
        { "4 BARS", 16.0 }, { "2 BARS", 8.0 }, { "1 BAR", 4.0 }, { "1/2", 2.0 }, { "1/2 T", 4.0 / 3.0 },
        { "1/4 D", 1.5 }, { "1/4", 1.0 }, { "1/4 T", 2.0 / 3.0 }, { "1/8 D", 0.75 }, { "1/8", 0.5 },
        { "1/8 T", 1.0 / 3.0 }, { "1/16 D", 0.375 }, { "1/16", 0.25 }, { "1/16 T", 1.0 / 6.0 }, { "1/32", 0.125 },
    };
    inline constexpr int kNumDivisions = (int) (sizeof (kDivisions) / sizeof (kDivisions[0]));
}

class BotherProcessor : public juce::AudioProcessor
{
public:
    BotherProcessor();
    ~BotherProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BOTHER"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::UndoManager undo;                    // declared before apvts: apvts records into it
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<manners::Presets> presets;

    bother::Engine engine;                     // the face reads its telemetry directly
    std::atomic<float> uiInLevel { 0.0f };
    std::atomic<bool> uiLocked { false };      // SYNC is following the host's beat

private:
    juce::AudioBuffer<float> dryBuf;
    juce::SmoothedValue<float> inGainS, outGainS, mixS;
    float inPeak = 0.0f;
    bool wasPlaying = false;

    std::atomic<float>* p[18] {};
    enum P { pIn, pOut, pMix, pShape, pEdge, pRate, pBeat, pSync, pDepth, pSpread, pHarmonic,
             pDriftRate, pDriftDepth, pDriftShape, pSway, pDrive, pGrit, pTone };
    float get (P which) const { return p[which]->load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BotherProcessor)
};
