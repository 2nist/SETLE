#include "NoteModeView.h"

#include "../theme/ThemeManager.h"
#include "../theory/BachTheory.h"
#include "../theory/DiatonicHarmony.h"
#include "../theory/MeterContext.h"
#include "../ui/EditToolManager.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

namespace setle::gridroll
{
namespace
{
bool containsPitchClass(const std::vector<int>& pitchClasses, int midiNote)
{
    const int pc = ((midiNote % 12) + 12) % 12;
    return std::find(pitchClasses.begin(), pitchClasses.end(), pc) != pitchClasses.end();
}

juce::Colour functionTint(const ThemeData& theme, const juce::String& functionName)
{
    if (functionName.equalsIgnoreCase("T") || functionName.equalsIgnoreCase("tonic"))
        return theme.zoneB;
    if (functionName.equalsIgnoreCase("PD") || functionName.equalsIgnoreCase("predominant") || functionName.equalsIgnoreCase("subdominant"))
        return theme.zoneA;
    if (functionName.equalsIgnoreCase("D") || functionName.equalsIgnoreCase("dominant"))
        return theme.accentWarm;
    return theme.surfaceEdge;
}
} // namespace

NoteModeView::NoteModeView(model::Song& songRef, te::Edit& editRef)
    : song(songRef)
    , edit(editRef)
{
    setMouseClickGrabsKeyboardFocus(true);
    startTimerHz(30);
}

void NoteModeView::setTargetProgression(const juce::String& id)
{
    progressionId = id;
    selectedNoteId.clear();
    dragNoteId.clear();
    rebuildNoteCache();
    emitSelectionChanged();

    const auto total = progressionLengthBeats();
    visibleStartBeat = 0.0;
    visibleEndBeat = juce::jmax(2.0, total);

    lowestMidiVisible = 48; // C3
    highestMidiVisible = 72; // C5

    updateActiveChordPitchClasses();
    emitBeatRangeChanged();
    emitPitchRangeChanged();
    repaint();
}

void NoteModeView::setPlayheadBeat(double beat)
{
    if (std::abs(playheadBeat - beat) < 0.0001)
        return;

    playheadBeat = beat;
    updateActiveChordPitchClasses();
    repaint();
}

void NoteModeView::setVisibleBeatRange(double startBeat, double endBeat)
{
    auto span = endBeat - startBeat;
    span = clampBeatSpan(span);

    const auto total = progressionLengthBeats();
    const auto maxStart = juce::jmax(0.0, total - span);
    const auto clampedStart = juce::jlimit(0.0, maxStart, startBeat);
    const auto clampedEnd = clampedStart + span;

    if (std::abs(visibleStartBeat - clampedStart) < 0.0001
        && std::abs(visibleEndBeat - clampedEnd) < 0.0001)
        return;

    visibleStartBeat = clampedStart;
    visibleEndBeat = clampedEnd;
    emitBeatRangeChanged();
    repaint();
}

void NoteModeView::setVisiblePitchRange(int lowestMidi, int highestMidi)
{
    int low = juce::jlimit(0, 127, juce::jmin(lowestMidi, highestMidi));
    int high = juce::jlimit(0, 127, juce::jmax(lowestMidi, highestMidi));

    int span = high - low;
    span = juce::jlimit(12, 72, span);

    if (low + span > 127)
        low = 127 - span;
    high = low + span;

    if (low == lowestMidiVisible && high == highestMidiVisible)
        return;

    lowestMidiVisible = low;
    highestMidiVisible = high;
    emitPitchRangeChanged();
    repaint();
}

void NoteModeView::scrollToBeat(double beat, bool centreBeat)
{
    const auto span = visibleBeatSpan();
    const auto targetStart = centreBeat ? (beat - span * 0.5) : beat;
    setVisibleBeatRange(targetStart, targetStart + span);
}

void NoteModeView::scrollByBeats(double deltaBeats)
{
    setVisibleBeatRange(visibleStartBeat + deltaBeats, visibleEndBeat + deltaBeats);
}

void NoteModeView::refreshFromEdit()
{
    rebuildNoteCache();
    updateActiveChordPitchClasses();
    repaint();
}

void NoteModeView::timerCallback()
{
    const auto bpm = juce::jmax(1.0, song.getBpm());
    const auto currentSeconds = edit.getTransport().getPosition().inSeconds();
    const auto newPlayhead = currentSeconds / (60.0 / bpm);
    if (std::abs(newPlayhead - playheadBeat) > 0.0005)
    {
        playheadBeat = newPlayhead;
        updateActiveChordPitchClasses();
        repaint();
    }
}

void NoteModeView::rebuildNoteCache()
{
    notes.clear();
    chordBoundaries.clear();
    scalePitchClasses.clear();

    const auto progression = song.findProgressionById(progressionId);
    if (!progression.has_value())
        return;

    const int rootPc = theory::DiatonicHarmony::pitchClassForRoot(song.getSessionKey());
    const auto intervals = theory::DiatonicHarmony::modeIntervals(song.getSessionMode());
    for (const auto interval : intervals)
        scalePitchClasses.push_back((rootPc + interval) % 12);

    auto chords = progression->getChords();
    std::sort(chords.begin(), chords.end(),
              [](const model::Chord& a, const model::Chord& b)
              {
                  return a.getStartBeats() < b.getStartBeats();
              });

    for (const auto& chord : chords)
    {
        ChordBoundary boundary;
        boundary.chordId = chord.getId();
        boundary.startBeat = chord.getStartBeats();
        boundary.endBeat = boundary.startBeat + juce::jmax(0.0625, chord.getDurationBeats());
        boundary.symbol = chord.getSymbol();
        boundary.romanNumeral = chord.getName();
        boundary.function = chord.getFunction();
        chordBoundaries.push_back(boundary);
    }

    for (auto* track : te::getAudioTracks(edit))
    {
        if (track == nullptr)
            continue;

        for (auto* clipBase : track->getClips())
        {
            auto* clip = dynamic_cast<te::MidiClip*>(clipBase);
            if (clip == nullptr)
                continue;

            const auto clipPropId = clip->state.getProperty("progressionId").toString();
            const auto clipName = clip->getName();
            if (clipPropId != progressionId && clipName != progressionId)
                continue;

            const auto clipStartBeat = edit.tempoSequence.toBeats(clip->getPosition().getStart()).inBeats();
            auto& seq = clip->getSequence();
            for (auto* note : seq.getNotes())
            {
                if (note == nullptr)
                    continue;

                NoteEntry entry;
                entry.noteId = makeNoteId(clip, note);
                entry.startBeat = clipStartBeat + note->getStartBeat().inBeats();
                entry.durationBeats = juce::jmax(0.0625, note->getLengthBeats().inBeats());
                entry.midiNote = note->getNoteNumber();
                entry.velocity = juce::jlimit(0.0f, 1.0f, static_cast<float>(note->getVelocity()) / 127.0f);

                const auto* boundary = findChordBoundaryForBeat(entry.startBeat);
                if (boundary != nullptr)
                {
                    entry.chordId = boundary->chordId;
                    entry.chordSymbol = boundary->symbol;
                }

                entry.isScaleTone = containsPitchClass(scalePitchClasses, entry.midiNote);
                if (entry.chordSymbol.isNotEmpty())
                {
                    const auto chordPcs = theory::BachTheory::getChordPitchClasses(entry.chordSymbol);
                    entry.isChordTone = containsPitchClass(chordPcs, entry.midiNote);
                }

                notes.push_back(std::move(entry));
            }
        }
    }

    std::sort(notes.begin(), notes.end(),
              [](const NoteEntry& a, const NoteEntry& b)
              {
                  if (std::abs(a.startBeat - b.startBeat) < 0.0001)
                      return a.midiNote < b.midiNote;
                  return a.startBeat < b.startBeat;
              });

    if (!selectedNoteId.isEmpty())
    {
        const auto hasSelected = std::any_of(notes.begin(), notes.end(),
                                             [this](const NoteEntry& entry)
                                             {
                                                 return entry.noteId == selectedNoteId;
                                             });
        if (!hasSelected)
            selectedNoteId.clear();
    }
}

void NoteModeView::updateActiveChordPitchClasses()
{
    activeChordPitchClasses.clear();
    constraintEngine.setScaleContext(song.getSessionKey(), song.getSessionMode());
    const auto* boundary = findChordBoundaryForBeat(playheadBeat);
    if (boundary == nullptr)
    {
        constraintEngine.setChordSymbol({});
        return;
    }

    activeChordPitchClasses = theory::BachTheory::getChordPitchClasses(boundary->symbol);
    constraintEngine.setChordSymbol(boundary->symbol);
}

juce::Rectangle<int> NoteModeView::keyBounds() const
{
    return getLocalBounds().withWidth(kPianoKeyWidth);
}

juce::Rectangle<int> NoteModeView::gridBounds() const
{
    return getLocalBounds().withTrimmedLeft(kPianoKeyWidth);
}

double NoteModeView::pxPerBeat() const
{
    const auto grid = gridBounds();
    return grid.getWidth() > 0 ? static_cast<double>(grid.getWidth()) / visibleBeatSpan() : 1.0;
}

double NoteModeView::pxPerPitch() const
{
    const auto grid = gridBounds();
    return grid.getHeight() > 0 ? static_cast<double>(grid.getHeight()) / static_cast<double>(visiblePitchSpan() + 1) : 1.0;
}

double NoteModeView::progressionLengthBeats() const
{
    double maxBeat = 0.0;
    for (const auto& boundary : chordBoundaries)
        maxBeat = juce::jmax(maxBeat, boundary.endBeat);

    if (maxBeat <= 0.0)
    {
        const auto progression = song.findProgressionById(progressionId);
        if (progression.has_value())
            maxBeat = juce::jmax(0.0, progression->getLengthBeats());
    }

    return juce::jmax(2.0, maxBeat);
}

double NoteModeView::visibleBeatSpan() const
{
    return juce::jmax(0.001, visibleEndBeat - visibleStartBeat);
}

int NoteModeView::visiblePitchSpan() const
{
    return juce::jmax(1, highestMidiVisible - lowestMidiVisible);
}

double NoteModeView::clampBeatSpan(double span) const
{
    const auto total = progressionLengthBeats();
    return juce::jlimit(2.0, juce::jmax(2.0, total), span);
}

double NoteModeView::xToBeat(int x) const
{
    const auto grid = gridBounds();
    const auto localX = static_cast<double>(x - grid.getX());
    const auto beat = visibleStartBeat + localX / pxPerBeat();
    return juce::jlimit(0.0, progressionLengthBeats(), beat);
}

int NoteModeView::yToMidi(int y) const
{
    const auto grid = gridBounds();
    const auto localY = static_cast<double>(grid.getBottom() - y);
    const auto midiOffset = static_cast<int>(std::floor(localY / pxPerPitch()));
    return juce::jlimit(0, 127, lowestMidiVisible + midiOffset);
}

double NoteModeView::snapToBeatGrid(double beat) const
{
    constexpr double gridSize = 0.25; // 1/16 at 4/4 quarter-beat basis.
    return std::round(beat / gridSize) * gridSize;
}

int NoteModeView::snapMidiToScale(int midiNote) const
{
    if (scalePitchClasses.empty())
        return midiNote;

    int best = midiNote;
    int bestDistance = std::numeric_limits<int>::max();
    const int octave = midiNote / 12;

    for (const auto pc : scalePitchClasses)
    {
        for (int o = octave - 2; o <= octave + 2; ++o)
        {
            const int candidate = o * 12 + pc;
            if (candidate < 0 || candidate > 127)
                continue;

            const int d = std::abs(candidate - midiNote);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = candidate;
            }
        }
    }

