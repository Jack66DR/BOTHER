#pragma once

// manners! DSP shared presets: the same menu behind every plugin's three bars.
// Factory presets are compiled in; yours live in ~/Music/manners! DSP/<PLUGIN>/Presets as .manners files.

#include <juce_audio_processors/juce_audio_processors.h>

namespace manners
{
    struct FactoryPreset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;   // real units; anything not listed goes to its default
    };

    // Where RANDOMISE may push a parameter (real units). A parameter with no range is left alone.
    struct RandomRange { juce::String id; float lo, hi; };

    class Presets
    {
    public:
        struct Setup
        {
            juce::String plugin;                       // "CREPUSCULAR"
            std::vector<FactoryPreset> factory;
            std::vector<RandomRange> randomise;
            juce::StringArray performance;             // never touched by presets (FREEZE, ERASE)
            juce::StringArray levels;                  // kept when a factory preset loads (IN, OUT)
        };

        Presets (juce::AudioProcessorValueTreeState&, juce::UndoManager&, Setup);

        const juce::String& plugin() const { return setup.plugin; }
        const std::vector<FactoryPreset>& factory() const { return setup.factory; }

        juce::String currentName() const;
        juce::File folder() const;
        juce::Array<juce::File> userPresets() const;

        void loadFactory (int index);
        bool loadFile (const juce::File&);
        bool save (const juce::String& name);
        void step (int delta);                         // previous / next through factory then yours

        void init();
        void randomise();
        void copyToClipboard() const;
        bool pasteFromClipboard();

        void beginGesture (const juce::String& what) { undo.beginNewTransaction (what); }
        juce::UndoManager& undo;

        std::function<void()> onChange;                // the preset name changed

    private:
        void setName (const juce::String&);
        void setReal (const juce::String& id, float value);
        void applyXml (const juce::XmlElement&, bool includeLevels);
        std::unique_ptr<juce::XmlElement> paramsXml() const;

        juce::AudioProcessorValueTreeState& state;
        Setup setup;
        juce::Random random;
    };
}
