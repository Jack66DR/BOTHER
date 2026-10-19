#pragma once

// The menu behind the three bars: a worn card that drops over the face. Same for every plugin.

#include "MannersFace.h"
#include "MannersPresets.h"

namespace manners
{
    class MenuCard : public juce::Component
    {
    public:
        struct Hooks
        {
            std::function<void (float)> setScale;   // window size, 1.0 = the design size
            std::function<float()> scale;
            juce::String about;                     // "CREPUSCULAR 0.2.0, BUILT 19 OCT 2026"
        };

        MenuCard (Presets&, Hooks);

        void open();
        void close();
        bool isOpen() const { return isVisible(); }

        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
        void mouseUp (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;

        enum class Page { main, load, save, size };
        void show (Page);   // public so the snapshot tool can open any page

    private:
        struct Item
        {
            enum Kind { action, header, separator, note, field } kind = action;
            juce::String text, right;
            std::function<void()> act;
            bool enabled = true, ticked = false, more = false, back = false;
            int col = 0;
            juce::Rectangle<float> bounds;
        };

        Item& add (Item::Kind, const juce::String& text = {}, std::function<void()> act = {}, int col = 0);
        void layout();
        void doSave();

        Presets& presets;
        Hooks hooks;
        std::vector<Item> items;
        juce::Rectangle<float> card;
        int hover = -1;
        Page page = Page::main;
        juce::TextEditor nameField;
        std::unique_ptr<juce::FileChooser> chooser;
        juce::String flash;   // a one-line note after something failed, shown at the top of the card
    };
}
