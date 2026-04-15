#include "ThemePresets.h"

#include "ThemeManager.h"

ThemeData ThemePresets::slateDark()
{
    ThemeData t;
    t.presetName = "Slate Dark";

    t.surface0 = juce::Colour(0xFF10161d);
    t.surface1 = juce::Colour(0xFF141d26);
    t.surface2 = juce::Colour(0xFF1a2631);
    t.surface3 = juce::Colour(0xFF223140);
    t.surface4 = juce::Colour(0xFF2b3c4d);
    t.surfaceEdge = juce::Colour(0xFF4a5a63);

    t.accent = juce::Colour(0xFFd67f5c);
    t.accentWarm = juce::Colour(0xFFee7c4a);
    t.zoneA = juce::Colour(0xFF4a9eff);
    t.zoneB = juce::Colour(0xFFd67f5c);
    t.zoneC = juce::Colour(0xFF4aee8a);
    t.zoneD = juce::Colour(0xFFeec44a);

    return t;
}

ThemeData ThemePresets::espresso()
{
    ThemeData t;
    t.presetName = "Espresso";
    t.accent = juce::Colour(0xFFc4822a);
    t.accentWarm = juce::Colour(0xFFd4603a);
    t.zoneB = juce::Colour(0xFFc4822a);
    return t;
}

ThemeData ThemePresets::midnight()
{
    auto t = slateDark();
    t.presetName = "Midnight";
    t.surface0 = juce::Colour(0xFF080810);
    t.surface1 = juce::Colour(0xFF0e0e1a);
    t.surface2 = juce::Colour(0xFF161626);
    t.surface3 = juce::Colour(0xFF1e1e32);
    t.surface4 = juce::Colour(0xFF26263e);
    t.surfaceEdge = juce::Colour(0xFF32324a);
    t.accent = juce::Colour(0xFF4a9eff);
    t.zoneA = juce::Colour(0xFF4a9eff);
    t.zoneB = juce::Colour(0xFF4a9eff);
    return t;
}

ThemeData ThemePresets::highContrast()
{
    auto t = slateDark();
    t.presetName = "High Contrast";
    t.surface0 = juce::Colours::black;
    t.surface1 = juce::Colour(0xFF0a0a0a);
    t.surface2 = juce::Colour(0xFF141414);
    t.surface3 = juce::Colour(0xFF1e1e1e);
    t.surface4 = juce::Colour(0xFF282828);
    t.surfaceEdge = juce::Colour(0xFF4b4b4b);
    t.inkLight = juce::Colours::white;
    t.inkMid = juce::Colour(0xFFe8e8e8);
    t.inkMuted = juce::Colour(0xFFd0d0d0);
    t.accent = juce::Colours::white;
    t.zoneA = juce::Colours::white;
    t.zoneB = juce::Colours::white;
    t.zoneC = juce::Colours::white;
    t.zoneD = juce::Colours::white;
    return t;
}

ThemeData ThemePresets::neon()
{
    auto t = slateDark();
    t.presetName = "Neon";
    t.surface0 = juce::Colour(0xFF0a0a0c);
    t.surface1 = juce::Colour(0xFF121216);
    t.surface2 = juce::Colour(0xFF1a1a20);
    t.surface3 = juce::Colour(0xFF22222a);
    t.surface4 = juce::Colour(0xFF2a2a34);
    t.surfaceEdge = juce::Colour(0xFF363644);
    t.accent = juce::Colour(0xFFee7c4a);
    t.accentWarm = juce::Colour(0xFFff9b44);
    t.zoneB = juce::Colour(0xFFee7c4a);
    return t;
}

juce::Array<ThemeData> ThemePresets::builtInPresets()
{
    return { ThemeManager::loadDefaultTheme(), slateDark(), espresso(), midnight(), highContrast(), neon() };
}

ThemeData ThemePresets::presetByName(const juce::String& name)
{
    for (const auto& theme : builtInPresets())
        if (theme.presetName == name)
            return theme;

    for (const auto& theme : userPresets())
        if (theme.presetName == name)
            return theme;

    return ThemeManager::loadDefaultTheme();
}

juce::Array<ThemeData> ThemePresets::userPresets()
{
    juce::Array<ThemeData> out;
    const auto dir = ThemeManager::get().getUserThemeDir();
    if (!dir.isDirectory())
        return out;

    for (const auto& file : juce::RangedDirectoryIterator(dir, false, "*.setle-theme"))
    {
        if (auto xml = juce::XmlDocument::parse(file.getFile()))
        {
            const auto tree = juce::ValueTree::fromXml(*xml);
            if (tree.isValid())
                out.add(ThemeManager::themeFromValueTree(tree));
        }
    }

    return out;
}

void ThemePresets::saveUserPreset(const ThemeData& theme)
{
    const auto dir = ThemeManager::get().getUserThemeDir();
    dir.createDirectory();

    const auto safeName = theme.presetName.replaceCharacters("/\\:*?\"<>|", "_________");
    const auto file = dir.getChildFile(safeName + ".setle-theme");

    const auto previous = ThemeManager::get().theme();
    ThemeManager::get().applyTheme(theme);
    ThemeManager::get().saveToFile(file);
    ThemeManager::get().applyTheme(previous);
}

void ThemePresets::deleteUserPreset(const juce::String& name)
{
    const auto safeName = name.replaceCharacters("/\\:*?\"<>|", "_________");
    ThemeManager::get().getUserThemeDir().getChildFile(safeName + ".setle-theme").deleteFile();
}