    return best;
}

juce::Rectangle<float> NoteModeView::noteRectFor(const NoteEntry& note) const
{
    const auto grid = gridBounds();
    const auto ppb = pxPerBeat();
    const auto ppp = pxPerPitch();

    const float x = static_cast<float>(grid.getX() + (note.startBeat - visibleStartBeat) * ppb);
    const float w = static_cast<float>(juce::jmax(2.0, note.durationBeats * ppb - 1.0));
    const float y = static_cast<float>(grid.getBottom() - (note.midiNote - lowestMidiVisible + 1) * ppp);
    const float h = static_cast<float>(juce::jmax(2.0, ppp - 2.0));

    return { x, y + 1.0f, w, h };
}

int NoteModeView::noteRightEdgeX(const NoteEntry& note) const
{
    return static_cast<int>(std::round(noteRectFor(note).getRight()));
}

int NoteModeView::findNoteIndexAt(juce::Point<int> pos) const
{
    const auto grid = gridBounds();
    if (!grid.contains(pos))
        return -1;

    for (int i = static_cast<int>(notes.size()) - 1; i >= 0; --i)
    {
        const auto rect = noteRectFor(notes[static_cast<size_t>(i)]).toNearestInt();
        if (rect.intersects(grid) && rect.contains(pos))
            return i;
    }

    return -1;
}

