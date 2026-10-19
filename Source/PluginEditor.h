#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "manners/MannersFace.h"
#include "manners/MannersMenu.h"

namespace bother::ui
{
    // The modulation display, set into the card. Left: two cycles of the shape as EDGE has it, printed.
    // Right: the gain you are actually hearing over the last two and a half seconds, lit.
    // Along the bottom: where the drift is pushing the rate.
    class Monitor : public juce::Component
    {
    public:
        explicit Monitor (BotherProcessor&);
        void paint (juce::Graphics&) override;
        void tick (double t);

    private:
        static constexpr int kPicture = 120;    // points per cycle in the printed picture
        static constexpr int kTrace = 1000;     // 2.5 s at the engine's scope rate

        BotherProcessor& proc;
        std::array<float, 2 * kPicture> picture {};
        int pictureShape = -1;
        float pictureEdge = 99.0f;
        std::vector<float> trace = std::vector<float> (kTrace, 1.0f);
        float drift = 0.0f, driftAmount = 0.0f, rate = 0.0f, inLevel = 0.0f;
        bool sync = false, locked = false, harmonic = false;
        juce::String beatName;
    };
}

class BotherEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BotherEditor (BotherProcessor&);
    ~BotherEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    // for the offline snapshot
    void pump (double dt = 1.0 / 30.0) { advance (dt); }
    void openMenu (manners::MenuCard::Page);

private:
    void timerCallback() override { advance (1.0 / 30.0); }
    void advance (double dt);
    void setScale (float);
    float scale() const { return (float) getWidth() / (float) manners::kBaseW; }

    BotherProcessor& proc;
    manners::Face face;
    bother::ui::Monitor monitor { proc };
    std::vector<std::unique_ptr<manners::Dial>> dials;
    manners::Dial* rateDial = nullptr;
    manners::Dial* beatDial = nullptr;
    manners::Lamp syncLamp { "SYNC" }, harmonicLamp { "HARMONIC" };
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;
    manners::NameStrip strip;
    manners::MenuIcon menuIcon;
    manners::MenuCard menu;
    juce::String shownPreset;
    double t = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BotherEditor)
};
