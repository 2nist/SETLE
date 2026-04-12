#include "TransitionAnalyzer.h"

#include "DiatonicHarmony.h"

#include <set>

namespace setle::theory
{
namespace
{
std::vector<int> qualityIntervals(const juce::String& quality)
{
    const auto q = quality.toLowerCase();
    if (q.contains("major7")) return { 0, 4, 7, 11 };
    if (q.contains("minor7")) return { 0, 3, 7, 10 };
    if (q.contains("dominant7")) return { 0, 4, 7, 10 };
    if (q.contains("halfdiminished")) return { 0, 3, 6, 10 };
    if (q.contains("diminished")) return { 0, 3, 6 };
    if (q.contains("augmented")) return { 0, 4, 8 };
    if (q.contains("sus2")) return { 0, 2, 7 };
    if (q.contains("sus4")) return { 0, 5, 7 };
    if (q.contains("minor")) return { 0, 3, 7 };
    return { 0, 4, 7 };
}

std::set<int> pitchClassesForChord(const model::Chord& chord)
{
    std::set<int> pcs;
    const auto notes = chord.getNotes();
    if (!notes.empty())
    {
        for (const auto& n : notes)
            pcs.insert(((n.getPitch() % 12) + 12) % 12);
        return pcs;
    }

    const auto root = ((chord.getRootMidi() % 12) + 12) % 12;
    for (const auto iv : qualityIntervals(chord.getQuality()))
        pcs.insert((root + iv) % 12);
    return pcs;
}

int degreeForChordInProgression(const model::Chord& chord, const model::Progression& progression)
{
    const auto keyPc = DiatonicHarmony::pitchClassForRoot(progression.getKey());
    const auto chordPc = ((chord.getRootMidi() % 12) + 12) % 12;
    const auto delta = (chordPc - keyPc + 12) % 12;
    const auto intervals = DiatonicHarmony::modeIntervals(progression.getMode());

    for (int i = 0; i < 7; ++i)
        if (intervals[static_cast<size_t>(i)] == delta)
            return i;

    return -1;
}

bool cadenceMatch(int degreeA, int degreeB)
{
    if (degreeA < 0 || degreeB < 0)
        return false;

    for (const auto& cadence : DiatonicHarmony::cadencePresets())
    {
        if (cadence.degrees.size() < 2)
            continue;

        const auto penultimate = cadence.degrees[cadence.degrees.size() - 2];
        const auto last = cadence.degrees.back();
        if (penultimate == degreeA && last == degreeB)
            return true;
    }

    return false;
}
} // namespace

BoundaryScore TransitionAnalyzer::getBoundaryScore(const juce::String& sectionIdA,
                                                   const juce::String& sectionIdB) const
{
    auto makeScore = [](BoundaryScore::Level level, juce::uint32 argb, const juce::String& label)
    {
        BoundaryScore score;
        score.level = level;
        score.colourArgb = argb;
        score.label = label;
        return score;
    };

    auto sections = song.getSections();
    auto progressions = song.getProgressions();

    auto findSection = [&sections](const juce::String& id) -> std::optional<model::Section>
    {
        for (const auto& s : sections)
            if (s.getId() == id)
                return s;
        return std::nullopt;
    };

    auto sectionA = findSection(sectionIdA);
    auto sectionB = findSection(sectionIdB);
    if (!sectionA.has_value() || !sectionB.has_value())
        return makeScore(BoundaryScore::Level::yellow, 0xffd9b84f, "missing section");

    auto refsA = sectionA->getProgressionRefs();
    auto refsB = sectionB->getProgressionRefs();
    if (refsA.empty() || refsB.empty())
        return makeScore(BoundaryScore::Level::yellow, 0xffd9b84f, "missing progression");

    std::optional<model::Progression> progA;
    std::optional<model::Progression> progB;
    for (const auto& p : progressions)
    {
        if (!progA.has_value() && p.getId() == refsA.back().getProgressionId()) progA = p;
        if (!progB.has_value() && p.getId() == refsB.front().getProgressionId()) progB = p;
    }

    if (!progA.has_value() || !progB.has_value())
        return makeScore(BoundaryScore::Level::yellow, 0xffd9b84f, "progression lookup failed");

    auto chordsA = progA->getChords();
    auto chordsB = progB->getChords();
    if (chordsA.empty() || chordsB.empty())
        return makeScore(BoundaryScore::Level::yellow, 0xffd9b84f, "missing chords");

    const auto& lastChord = chordsA.back();
    const auto& firstChord = chordsB.front();

    const int degA = degreeForChordInProgression(lastChord, *progA);
    const int degB = degreeForChordInProgression(firstChord, *progB);
    if (cadenceMatch(degA, degB))
        return makeScore(BoundaryScore::Level::green, 0xff58b368, "cadence match");

    const auto pcsA = pitchClassesForChord(lastChord);
    const auto pcsB = pitchClassesForChord(firstChord);

    bool shared = false;
    for (const auto pc : pcsA)
    {
        if (pcsB.find(pc) != pcsB.end())
        {
            shared = true;
            break;
        }
    }

    if (!shared)
        return makeScore(BoundaryScore::Level::red, 0xffc44040, "no shared tones");

    return makeScore(BoundaryScore::Level::yellow, 0xffd9b84f, "neutral boundary");
}

} // namespace setle::theory