const NoteModeView::ChordBoundary* NoteModeView::findChordBoundaryForBeat(double beat) const
{
    for (const auto& boundary : chordBoundaries)
    {
        if (beat >= boundary.startBeat && beat < boundary.endBeat)
            return &boundary;
    }

    if (!chordBoundaries.empty() && beat >= chordBoundaries.back().endBeat - 0.0001)
        return &chordBoundaries.back();

    return nullptr;
}

te::MidiClip* NoteModeView::findClipForProgression() const
{
    te::MidiClip* fallback = nullptr;

    for (auto* track : te::getAudioTracks(edit))
    {
        if (track == nullptr)
            continue;

        for (auto* clipBase : track->getClips())
        {
            auto* clip = dynamic_cast<te::MidiClip*>(clipBase);
            if (clip == nullptr)
                continue;

            const auto clipPropId = clip->state.getProperty("progressionId").toString();
            const auto clipName = clip->getName();
            if (clipPropId == progressionId)
                return clip;

            if (fallback == nullptr && clipName == progressionId)
                fallback = clip;
        }
    }

    return fallback;
}

juce::String NoteModeView::makeNoteId(const te::MidiClip* clip, const te::MidiNote* note)
{
    const auto clipPtr = reinterpret_cast<juce::pointer_sized_int>(clip);
    const auto notePtr = reinterpret_cast<juce::pointer_sized_int>(note);
    return juce::String::toHexString(clipPtr) + ":" + juce::String::toHexString(notePtr);
}

juce::String NoteModeView::createNoteAtBeatAndPitch(double beat, int midiNote)
{
    auto* clip = findClipForProgression();
    if (clip == nullptr)
        return {};

    const auto clipStartBeat = edit.tempoSequence.toBeats(clip->getPosition().getStart()).inBeats();
    const auto bpm = juce::jmax(1.0, song.getBpm());
    const auto clipLengthBeats = clip->getPosition().getLength().inSeconds() / (60.0 / bpm);
    auto relativeStart = juce::jmax(0.0, beat - clipStartBeat);
    relativeStart = juce::jlimit(0.0, juce::jmax(0.0, clipLengthBeats - 0.0625), relativeStart);
    auto duration = 0.5;
    duration = juce::jlimit(0.0625, juce::jmax(0.0625, clipLengthBeats - relativeStart), duration);

    auto& seq = clip->getSequence();
    if (auto* added = seq.addNote(juce::jlimit(0, 127, midiNote),
                                  tracktion::BeatPosition::fromBeats(relativeStart),
                                  tracktion::BeatDuration::fromBeats(duration),
                                  static_cast<int>(0.8f * 127.0f),
                                  0,
                                  nullptr))
        return makeNoteId(clip, added);

    return {};
}

bool NoteModeView::deleteNoteById(const juce::String& noteId)
{
    if (noteId.isEmpty())
        return false;

    auto* clip = findClipForProgression();
    if (clip == nullptr)
        return false;

    auto& seq = clip->getSequence();
    for (auto* note : seq.getNotes())
    {
        if (note == nullptr)
            continue;
        if (makeNoteId(clip, note) == noteId)
        {
            seq.removeNote(*note, nullptr);
            return true;
        }
    }

    return false;
}

