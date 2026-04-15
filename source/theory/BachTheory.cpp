#include "BachTheory.h"

#include "DiatonicHarmony.h"

#include <algorithm>

namespace setle::theory
{

std::vector<int> BachTheory::getChordPitchClasses(const juce::String& chordSymbol)
{
    auto sym = chordSymbol.trim();
    if (sym.isEmpty())
        return { 0, 4, 7 };

    juce::String root;
    root << sym[0];
    int index = 1;
    if (sym.length() > 1)
    {
        const auto accidental = sym[1];
        if (accidental == '#' || accidental == 'b')
        {
            root << accidental;
            index = 2;
        }
    }

    const auto quality = sym.substring(index).toLowerCase();
    const int rootPc = DiatonicHarmony::pitchClassForRoot(root);

    const bool isMinor = (quality.startsWith("m") || quality.startsWith("min"))
                         && !quality.startsWith("maj");
    const bool hasDim = quality.contains("dim");
    const bool hasAug = quality.contains("aug");
    const bool hasSus2 = quality.contains("sus2");
    const bool hasSus4 = quality.contains("sus4") || (quality.contains("sus") && !hasSus2);

    std::vector<int> intervals { 0, 4, 7 };
    if (hasDim) intervals = { 0, 3, 6 };
    else if (hasAug) intervals = { 0, 4, 8 };
    else if (hasSus2) intervals = { 0, 2, 7 };
    else if (hasSus4) intervals = { 0, 5, 7 };
    else if (isMinor) intervals = { 0, 3, 7 };

    const bool hasMaj9 = quality.contains("maj9");
    const bool hasMin9 = quality.contains("min9")
                         || (isMinor && quality.contains("m9"))
                         || (isMinor && quality.contains("9") && !hasMaj9 && !quality.contains("add9"));
    const bool hasAdd9 = quality.contains("add9");
    const bool hasMaj7 = quality.contains("maj7");
    const bool hasDominant7 = quality.contains("7") && !hasMaj7;
    const bool hasMin6 = quality.contains("min6") || (isMinor && quality.contains("m6"));
    const bool has6 = quality.contains("6");

    if (hasMaj9)
    {
        intervals.push_back(11);
        intervals.push_back(14);
    }
    else if (hasMin9)
    {
        intervals.push_back(10);
        intervals.push_back(14);
    }
    else
    {
        if (hasMaj7)
            intervals.push_back(11);
        else if (hasDominant7)
            intervals.push_back(10);

        if (hasAdd9)
            intervals.push_back(14);
    }

    if (has6 || hasMin6)
        intervals.push_back(9);

    std::vector<int> out;
    out.reserve(intervals.size());
    for (const auto interval : intervals)
        out.push_back((rootPc + interval) % 12);

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::vector<int> BachTheory::getChordPitchClasses(const juce::String& chordSymbol,
                                                  const juce::String& keyRoot,
                                                  const juce::String& mode)
{
    (void) keyRoot;
    (void) mode;
    return getChordPitchClasses(chordSymbol);
}

} // namespace setle::theory
