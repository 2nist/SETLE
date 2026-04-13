#pragma once

#include <juce_core/juce_core.h>

#include "ThemeData.h"

class ThemePresets
{
public:
    static juce::Array<ThemeData> builtInPresets();
    static ThemeData presetByName(const juce::String& name);

    static juce::Array<ThemeData> userPresets();
    static void saveUserPreset(const ThemeData& theme);
    static void deleteUserPreset(const juce::String& name);

private:
    static ThemeData slateDark();
    static ThemeData espresso();
    static ThemeData midnight();
    static ThemeData highContrast();
    static ThemeData neon();
};