bool NoteModeView::updateNoteInClip(const juce::String& noteId,
                                    double absoluteStartBeat,
                                    double durationBeats,
                                    int midiNote,
                                    float velocity)
{
    auto* clip = findClipForProgression();
    if (clip == nullptr || noteId.isEmpty())
        return false;

    auto& seq = clip->getSequence();
    te::MidiNote* target = nullptr;
    for (auto* note : seq.getNotes())
    {
        if (note != nullptr && makeNoteId(clip, note) == noteId)
        {
            target = note;
            break;
        }
    }
    if (target == nullptr)
        return false;

    const auto clipStartBeat = edit.tempoSequence.toBeats(clip->getPosition().getStart()).inBeats();
    const auto bpm = juce::jmax(1.0, song.getBpm());
    const auto clipLengthBeats = clip->getPosition().getLength().inSeconds() / (60.0 / bpm);
    auto relativeStart = juce::jmax(0.0, absoluteStartBeat - clipStartBeat);
    relativeStart = juce::jlimit(0.0, juce::jmax(0.0, clipLengthBeats - 0.0625), relativeStart);

    auto dur = juce::jmax(0.0625, durationBeats);
    dur = juce::jlimit(0.0625, juce::jmax(0.0625, clipLengthBeats - relativeStart), dur);
    const auto clampedPitch = juce::jlimit(0, 127, midiNote);
    const auto clampedVelocity = juce::jlimit(1, 127, juce::roundToInt(juce::jlimit(0.0f, 1.0f, velocity) * 127.0f));

    target->setStartAndLength(tracktion::BeatPosition::fromBeats(relativeStart),
                              tracktion::BeatDuration::fromBeats(dur),
                              nullptr);
    target->setNoteNumber(clampedPitch, nullptr);
    target->setVelocity(clampedVelocity, nullptr);
    return true;
}

void NoteModeView::auditionNote(int midiNote)
{
    if (onStatusMessage != nullptr)
    {
        const auto name = juce::MidiMessage::getMidiNoteName(juce::jlimit(0, 127, midiNote), true, true, 4);
        onStatusMessage("Audition: " + name);
    }
}

void NoteModeView::emitBeatRangeChanged()
{
    if (onVisibleBeatRangeChanged != nullptr)
        onVisibleBeatRangeChanged(visibleStartBeat, visibleEndBeat);
}

void NoteModeView::emitPitchRangeChanged()
{
    if (onVisiblePitchRangeChanged != nullptr)
        onVisiblePitchRangeChanged(lowestMidiVisible, highestMidiVisible);
}

