#include "DiatonicHarmony.h"
#include "MeterContext.h"
#include "../model/SetleSongModel.h"

namespace setle::theory
{
namespace
{
juce::String parseChordRoot(const juce::String& chordSymbol)
{
    auto symbol = chordSymbol.trim();
    if (symbol.isEmpty())
        return {};

    juce::String root;
    root << symbol[0];

    if (symbol.length() > 1)
    {
        const auto accidental = symbol[1];
        if (accidental == '#' || accidental == 'b')
            root << accidental;
    }

    return root;
}
} // namespace

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

juce::String DiatonicHarmony::functionForChord(const juce::String& chordSymbol,
                                               const juce::String& keyRoot,
                                               const juce::String& mode)
{
    const auto root = parseChordRoot(chordSymbol);
    if (root.isEmpty())
        return "T";

    const auto keyPc = pitchClassForRoot(keyRoot);
    const auto chordPc = pitchClassForRoot(root);
    const auto intervals = modeIntervals(mode);

    int degree = -1;
    for (size_t i = 0; i < intervals.size(); ++i)
    {
        if (((keyPc + intervals[i]) % 12) == chordPc)
        {
            degree = static_cast<int>(i);
            break;
        }
    }

    if (degree < 0)
    {
        // Chromatic fallback: dominant-ish qualities are usually tension-driving.
        const auto lower = chordSymbol.toLowerCase();
        if (lower.contains("7") || lower.contains("dim"))
            return "D";
        return "T";
    }

    if (degree == 0 || degree == 2 || degree == 5)
        return "T";

    if (degree == 1 || degree == 3)
        return "PD";

    return "D";
}

std::vector<setle::model::Chord> buildProgressionFromTemplate(
    const juce::String& keyRoot,
    const juce::String& mode,
    const ProgressionTemplate& preset,
    const MeterContext& meter)
{
    std::vector<setle::model::Chord> out;
    
    const double defaultDurationBeats = meter.beatsPerBar(); // one bar per chord by default
    const int rootPitch = DiatonicHarmony::pitchClassForRoot(keyRoot);
    const auto intervals = DiatonicHarmony::modeIntervals(mode);

    for (int i = 0; i < (int)preset.degrees.size(); ++i)
    {
        const int degree = preset.degrees[i];
        // Calculate the root pitch for this chord based on scale degree
        // Degree 1 = root, degree 2 = 2nd, etc.
        const int scalePC = (degree >= 1 && degree <= 7) ? intervals[degree - 1] : 0;
        const int chordRootPitch = (rootPitch + scalePC) % 12 + 60; // octave 4

        // Create a basic chord with the calculated root
        auto chord = setle::model::Chord::create("X", "triad", chordRootPitch);

        // Set duration: use preset duration if provided, otherwise use meter-based default
        if (!preset.durationEighths.empty() && i < (int)preset.durationEighths.size())
            chord.setDurationBeats(preset.durationEighths[i] * 0.5); // eighths to quarter beats
        else
            chord.setDurationBeats(defaultDurationBeats);

        out.push_back(chord);
    }
    return out;
}

} // namespace setle::theory
