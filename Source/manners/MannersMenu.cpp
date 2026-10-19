#include "MannersMenu.h"

namespace manners
{
static constexpr float kColW = 380.0f, kPad = 18.0f, kRight = (float) kBaseW - 36.0f, kTop = 100.0f;

MenuCard::MenuCard (Presets& p, Hooks h) : presets (p), hooks (std::move (h))
{
    setVisible (false);
    setWantsKeyboardFocus (true);
    setTitle ("Menu");

    nameField.setMultiLine (false);
    nameField.setFont (font (26.0f, 0.04f));
    nameField.setJustification (juce::Justification::centredLeft);
    nameField.setInputRestrictions (40);
    nameField.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    nameField.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    nameField.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    nameField.setColour (juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
    nameField.setIndents (2, 0);
    nameField.onReturnKey = [this] { doSave(); };
    nameField.onEscapeKey = [this] { show (Page::main); };
    addChildComponent (nameField);
}

void MenuCard::open()
{
    if (auto* f = findParentComponentOfClass<Face>())
    {
        setBounds (f->getLocalBounds());
        const auto ink = f->theme().ink;
        nameField.setColour (juce::TextEditor::textColourId, ink);
        nameField.setColour (juce::TextEditor::highlightColourId, ink.withAlpha (0.22f));
        nameField.setColour (juce::TextEditor::highlightedTextColourId, ink.brighter (0.2f));
        nameField.setColour (juce::CaretComponent::caretColourId, ink);
        nameField.applyColourToAllText (ink);
        nameField.applyFontToAllText (font (26.0f, 0.04f));
    }
    flash.clear();
    show (Page::main);
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

void MenuCard::close()
{
    nameField.setVisible (false);
    setVisible (false);
}

MenuCard::Item& MenuCard::add (Item::Kind k, const juce::String& t, std::function<void()> act, int col)
{
    Item it;
    it.kind = k;
    it.text = t;
    it.act = std::move (act);
    it.col = col;
    items.push_back (std::move (it));
    return items.back();
}

void MenuCard::show (Page p)
{
    page = p;
    items.clear();
    hover = -1;
    nameField.setVisible (false);
    auto& ps = presets;
    auto& ud = presets.undo;

    if (p == Page::main)
    {
        add (Item::header, "PRESET   " + ps.currentName().toUpperCase());
        add (Item::action, "LOAD PRESET", [this] { show (Page::load); }).more = true;
        add (Item::action, "SAVE PRESET", [this] { show (Page::save); }).more = true;
        add (Item::action, "PREVIOUS PRESET", [this, &ps] { ps.step (-1); show (Page::main); });
        add (Item::action, "NEXT PRESET", [this, &ps] { ps.step (1); show (Page::main); });
        add (Item::separator);
        add (Item::header, "EDIT");
        auto& u = add (Item::action, "UNDO", [this, &ud] { ud.undo(); show (Page::main); });
        u.enabled = ud.canUndo();
        u.right = ud.getUndoDescription().toUpperCase();
        auto& r = add (Item::action, "REDO", [this, &ud] { ud.redo(); show (Page::main); });
        r.enabled = ud.canRedo();
        r.right = ud.getRedoDescription().toUpperCase();
        add (Item::action, "COPY SETTINGS", [this, &ps] { ps.copyToClipboard(); close(); });
        add (Item::action, "PASTE SETTINGS", [this, &ps]
        {
            if (ps.pasteFromClipboard()) close();
            else { flash = "NOTHING FROM " + ps.plugin() + " ON THE CLIPBOARD"; show (Page::main); }
        });
        add (Item::action, "INIT", [this, &ps] { ps.init(); close(); });
        add (Item::action, "RANDOMISE", [this, &ps] { ps.randomise(); show (Page::main); });
        add (Item::separator);
        auto& sz = add (Item::action, "WINDOW SIZE", [this] { show (Page::size); });
        sz.more = true;
        if (hooks.scale)
            sz.right = juce::String (juce::roundToInt (hooks.scale() * 100.0f)) + " %";
        add (Item::separator);
        add (Item::note, hooks.about);
    }
    else if (p == Page::load)
    {
        add (Item::header, "FACTORY", {}, 0);
        const auto current = ps.currentName();
        for (int i = 0; i < (int) ps.factory().size(); ++i)
        {
            auto& it = add (Item::action, ps.factory()[(size_t) i].name.toUpperCase(), [this, &ps, i] { ps.loadFactory (i); show (Page::load); }, 0);
            it.ticked = ps.factory()[(size_t) i].name == current;
        }
        add (Item::separator, {}, {}, 0);
        add (Item::action, "LOAD FROM FILE...", [this, &ps]
        {
            chooser = std::make_unique<juce::FileChooser> ("Load a " + ps.plugin() + " preset", ps.folder(), "*.manners");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this, &ps] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile() && ! ps.loadFile (f))
                                      {
                                          flash = "THAT FILE ISN'T A " + ps.plugin() + " PRESET";
                                          if (isVisible()) show (Page::load);
                                      }
                                  });
        }, 0);
        add (Item::action, "SHOW PRESET FOLDER", [&ps]
        {
            const auto dir = ps.folder();
            dir.createDirectory();
            dir.startAsProcess();
        }, 0);
        add (Item::action, "BACK", [this] { show (Page::main); }, 0).back = true;

