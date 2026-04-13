#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace setle::model
{
class Chord;
} // namespace setle::model

namespace setle::theory
{

struct MeterContext;

struct ProgressionTemplate
{
    juce::String id;
    juce::String name;
    juce::String summary;
    std::vector<int>    degrees;
    std::vector<double> durationEighths;  // optional — empty = use default (bar per chord)
};

class DiatonicHarmony
{
public:
    static int pitchClassForRoot(const juce::String& root);
    static std::vector<int> modeIntervals(const juce::String& mode);
};

// Build a chord progression from a template,
// using the meter to determine default chord durations
std::vector<setle::model::Chord> buildProgressionFromTemplate(
    const juce::String& keyRoot,
    const juce::String& mode,
    const ProgressionTemplate& preset,
    const MeterContext& meter);

} // namespace setle::theory
