#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "ThemeData.h"

class ThemeManager : private juce::Timer
{
public:
    static ThemeManager& get();

    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    const ThemeData& theme() const noexcept { return currentTheme; }

    void setColour(juce::Colour ThemeData::* member, juce::Colour value);
    void setFloat(float ThemeData::* member, float value);
    void applyTheme(const ThemeData& themeData);

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void themeChanged() = 0;
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

private:
    ThemeManager() = default;
    void timerCallback() override;
    void broadcast();

    ThemeData currentTheme;
    juce::ListenerList<Listener> listeners;
};
