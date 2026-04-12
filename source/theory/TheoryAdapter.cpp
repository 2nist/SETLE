#include "TheoryAdapter.h"

#include "BachTheory.h"
#include "DiatonicHarmony.h"

namespace setle::theory
{

std::vector<int> TheoryAdapter::getScalePitchClasses(const juce::String& root, const juce::String& mode) const
{
    const auto tonic = DiatonicHarmony::pitchClassForRoot(root);
    const auto intervals = DiatonicHarmony::modeIntervals(mode);

    std::vector<int> out;
    out.reserve(intervals.size());
    for (const auto interval : intervals)
        out.push_back((tonic + interval) % 12);

    return out;
}

std::vector<int> TheoryAdapter::getChordPitchClasses(const juce::String& chordSymbol) const
{
    return BachTheory::getChordPitchClasses(chordSymbol);
}

} // namespace setle::theory
