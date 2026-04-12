#pragma once

#include <juce_core/juce_core.h>

#include <vector>

#include "../theory/TheoryAdapter.h"

namespace setle::midi
{

class MidiConstraintEngine
{
public:
    enum class NotePolicy
    {
        Constrain,
        Guide,
        Bypass
    };

    struct GuidanceResult
    {
        int inputNote { 60 };
        int suggestedNote { 60 };
        bool wouldChange { false };
        bool inScale { true };
        bool inChord { true };
    };

    void setScaleContext(const juce::String& root, const juce::String& mode);
    void setChordSymbol(const juce::String& chordSymbol);

    int processNote(int midiNote, double beat = 0.0) const;
    GuidanceResult analyzeNote(int midiNote, double beat = 0.0) const;

    void setScaleLock(bool enabled) { scaleLockEnabled = enabled; }
    void setChordLock(bool enabled) { chordLockEnabled = enabled; }
    bool getScaleLock() const noexcept { return scaleLockEnabled; }
    bool getChordLock() const noexcept { return chordLockEnabled; }

    void setNotePolicy(NotePolicy next) { notePolicy = next; }
    NotePolicy getNotePolicy() const noexcept { return notePolicy; }

private:
    std::vector<int> buildAllowedPitchClasses(bool* inScale, bool* inChord, int midiNote) const;
    static int snapToNearest(const std::vector<int>& allowedPitchClasses, int inputNote);

    theory::TheoryAdapter theoryAdapter;
    std::vector<int> scalePitchClasses;
    std::vector<int> chordPitchClasses;

    bool scaleLockEnabled { false };
    bool chordLockEnabled { false };
    NotePolicy notePolicy { NotePolicy::Constrain };
};

} // namespace setle::midi
