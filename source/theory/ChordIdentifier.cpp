#include "ChordIdentifier.h"

#include "DiatonicHarmony.h"

#include <array>
#include <set>

namespace setle::theory
{
namespace
{
struct Pattern
{
    juce::String quality;
    juce::String suffix;
    std::vector<int> intervals;
};

const std::array<Pattern, 10>& patterns()
{
    static const std::array<Pattern, 10> table
    {{
        { "major", "", { 0, 4, 7 } },
        { "minor", "m", { 0, 3, 7 } },
        { "diminished", "dim", { 0, 3, 6 } },
        { "augmented", "aug", { 0, 4, 8 } },
        { "major7", "maj7", { 0, 4, 7, 11 } },
        { "minor7", "m7", { 0, 3, 7, 10 } },
        { "dominant7", "7", { 0, 4, 7, 10 } },
        { "halfDiminished", "m7b5", { 0, 3, 6, 10 } },
        { "sus2", "sus2", { 0, 2, 7 } },
        { "sus4", "sus4", { 0, 5, 7 } }
    }};
    return table;
}

std::set<int> normalizePitchClassSet(const std::vector<int>& pcs)
{
    std::set<int> normalized;
    for (const auto pc : pcs)
        normalized.insert((pc % 12 + 12) % 12);
    return normalized;
}

float scorePattern(const std::set<int>& input, int rootPc, const Pattern& pattern)
{
    int matched = 0;
    for (const auto interval : pattern.intervals)
    {
        const int expected = (rootPc + interval) % 12;
        if (input.find(expected) != input.end())
            ++matched;
    }

    return static_cast<float>(matched) / static_cast<float>(juce::jmax(1, static_cast<int>(pattern.intervals.size())));
}
} // namespace

ChordIdentification identify(const std::vector<int>& pitchClasses)
{
    ChordIdentification best;
    const auto normalized = normalizePitchClassSet(pitchClasses);

    if (normalized.empty())
        return best;

    for (int rootPc = 0; rootPc < 12; ++rootPc)
    {
        const auto rootName = DiatonicHarmony::rootNameForPitchClass(rootPc);
        for (const auto& pattern : patterns())
        {
            const auto confidence = scorePattern(normalized, rootPc, pattern);
            if (confidence > best.confidence)
            {
                best.confidence = confidence;
                best.root = rootName;
                best.quality = pattern.quality;
                best.symbol = rootName + pattern.suffix;
            }
        }
    }

    return best;
}

} // namespace setle::theory