void NoteModeView::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    const auto bounds = getLocalBounds();
    const auto keys = keyBounds();
    const auto grid = gridBounds();

    g.fillAll(theme.surface1);
    g.setColour(theme.surfaceEdge.withAlpha(0.35f));
    g.drawVerticalLine(keys.getRight(), static_cast<float>(bounds.getY()), static_cast<float>(bounds.getBottom()));

    const auto ppb = pxPerBeat();
    const auto ppp = pxPerPitch();

    for (int midi = lowestMidiVisible; midi <= highestMidiVisible; ++midi)
    {
        const float y = static_cast<float>(grid.getBottom() - (midi - lowestMidiVisible + 1) * ppp);
        const auto row = juce::Rectangle<float>(static_cast<float>(grid.getX()), y,
                                                static_cast<float>(grid.getWidth()),
                                                static_cast<float>(ppp));

        const int pc = ((midi % 12) + 12) % 12;
        const bool isBlack = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        const bool isScale = containsPitchClass(scalePitchClasses, midi);
        const bool isChord = containsPitchClass(activeChordPitchClasses, midi);

        if (scaleLockEnabled || chordLockEnabled)
        {
            const auto analysis = constraintEngine.analyzeNote(midi, playheadBeat);
            const bool allowed = !analysis.wouldChange;
            if (allowed)
                g.setColour(theme.zoneC.withAlpha(isChord ? 0.24f : 0.18f));
            else
                g.setColour(theme.signalMidi.withAlpha(0.28f));
        }
        else if (isChord)
            g.setColour(theme.zoneB.withAlpha(0.18f));
        else if (isScale)
            g.setColour(theme.zoneC.withAlpha(0.10f));
        else if (isBlack)
            g.setColour(theme.surface0.withAlpha(0.60f));
        else
            g.setColour(theme.surface2.withAlpha(0.45f));
        g.fillRect(row);

        g.setColour(theme.surfaceEdge.withAlpha(0.15f));
        g.drawHorizontalLine(static_cast<int>(y), static_cast<float>(grid.getX()), static_cast<float>(grid.getRight()));
    }

    for (const auto& boundary : chordBoundaries)
    {
        if (boundary.endBeat < visibleStartBeat || boundary.startBeat > visibleEndBeat)
            continue;

        const float x = static_cast<float>(grid.getX() + (boundary.startBeat - visibleStartBeat) * ppb);
        const float startX = juce::jlimit(static_cast<float>(grid.getX()),
                                          static_cast<float>(grid.getRight()),
                                          x);
        const float endX = juce::jlimit(static_cast<float>(grid.getX()),
                                        static_cast<float>(grid.getRight()),
                                        static_cast<float>(grid.getX() + (boundary.endBeat - visibleStartBeat) * ppb));

        g.setColour(theme.surfaceEdge.withAlpha(0.50f));
        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(grid.getY()), static_cast<float>(grid.getBottom()));

        juce::Rectangle<float> labelBand(startX, static_cast<float>(grid.getY()),
                                         juce::jmax(0.0f, endX - startX), 14.0f);
        g.setColour(functionTint(theme, boundary.function).withAlpha(0.28f));
        g.fillRect(labelBand);

        g.setFont(10.0f);
        g.setColour(theme.inkMuted);
        g.drawText((boundary.romanNumeral + " " + boundary.symbol).trim(),
                   juce::Rectangle<int>(static_cast<int>(startX) + 2, grid.getY() + 1,
                                        juce::jmax(36, static_cast<int>(endX - startX) - 4), 12),
                   juce::Justification::centredLeft, true);
    }

    const auto meter = theory::MeterContext::forBeat(visibleStartBeat, song);
    const auto beatUnit = juce::jmax(0.0625, meter.beatUnit());
    const auto barSize = juce::jmax(beatUnit, meter.beatsPerBar());
    auto firstBeat = std::floor(visibleStartBeat / beatUnit) * beatUnit;
    for (double beat = firstBeat; beat <= visibleEndBeat + beatUnit; beat += beatUnit)
    {
        if (beat < visibleStartBeat - beatUnit)
            continue;

        const float x = static_cast<float>(grid.getX() + (beat - visibleStartBeat) * ppb);
        const bool isBar = std::abs(std::fmod(beat, barSize)) < 0.0005
                           || std::abs(std::fmod(beat, barSize) - barSize) < 0.0005;
        g.setColour(theme.surfaceEdge.withAlpha(isBar ? 0.38f : 0.18f));
        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(grid.getY()), static_cast<float>(grid.getBottom()));
    }

    for (const auto& note : notes)
    {
        if (note.startBeat > visibleEndBeat || note.startBeat + note.durationBeats < visibleStartBeat)
            continue;
        if (note.midiNote < lowestMidiVisible || note.midiNote > highestMidiVisible)
            continue;

        auto rect = noteRectFor(note);
        if (rect.getRight() < static_cast<float>(grid.getX()) || rect.getX() > static_cast<float>(grid.getRight()))
            continue;

        juce::Colour colour;
        if (note.isChordTone)
            colour = theme.accent;
        else if (note.isScaleTone)
            colour = theme.zoneC;
        else
            colour = theme.accentWarm;

        const auto brightness = juce::jmap(note.velocity, 0.0f, 1.0f, 0.52f, 1.0f);
        colour = colour.withMultipliedBrightness(brightness);

        g.setColour(colour);
        g.fillRoundedRectangle(rect, 3.0f);

        if (note.noteId == selectedNoteId)
        {
            g.setColour(theme.inkLight.withAlpha(0.95f));
            g.drawRoundedRectangle(rect, 3.0f, 1.5f);
        }

        g.setColour(colour.darker(0.35f));
        g.fillRect(rect.getRight() - 4.0f, rect.getY(), 4.0f, rect.getHeight());

        if (rect.getWidth() >= 24.0f)
        {
            const auto name = juce::MidiMessage::getMidiNoteName(note.midiNote, true, true, 4);
            g.setColour(theme.inkDark.withAlpha(0.9f));
            g.setFont(8.0f);
            g.drawText(name, rect.toNearestInt().reduced(2, 0), juce::Justification::centredLeft, false);
        }
    }

    const float playheadX = static_cast<float>(grid.getX() + (playheadBeat - visibleStartBeat) * ppb);
    if (playheadX >= grid.getX() && playheadX <= grid.getRight())
    {
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawVerticalLine(static_cast<int>(playheadX), static_cast<float>(grid.getY()), static_cast<float>(grid.getBottom()));
    }

    paintPianoKeys(g, keys);
}

void NoteModeView::paintPianoKeys(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto& theme = ThemeManager::get().theme();
    const auto ppp = pxPerPitch();

    for (int midi = lowestMidiVisible; midi <= highestMidiVisible; ++midi)
    {
        const int pc = ((midi % 12) + 12) % 12;
        const bool isBlack = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);
        const bool isScale = containsPitchClass(scalePitchClasses, midi);
        const bool isChord = containsPitchClass(activeChordPitchClasses, midi);

        const float y = static_cast<float>(bounds.getBottom() - (midi - lowestMidiVisible + 1) * ppp);
        const auto row = juce::Rectangle<float>(static_cast<float>(bounds.getX()),
                                                y,
                                                static_cast<float>(bounds.getWidth()),
                                                static_cast<float>(juce::jmax(1.0, ppp - 0.5)));

        if (isChord)
            g.setColour(theme.zoneB.withAlpha(0.72f));
        else if (isScale)
            g.setColour(theme.zoneC.withAlpha(0.52f));
        else if (isBlack)
            g.setColour(theme.surface0);
        else
            g.setColour(theme.inkLight.withAlpha(0.85f));
        g.fillRect(row);

        g.setColour(theme.surfaceEdge.withAlpha(0.45f));
        g.drawRect(row, 0.5f);

        if (pc == 0)
        {
            const int octave = (midi / 12) - 1;
            g.setFont(8.0f);
            g.setColour(theme.inkGhost.withAlpha(0.95f));
            g.drawText("C" + juce::String(octave),
                       row.toNearestInt().reduced(2, 0),
                       juce::Justification::centredRight,
                       false);
        }
    }
}

void NoteModeView::resized()
{
    repaint();
}

