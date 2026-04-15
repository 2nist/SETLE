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

static bool isRomanNumeralSymbol(const juce::String& sym)
{
    auto s = sym.trim().toLowerCase();
    const int spacePos = s.indexOfChar(' ');
    if (spacePos > 0 && spacePos <= 2)
        s = s.substring(spacePos + 1).trim();
    return s.startsWith("i") || s.startsWith("v")
           || s.startsWith("ii") || s.startsWith("iii")
           || s.startsWith("iv") || s.startsWith("vi")
           || s.startsWith("vii");
}

static juce::String romanToLetterSymbol(
    const juce::String& rawSym,
    const juce::String& key,
    const juce::String& mode)
{
    auto sym = rawSym.trim();
    const int spacePos = sym.indexOfChar(' ');
    if (spacePos > 0 && spacePos <= 2)
        sym = sym.substring(spacePos + 1).trim();
    const int slashPos = sym.indexOfChar('/');
    if (slashPos > 0)
        sym = sym.substring(0, slashPos).trim();
    sym = sym.replace("[", "").replace("]", "")
             .replace("(", "").replace(")", "");
    const auto lower = sym.toLowerCase();
    int degree = -1;
    juce::String suffix;
    if      (lower.startsWith("vii")) { degree = 6; suffix = sym.substring(3); }
    else if (lower.startsWith("vi"))  { degree = 5; suffix = sym.substring(2); }
    else if (lower.startsWith("iv"))  { degree = 3; suffix = sym.substring(2); }
    else if (lower.startsWith("iii")) { degree = 2; suffix = sym.substring(3); }
    else if (lower.startsWith("ii"))  { degree = 1; suffix = sym.substring(2); }
    else if (lower.startsWith("v"))   { degree = 4; suffix = sym.substring(1); }
    else if (lower.startsWith("i"))   { degree = 0; suffix = sym.substring(1); }
    if (degree < 0)
        return sym;
    const int rootPc = DiatonicHarmony::pitchClassForRoot(key);
    const auto intervals = DiatonicHarmony::modeIntervals(mode);
    const int scaleDegreeInterval = intervals[degree % 7];
    const int chordRootPc = (rootPc + scaleDegreeInterval) % 12;
    static const char* noteNames[] = {
        "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"
    };
    juce::String letterRoot = noteNames[chordRootPc];
    const bool isUppercase = juce::CharacterFunctions::isUpperCase(sym[0]);
    const bool isDim = suffix.toLowerCase().startsWith("o")
                       || suffix.toLowerCase().contains("dim");
    const bool isHalfDim = suffix.startsWith(juce::CharPointer_UTF8("\xc3\xb8"))
                           || suffix.toLowerCase().contains("m7b5");
    juce::String resolved = letterRoot;
    if (isDim)
    {
        resolved += "dim";
        if (suffix.containsAnyOf("7"))
            resolved += "7";
    }
    else if (isHalfDim)
    {
        resolved += "m7b5";
    }
    else if (!isUppercase)
    {
        resolved += "m";
        if (suffix.contains("maj7"))        resolved += "maj7";
        else if (suffix.containsAnyOf("7")) resolved += "7";
        else if (suffix.contains("9"))      resolved += "9";
    }
    else
    {
        if (suffix.contains("maj7"))        resolved += "maj7";
        else if (suffix.containsAnyOf("7")) resolved += "7";
        else if (suffix.contains("9"))      resolved += "9";
        else if (suffix.contains("sus4"))   resolved += "sus4";
        else if (suffix.contains("sus2"))   resolved += "sus2";
        else if (suffix.contains("add9"))   resolved += "add9";
    }
    return resolved;
}

std::vector<int> BachTheory::getChordPitchClasses(
    const juce::String& symbol,
    const juce::String& key,
    const juce::String& mode)
{
    if (isRomanNumeralSymbol(symbol))
    {
        const auto resolved = romanToLetterSymbol(symbol, key, mode);
        return getChordPitchClasses(resolved);
    }
    return getChordPitchClasses(symbol);
}

} // namespace setle::theory