        add (Item::header, "YOURS", {}, 1);
        const auto files = ps.userPresets();
        if (files.isEmpty())
            add (Item::note, "NOTHING SAVED YET", {}, 1);
        const int shown = juce::jmin (files.size(), 14);
        for (int i = 0; i < shown; ++i)
        {
            const auto f = files[i];
            auto& it = add (Item::action, f.getFileNameWithoutExtension().toUpperCase(), [this, &ps, f] { ps.loadFile (f); show (Page::load); }, 1);
            it.ticked = f.getFileNameWithoutExtension().equalsIgnoreCase (current);
        }
        if (files.size() > shown)
            add (Item::note, juce::String (files.size() - shown) + " MORE: LOAD FROM FILE", {}, 1);
    }
    else if (p == Page::save)
    {
        add (Item::header, "SAVE PRESET AS");
        add (Item::field);
        add (Item::action, "SAVE", [this] { doSave(); });
        add (Item::action, "BACK", [this] { show (Page::main); }).back = true;
        auto n = ps.currentName();
        if (n == "INIT" || n == "RANDOMISED")
            n.clear();
        nameField.setText (n, false);
    }
    else if (p == Page::size)
    {
        add (Item::header, "WINDOW SIZE");
        for (float s : { 0.75f, 1.0f, 1.25f, 1.5f })
        {
            auto& it = add (Item::action, juce::String (juce::roundToInt (s * 100.0f)) + " %", [this, s]
            {
                if (hooks.setScale) hooks.setScale (s);
                show (Page::size);
            });
            it.ticked = hooks.scale && std::abs (hooks.scale() - s) < 0.02f;
        }
        add (Item::separator);
        add (Item::action, "BACK", [this] { show (Page::main); }).back = true;
    }

    layout();
    repaint();
}

void MenuCard::doSave()
{
    if (presets.save (nameField.getText()))
        close();
    else
    {
        flash = "COULDN'T SAVE THAT NAME";
        show (Page::save);
    }
}

void MenuCard::layout()
{
    int cols = 1;
    for (auto& it : items)
        cols = juce::jmax (cols, it.col + 1);
    const float rowH = page == Page::load ? 34.0f : 40.0f;
    const float x0 = kRight - kColW * (float) cols;
    float top = kTop + kPad + (flash.isNotEmpty() ? 34.0f : 0.0f);
    float bottom = top;

    for (int c = 0; c < cols; ++c)
    {
        float y = top;
        for (auto& it : items)
        {
            if (it.col != c)
                continue;
            const float h = it.kind == Item::header ? 38.0f : it.kind == Item::separator ? 18.0f
                          : it.kind == Item::note ? 34.0f : it.kind == Item::field ? 54.0f : rowH;
            it.bounds = { x0 + kColW * (float) c, y, kColW, h };
            y += h;
        }
        bottom = juce::jmax (bottom, y);
    }
    card = { x0, kTop, kColW * (float) cols, bottom + kPad - kTop };

    for (auto& it : items)
        if (it.kind == Item::field)
        {
            nameField.setBounds (it.bounds.reduced (26.0f, 8.0f).toNearestInt());
            nameField.setVisible (true);
            nameField.grabKeyboardFocus();
            nameField.selectAll();
        }
}

