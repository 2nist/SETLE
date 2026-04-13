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

    std::vector<int> intervals { 0, 4, 7 };
    if (quality.startsWith("m") && !quality.startsWith("maj")) intervals = { 0, 3, 7 };
    if (quality.contains("dim")) intervals = { 0, 3, 6 };
    if (quality.contains("aug")) intervals = { 0, 4, 8 };

    if (quality.contains("maj7")) intervals.push_back(11);
    else if (quality.contains("7")) intervals.push_back(10);

    std::vector<int> out;
    out.reserve(intervals.size());
    for (const auto interval : intervals)
        out.push_back((rootPc + interval) % 12);

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

} // namespace setle::theory