void NoteModeView::mouseDown(const juce::MouseEvent& e)
{
    isDragging = false;
    isResizing = false;
    dragChanged = false;
    dragNoteId.clear();

    if (e.position.x < static_cast<float>(kPianoKeyWidth))
    {
        auditionNote(yToMidi(e.getPosition().y));
        return;
    }

    using setle::ui::EditTool;
    const auto tool = setle::ui::EditToolManager::get().getActiveTool();
    const bool scaleSnapTool = (tool == EditTool::ScaleSnap);

    if (tool == EditTool::Draw || scaleSnapTool)
    {
        int midi = yToMidi(e.getPosition().y);
        if (scaleSnapTool)
            midi = snapMidiToScale(midi);

        auto beat = snapToBeatGrid(xToBeat(e.getPosition().x));
        const auto id = createNoteAtBeatAndPitch(beat, midi);
        if (id.isEmpty())
            return;

        rebuildNoteCache();
        selectedNoteId = id;
        emitSelectionChanged();
        dragNoteId = id;
        dragStart = e.getPosition();
        isDragging = true;
        isResizing = true;

        for (const auto& note : notes)
        {
            if (note.noteId != id)
                continue;
            dragOrigStartBeat = note.startBeat;
            dragOrigDuration = note.durationBeats;
            dragOrigMidiNote = note.midiNote;
            break;
        }

        syncNotesToSongModel();
        if (onNotesChanged != nullptr)
            onNotesChanged();
        repaint();
        return;
    }

    if (tool == EditTool::Erase)
    {
        const auto noteIndex = findNoteIndexAt(e.getPosition());
        if (noteIndex >= 0)
        {
            const auto& note = notes[static_cast<size_t>(noteIndex)];
            if (deleteNoteById(note.noteId))
            {
                selectedNoteId.clear();
                rebuildNoteCache();
                emitSelectionChanged();
                syncNotesToSongModel();
                repaint();
                if (onNotesChanged != nullptr)
                    onNotesChanged();
            }
        }
        return;
    }

    if (tool == EditTool::Select || tool == EditTool::ScaleSnap)
    {
        const auto noteIndex = findNoteIndexAt(e.getPosition());
        if (noteIndex < 0)
        {
            selectedNoteId.clear();
            emitSelectionChanged();
            repaint();
            return;
        }

        const auto& note = notes[static_cast<size_t>(noteIndex)];
        selectedNoteId = note.noteId;
        dragNoteId = note.noteId;
        dragOrigStartBeat = note.startBeat;
        dragOrigDuration = note.durationBeats;
        dragOrigMidiNote = note.midiNote;
        dragStart = e.getPosition();
        isDragging = true;
        isResizing = e.getPosition().x > noteRightEdgeX(note) - 6;
        emitSelectionChanged();
        repaint();
    }
}

void NoteModeView::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging || dragNoteId.isEmpty())
        return;

    auto currentNoteIt = std::find_if(notes.begin(), notes.end(),
                                      [this](const NoteEntry& n) { return n.noteId == dragNoteId; });
    if (currentNoteIt == notes.end())
        return;

    using setle::ui::EditTool;
    const auto tool = setle::ui::EditToolManager::get().getActiveTool();
    const bool scaleSnapTool = (tool == EditTool::ScaleSnap);

    const auto dxBeats = (e.getPosition().x - dragStart.x) / pxPerBeat();
    const auto dyPitches = static_cast<int>(std::round((dragStart.y - e.getPosition().y) / pxPerPitch()));

    auto newStart = dragOrigStartBeat;
    auto newDuration = dragOrigDuration;
    auto newMidi = dragOrigMidiNote;

    if (isResizing)
    {
        newDuration = juce::jmax(0.0625, snapToBeatGrid(dragOrigDuration + dxBeats));
    }
    else
    {
        newStart = snapToBeatGrid(dragOrigStartBeat + dxBeats);
        newMidi = juce::jlimit(0, 127, dragOrigMidiNote + dyPitches);
        if (scaleSnapTool)
            newMidi = snapMidiToScale(newMidi);
    }

    if (updateNoteInClip(dragNoteId, newStart, newDuration, newMidi, currentNoteIt->velocity))
    {
        dragChanged = true;
        rebuildNoteCache();
        repaint();
    }
}

void NoteModeView::mouseUp(const juce::MouseEvent&)
{
    isDragging = false;
    isResizing = false;
    if (dragChanged)
    {
        syncNotesToSongModel();
        if (onNotesChanged != nullptr)
            onNotesChanged();
    }
    dragChanged = false;
}

void NoteModeView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.position.x < static_cast<float>(kPianoKeyWidth))
    {
        auditionNote(yToMidi(e.getPosition().y));
        return;
    }

    using setle::ui::EditTool;
    const auto tool = setle::ui::EditToolManager::get().getActiveTool();
    if (tool != EditTool::Select && tool != EditTool::Draw && tool != EditTool::ScaleSnap)
        return;

    if (findNoteIndexAt(e.getPosition()) >= 0)
        return;

    int midi = yToMidi(e.getPosition().y);
    if (tool == EditTool::ScaleSnap)
        midi = snapMidiToScale(midi);

    const auto id = createNoteAtBeatAndPitch(snapToBeatGrid(xToBeat(e.getPosition().x)), midi);
    if (id.isEmpty())
        return;

    selectedNoteId = id;
    rebuildNoteCache();
    emitSelectionChanged();
    repaint();
    syncNotesToSongModel();
    if (onNotesChanged != nullptr)
        onNotesChanged();
}

