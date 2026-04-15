#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ThemeData.h"

class ThemeManager : private juce::Timer
{
public:
    enum class ThemeKey
    {
        windowBackground,
        surfaceLow,
        surfaceVariant,
        primaryAccent,
        secondaryAccent,
        signalMidi,
        signalAudio,
        alertColor,

        surface0,
        surface1,
        surface2,
        surface3,
        surface4,
        surfaceEdge,

        inkLight,
        inkMid,
        inkMuted,
        inkGhost,
        inkDark,

        accent,
        accentWarm,
        accentDim,

        headerBg,
        cardBg,
        rowBg,
        rowHover,
        rowSelected,
        badgeBg,
        controlBg,
        controlOnBg,
        controlText,
        controlTextOn,
        focusOutline,
        sliderTrack,
        sliderThumb,

        zoneA,
        zoneB,
        zoneC,
        zoneD,
        zoneMenu,
        zoneBgA,
        zoneBgB,
        zoneBgC,
        zoneBgD,
        signalCv,
        signalGate,
        tapeBase,
        tapeClipBg,
        tapeClipBorder,
        tapeSeam,
        tapeBeatTick,
        playheadColor,
        housingEdge
    };

    struct MeltColors
    {
        juce::Colour top;
        juce::Colour bottom;
    };

    static ThemeManager& get();
    static ThemeData loadDefaultTheme();

    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    const ThemeData& theme() const noexcept { return currentTheme; }
    juce::Colour getColor(ThemeKey key) const noexcept;
    void setColor(ThemeKey key, juce::Colour value);
    MeltColors getMeltColors() const noexcept;

    bool isPebbled() const noexcept;
    float glassDistortion() const noexcept;
    float glowWarmth() const noexcept;
    void setPebbled(bool enabled);
    void setGlassDistortion(float amount);
    void setGlowWarmth(float amount);

    void setColour(juce::Colour ThemeData::* member, juce::Colour value);
    void setFloat(float ThemeData::* member, float value);
    void applyTheme(const ThemeData& themeData);

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void themeChanged() = 0;
        virtual void themePreviewTargetChanged(const juce::String&) {}
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);
    void saveToFile(const juce::File& file) const;
    bool loadFromFile(const juce::File& file);
    static ThemeData themeFromValueTree(const juce::ValueTree& tree);

    juce::File getUserThemeDir() const;
    juce::File getCurrentThemeFile() const;

    void setPreviewHighlightToken(const juce::String& token);
    const juce::String& getPreviewHighlightToken() const noexcept { return previewHighlightToken; }

private:
    ThemeManager();
    void timerCallback() override;
    void broadcast();

    ThemeData currentTheme;
    juce::String previewHighlightToken;
    juce::ListenerList<Listener> listeners;
};
