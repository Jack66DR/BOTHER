#pragma once

// manners! DSP shared face
// ------------------------
// One worn card for every plugin in the family. The card (plate.jpg) is the same for all of them;
// each plugin re-tints it with a Theme and lays its own controls on top. Ink is weathered with the
// same wear map (maps.png), so lettering and rings sit *in* the surface rather than on it.
//
// Drop the whole Source/manners folder into a plugin, add Resources/plate.jpg, Resources/maps.png
// and Resources/BarlowSemiCondensed-Medium.ttf to its binary data, and build the editor from a Face.

#include <juce_audio_processors/juce_audio_processors.h>

namespace manners
{
    struct Theme
    {
        juce::String name;                  // the name strip at the bottom
        juce::Colour ink;                   // lettering, rings, pointers
        juce::Colour accent;                // anything live: displays, rays, lamps that glow
        float base[3] { 1.0f, 1.0f, 1.0f }; // how the card's grime is tinted
        float leak[3] { 1.0f, 0.62f, 0.30f };   // how the light leaks are tinted
    };

    // The house face is designed at this size; editors scale it.
    inline constexpr int kBaseW = 1431, kBaseH = 918;

    // Barlow Semi Condensed Medium. size is the em size in face pixels, tracking is extra space per
    // letter as a fraction of the em, as in the design stills.
    juce::Font font (float size, float tracking = 0.0f);

    enum class Weather { ink, screen };

    class Face;

    // Paints whatever `draw` draws (in the component's own coordinates, in real colours) as ink that
    // has lived a life: lit by the card, rubbed, scratched and debossed. Screen light (Weather::screen)
    // is barely worn and not lit by the card, because it shines through the glass.
    void paintWorn (juce::Graphics&, juce::Component& self, const std::function<void (juce::Graphics&)>& draw,
                    Weather = Weather::ink);

    // Text helper that matches the stills: centred on (x, y) by default.
    void text (juce::Graphics&, const juce::String&, float x, float y, float size, float tracking,
               juce::Justification = juce::Justification::centred, float boxW = 900.0f);

    //==========================================================================
    class Face : public juce::Component
    {
    public:
        explicit Face (Theme);

        const Theme& theme() const { return th; }

        // static furniture, in face coordinates
        void addHeading (const juce::String&, float x0, float x1, float y);
        void addWindow (juce::Rectangle<float>);   // a recess cut into the card, for a display

        // the name strip shows this briefly instead of the plugin name (knob values while you turn)
        std::function<void (const juce::String&)> onReadout;
        // called when a control starts a gesture, so the editor can open an undo step
        std::function<void()> onGesture;

        void paint (juce::Graphics&) override;
        void resized() override { cache = {}; }

        // weathers a layer drawn at `scale` physical pixels per face pixel, whose top-left sits at `origin`
        void weather (juce::Image& layer, juce::Point<float> origin, float scale, Weather) const;
        const juce::Image& surface() const { return surf; }

    private:
        struct Heading { juce::String text; float x0, x1, y; };

        void buildSurface();
        void paintStatic (juce::Graphics&);

        Theme th;
        juce::Image surf;                       // tinted plate, map resolution
        std::vector<uint8_t> wear, light, warm; // map resolution
        int mapW = 0, mapH = 0;
        std::vector<Heading> headings;
        std::vector<juce::Rectangle<float>> windows;
        juce::Image cache;
        float cacheScale = 0.0f;
    };

    //==========================================================================
    // A knob: double ring, short pointer, label under it or to its right. The whole component is the
    // drag area, label included.
    class Dial : public juce::Slider
    {
    public:
        enum class Label { below, right };

        Dial (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label,
              float radius, float labelSize = 24.0f, Label = Label::below);

        // place by knob centre, in face coordinates
        void place (float cx, float cy);

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override { readout(); }
        void startedDragging() override;
        void valueChanged() override { readout(); repaint(); }

    private:
        void readout();
        juce::Point<float> knobCentre() const;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::RangedAudioParameter* param = nullptr;
        juce::String label;
        float r, labelSize;
        Label where;
    };

    // A small square lamp with an engraved word beside it. Toggle or momentary.
    class Lamp : public juce::Button
    {
    public:
        Lamp (const juce::String& word, float size = 26.0f);
        void place (float lampX, float y);   // lamp centre, face coordinates
        void paintButton (juce::Graphics&, bool over, bool down) override;
        float size;
    };

    // The bottom strip: the plugin's name, knob values while you turn them, and the preset with < >.
    class NameStrip : public juce::Component, private juce::Timer
    {
    public:
        NameStrip();
        void show (const juce::String& readout);
        void setPreset (const juce::String& name) { preset = name; repaint(); }
        std::function<void (int)> onStep;

        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        void timerCallback() override { stopTimer(); shown.clear(); repaint(); }
        juce::Rectangle<float> presetArea() const;
        juce::String shown, preset;
    };

    // The three bars, top right.
    class MenuIcon : public juce::Component
    {
    public:
        MenuIcon() { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTitle ("Menu"); }
        std::function<void()> onClick;
        void paint (juce::Graphics&) override;
        void mouseUp (const juce::MouseEvent& e) override { if (onClick && contains (e.getPosition())) onClick(); }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
    };
}