void NoteModeView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto wheelPrimary = std::abs(wheel.deltaY) > std::abs(wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
    if (std::abs(wheelPrimary) < 0.0001f)
        return;

    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        const auto span = visibleBeatSpan();
        const auto factor = wheelPrimary > 0.0f ? 0.88 : 1.12;
        const auto newSpan = clampBeatSpan(span * factor);
        const auto anchorBeat = xToBeat(e.getPosition().x);
        const auto anchorRatio = span > 0.0001 ? (anchorBeat - visibleStartBeat) / span : 0.5;
        const auto newStart = anchorBeat - newSpan * anchorRatio;
        setVisibleBeatRange(newStart, newStart + newSpan);
        return;
    }

    if (e.position.x < static_cast<float>(kPianoKeyWidth))
    {
        const int pitchDelta = juce::roundToInt(-wheelPrimary * 8.0f);
        if (pitchDelta != 0)
            setVisiblePitchRange(lowestMidiVisible + pitchDelta, highestMidiVisible + pitchDelta);
        return;
    }

    const auto delta = -static_cast<double>(wheelPrimary) * visibleBeatSpan() * 0.15;
    scrollByBeats(delta);
}

std::optional<NoteModeView::SelectionMetadata> NoteModeView::getCurrentSelectionMetadata() const
{
    if (selectedNoteId.isEmpty())
        return std::nullopt;

    const auto found = std::find_if(notes.begin(), notes.end(),
                                    [this](const NoteEntry& entry)
                                    {
                                        return entry.noteId == selectedNoteId;
                                    });
    if (found == notes.end())
        return std::nullopt;

    SelectionMetadata metadata;
    metadata.valid = true;
    metadata.noteId = found->noteId;
    metadata.chordId = found->chordId;
    metadata.chordSymbol = found->chordSymbol;
    metadata.velocity = found->velocity;

    auto symbol = found->chordSymbol.trim();
    if (symbol.isEmpty())
    {
        metadata.root = juce::MidiMessage::getMidiNoteName(found->midiNote, true, true, 4)
                            .retainCharacters("ABCDEFG#b");
        metadata.extension = {};
        metadata.inversion = 0;
        return metadata;
    }

    auto slash = symbol.indexOfChar('/');
    if (slash >= 0)
    {
        const auto inversionToken = symbol.substring(slash + 1).trim();
        if (inversionToken.isNotEmpty())
            metadata.inversion = juce::jmax(0, inversionToken.getIntValue());
        symbol = symbol.substring(0, slash).trim();
    }

    if (symbol.isNotEmpty())
    {
        metadata.root << symbol[0];
        if (symbol.length() > 1 && (symbol[1] == '#' || symbol[1] == 'b'))
        {
            metadata.root << symbol[1];
            metadata.extension = symbol.substring(2);
        }
        else
        {
            metadata.extension = symbol.substring(1);
        }
    }

    return metadata;
}

bool NoteModeView::applyTheoryMutationToSelection(const juce::String& root,
                                                  const juce::String& extension,
                                                  int inversion,
                                                  float velocity)
{
    const auto selected = getCurrentSelectionMetadata();
    if (!selected.has_value())
        return false;

    auto* clip = findClipForProgression();
    if (clip == nullptr)
        return false;

    const auto found = std::find_if(notes.begin(), notes.end(),
                                    [this](const NoteEntry& entry)
                                    {
                                        return entry.noteId == selectedNoteId;
                                    });
    if (found == notes.end())
        return false;

    const auto selectedBeat = found->startBeat;
    const auto* boundary = findChordBoundaryForBeat(selectedBeat);
    const auto symbol = (root + extension).trim();
    auto pitchClasses = theory::BachTheory::getChordPitchClasses(symbol);
    if (pitchClasses.empty())
        pitchClasses = theory::BachTheory::getChordPitchClasses(selected->chordSymbol);

    std::vector<NoteEntry> targetNotes;
    for (const auto& note : notes)
    {
        bool inTarget = (note.noteId == selectedNoteId);
        if (boundary != nullptr)
            inTarget = note.startBeat >= boundary->startBeat - 0.0001 && note.startBeat < boundary->endBeat + 0.0001;
        if (inTarget)
            targetNotes.push_back(note);
    }

    if (targetNotes.empty())
        return false;

    std::sort(targetNotes.begin(), targetNotes.end(),
              [](const NoteEntry& a, const NoteEntry& b) { return a.midiNote < b.midiNote; });

    std::vector<int> targetPitches;
    targetPitches.reserve(targetNotes.size());

    for (size_t i = 0; i < targetNotes.size(); ++i)
    {
        auto pitch = targetNotes[i].midiNote;
        if (!pitchClasses.empty())
        {
            const int pc = pitchClasses[i % pitchClasses.size()];
            int best = pitch;
            int bestDistance = std::numeric_limits<int>::max();
            const int octave = pitch / 12;
            for (int o = octave - 2; o <= octave + 2; ++o)
            {
                const int candidate = o * 12 + pc;
                if (candidate < 0 || candidate > 127)
                    continue;
                const int distance = std::abs(candidate - pitch);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = candidate;
                }
            }
            pitch = best;
        }
        targetPitches.push_back(juce::jlimit(0, 127, pitch));
    }

    const int absInversion = std::abs(inversion);
    const int shiftCount = juce::jmin(absInversion, static_cast<int>(targetPitches.size()));
    if (inversion > 0)
    {
        for (int i = 0; i < shiftCount; ++i)
            targetPitches[static_cast<size_t>(i)] = juce::jmin(127, targetPitches[static_cast<size_t>(i)] + 12);
    }
    else if (inversion < 0)
    {
        for (int i = 0; i < shiftCount; ++i)
        {
            const auto idx = static_cast<size_t>(targetPitches.size() - 1 - i);
            targetPitches[idx] = juce::jmax(0, targetPitches[idx] - 12);
        }
    }

    const auto clampedVelocity = juce::jlimit(0.0f, 1.0f, velocity);
    bool changed = false;
    for (size_t i = 0; i < targetNotes.size(); ++i)
    {
        changed |= updateNoteInClip(targetNotes[i].noteId,
                                    targetNotes[i].startBeat,
                                    targetNotes[i].durationBeats,
                                    targetPitches[i],
                                    clampedVelocity);
    }

    if (!changed)
        return false;

    rebuildNoteCache();
    emitSelectionChanged();
    syncNotesToSongModel();
    repaint();
    if (onNotesChanged != nullptr)
        onNotesChanged();
    return true;
}

