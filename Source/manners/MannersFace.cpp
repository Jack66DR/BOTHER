#include "MannersFace.h"
#include "BinaryData.h"

namespace manners
{
//==============================================================================
juce::Font font (float size, float tracking)
{
    static juce::Typeface::Ptr barlow = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiCondensedMedium_ttf,
                                                                                BinaryData::BarlowSemiCondensedMedium_ttfSize);
    juce::Font f (juce::FontOptions (barlow).withPointHeight (size));
    if (tracking != 0.0f)
        f.setExtraKerningFactor (tracking * size / f.getHeight());
    return f;
}

void text (juce::Graphics& g, const juce::String& s, float x, float y, float size, float tracking,
           juce::Justification j, float boxW)
{
    const auto f = font (size, tracking);
    g.setFont (f);
    const float h = f.getHeight() + 4.0f;
    const float trail = tracking * size;   // tracking is added after the last letter too; give it back
    juce::Rectangle<float> r;
    if (j.testFlags (juce::Justification::left))
        r = { x, y - h * 0.5f, boxW, h };
    else if (j.testFlags (juce::Justification::right))
        r = { x - boxW + trail, y - h * 0.5f, boxW, h };
    else
        r = { x - boxW * 0.5f + trail * 0.5f, y - h * 0.5f, boxW, h };
    g.drawText (s, r, j.getOnlyHorizontalFlags() | juce::Justification::verticallyCentred, false);
}

