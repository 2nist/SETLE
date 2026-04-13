#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace setle::gridroll
{

struct GridRollCell
{
    // Identity
    juce::String cellId;       // UUID
    juce::String sourceId;     // progressionId (chord mode) or trackId (drum mode)

    // Position & duration
    double startBeat     { 0.0 };
    double durationBeats { 1.0 };

    // Content
    enum class CellType { Chord, DrumHit, Note };
    CellType type { CellType::Chord };

    // Chord mode content
    juce::String chordSymbol;
    juce::String chordFunction;  // T / PD / D
    juce::String romanNumeral;
    juce::String source;         // e.g. "midi", "user"
    float confidence { 1.0f };   // 0.0-1.0, usually from identifier
    std::vector<int> pitchClasses;

    // Drum / note mode content
    int          midiNote  { 60 };    // single note for drum hits
    juce::String drumName;            // "Kick", "Snare" etc if drum-mapped

    // Performance
    float velocity     { 0.8f };  // 0.0–1.0
    float probability  { 1.0f };  // 0.0–1.0
    int   ratchetCount { 1 };     // 1 = no ratchet, 2–8 = repeat within duration
    float swing        { 0.0f };  // 0.0–1.0 pushes off-beats later
    bool  muted        { false };
    bool  accented     { false };

    // Arpeggiator (chord mode only)
    enum class ArpMode { Off, Up, Down, UpDown, Random };
    ArpMode arpMode      { ArpMode::Off };
    int     arpStepLength { 4 };  // subdivision: 4 = 16th, 2 = 8th, 1 = quarter

    // Strum (chord mode only)
    enum class StrumDir { None, Up, Down };
    StrumDir strumDir      { StrumDir::None };
    double   strumSpreadMs { 20.0 };
};

} // namespace setle::gridroll
