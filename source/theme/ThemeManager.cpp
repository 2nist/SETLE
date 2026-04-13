#include "ThemeManager.h"

namespace
{
struct ColourField
{
    const char* name;
    juce::Colour ThemeData::* member;
};

struct FloatField
{
    const char* name;
    float ThemeData::* member;
};

constexpr ColourField kColourFields[] {
    { "surface0", &ThemeData::surface0 },
    { "surface1", &ThemeData::surface1 },
    { "surface2", &ThemeData::surface2 },
    { "surface3", &ThemeData::surface3 },
    { "surface4", &ThemeData::surface4 },
    { "surfaceEdge", &ThemeData::surfaceEdge },

    { "inkLight", &ThemeData::inkLight },
    { "inkMid", &ThemeData::inkMid },
    { "inkMuted", &ThemeData::inkMuted },
    { "inkGhost", &ThemeData::inkGhost },
    { "inkDark", &ThemeData::inkDark },

    { "accent", &ThemeData::accent },
    { "accentWarm", &ThemeData::accentWarm },
    { "accentDim", &ThemeData::accentDim },

    { "headerBg", &ThemeData::headerBg },
    { "cardBg", &ThemeData::cardBg },
    { "rowBg", &ThemeData::rowBg },
    { "rowHover", &ThemeData::rowHover },
    { "rowSelected", &ThemeData::rowSelected },
    { "badgeBg", &ThemeData::badgeBg },
    { "controlBg", &ThemeData::controlBg },
    { "controlOnBg", &ThemeData::controlOnBg },
    { "controlText", &ThemeData::controlText },
    { "controlTextOn", &ThemeData::controlTextOn },
    { "focusOutline", &ThemeData::focusOutline },
    { "sliderTrack", &ThemeData::sliderTrack },
    { "sliderThumb", &ThemeData::sliderThumb },

    { "zoneA", &ThemeData::zoneA },
    { "zoneB", &ThemeData::zoneB },
    { "zoneC", &ThemeData::zoneC },
    { "zoneD", &ThemeData::zoneD },
    { "zoneMenu", &ThemeData::zoneMenu },

    { "zoneBgA", &ThemeData::zoneBgA },
    { "zoneBgB", &ThemeData::zoneBgB },
    { "zoneBgC", &ThemeData::zoneBgC },
    { "zoneBgD", &ThemeData::zoneBgD },

    { "signalAudio", &ThemeData::signalAudio },
    { "signalMidi", &ThemeData::signalMidi },
    { "signalCv", &ThemeData::signalCv },
    { "signalGate", &ThemeData::signalGate },

    { "tapeBase", &ThemeData::tapeBase },
    { "tapeClipBg", &ThemeData::tapeClipBg },
    { "tapeClipBorder", &ThemeData::tapeClipBorder },
    { "tapeSeam", &ThemeData::tapeSeam },
    { "tapeBeatTick", &ThemeData::tapeBeatTick },
    { "playheadColor", &ThemeData::playheadColor },
    { "housingEdge", &ThemeData::housingEdge }
};

constexpr FloatField kFloatFields[] {
    { "sizeDisplay", &ThemeData::sizeDisplay },
    { "sizeHeading", &ThemeData::sizeHeading },
    { "sizeLabel", &ThemeData::sizeLabel },
    { "sizeBody", &ThemeData::sizeBody },
    { "sizeMicro", &ThemeData::sizeMicro },
    { "sizeValue", &ThemeData::sizeValue },
    { "sizeTransport", &ThemeData::sizeTransport },

    { "spaceXs", &ThemeData::spaceXs },
    { "spaceSm", &ThemeData::spaceSm },
    { "spaceMd", &ThemeData::spaceMd },
    { "spaceLg", &ThemeData::spaceLg },
    { "spaceXl", &ThemeData::spaceXl },
    { "spaceXxl", &ThemeData::spaceXxl },

    { "menuBarHeight", &ThemeData::menuBarHeight },
    { "zoneAWidth", &ThemeData::zoneAWidth },
    { "zoneCWidth", &ThemeData::zoneCWidth },
    { "zoneDNormHeight", &ThemeData::zoneDNormHeight },
    { "moduleSlotHeight", &ThemeData::moduleSlotHeight },
    { "stepHeight", &ThemeData::stepHeight },
    { "stepWidth", &ThemeData::stepWidth },
    { "knobSize", &ThemeData::knobSize },

    { "radiusXs", &ThemeData::radiusXs },
    { "radiusChip", &ThemeData::radiusChip },
    { "radiusSm", &ThemeData::radiusSm },
    { "radiusMd", &ThemeData::radiusMd },
    { "radiusLg", &ThemeData::radiusLg },

    { "strokeSubtle", &ThemeData::strokeSubtle },
    { "strokeNormal", &ThemeData::strokeNormal },
    { "strokeAccent", &ThemeData::strokeAccent },
    { "strokeThick", &ThemeData::strokeThick },

    { "btnCornerRadius", &ThemeData::btnCornerRadius },
    { "btnBorderStrength", &ThemeData::btnBorderStrength },
    { "btnFillStrength", &ThemeData::btnFillStrength },
    { "btnOnFillStrength", &ThemeData::btnOnFillStrength },
    { "sliderTrackThickness", &ThemeData::sliderTrackThickness },
    { "sliderCornerRadius", &ThemeData::sliderCornerRadius },
    { "sliderThumbSize", &ThemeData::sliderThumbSize },
    { "knobRingThickness", &ThemeData::knobRingThickness },
    { "knobCapSize", &ThemeData::knobCapSize },
    { "knobDotSize", &ThemeData::knobDotSize },
    { "switchWidth", &ThemeData::switchWidth },
    { "switchHeight", &ThemeData::switchHeight },
    { "switchCornerRadius", &ThemeData::switchCornerRadius },
    { "switchThumbInset", &ThemeData::switchThumbInset }
};

juce::String colourToString(juce::Colour colour)
{
    return "0x" + colour.toString().toUpperCase();
}

juce::Colour stringToColour(const juce::String& text)
{
    if (text.startsWithIgnoreCase("0x"))
        return juce::Colour(static_cast<uint32_t>(text.substring(2).getHexValue64()));

    return juce::Colour::fromString(text);
}

ThemeData readThemeData(const juce::ValueTree& tree, const ThemeData& baseTheme)
{
    if (!tree.isValid() || tree.getType().toString() != "ThemeData")
        return baseTheme;

    ThemeData out = baseTheme;

    for (const auto& field : kColourFields)
    {
        if (tree.hasProperty(field.name))
            out.*(field.member) = stringToColour(tree[field.name].toString());
    }

    for (const auto& field : kFloatFields)
    {
        if (tree.hasProperty(field.name))
            out.*(field.member) = static_cast<float>(tree[field.name]);
    }

    if (!tree.hasProperty("radiusMd") && tree.hasProperty("surfaceRadius"))
        out.radiusMd = static_cast<float>(tree["surfaceRadius"]);

    if (tree.hasProperty("presetName"))
        out.presetName = tree["presetName"].toString();

    return out;
}

void writeThemeData(const ThemeData& theme, juce::ValueTree& tree)
{
    for (const auto& field : kColourFields)
        tree.setProperty(field.name, colourToString(theme.*(field.member)), nullptr);

    for (const auto& field : kFloatFields)
        tree.setProperty(field.name, theme.*(field.member), nullptr);

    // Legacy alias for older theme files.
    tree.setProperty("surfaceRadius", theme.radiusMd, nullptr);
    tree.setProperty("presetName", theme.presetName, nullptr);
}
} // namespace

