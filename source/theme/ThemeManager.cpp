#include "ThemeManager.h"

namespace
{
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

    tree.setProperty("surface0", colourToString(currentTheme.surface0), nullptr);
    tree.setProperty("surface1", colourToString(currentTheme.surface1), nullptr);
    tree.setProperty("surface2", colourToString(currentTheme.surface2), nullptr);
    tree.setProperty("surface3", colourToString(currentTheme.surface3), nullptr);
    tree.setProperty("surface4", colourToString(currentTheme.surface4), nullptr);
    tree.setProperty("surfaceEdge", colourToString(currentTheme.surfaceEdge), nullptr);

    tree.setProperty("inkLight", colourToString(currentTheme.inkLight), nullptr);
    tree.setProperty("inkMid", colourToString(currentTheme.inkMid), nullptr);
    tree.setProperty("inkMuted", colourToString(currentTheme.inkMuted), nullptr);
    tree.setProperty("inkGhost", colourToString(currentTheme.inkGhost), nullptr);
    tree.setProperty("inkDark", colourToString(currentTheme.inkDark), nullptr);

    tree.setProperty("accent", colourToString(currentTheme.accent), nullptr);
    tree.setProperty("accentWarm", colourToString(currentTheme.accentWarm), nullptr);
    tree.setProperty("accentDim", colourToString(currentTheme.accentDim), nullptr);

    tree.setProperty("zoneA", colourToString(currentTheme.zoneA), nullptr);
    tree.setProperty("zoneB", colourToString(currentTheme.zoneB), nullptr);
    tree.setProperty("zoneC", colourToString(currentTheme.zoneC), nullptr);
    tree.setProperty("zoneD", colourToString(currentTheme.zoneD), nullptr);

    tree.setProperty("zoneBgA", colourToString(currentTheme.zoneBgA), nullptr);
    tree.setProperty("zoneBgB", colourToString(currentTheme.zoneBgB), nullptr);
    tree.setProperty("zoneBgC", colourToString(currentTheme.zoneBgC), nullptr);
    tree.setProperty("zoneBgD", colourToString(currentTheme.zoneBgD), nullptr);

    tree.setProperty("controlBg", colourToString(currentTheme.controlBg), nullptr);
    tree.setProperty("controlOnBg", colourToString(currentTheme.controlOnBg), nullptr);
    tree.setProperty("controlText", colourToString(currentTheme.controlText), nullptr);
    tree.setProperty("controlTextOn", colourToString(currentTheme.controlTextOn), nullptr);
    tree.setProperty("sliderTrack", colourToString(currentTheme.sliderTrack), nullptr);
    tree.setProperty("sliderThumb", colourToString(currentTheme.sliderThumb), nullptr);
    tree.setProperty("focusOutline", colourToString(currentTheme.focusOutline), nullptr);

    tree.setProperty("surfaceRadius", currentTheme.radiusMd, nullptr);
    tree.setProperty("btnCornerRadius", currentTheme.btnCornerRadius, nullptr);
    tree.setProperty("sliderTrackThickness", currentTheme.sliderTrackThickness, nullptr);
    tree.setProperty("sliderThumbSize", currentTheme.sliderThumbSize, nullptr);

    tree.setProperty("presetName", currentTheme.presetName, nullptr);

    return tree;
}

void ThemeManager::fromValueTree(const juce::ValueTree& tree)
{
    if (!tree.isValid() || tree.getType().toString() != "ThemeData")
        return;

    auto readColour = [&tree](const char* name, juce::Colour& out)
    {
        if (tree.hasProperty(name))
            out = stringToColour(tree[name].toString());
    };

    auto readFloat = [&tree](const char* name, float& out)
    {
        if (tree.hasProperty(name))
            out = static_cast<float>(tree[name]);
    };

    auto readString = [&tree](const char* name, juce::String& out)
    {
        if (tree.hasProperty(name))
            out = tree[name].toString();
    };

    readColour("surface0", currentTheme.surface0);
    readColour("surface1", currentTheme.surface1);
    readColour("surface2", currentTheme.surface2);
    readColour("surface3", currentTheme.surface3);
    readColour("surface4", currentTheme.surface4);
    readColour("surfaceEdge", currentTheme.surfaceEdge);

    readColour("inkLight", currentTheme.inkLight);
    readColour("inkMid", currentTheme.inkMid);
    readColour("inkMuted", currentTheme.inkMuted);
    readColour("inkGhost", currentTheme.inkGhost);
    readColour("inkDark", currentTheme.inkDark);

    readColour("accent", currentTheme.accent);
    readColour("accentWarm", currentTheme.accentWarm);
    readColour("accentDim", currentTheme.accentDim);

    readColour("zoneA", currentTheme.zoneA);
    readColour("zoneB", currentTheme.zoneB);
    readColour("zoneC", currentTheme.zoneC);
    readColour("zoneD", currentTheme.zoneD);

    readColour("zoneBgA", currentTheme.zoneBgA);
    readColour("zoneBgB", currentTheme.zoneBgB);
    readColour("zoneBgC", currentTheme.zoneBgC);
    readColour("zoneBgD", currentTheme.zoneBgD);

    readColour("controlBg", currentTheme.controlBg);
    readColour("controlOnBg", currentTheme.controlOnBg);
    readColour("controlText", currentTheme.controlText);
    readColour("controlTextOn", currentTheme.controlTextOn);
    readColour("sliderTrack", currentTheme.sliderTrack);
    readColour("sliderThumb", currentTheme.sliderThumb);
    readColour("focusOutline", currentTheme.focusOutline);

    readFloat("btnCornerRadius", currentTheme.btnCornerRadius);
    readFloat("sliderTrackThickness", currentTheme.sliderTrackThickness);
    readFloat("sliderThumbSize", currentTheme.sliderThumbSize);
    readString("presetName", currentTheme.presetName);

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
    ThemeData theme;
    ThemeManager scratch;
    scratch.currentTheme = theme;
    scratch.fromValueTree(tree);
    return scratch.currentTheme;
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