void MenuCard::paint (juce::Graphics& g)
{
    auto* face = findParentComponentOfClass<Face>();
    const auto ink = face != nullptr ? face->theme().ink : juce::Colours::white;

    g.fillAll (juce::Colours::black.withAlpha (0.32f));
    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 22, { 0, 8 }).drawForRectangle (g, card.toNearestInt());

    // the card is a different patch of the same worn stock
    if (face != nullptr)
    {
        const auto& s = face->surface();
        const int sx = juce::jlimit (0, juce::jmax (0, s.getWidth() - (int) card.getWidth()), 210);
        const int sy = juce::jlimit (0, juce::jmax (0, s.getHeight() - (int) card.getHeight()), 110);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (s, (int) card.getX(), (int) card.getY(), (int) card.getWidth(), (int) card.getHeight(),
                     sx, sy, juce::jmin ((int) card.getWidth(), s.getWidth()), juce::jmin ((int) card.getHeight(), s.getHeight()));
    }
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (card);

    paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        lg.setColour (ink);
        lg.drawRect (card.reduced (6.0f), 1.6f);

        if (flash.isNotEmpty())
        {
            lg.setColour (ink.withAlpha (0.85f));
            text (lg, flash, card.getX() + 28.0f, kTop + kPad + 14.0f, 19.0f, 0.12f, juce::Justification::centredLeft, card.getWidth() - 40.0f);
        }

        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto& it = items[i];
            const auto b = it.bounds;
            const float cy = b.getCentreY();
            switch (it.kind)
            {
                case Item::header:
                    lg.setColour (ink.withAlpha (0.7f));
                    text (lg, it.text, b.getX() + 28.0f, cy + 3.0f, 19.0f, 0.2f, juce::Justification::centredLeft, b.getWidth() - 40.0f);
                    break;
                case Item::separator:
                    lg.setColour (ink.withAlpha (0.35f));
                    lg.fillRect (b.getX() + 22.0f, cy - 0.6f, b.getWidth() - 44.0f, 1.2f);
                    break;
                case Item::note:
                    lg.setColour (ink.withAlpha (0.6f));
                    text (lg, it.text, b.getX() + 28.0f, cy, 18.0f, 0.05f, juce::Justification::centredLeft, b.getWidth() - 40.0f);
                    break;
                case Item::field:
                    lg.setColour (ink.withAlpha (0.6f));
                    lg.fillRect (b.getX() + 26.0f, b.getBottom() - 8.0f, b.getWidth() - 52.0f, 1.4f);
                    break;
                case Item::action:
                {
                    const float alpha = it.enabled ? 1.0f : 0.35f;
                    if ((int) i == hover && it.enabled)
                    {
                        lg.setColour (ink.withAlpha (0.12f));
                        lg.fillRect (b.reduced (10.0f, 2.0f));
                    }
                    lg.setColour (ink.withAlpha (alpha));
                    float tx = b.getX() + 28.0f;
                    if (it.back)
                    {
                        juce::Path p;
                        p.startNewSubPath (tx + 5.0f, cy - 6.0f);
                        p.lineTo (tx, cy);
                        p.lineTo (tx + 5.0f, cy + 6.0f);
                        lg.strokePath (p, juce::PathStrokeType (1.8f));
                        tx += 16.0f;
                    }
                    if (it.ticked)
                        lg.fillRect (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ b.getX() + 17.0f, cy }));
                    const float size = page == Page::load ? 22.0f : 24.0f;
                    text (lg, it.text, tx, cy, size, 0.05f, juce::Justification::centredLeft, b.getWidth() - 60.0f);
                    if (it.right.isNotEmpty())
                    {
                        lg.setColour (ink.withAlpha (0.55f * alpha));
                        text (lg, it.right, b.getRight() - (it.more ? 44.0f : 28.0f), cy, 18.0f, 0.08f,
                              juce::Justification::centredRight, 180.0f);
                    }
                    if (it.more)
                    {
                        const float x = b.getRight() - 28.0f;
                        juce::Path p;
                        p.startNewSubPath (x - 5.0f, cy - 6.0f);
                        p.lineTo (x, cy);
                        p.lineTo (x - 5.0f, cy + 6.0f);
                        lg.strokePath (p, juce::PathStrokeType (1.8f));
                    }
                    break;
                }
            }
        }
    });
}

void MenuCard::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i].kind == Item::action && items[i].bounds.contains (e.position))
            h = (int) i;
    if (h != hover)
    {
        hover = h;
        repaint();
    }
}

void MenuCard::mouseUp (const juce::MouseEvent& e)
{
    if (! card.contains (e.position))
    {
        close();
        return;
    }
    for (auto& it : items)
        if (it.kind == Item::action && it.enabled && it.bounds.contains (e.position) && it.act)
        {
            flash.clear();
            auto act = it.act;   // the action may rebuild the item list
            act();
            return;
        }
}

bool MenuCard::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        if (page == Page::main) close();
        else show (Page::main);
        return true;
    }
    return false;
}
}
