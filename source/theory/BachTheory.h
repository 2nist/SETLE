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

    // Overload that resolves Roman numeral symbols (e.g. "V7", "viio", "ii")
    // using the supplied key and mode before delegating to the symbol parser.
    // Falls through to the single-arg overload for non-Roman symbols.
    static std::vector<int> getChordPitchClasses(const juce::String& chordSymbol,
                                                  const juce::String& sessionKey,
                                                  const juce::String& sessionMode);
};

} // namespace setle::theory