//==============================================================================
void paintWorn (juce::Graphics& g, juce::Component& self, const std::function<void (juce::Graphics&)>& draw, Weather w)
{
    auto* face = dynamic_cast<Face*> (&self);
    if (face == nullptr)
        face = self.findParentComponentOfClass<Face>();
    if (face == nullptr)
    {
        draw (g);
        return;
    }

    const auto area = g.getClipBounds().getIntersection (self.getLocalBounds());
    if (area.isEmpty())
        return;

    const float s = juce::jlimit (0.25f, 8.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    juce::Image layer (juce::Image::ARGB, juce::jmax (1, (int) std::ceil ((float) area.getWidth() * s)),
                       juce::jmax (1, (int) std::ceil ((float) area.getHeight() * s)), true, juce::SoftwareImageType());
    {
        juce::Graphics lg (layer);
        lg.addTransform (juce::AffineTransform::translation ((float) -area.getX(), (float) -area.getY()).scaled (s));
        draw (lg);
    }

    const auto xf = juce::AffineTransform::scale (1.0f / s).translated ((float) area.getX(), (float) area.getY());
    if (w == Weather::ink)
    {
        // debossed: the ink sits in a shallow groove, so a soft shadow falls below and right of it
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.drawImageTransformed (layer, xf.translated (1.0f, 1.8f), true);
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.drawImageTransformed (layer, xf.translated (1.6f, 3.0f), true);
    }

    face->weather (layer, face->getLocalPoint (&self, area.getTopLeft()).toFloat(), s, w);
    g.setOpacity (1.0f);   // drawImage takes its opacity from the current colour
    g.drawImageTransformed (layer, xf);
}

//==============================================================================
Face::Face (Theme t) : th (std::move (t))
{
    setOpaque (true);
    buildSurface();
}

void Face::buildSurface()
{
    auto plate = juce::ImageFileFormat::loadFrom (BinaryData::plate_jpg, BinaryData::plate_jpgSize);
    auto maps = juce::ImageFileFormat::loadFrom (BinaryData::maps_png, BinaryData::maps_pngSize);
    jassert (plate.isValid() && maps.isValid());
    mapW = maps.getWidth();
    mapH = maps.getHeight();
    if (plate.getWidth() != mapW || plate.getHeight() != mapH)
        plate = plate.rescaled (mapW, mapH);

    const size_t n = (size_t) mapW * (size_t) mapH;
    wear.resize (n);
    light.resize (n);
    warm.resize (n);
    surf = juce::Image (juce::Image::RGB, mapW, mapH, false, juce::SoftwareImageType());

    const float bm = (th.base[0] + th.base[1] + th.base[2]) / 3.0f;
    const float lm = 0.30f * th.leak[0] + 0.55f * th.leak[1] + 0.15f * th.leak[2];

    juce::Image::BitmapData pd (plate, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData md (maps, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData sd (surf, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < mapH; ++y)
        for (int x = 0; x < mapW; ++x)
        {
            const auto m = md.getPixelColour (x, y);
            const size_t i = (size_t) y * (size_t) mapW + (size_t) x;
            wear[i] = m.getRed();
            light[i] = m.getGreen();
            warm[i] = m.getBlue();

            const auto c = pd.getPixelColour (x, y);
            const float L = 0.30f * c.getRed() + 0.55f * c.getGreen() + 0.15f * c.getBlue();
            const float k = (float) warm[i] / 255.0f;
            float rgb[3];
            for (int ch = 0; ch < 3; ++ch)
            {
                const float b = L * th.base[ch] / bm * 0.86f;
                const float l = L * th.leak[ch] / lm * 0.92f;
                rgb[ch] = juce::jlimit (0.0f, 255.0f, b * (1.0f - k) + l * k);
            }
            sd.setPixelColour (x, y, juce::Colour ((uint8_t) rgb[0], (uint8_t) rgb[1], (uint8_t) rgb[2]));
        }
}

void Face::weather (juce::Image& layer, juce::Point<float> origin, float s, Weather w) const
{
    juce::Image::BitmapData bd (layer, juce::Image::BitmapData::readWrite);
    const float fx = (float) mapW / (float) juce::jmax (1, getWidth());
    const float fy = (float) mapH / (float) juce::jmax (1, getHeight());

    for (int y = 0; y < bd.height; ++y)
    {
        const int my = juce::jlimit (0, mapH - 1, (int) ((origin.y + ((float) y + 0.5f) / s) * fy));
        auto* row = bd.getLinePointer (y);
        for (int x = 0; x < bd.width; ++x)
        {
            auto* p = reinterpret_cast<juce::PixelARGB*> (row + (size_t) x * (size_t) bd.pixelStride);
            const uint8_t a = p->getAlpha();
            if (a == 0)
                continue;

            const int mx = juce::jlimit (0, mapW - 1, (int) ((origin.x + ((float) x + 0.5f) / s) * fx));
            const size_t i = (size_t) my * (size_t) mapW + (size_t) mx;
            const float wv = (float) wear[i] / 255.0f;

            p->unpremultiply();
            float r = p->getRed(), g = p->getGreen(), b = p->getBlue(), alpha = a;
            if (w == Weather::ink)
            {
                // lit by the card, warmed where the light leaks in, rubbed by the wear
                const float l = 0.8f + (float) light[i] / 255.0f * 1.2f;
                const float k = (float) warm[i] / 255.0f;
                const float kk = 0.45f * k;
                r = r * l * 0.92f * (1.0f - kk) + (r * 0.5f + 255.0f * 0.6f) * l * kk;
                g = g * l * 0.92f * (1.0f - kk) + (g * 0.5f + 170.0f * 0.6f) * l * kk;
                b = b * l * 0.92f * (1.0f - kk) + (b * 0.5f + 95.0f * 0.6f) * l * kk;
                alpha *= wv;
            }
            else
                alpha *= 0.65f + 0.35f * wv;   // screen light: behind dirty glass, barely worn

            p->setARGB ((uint8_t) juce::jlimit (0.0f, 255.0f, alpha), (uint8_t) juce::jlimit (0.0f, 255.0f, r),
                        (uint8_t) juce::jlimit (0.0f, 255.0f, g), (uint8_t) juce::jlimit (0.0f, 255.0f, b));
            p->premultiply();
        }
    }
}

void Face::addHeading (const juce::String& s, float x0, float x1, float y)
{
    headings.push_back ({ s, x0, x1, y });
    cache = {};
}

void Face::addWindow (juce::Rectangle<float> r)
{
    windows.push_back (r);
    cache = {};
}

void Face::paintStatic (juce::Graphics& g)
{
    const float W = (float) getWidth();
    g.setColour (th.ink);
    text (g, "MANNERS!", W * 0.5f, 72.0f, 52.0f, 0.02f);

    for (auto& h : headings)
    {
        g.setColour (th.ink);
        const float mid = (h.x0 + h.x1) * 0.5f;
        text (g, h.text, mid, h.y, 22.0f, 0.22f);
        const float tw = juce::GlyphArrangement::getStringWidth (font (22.0f, 0.22f), h.text);
        g.setColour (th.ink.withAlpha (140.0f / 255.0f));
        g.fillRect (h.x0, h.y - 0.6f, mid - tw * 0.5f - 14.0f - h.x0, 1.2f);
        g.fillRect (mid + tw * 0.5f + 14.0f, h.y - 0.6f, h.x1 - (mid + tw * 0.5f + 14.0f), 1.2f);
    }

    g.setColour (th.ink);
    for (auto& r : windows)
        g.drawRoundedRectangle (r.reduced (1.1f), 14.0f, 2.2f);

    // the rule above the name strip
    g.setColour (th.ink.withAlpha (150.0f / 255.0f));
    g.fillRect (36.0f, 794.3f, W - 72.0f, 1.4f);
}

void Face::paint (juce::Graphics& g)
{
    const float s = juce::jlimit (0.25f, 8.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (cache.isNull() || std::abs (s - cacheScale) > 0.01f)
    {
        cache = juce::Image (juce::Image::RGB, juce::jmax (1, (int) std::ceil ((float) getWidth() * s)),
                             juce::jmax (1, (int) std::ceil ((float) getHeight() * s)), false, juce::SoftwareImageType());
        juce::Graphics cg (cache);
        cg.addTransform (juce::AffineTransform::scale (s));
        cg.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        cg.drawImage (surf, getLocalBounds().toFloat());

        // displays are recessed: darker glass, and a shadow falling in from the top lip
        for (auto& r : windows)
        {
            juce::Path p;
            p.addRoundedRectangle (r, 14.0f);
            cg.setColour (juce::Colours::black.withAlpha (0.55f));
            cg.fillPath (p);
            juce::Graphics::ScopedSaveState ss (cg);
            cg.reduceClipRegion (p);
            cg.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), 0.0f, r.getY(),
                                                      juce::Colours::transparentBlack, 0.0f, r.getY() + 20.0f, false));
            cg.fillRect (r);
            cg.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.3f), r.getX(), 0.0f,
                                                      juce::Colours::transparentBlack, r.getX() + 14.0f, 0.0f, false));
            cg.fillRect (r);
        }

        paintWorn (cg, *this, [this] (juce::Graphics& lg) { paintStatic (lg); });
        cacheScale = s;
    }
    g.drawImageTransformed (cache, juce::AffineTransform::scale (1.0f / s));
}

