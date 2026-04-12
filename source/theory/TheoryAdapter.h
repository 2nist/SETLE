#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace setle::theory
{

class TheoryAdapter
{
public:
    std::vector<int> getScalePitchClasses(const juce::String& root, const juce::String& mode) const;
    std::vector<int> getChordPitchClasses(const juce::String& chordSymbol) const;
};

} // namespace setle::theory
