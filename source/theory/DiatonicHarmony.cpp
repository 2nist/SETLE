#include "DiatonicHarmony.h"

namespace setle::theory
{

int DiatonicHarmony::pitchClassForRoot(const juce::String& root)
{
    const auto normalized = root.trim().toUpperCase();
    if (normalized == "C" || normalized == "B#") return 0;
    if (normalized == "C#" || normalized == "DB") return 1;
    if (normalized == "D") return 2;
    if (normalized == "D#" || normalized == "EB") return 3;
    if (normalized == "E" || normalized == "FB") return 4;
    if (normalized == "F" || normalized == "E#") return 5;
    if (normalized == "F#" || normalized == "GB") return 6;
    if (normalized == "G") return 7;
    if (normalized == "G#" || normalized == "AB") return 8;
    if (normalized == "A") return 9;
    if (normalized == "A#" || normalized == "BB") return 10;
    if (normalized == "B" || normalized == "CB") return 11;
    return 0;
}

std::vector<int> DiatonicHarmony::modeIntervals(const juce::String& mode)
{
    const auto lower = mode.trim().toLowerCase();
    if (lower == "dorian") return { 0, 2, 3, 5, 7, 9, 10 };
    if (lower == "phrygian") return { 0, 1, 3, 5, 7, 8, 10 };
    if (lower == "lydian") return { 0, 2, 4, 6, 7, 9, 11 };
    if (lower == "mixolydian") return { 0, 2, 4, 5, 7, 9, 10 };
    if (lower == "aeolian" || lower == "minor") return { 0, 2, 3, 5, 7, 8, 10 };
    if (lower == "locrian") return { 0, 1, 3, 5, 6, 8, 10 };
    return { 0, 2, 4, 5, 7, 9, 11 };
}

} // namespace setle::theory