//==============================================================================
static Face* faceOf (juce::Component& c) { return c.findParentComponentOfClass<Face>(); }
static juce::Colour inkOf (juce::Component& c) { auto* f = faceOf (c); return f != nullptr ? f->theme().ink : juce::Colours::white; }

Dial::Dial (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text_,
            float radius, float labelSize_, Label l)
    : label (text_), r (radius), labelSize (labelSize_), where (l)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (240);
    setVelocityBasedMode (false);
    setTitle (label);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, *this);
    param = state.getParameter (paramId);
    if (param != nullptr)
        setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

juce::Point<float> Dial::knobCentre() const { return { where == Label::below ? (float) getWidth() * 0.5f : r + 6.0f, r + 6.0f }; }

void Dial::place (float cx, float cy)
{
    const float tw = juce::GlyphArrangement::getStringWidth (font (labelSize, 0.04f), label);
    if (where == Label::below)
    {
        const float w = juce::jmax (2.0f * r + 12.0f, tw + 16.0f);
        const float h = 2.0f * r + 12.0f + 18.0f + labelSize;
        setBounds (juce::Rectangle<float> (cx - w * 0.5f, cy - r - 6.0f, w, h).getSmallestIntegerContainer());
    }
    else
    {
        const float w = r + 6.0f + r + 34.0f + tw + 12.0f;
        setBounds (juce::Rectangle<float> (cx - r - 6.0f, cy - r - 6.0f, w, 2.0f * r + 12.0f).getSmallestIntegerContainer());
    }
}

void Dial::paint (juce::Graphics& g)
{
    const auto ink = inkOf (*this);
    const auto c = knobCentre();
    const auto pos = (float) valueToProportionOfLength (getValue());
    paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        lg.setColour (ink);
        const float w1 = juce::jmax (1.8f, r * 0.011f);
        const float gap = juce::jmax (3.2f, r * 0.028f);
        const float w2 = juce::jmax (3.0f, r * 0.02f);
        lg.drawEllipse (juce::Rectangle<float> (2.0f * (r - w1 * 0.5f), 2.0f * (r - w1 * 0.5f)).withCentre (c), w1);
        const float r2 = r - gap - w2 * 0.5f;
        lg.drawEllipse (juce::Rectangle<float> (2.0f * r2, 2.0f * r2).withCentre (c), w2);

        const float a = juce::degreesToRadians (-135.0f + 270.0f * pos);
        const float sx = std::sin (a), sy = -std::cos (a);
        juce::Path ptr;
        ptr.startNewSubPath (c.x + sx * r * 0.44f, c.y + sy * r * 0.44f);
        ptr.lineTo (c.x + sx * r * 0.74f, c.y + sy * r * 0.74f);
        lg.strokePath (ptr, juce::PathStrokeType (juce::jmax (3.8f, r * 0.03f), juce::PathStrokeType::mitered,
                                                  juce::PathStrokeType::butt));

        if (where == Label::below)
            text (lg, label, c.x, c.y + r + 24.0f, labelSize, 0.04f);
        else
            text (lg, label, c.x + r + 34.0f, c.y, labelSize, 0.02f, juce::Justification::centredLeft, 600.0f);
    });
}

