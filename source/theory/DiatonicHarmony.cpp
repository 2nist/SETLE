#include "DiatonicHarmony.h"

namespace DiatonicHarmony
{
namespace
{
constexpr const char* kRoots[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
constexpr const char* kRomanBase[] = { "I", "II", "III", "IV", "V", "VI", "VII" };

juce::String normalizedMode(const juce::String& mode)
{
    return mode.trim().toLowerCase();
}

juce::String romanForQuality(const juce::String& base, const juce::String& quality)
{
    if (quality.startsWithIgnoreCase("min"))
        return base.toLowerCase();
    if (quality.startsWithIgnoreCase("dim"))
        return base.toLowerCase() + "o";
    return base;
}
} // namespace

std::array<int, 7> modeIntervals(const juce::String& mode)
{
    const auto normalized = normalizedMode(mode);
    if (normalized == "minor" || normalized == "aeolian") return { 0, 2, 3, 5, 7, 8, 10 };
    if (normalized == "dorian") return { 0, 2, 3, 5, 7, 9, 10 };
    if (normalized == "mixolydian") return { 0, 2, 4, 5, 7, 9, 10 };
    if (normalized == "lydian") return { 0, 2, 4, 6, 7, 9, 11 };
    if (normalized == "phrygian") return { 0, 1, 3, 5, 7, 8, 10 };
    if (normalized == "locrian") return { 0, 1, 3, 5, 6, 8, 10 };
    return { 0, 2, 4, 5, 7, 9, 11 };
}

std::array<juce::String, 7> modeQualities(const juce::String& mode)
{
    const auto normalized = normalizedMode(mode);
    if (normalized == "minor" || normalized == "aeolian") return { "min", "dim", "maj", "min", "min", "maj", "maj" };
    if (normalized == "dorian") return { "min", "min", "maj", "maj", "min", "dim", "maj" };
    if (normalized == "mixolydian") return { "maj", "min", "dim", "maj", "min", "min", "maj" };
    if (normalized == "lydian") return { "maj", "maj", "min", "dim", "maj", "min", "min" };
    if (normalized == "phrygian") return { "min", "maj", "maj", "min", "dim", "maj", "min" };
    if (normalized == "locrian") return { "dim", "maj", "min", "min", "maj", "maj", "min" };
    return { "maj", "min", "min", "maj", "maj", "min", "dim" };
}

std::array<juce::String, 7> modeFunctions(const juce::String& mode)
{
    juce::ignoreUnused(mode);
    return { "T", "PD", "T", "PD", "D", "T", "D" };
}

std::array<juce::String, 7> modeRomanNumerals(const juce::String& mode)
{
    const auto qualities = modeQualities(mode);
    std::array<juce::String, 7> out;
    for (int i = 0; i < 7; ++i)
        out[(size_t) i] = romanForQuality(kRomanBase[i], qualities[(size_t) i]);
    return out;
}

int pitchClassForRoot(const juce::String& root)
{
    const auto r = root.trim();
    if (r.equalsIgnoreCase("C")) return 0;
    if (r.equalsIgnoreCase("C#") || r.equalsIgnoreCase("Db")) return 1;
    if (r.equalsIgnoreCase("D")) return 2;
    if (r.equalsIgnoreCase("D#") || r.equalsIgnoreCase("Eb")) return 3;
    if (r.equalsIgnoreCase("E")) return 4;
    if (r.equalsIgnoreCase("F")) return 5;
    if (r.equalsIgnoreCase("F#") || r.equalsIgnoreCase("Gb")) return 6;
    if (r.equalsIgnoreCase("G")) return 7;
    if (r.equalsIgnoreCase("G#") || r.equalsIgnoreCase("Ab")) return 8;
    if (r.equalsIgnoreCase("A")) return 9;
    if (r.equalsIgnoreCase("A#") || r.equalsIgnoreCase("Bb")) return 10;
    return 11;
}

juce::String rootNameForPitchClass(int pitchClass)
{
    return kRoots[(pitchClass % 12 + 12) % 12];
}

int degreeIndexForRoman(const juce::String& roman)
{
    const auto normalized = roman.trim().toUpperCase().retainCharacters("IV");
    for (int i = 0; i < 7; ++i)
        if (normalized == kRomanBase[i])
            return i;
    return 0;
}

DiatonicChordInfo buildDiatonicChord(const juce::String& keyRoot,
                                     const juce::String& mode,
                                     int degreeIndex)
{
    const int index = juce::jlimit(0, 6, degreeIndex);
    const auto intervals = modeIntervals(mode);
    const auto qualities = modeQualities(mode);
    const auto romans = modeRomanNumerals(mode);
    const auto functions = modeFunctions(mode);

    DiatonicChordInfo chord;
    chord.degreeIndex = index;
    chord.root = rootNameForPitchClass(pitchClassForRoot(keyRoot) + intervals[(size_t) index]);
    chord.type = qualities[(size_t) index];
    chord.roman = romans[(size_t) index];
    chord.function = functions[(size_t) index];
    return chord;
}

const std::vector<ProgressionTemplate>& progressionPresets()
{
    static const std::vector<ProgressionTemplate> presets {
        { "p01", "Axis Pop",           "I-V-vi-IV",                { 0, 4, 5, 3 } },
        { "p02", "50s / Soul",         "I-vi-IV-V",                { 0, 5, 3, 4 } },
        { "p03", "Doo-Wop",            "I-vi-ii-V",                { 0, 5, 1, 4 } },
        { "p04", "Jazz ii-V-I",        "ii-V-I",                   { 1, 4, 0 } },
        { "p05", "Tonic Vamp",         "I-IV",                     { 0, 3 } },
        { "p06", "Circle",             "vi-ii-V-I",                { 5, 1, 4, 0 } },
        { "p07", "Basic Cadential",    "I-IV-V-I",                 { 0, 3, 4, 0 } },
        { "p08", "Simple Resolution",  "I-V-I",                    { 0, 4, 0 } },
        { "p09", "Montgomery-Ward",    "I-IV-ii-V",                { 0, 3, 1, 4 } },
        { "p10", "Pachelbel",          "I-V-vi-iii-IV-I-IV-V",     { 0, 4, 5, 2, 3, 0, 3, 4 } },
        { "p11", "Reversed Axis",      "IV-V-I-vi",                { 3, 4, 0, 5 } },
        { "p12", "Jazz Extended",      "IV-V-iii-vi",              { 3, 4, 2, 5 } },
        { "p13", "Turnaround",         "V-IV-I",                   { 4, 3, 0 } },
        { "p14", "Three-Chord",        "I-IV-V",                   { 0, 3, 4 } },
        { "p15", "ii Setup",           "I-ii-V-I",                 { 0, 1, 4, 0 } },
        { "p16", "Ascending Scale",    "I-iii-IV-V",               { 0, 2, 3, 4 } },
        { "p17", "Introspective",      "I-iii-vi-IV",              { 0, 2, 5, 3 } },
        { "p18", "Uplifting",          "vi-IV-I-V",                { 5, 3, 0, 4 } },
        { "p19", "Descending Bass",    "I-vii-vi-V",               { 0, 6, 5, 4 } },
        { "p20", "Jazz Cycle",         "iii-vi-ii-V",              { 2, 5, 1, 4 } },
        { "p21", "Passamezzo",         "I-IV-I-V",                 { 0, 3, 0, 4 } },
        { "p22", "Extended Circle",    "I-vi-ii-V-I",              { 0, 5, 1, 4, 0 } },
        { "p23", "Soft Rock",          "I-IV-vi-V",                { 0, 3, 5, 4 } },
        { "p24", "Rhythm Changes A",   "I-vi-ii-V",                { 0, 5, 1, 4 } },
        { "p25", "Long Form",          "I-iii-IV-vi-V-I",          { 0, 2, 3, 5, 4, 0 } },
        { "p26", "Natural Minor Walk", "i-VII-VI-VII",             { 0, 6, 5, 6 } },
        { "p27", "Aeolian Rock",       "i-VI-III-VII",             { 0, 5, 2, 6 } },
        { "p28", "Pure Minor",         "i-iv-v",                   { 0, 3, 4 } },
        { "p29", "Baroque Minor",      "i-iv-VII-III",             { 0, 3, 6, 2 } },
        { "p30", "Descending Minor",   "i-VII-VI-V",               { 0, 6, 5, 4 } },
        { "p31", "Minor Vamp",         "i-iv-i-VII",               { 0, 3, 0, 6 } },
        { "p32", "Minor Lift",         "i-VI-VII-i",               { 0, 5, 6, 0 } },
        { "p33", "Minor Authentic",    "i-ii-V-i",                 { 0, 1, 4, 0 } },
        { "p34", "Modal Minor",        "i-III-VII-i",              { 0, 2, 6, 0 } },
        { "p35", "Full Aeolian",       "i-VI-III-VII-i",           { 0, 5, 2, 6, 0 } },
        { "p36", "Flipped Minor",      "i-iv-VI-III",              { 0, 3, 5, 2 } },
        { "p37", "Romanesca",          "III-VII-i-V",              { 2, 6, 0, 4 } },
        { "p38", "Dark Baroque",       "i-VII-III-iv",             { 0, 6, 2, 3 } },
        { "p39", "Minor Power",        "i-VI-VII-V",               { 0, 5, 6, 4 } },
        { "p40", "Dorian Vamp",        "i-IV (Dorian)",            { 0, 3 } },
        { "p41", "Dorian Round",       "i-IV-VII-i (Dorian)",      { 0, 3, 6, 0 } },
        { "p42", "Dorian Jazz",        "i-ii-IV-i (Dorian)",       { 0, 1, 3, 0 } },
        { "p43", "Mixolydian Rock",    "I-VII-IV (Mixolydian)",    { 0, 6, 3 } },
        { "p44", "Mixolydian Round",   "I-IV-VII-I (Mixolydian)",  { 0, 3, 6, 0 } },
        { "p45", "Mixolydian Cadence", "I-VII-V-I (Mixolydian)",   { 0, 6, 4, 0 } }
    };

    return presets;
}

const std::vector<ProgressionTemplate>& cadencePresets()
{
    static const std::vector<ProgressionTemplate> cadences {
        { "authentic", "Authentic", "V-I", { 4, 0 } },
        { "plagal", "Plagal", "IV-I", { 3, 0 } },
        { "deceptive", "Deceptive", "V-vi", { 4, 5 } },
        { "half", "Half Cadence", "ii-V", { 1, 4 } },
        { "full-jazz", "Full Jazz", "ii-V-I", { 1, 4, 0 } },
        { "leading", "Leading Tone", "vii-I", { 6, 0 } },
        { "plagal-half", "Plagal Half", "IV-V", { 3, 4 } },
        { "double", "Double", "V-IV-I", { 4, 3, 0 } },
        { "dbl-deceptive", "Deceptive to iii", "V-iii", { 4, 2 } },
        { "turnaround", "Turnaround", "I-V-I", { 0, 4, 0 } },
        { "extended", "Extended Resolve", "V-vi-ii-V-I", { 4, 5, 1, 4, 0 } },
        { "smooth-jazz", "Smooth Jazz", "IV-ii-V-I", { 3, 1, 4, 0 } }
    };

    return cadences;
}

} // namespace DiatonicHarmony