ThemeManager& ThemeManager::get()
{
    static ThemeManager instance;
    return instance;
}

void ThemeManager::setColour(juce::Colour ThemeData::* member, juce::Colour value)
{
    currentTheme.*member = value;
    broadcast();
}

void ThemeManager::setFloat(float ThemeData::* member, float value)
{
    currentTheme.*member = value;
    broadcast();
}

void ThemeManager::applyTheme(const ThemeData& themeData)
{
    currentTheme = themeData;
    broadcast();
}

void ThemeManager::addListener(Listener* listener)
{
    listeners.add(listener);
}

void ThemeManager::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

juce::ValueTree ThemeManager::toValueTree() const
{
    juce::ValueTree tree("ThemeData");
    writeThemeData(currentTheme, tree);
    return tree;
}

void ThemeManager::fromValueTree(const juce::ValueTree& tree)
{
    if (!tree.isValid() || tree.getType().toString() != "ThemeData")
        return;

    currentTheme = readThemeData(tree, currentTheme);
    broadcast();
}

void ThemeManager::saveToFile(const juce::File& file) const
{
    getUserThemeDir().createDirectory();
    if (auto xml = toValueTree().createXml())
        xml->writeTo(file);
}

bool ThemeManager::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        const auto valueTree = juce::ValueTree::fromXml(*xml);
        if (valueTree.isValid())
        {
            fromValueTree(valueTree);
            return true;
        }
    }

    return false;
}

ThemeData ThemeManager::themeFromValueTree(const juce::ValueTree& tree)
{
    ThemeData defaults;
    return readThemeData(tree, defaults);
}

juce::File ThemeManager::getUserThemeDir() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SETLE")
        .getChildFile("setle-themes");
}

juce::File ThemeManager::getCurrentThemeFile() const
{
    return getUserThemeDir().getChildFile("current-theme.setle-theme");
}

void ThemeManager::timerCallback()
{
    stopTimer();
    saveToFile(getCurrentThemeFile());
}

void ThemeManager::broadcast()
{
    listeners.call(&Listener::themeChanged);
    startTimer(500);
}
