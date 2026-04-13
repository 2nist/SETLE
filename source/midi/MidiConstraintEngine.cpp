#include "MidiConstraintEngine.h"

#include <algorithm>
#include <limits>

namespace setle::midi
{

namespace
{
bool containsPitchClass(const std::vector<int>& pitchClasses, int midiNote)
{
    const auto pitchClass = (midiNote % 12 + 12) % 12;
    return std::find(pitchClasses.begin(), pitchClasses.end(), pitchClass) != pitchClasses.end();
}
} // namespace

void MidiConstraintEngine::setScaleContext(const juce::String& root, const juce::String& mode)
{
    scalePitchClasses = theoryAdapter.getScalePitchClasses(root, mode);
}

void MidiConstraintEngine::setChordSymbol(const juce::String& chordSymbol)
{
    chordPitchClasses = theoryAdapter.getChordPitchClasses(chordSymbol);
}

int MidiConstraintEngine::processNote(int midiNote, double) const
{
    if (notePolicy == NotePolicy::Bypass)
        return midiNote;

    bool inScale = true;
    bool inChord = true;
    const auto allowed = buildAllowedPitchClasses(&inScale, &inChord, midiNote);
    if (allowed.empty() || notePolicy == NotePolicy::Guide)
        return midiNote;

    return snapToNearest(allowed, midiNote);
}

MidiConstraintEngine::GuidanceResult MidiConstraintEngine::analyzeNote(int midiNote, double) const
{
    GuidanceResult result;
    result.inputNote = midiNote;

    bool inScale = true;
    bool inChord = true;
    const auto allowed = buildAllowedPitchClasses(&inScale, &inChord, midiNote);

    result.inScale = inScale;
    result.inChord = inChord;
    result.suggestedNote = allowed.empty() ? midiNote : snapToNearest(allowed, midiNote);
    result.wouldChange = result.suggestedNote != midiNote;
    return result;
}

std::vector<int> MidiConstraintEngine::buildAllowedPitchClasses(bool* inScale, bool* inChord, int midiNote) const
{
    if (inScale != nullptr)
        *inScale = scalePitchClasses.empty() || containsPitchClass(scalePitchClasses, midiNote);

    if (inChord != nullptr)
        *inChord = chordPitchClasses.empty() || containsPitchClass(chordPitchClasses, midiNote);

    if (!scaleLockEnabled && !chordLockEnabled)
        return {};

    if (scaleLockEnabled && chordLockEnabled)
    {
        std::vector<int> both;
        for (const auto pc : chordPitchClasses)
            if (std::find(scalePitchClasses.begin(), scalePitchClasses.end(), pc) != scalePitchClasses.end())
                both.push_back(pc);

        if (!both.empty())
            return both;
    }

    if (chordLockEnabled && !chordPitchClasses.empty())
        return chordPitchClasses;

    return scalePitchClasses;
}

int MidiConstraintEngine::snapToNearest(const std::vector<int>& allowedPitchClasses, int inputNote)
{
    int best = inputNote;
    int bestDistance = std::numeric_limits<int>::max();

    const int inputOctave = inputNote / 12;
    for (const auto pitchClass : allowedPitchClasses)
    {
        for (int octave = inputOctave - 2; octave <= inputOctave + 2; ++octave)
        {
            const int candidate = octave * 12 + pitchClass;
            if (candidate < 0 || candidate > 127)
                continue;

            const int distance = std::abs(candidate - inputNote);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = candidate;
            }
        }
    }

    return best;
}

} // namespace setle::midi
