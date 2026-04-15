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

struct BoolField
{
    const char* name;
    bool ThemeData::* member;
};

constexpr ColourField kColourFields[] {
    { "windowBackground", &ThemeData::windowBackground },
    { "surfaceLow", &ThemeData::surfaceLow },
    { "surfaceVariant", &ThemeData::surfaceVariant },
    { "primaryAccent", &ThemeData::primaryAccent },
    { "secondaryAccent", &ThemeData::secondaryAccent },
    { "signalMidi", &ThemeData::signalMidi },
    { "signalAudio", &ThemeData::signalAudio },
    { "alertColor", &ThemeData::alertColor },

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
    { "switchThumbInset", &ThemeData::switchThumbInset },

    { "glassDistortion", &ThemeData::glassDistortion },
    { "glowWarmth", &ThemeData::glowWarmth },
    { "materialChassisTexture", &ThemeData::materialChassisTexture },
    { "materialGlassDepth", &ThemeData::materialGlassDepth },
    { "materialInsetFuzz", &ThemeData::materialInsetFuzz },
    { "materialGlowAmount", &ThemeData::materialGlowAmount }
};

constexpr BoolField kBoolFields[] {
    { "isPebbled", &ThemeData::isPebbled }
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

juce::Colour ThemeData::* memberForKey(ThemeManager::ThemeKey key)
{
    using ThemeKey = ThemeManager::ThemeKey;

    switch (key)
    {
        case ThemeKey::windowBackground: return &ThemeData::windowBackground;
        case ThemeKey::surfaceLow: return &ThemeData::surfaceLow;
        case ThemeKey::surfaceVariant: return &ThemeData::surfaceVariant;
        case ThemeKey::primaryAccent: return &ThemeData::primaryAccent;
        case ThemeKey::secondaryAccent: return &ThemeData::secondaryAccent;
        case ThemeKey::signalMidi: return &ThemeData::signalMidi;
        case ThemeKey::signalAudio: return &ThemeData::signalAudio;
        case ThemeKey::alertColor: return &ThemeData::alertColor;
        case ThemeKey::surface0: return &ThemeData::surface0;
        case ThemeKey::surface1: return &ThemeData::surface1;
        case ThemeKey::surface2: return &ThemeData::surface2;
        case ThemeKey::surface3: return &ThemeData::surface3;
        case ThemeKey::surface4: return &ThemeData::surface4;
        case ThemeKey::surfaceEdge: return &ThemeData::surfaceEdge;
        case ThemeKey::inkLight: return &ThemeData::inkLight;
        case ThemeKey::inkMid: return &ThemeData::inkMid;
        case ThemeKey::inkMuted: return &ThemeData::inkMuted;
        case ThemeKey::inkGhost: return &ThemeData::inkGhost;
        case ThemeKey::inkDark: return &ThemeData::inkDark;
        case ThemeKey::accent: return &ThemeData::accent;
        case ThemeKey::accentWarm: return &ThemeData::accentWarm;
        case ThemeKey::accentDim: return &ThemeData::accentDim;
        case ThemeKey::headerBg: return &ThemeData::headerBg;
        case ThemeKey::cardBg: return &ThemeData::cardBg;
        case ThemeKey::rowBg: return &ThemeData::rowBg;
        case ThemeKey::rowHover: return &ThemeData::rowHover;
        case ThemeKey::rowSelected: return &ThemeData::rowSelected;
        case ThemeKey::badgeBg: return &ThemeData::badgeBg;
        case ThemeKey::controlBg: return &ThemeData::controlBg;
        case ThemeKey::controlOnBg: return &ThemeData::controlOnBg;
        case ThemeKey::controlText: return &ThemeData::controlText;
        case ThemeKey::controlTextOn: return &ThemeData::controlTextOn;
        case ThemeKey::focusOutline: return &ThemeData::focusOutline;
        case ThemeKey::sliderTrack: return &ThemeData::sliderTrack;
        case ThemeKey::sliderThumb: return &ThemeData::sliderThumb;
        case ThemeKey::zoneA: return &ThemeData::zoneA;
        case ThemeKey::zoneB: return &ThemeData::zoneB;
        case ThemeKey::zoneC: return &ThemeData::zoneC;
        case ThemeKey::zoneD: return &ThemeData::zoneD;
        case ThemeKey::zoneMenu: return &ThemeData::zoneMenu;
        case ThemeKey::zoneBgA: return &ThemeData::zoneBgA;
        case ThemeKey::zoneBgB: return &ThemeData::zoneBgB;
        case ThemeKey::zoneBgC: return &ThemeData::zoneBgC;
        case ThemeKey::zoneBgD: return &ThemeData::zoneBgD;
        case ThemeKey::signalCv: return &ThemeData::signalCv;
        case ThemeKey::signalGate: return &ThemeData::signalGate;
        case ThemeKey::tapeBase: return &ThemeData::tapeBase;
        case ThemeKey::tapeClipBg: return &ThemeData::tapeClipBg;
        case ThemeKey::tapeClipBorder: return &ThemeData::tapeClipBorder;
        case ThemeKey::tapeSeam: return &ThemeData::tapeSeam;
        case ThemeKey::tapeBeatTick: return &ThemeData::tapeBeatTick;
        case ThemeKey::playheadColor: return &ThemeData::playheadColor;
        case ThemeKey::housingEdge: return &ThemeData::housingEdge;
    }

    return &ThemeData::primaryAccent;
}

void syncLegacyMaterialFields(ThemeData& theme)
{
    theme.glassDistortion = juce::jlimit(0.0f, 1.0f, theme.glassDistortion);
    theme.glowWarmth = juce::jlimit(0.0f, 1.0f, theme.glowWarmth);
    theme.materialChassisTexture = theme.isPebbled ? 1.0f : 0.0f;
    theme.materialGlassDepth = theme.glassDistortion;
    theme.materialGlowAmount = theme.glowWarmth;
}

void onFloatEdited(ThemeData& theme, float ThemeData::* member)
{
    if (member == &ThemeData::glassDistortion)
        theme.glassDistortion = juce::jlimit(0.0f, 1.0f, theme.glassDistortion);
    else if (member == &ThemeData::glowWarmth)
        theme.glowWarmth = juce::jlimit(0.0f, 1.0f, theme.glowWarmth);
    else if (member == &ThemeData::materialChassisTexture)
        theme.isPebbled = theme.materialChassisTexture >= 0.5f;
    else if (member == &ThemeData::materialGlassDepth)
        theme.glassDistortion = juce::jlimit(0.0f, 1.0f, theme.materialGlassDepth);
    else if (member == &ThemeData::materialGlowAmount)
        theme.glowWarmth = juce::jlimit(0.0f, 1.0f, theme.materialGlowAmount);

    syncLegacyMaterialFields(theme);
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

    for (const auto& field : kBoolFields)
    {
        if (tree.hasProperty(field.name))
            out.*(field.member) = static_cast<bool>(tree[field.name]);
    }

    if (!tree.hasProperty("isPebbled") && tree.hasProperty("materialChassisTexture"))
        out.isPebbled = static_cast<float>(tree["materialChassisTexture"]) >= 0.5f;
    if (!tree.hasProperty("glassDistortion") && tree.hasProperty("materialGlassDepth"))
        out.glassDistortion = static_cast<float>(tree["materialGlassDepth"]);
    if (!tree.hasProperty("glowWarmth") && tree.hasProperty("materialGlowAmount"))
        out.glowWarmth = static_cast<float>(tree["materialGlowAmount"]);

    syncLegacyMaterialFields(out);

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

    for (const auto& field : kBoolFields)
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

ThemeData ThemeManager::loadDefaultTheme()
{
    ThemeData theme;
    theme.presetName = "Espresso Laboratory";

    theme.windowBackground = juce::Colour(0xFF1F1A1B);
    theme.surfaceLow = juce::Colour(0xFF3A2E2E);
    theme.surfaceVariant = juce::Colour(0xFF140F0F);
    theme.primaryAccent = juce::Colour(0xFFF4C430);
    theme.secondaryAccent = juce::Colour(0xFF40E0D0);
    theme.signalMidi = juce::Colour(0xFFEE8131);
    theme.signalAudio = juce::Colour(0xFF2ABA9E);
    theme.alertColor = juce::Colour(0xFFFF4500);

    theme.surface0 = theme.windowBackground;
    theme.surface1 = theme.surfaceLow;
    theme.surface2 = theme.surfaceLow;
    theme.surface3 = theme.surfaceVariant;
    theme.surface4 = theme.surfaceLow;
    theme.surfaceEdge = theme.surfaceLow.interpolatedWith(theme.inkDark, 0.35f);

    theme.accent = theme.primaryAccent;
    theme.accentWarm = theme.signalMidi;
    theme.zoneB = theme.primaryAccent;
    theme.zoneC = theme.secondaryAccent;

    theme.isPebbled = true;
    theme.glassDistortion = 0.30f;
    theme.glowWarmth = 0.70f;
    syncLegacyMaterialFields(theme);

    return theme;
}

ThemeManager::ThemeManager()
    : currentTheme(loadDefaultTheme())
{
}

juce::Colour ThemeManager::getColor(ThemeKey key) const noexcept
{
    return currentTheme.*memberForKey(key);
}

void ThemeManager::setColor(ThemeKey key, juce::Colour value)
{
    setColour(memberForKey(key), value);
}

ThemeManager::MeltColors ThemeManager::getMeltColors() const noexcept
{
    return { getColor(ThemeKey::primaryAccent), getColor(ThemeKey::alertColor) };
}

bool ThemeManager::isPebbled() const noexcept
{
    return currentTheme.isPebbled;
}

float ThemeManager::glassDistortion() const noexcept
{
    return currentTheme.glassDistortion;
}

float ThemeManager::glowWarmth() const noexcept
{
    return currentTheme.glowWarmth;
}

void ThemeManager::setPebbled(bool enabled)
{
    currentTheme.isPebbled = enabled;
    syncLegacyMaterialFields(currentTheme);
    broadcast();
}

void ThemeManager::setGlassDistortion(float amount)
{
    currentTheme.glassDistortion = juce::jlimit(0.0f, 1.0f, amount);
    syncLegacyMaterialFields(currentTheme);
    broadcast();
}

void ThemeManager::setGlowWarmth(float amount)
{
    currentTheme.glowWarmth = juce::jlimit(0.0f, 1.0f, amount);
    syncLegacyMaterialFields(currentTheme);
    broadcast();
}

void ThemeManager::setColour(juce::Colour ThemeData::* member, juce::Colour value)
{
    currentTheme.*member = value;
    broadcast();
}

void ThemeManager::setFloat(float ThemeData::* member, float value)
{
    currentTheme.*member = value;
    onFloatEdited(currentTheme, member);
    broadcast();
}

void ThemeManager::applyTheme(const ThemeData& themeData)
{
    currentTheme = themeData;
    syncLegacyMaterialFields(currentTheme);
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
    ThemeData defaults = loadDefaultTheme();
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

void ThemeManager::setPreviewHighlightToken(const juce::String& token)
{
    const auto trimmed = token.trim();
    if (previewHighlightToken == trimmed)
        return;

    previewHighlightToken = trimmed;
    listeners.call(&Listener::themePreviewTargetChanged, previewHighlightToken);
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
