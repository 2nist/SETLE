#include "NoteModeView.h"

#include "../theme/ThemeManager.h"
#include "../theory/BachTheory.h"
#include "../theory/DiatonicHarmony.h"
#include "../theory/MeterContext.h"
#include "../ui/EditToolManager.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

        const auto chordPcs = theory::BachTheory::getChordPitchClasses(chord.getSymbol());
        for (const auto& note : chord.getNotes())
        {
            NoteEntry entry;
            entry.noteId = note.getId();
            entry.startBeat = boundary.startBeat + note.getStartBeats();
            entry.durationBeats = juce::jmax(0.0625, note.getDurationBeats());
            entry.midiNote = note.getPitch();
            entry.velocity = note.getVelocity();
            entry.chordId = chord.getId();
            entry.chordSymbol = chord.getSymbol();
            entry.isChordTone = containsPitchClass(chordPcs, note.getPitch());
            entry.isScaleTone = containsPitchClass(scalePitchClasses, note.getPitch());
            notes.push_back(std::move(entry));
        }
    }

    std::sort(notes.begin(), notes.end(),
              [](const NoteEntry& a, const NoteEntry& b)
              {
                  if (std::abs(a.startBeat - b.startBeat) < 0.0001)
                      return a.midiNote < b.midiNote;
                  return a.startBeat < b.startBeat;
              });
}

