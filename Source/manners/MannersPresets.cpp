#include "MannersPresets.h"
#include <numeric>
#include <random>

namespace manners
{
static const juce::Identifier kNameProp { "presetName" };
static constexpr const char* kExt = ".manners";

Presets::Presets (juce::AudioProcessorValueTreeState& s, juce::UndoManager& u, Setup su)
    : undo (u), state (s), setup (std::move (su))
{
    if (! setup.factory.empty())
        state.state.setProperty (kNameProp, setup.factory.front().name, nullptr);
}

juce::String Presets::currentName() const { return state.state.getProperty (kNameProp).toString(); }

void Presets::setName (const juce::String& n)
{
    state.state.setProperty (kNameProp, n, nullptr);
    if (onChange)
        onChange();
}

juce::File Presets::folder() const
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory)
        .getChildFile ("manners! DSP").getChildFile (setup.plugin).getChildFile ("Presets");
}

juce::Array<juce::File> Presets::userPresets() const
{
    auto files = folder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExt);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });
    return files;
}

void Presets::setReal (const juce::String& id, float value)
{
    if (auto* p = state.getParameter (id))
    {
        const float v = p->convertTo0to1 (value);
        if (std::abs (p->getValue() - v) < 1.0e-6f)
            return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    }
}

static juce::Array<juce::RangedAudioParameter*> rangedParams (juce::AudioProcessorValueTreeState& s)
{
    juce::Array<juce::RangedAudioParameter*> out;
    for (auto* p : s.processor.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (p))
            out.add (r);
    return out;
}

void Presets::loadFactory (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) setup.factory.size()))
        return;
    const auto& fp = setup.factory[(size_t) index];
    beginGesture ("Load " + fp.name);
    for (auto* p : rangedParams (state))
    {
        const auto id = p->getParameterID();
        if (setup.performance.contains (id) || setup.levels.contains (id))
            continue;
        float v = p->convertFrom0to1 (p->getDefaultValue());
        for (auto& [k, val] : fp.values)
            if (k == id)
                v = val;
        setReal (id, v);
    }
    setName (fp.name);
}

std::unique_ptr<juce::XmlElement> Presets::paramsXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("MANNERS");
    xml->setAttribute ("plugin", setup.plugin);
    xml->setAttribute ("name", currentName());
    for (auto* p : rangedParams (state))
    {
        if (setup.performance.contains (p->getParameterID()))
            continue;
        auto* e = xml->createNewChildElement ("PARAM");
        e->setAttribute ("id", p->getParameterID());
        e->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
    }
    return xml;
}

void Presets::applyXml (const juce::XmlElement& xml, bool includeLevels)
{
    for (auto* e : xml.getChildWithTagNameIterator ("PARAM"))
    {
        const auto id = e->getStringAttribute ("id");
        if (setup.performance.contains (id) || (! includeLevels && setup.levels.contains (id)))
            continue;
        setReal (id, (float) e->getDoubleAttribute ("value"));
    }
}

bool Presets::loadFile (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("MANNERS") || xml->getStringAttribute ("plugin") != setup.plugin)
        return false;
    beginGesture ("Load preset");
    applyXml (*xml, true);
    setName (xml->getStringAttribute ("name", f.getFileNameWithoutExtension()));
    return true;
}

bool Presets::save (const juce::String& rawName)
{
    const auto name = rawName.trim().toUpperCase();
    if (name.isEmpty())
        return false;
    const auto dir = folder();
    if (! dir.createDirectory())
        return false;
    state.state.setProperty (kNameProp, name, nullptr);
    auto xml = paramsXml();
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + kExt);
    if (! xml->writeTo (file))
        return false;
    setName (name);
    return true;
}

void Presets::step (int delta)
{
    juce::StringArray names;
    for (auto& fp : setup.factory)
        names.add (fp.name);
    const auto files = userPresets();
    const int nf = names.size();
    for (auto& f : files)
        names.add (f.getFileNameWithoutExtension());
    if (names.isEmpty())
        return;

    int i = names.indexOf (currentName());
    i = i < 0 ? (delta > 0 ? 0 : names.size() - 1) : (i + delta + names.size()) % names.size();
    if (i < nf)
        loadFactory (i);
    else
        loadFile (files[i - nf]);
}

void Presets::init()
{
    beginGesture ("Init");
    for (auto* p : rangedParams (state))
        if (! setup.performance.contains (p->getParameterID()))
            setReal (p->getParameterID(), p->convertFrom0to1 (p->getDefaultValue()));
    setName ("INIT");
}

void Presets::randomise()
{
    if (setup.randomise.empty())
        return;
    beginGesture ("Randomise");
    // shove a handful of knobs somewhere unexpected, never fewer than three
    std::vector<size_t> order (setup.randomise.size());
    std::iota (order.begin(), order.end(), size_t (0));
    std::shuffle (order.begin(), order.end(), std::mt19937 ((unsigned) random.nextInt()));
    const size_t n = juce::jmin (order.size(), (size_t) 3 + (size_t) random.nextInt ((int) order.size() / 2 + 1));
    for (size_t k = 0; k < n; ++k)
    {
        const auto& rr = setup.randomise[order[k]];
        setReal (rr.id, rr.lo + random.nextFloat() * (rr.hi - rr.lo));
    }
    setName ("RANDOMISED");
}

void Presets::copyToClipboard() const
{
    juce::SystemClipboard::copyTextToClipboard (paramsXml()->toString());
}

bool Presets::pasteFromClipboard()
{
    auto xml = juce::XmlDocument::parse (juce::SystemClipboard::getTextFromClipboard());
    if (xml == nullptr || ! xml->hasTagName ("MANNERS") || xml->getStringAttribute ("plugin") != setup.plugin)
        return false;
    beginGesture ("Paste settings");
    applyXml (*xml, true);
    setName (xml->getStringAttribute ("name", "PASTED"));
    return true;
}
}
