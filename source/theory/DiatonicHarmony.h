#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace setle::theory
{

class DiatonicHarmony
{
public:
    static int pitchClassForRoot(const juce::String& root);
    static std::vector<int> modeIntervals(const juce::String& mode);
};

} // namespace setle::theory
