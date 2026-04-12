#pragma once

#include <array>
#include <vector>

#include <juce_core/juce_core.h>

namespace DiatonicHarmony
{
struct DiatonicChordInfo
{
    juce::String roman;
    juce::String root;
    juce::String type;
    juce::String function;
    int degreeIndex = 0;
};

struct ProgressionTemplate
{
    juce::String id;
    juce::String name;
    juce::String summary;
    std::vector<int> degrees;
};

std::array<int, 7> modeIntervals(const juce::String& mode);
std::array<juce::String, 7> modeQualities(const juce::String& mode);
std::array<juce::String, 7> modeFunctions(const juce::String& mode);
std::array<juce::String, 7> modeRomanNumerals(const juce::String& mode);

int pitchClassForRoot(const juce::String& root);
juce::String rootNameForPitchClass(int pitchClass);
int degreeIndexForRoman(const juce::String& roman);

DiatonicChordInfo buildDiatonicChord(const juce::String& keyRoot,
                                     const juce::String& mode,
                                     int degreeIndex);

const std::vector<ProgressionTemplate>& progressionPresets();
const std::vector<ProgressionTemplate>& cadencePresets();
}