void NoteModeView::updateActiveChordPitchClasses()
{
    activeChordPitchClasses.clear();
    const auto* boundary = findChordBoundaryForBeat(playheadBeat);
    if (boundary == nullptr)
        return;

    activeChordPitchClasses = theory::BachTheory::getChordPitchClasses(boundary->symbol);
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

juce::String NoteModeView::createNoteAtBeatAndPitch(double beat, int midiNote)
{
    const auto* boundary = findChordBoundaryForBeat(beat);
    if (boundary == nullptr)
        return {};

    const auto progression = song.findProgressionById(progressionId);
    if (!progression.has_value())
        return {};

    auto chords = progression->getChords();
    for (auto& chord : chords)
    {
        if (chord.getId() != boundary->chordId)
            continue;

        const auto relativeStart = juce::jlimit(0.0,
                                                juce::jmax(0.0, boundary->endBeat - boundary->startBeat - 0.0625),
                                                beat - boundary->startBeat);
        const auto maxDur = juce::jmax(0.0625, boundary->endBeat - (boundary->startBeat + relativeStart));
        const auto duration = juce::jmin(0.5, maxDur);

        auto note = model::Note::create(juce::jlimit(0, 127, midiNote),
                                        0.8f,
                                        relativeStart,
                                        duration);
        const auto id = note.getId();
        chord.addNote(note);
        return id;
    }

    return {};
}

bool NoteModeView::deleteNoteById(const juce::String& chordId, const juce::String& noteId)
{
    if (noteId.isEmpty())
        return false;

    const auto progression = song.findProgressionById(progressionId);
    if (!progression.has_value())
        return false;

    const auto chords = progression->getChords();
    for (const auto& chord : chords)
    {
        if (chordId.isNotEmpty() && chord.getId() != chordId)
            continue;

        auto chordTree = chord.valueTree();
        for (int i = chordTree.getNumChildren() - 1; i >= 0; --i)
        {
            auto noteTree = chordTree.getChild(i);
            if (!noteTree.hasType(model::Schema::noteType))
                continue;
            if (noteTree.getProperty(model::Schema::idProp).toString() != noteId)
                continue;

            chordTree.removeChild(i, nullptr);
            return true;
        }
    }

    return false;
}

bool NoteModeView::updateNoteInModel(const juce::String& sourceChordId,
                                     const juce::String& noteId,
                                     double absoluteStartBeat,
                                     double durationBeats,
                                     int midiNote,
                                     float velocity)
{
    if (noteId.isEmpty())
        return false;

    const auto progression = song.findProgressionById(progressionId);
    if (!progression.has_value())
        return false;

    const auto chords = progression->getChords();
    juce::ValueTree sourceChordTree;
    juce::ValueTree sourceNoteTree;
    juce::String sourceChordIdResolved = sourceChordId;

    auto findNoteInChord = [&noteId](const juce::ValueTree& chordTree) -> juce::ValueTree
    {
        for (const auto& child : chordTree)
        {
            if (!child.hasType(model::Schema::noteType))
                continue;
            if (child.getProperty(model::Schema::idProp).toString() == noteId)
                return child;
        }
        return {};
    };

    for (const auto& chord : chords)
    {
        if (sourceChordIdResolved.isNotEmpty() && chord.getId() != sourceChordIdResolved)
            continue;

        auto candidate = chord.valueTree();
        auto candidateNote = findNoteInChord(candidate);
        if (candidateNote.isValid())
        {
            sourceChordTree = candidate;
            sourceNoteTree = candidateNote;
            sourceChordIdResolved = chord.getId();
            break;
        }
    }

    if (!sourceNoteTree.isValid())
    {
        for (const auto& chord : chords)
        {
            auto candidate = chord.valueTree();
            auto candidateNote = findNoteInChord(candidate);
            if (candidateNote.isValid())
            {
                sourceChordTree = candidate;
                sourceNoteTree = candidateNote;
                sourceChordIdResolved = chord.getId();
                break;
            }
        }
    }

    if (!sourceNoteTree.isValid() || !sourceChordTree.isValid())
        return false;

    const auto* targetBoundary = findChordBoundaryForBeat(absoluteStartBeat);
    if (targetBoundary == nullptr)
    {
        for (const auto& boundary : chordBoundaries)
        {
            if (boundary.chordId == sourceChordIdResolved)
            {
                targetBoundary = &boundary;
                break;
            }
        }
    }

    if (targetBoundary == nullptr)
        return false;

    juce::ValueTree targetChordTree;
    for (const auto& chord : chords)
    {
        if (chord.getId() == targetBoundary->chordId)
        {
            targetChordTree = chord.valueTree();
            break;
        }
    }

    if (!targetChordTree.isValid())
        targetChordTree = sourceChordTree;

    auto start = juce::jlimit(targetBoundary->startBeat,
                              juce::jmax(targetBoundary->startBeat, targetBoundary->endBeat - 0.0625),
                              absoluteStartBeat);
    auto dur = juce::jmax(0.0625, durationBeats);
    const auto maxDur = juce::jmax(0.0625, targetBoundary->endBeat - start);
    dur = juce::jmin(dur, maxDur);

    const auto relativeStart = juce::jmax(0.0, start - targetBoundary->startBeat);
    const auto clampedPitch = juce::jlimit(0, 127, midiNote);
    const auto clampedVelocity = juce::jlimit(0.0f, 1.0f, velocity);

    if (targetChordTree == sourceChordTree)
    {
        sourceNoteTree.setProperty(model::Schema::startBeatsProp, relativeStart, nullptr);
        sourceNoteTree.setProperty(model::Schema::durationBeatsProp, dur, nullptr);
        sourceNoteTree.setProperty(model::Schema::pitchProp, clampedPitch, nullptr);
        sourceNoteTree.setProperty(model::Schema::velocityProp, clampedVelocity, nullptr);
        return true;
    }

    auto movedNote = sourceNoteTree.createCopy();
    movedNote.setProperty(model::Schema::startBeatsProp, relativeStart, nullptr);
    movedNote.setProperty(model::Schema::durationBeatsProp, dur, nullptr);
    movedNote.setProperty(model::Schema::pitchProp, clampedPitch, nullptr);
    movedNote.setProperty(model::Schema::velocityProp, clampedVelocity, nullptr);

    sourceChordTree.removeChild(sourceNoteTree, nullptr);
    targetChordTree.appendChild(movedNote, nullptr);
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

        if (isChord)
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
    dragSourceChordId.clear();

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
            dragSourceChordId = note.chordId;
            break;
        }

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
            if (deleteNoteById(note.chordId, note.noteId))
            {
                selectedNoteId.clear();
                rebuildNoteCache();
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
            repaint();
            return;
        }

        const auto& note = notes[static_cast<size_t>(noteIndex)];
        selectedNoteId = note.noteId;
        dragNoteId = note.noteId;
        dragSourceChordId = note.chordId;
        dragOrigStartBeat = note.startBeat;
        dragOrigDuration = note.durationBeats;
        dragOrigMidiNote = note.midiNote;
        dragStart = e.getPosition();
        isDragging = true;
        isResizing = e.getPosition().x > noteRightEdgeX(note) - 6;
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

    if (updateNoteInModel(dragSourceChordId, dragNoteId, newStart, newDuration, newMidi, currentNoteIt->velocity))
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
    if (dragChanged && onNotesChanged != nullptr)
        onNotesChanged();
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
    repaint();
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

} // namespace setle::gridroll