void Dial::startedDragging()
{
    if (auto* f = faceOf (*this))
        if (f->onGesture)
            f->onGesture();
}

void Dial::readout()
{
    if (! (isMouseOverOrDragging() || isMouseButtonDown()))
        return;
    if (auto* f = faceOf (*this))
        if (f->onReadout && param != nullptr)
            f->onReadout (label + "   " + param->getCurrentValueAsText().toUpperCase());
}

//==============================================================================
Lamp::Lamp (const juce::String& word, float s) : juce::Button (word), size (s)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Lamp::place (float lampX, float y)
{
    const float tw = juce::GlyphArrangement::getStringWidth (font (size, 0.07f), getButtonText());
    setBounds (juce::Rectangle<float> (lampX - 12.0f, y - 18.0f, 30.0f + tw + 10.0f, 36.0f).getSmallestIntegerContainer());
}

void Lamp::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto ink = inkOf (*this);
    const bool on = getToggleState() || down;
    paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        const auto sq = juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ 12.0f, 18.0f });
        lg.setColour (ink);
        if (on)
            lg.fillRect (sq);
        else
            lg.drawRect (sq.reduced (0.4f), 1.6f);
        lg.setColour (over ? ink.brighter (0.25f) : ink);
        manners::text (lg, getButtonText(), 30.0f, 18.0f, size, 0.07f, juce::Justification::centredLeft, 400.0f);
    });
}

//==============================================================================
NameStrip::NameStrip()
{
    setTitle ("Name and preset");
}

void NameStrip::show (const juce::String& s)
{
    shown = s;
    repaint();
    startTimer (1400);
}

juce::Rectangle<float> NameStrip::presetArea() const
{
    if (preset.isEmpty())
        return {};
    const float tw = juce::GlyphArrangement::getStringWidth (font (22.0f, 0.08f), preset);
    const float right = (float) getWidth() - 14.0f;
    return { right - tw - 48.0f, 8.0f, tw + 48.0f, (float) getHeight() - 16.0f };
}

void NameStrip::paint (juce::Graphics& g)
{
    auto* f = faceOf (*this);
    const auto ink = inkOf (*this);
    const juce::String name = shown.isNotEmpty() ? shown : (f != nullptr ? f->theme().name : juce::String());
    paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        lg.setColour (ink);
        text (lg, name, (float) getWidth() * 0.5f, 29.0f, 37.0f, 0.10f);

        const auto pa = presetArea();
        if (! pa.isEmpty())
        {
            lg.setColour (ink.withAlpha (0.8f));
            text (lg, preset, pa.getRight() - 22.0f, 29.0f, 22.0f, 0.08f, juce::Justification::centredRight, 600.0f);
            auto chevron = [&lg] (float x, float dir)
            {
                juce::Path p;
                p.startNewSubPath (x + 3.5f * dir, 23.0f);
                p.lineTo (x - 3.5f * dir, 29.0f);
                p.lineTo (x + 3.5f * dir, 35.0f);
                lg.strokePath (p, juce::PathStrokeType (1.8f));
            };
            chevron (pa.getX() + 8.0f, 1.0f);
            chevron (pa.getRight() - 6.0f, -1.0f);
        }
    });
}

void NameStrip::mouseUp (const juce::MouseEvent& e)
{
    const auto pa = presetArea();
    if (onStep && pa.contains (e.position))
        onStep (e.position.x < pa.getCentreX() ? -1 : 1);
}

//==============================================================================
void MenuIcon::paint (juce::Graphics& g)
{
    const auto ink = inkOf (*this);
    const bool over = isMouseOver();
    paintWorn (g, *this, [&] (juce::Graphics& lg)
    {
        lg.setColour (over ? ink.brighter (0.25f) : ink);
        for (int i = 0; i < 3; ++i)
            lg.fillRoundedRectangle (10.0f, 10.5f + 13.0f * (float) i, 54.0f, 5.0f, 2.5f);
    });
}
}