bool NoteModeView::applyHumanizeToSelection(int timingJitterMs, int velocityDeviation)
{
    const auto selected = getCurrentSelectionMetadata();
    if (!selected.has_value())
        return false;

    auto found = std::find_if(notes.begin(), notes.end(),
                              [this](const NoteEntry& entry)
                              {
                                  return entry.noteId == selectedNoteId;
                              });
    if (found == notes.end())
        return false;

    const auto selectedBeat = found->startBeat;
    const auto* boundary = findChordBoundaryForBeat(selectedBeat);
    std::vector<NoteEntry> targetNotes;
    for (const auto& note : notes)
    {
        bool inTarget = (note.noteId == selectedNoteId);
        if (boundary != nullptr)
            inTarget = note.startBeat >= boundary->startBeat - 0.0001 && note.startBeat < boundary->endBeat + 0.0001;
        if (inTarget)
            targetNotes.push_back(note);
    }

    if (targetNotes.empty())
        return false;

    const auto bpm = juce::jmax(1.0, song.getBpm());
    const auto secondsPerBeat = 60.0 / bpm;
    const auto jitterBeats = juce::jmax(0.0, static_cast<double>(timingJitterMs) / 1000.0 / secondsPerBeat);
    const auto velocityNorm = juce::jmax(0.0f, static_cast<float>(velocityDeviation) / 127.0f);

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> jitterDist(-jitterBeats, jitterBeats);
    std::uniform_real_distribution<float> velDist(-velocityNorm, velocityNorm);

    bool changed = false;
    for (const auto& note : targetNotes)
    {
        const auto updatedStart = juce::jmax(0.0, note.startBeat + jitterDist(rng));
        const auto updatedVel = juce::jlimit(0.0f, 1.0f, note.velocity + velDist(rng));
        changed |= updateNoteInClip(note.noteId,
                                    updatedStart,
                                    note.durationBeats,
                                    note.midiNote,
                                    updatedVel);
    }

    if (!changed)
        return false;

    rebuildNoteCache();
    emitSelectionChanged();
    syncNotesToSongModel();
    repaint();
    if (onNotesChanged != nullptr)
        onNotesChanged();
    return true;
}

void NoteModeView::setScaleLockEnabled(bool enabled)
{
    scaleLockEnabled = enabled;
    constraintEngine.setScaleLock(enabled);
    repaint();
}

void NoteModeView::setChordLockEnabled(bool enabled)
{
    chordLockEnabled = enabled;
    constraintEngine.setChordLock(enabled);
    repaint();
}

void NoteModeView::syncNotesToSongModel()
{
    auto progression = song.findProgressionById(progressionId);
    if (!progression.has_value())
        return;

    auto progressionTree = progression->valueTree();
    if (!progressionTree.isValid())
        return;

    std::vector<ChordBoundary> boundaries;
    for (const auto& boundary : chordBoundaries)
        boundaries.push_back(boundary);

    for (int chordIndex = 0; chordIndex < progressionTree.getNumChildren(); ++chordIndex)
    {
        auto chordTree = progressionTree.getChild(chordIndex);
        if (!chordTree.hasType(model::Schema::chordType))
            continue;

        const auto chordId = chordTree.getProperty(model::Schema::idProp).toString();
        const auto chordStart = chordTree.getProperty(model::Schema::startBeatsProp);

        for (int i = chordTree.getNumChildren(); --i >= 0;)
        {
            if (chordTree.getChild(i).hasType(model::Schema::noteType))
                chordTree.removeChild(i, nullptr);
        }

        for (const auto& note : notes)
        {
            bool belongsToChord = (note.chordId == chordId);
            if (!belongsToChord)
            {
                for (const auto& boundary : boundaries)
                {
                    if (boundary.chordId != chordId)
                        continue;

                    if (note.startBeat >= boundary.startBeat - 0.0001
                        && note.startBeat < boundary.endBeat + 0.0001)
                    {
                        belongsToChord = true;
                        break;
                    }
                }
            }

            if (!belongsToChord)
                continue;

            auto newNote = model::Note::create(note.midiNote,
                                               note.velocity,
                                               juce::jmax(0.0, note.startBeat - static_cast<double>(chordStart)),
                                               note.durationBeats,
                                               1);
            chordTree.appendChild(newNote.valueTree(), nullptr);
        }
    }
}

void NoteModeView::emitSelectionChanged()
{
    if (onSelectionChanged == nullptr)
        return;

    auto metadata = getCurrentSelectionMetadata();
    if (metadata.has_value())
    {
        onSelectionChanged(*metadata);
        return;
    }

    SelectionMetadata empty;
    onSelectionChanged(empty);
}

} // namespace setle::gridroll
