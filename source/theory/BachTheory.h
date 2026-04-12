#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace setle::theory
{

class BachTheory
{
public:
    static bool isAvailable() noexcept { return false; }
    static std::vector<int> getChordPitchClasses(const juce::String& chordSymbol);
};

} // namespace setle::theory
