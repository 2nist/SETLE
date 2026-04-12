#include "NoteDetailView.h"

#include <algorithm>
#include <cmath>
#include <array>

namespace setle::gridroll
{

// Whole-step / half-step pattern for each mode (7 modes, 7 intervals each)
static const std::array<int,7> kModeIntervals[] = {
    { 0, 2, 4, 5, 7, 9, 11 },  // ionian / major
    { 0, 2, 3, 5, 7, 9, 10 },  // dorian
    { 0, 1, 3, 5, 7, 8, 10 },  // phrygian
    { 0, 2, 4, 6, 7, 9, 11 },  // lydian
    { 0, 2, 4, 5, 7, 9, 10 },  // mixolydian
    { 0, 2, 3, 5, 7, 8, 10 },  // aeolian / natural minor
    { 0, 1, 3, 5, 6, 8, 10 },  // locrian
};

static int semitoneFromName(const juce::String& name)
{
    static const juce::String notes[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    for (int i = 0; i < 12; ++i)
        if (notes[i].equalsIgnoreCase(name)) return i;
    return 0;
}

static std::vector<int> buildScaleIntervals(const juce::String& key, const juce::String& mode)
{
    const int root = semitoneFromName(key);
    const std::array<int,7>* intervals = &kModeIntervals[0]; // default ionian
    const juce::String lc = mode.toLowerCase();
    if      (lc == "dorian")      intervals = &kModeIntervals[1];
    else if (lc == "phrygian")    intervals = &kModeIntervals[2];
    else if (lc == "lydian")      intervals = &kModeIntervals[3];
    else if (lc == "mixolydian")  intervals = &kModeIntervals[4];
    else if (lc == "aeolian" || lc == "minor") intervals = &kModeIntervals[5];
    else if (lc == "locrian")     intervals = &kModeIntervals[6];

    std::vector<int> result;
    for (int iv : *intervals)
        result.push_back((root + iv) % 12);
    return result;
}

// ---------------------------------------------------------------
NoteDetailView::NoteDetailView(model::Song& songRef)
    : song(songRef)
{
    setWantsKeyboardFocus(true);
    setSize(400, kHeight);
}

// ---------------------------------------------------------------
void NoteDetailView::loadCell(const GridRollCell& cell,
                               const juce::String& key,
                               const juce::String& mode)
{
    sourceCell   = cell;
    sessionKey   = key;
    sessionMode  = mode;
    notes.clear();
    noteRects.clear();
    selectedNoteIdx = -1;

    scaleIntervals = buildScaleIntervals(key, mode);
    chordTones     = cell.pitchClasses;

    // Centre the keyboard on chord root ± 1 octave
    const int root = cell.midiNote;
    lowestPitch = juce::jlimit(0, 127 - kNoteRows, root - 12);

    // Load existing notes from model chord if available
    auto progOpt = song.findProgressionById(cell.sourceId);
    if (progOpt.has_value())
    {
        for (const auto& chord : progOpt->getChords())
        {
            if (chord.getId() == cell.cellId)
            {
                notes = chord.getNotes();
                break;
            }
        }
    }

    // If no notes, seed from pitch classes
    if (notes.empty() && !cell.pitchClasses.empty())
    {
        double t = 0.0;
        for (int pc : cell.pitchClasses)
        {
            int pitch = lowestPitch + ((pc - lowestPitch % 12 + 12) % 12);
            if (pitch < lowestPitch) pitch += 12;
            notes.push_back(model::Note::create(pitch, cell.velocity, t, 0.5));
            t += 0.25;
        }
    }

    rebuildNoteRects();
    repaint();
}

// ---------------------------------------------------------------
juce::Rectangle<int> NoteDetailView::keyBounds() const
{
    return { 0, 0, kKeyWidth, getHeight() };
}

juce::Rectangle<int> NoteDetailView::noteGridBounds() const
{
    return { kKeyWidth, 0, getWidth() - kKeyWidth, getHeight() };
}

int NoteDetailView::pitchToRow(int pitch) const
{
    // row 0 = top = highest pitch
    return (lowestPitch + kNoteRows - 1) - pitch;
}

int NoteDetailView::rowToPitch(int row) const
{
    return (lowestPitch + kNoteRows - 1) - row;
}

double NoteDetailView::pixelToTime(int px) const
{
    const auto grid = noteGridBounds();
    const double fraction = static_cast<double>(px - grid.getX()) /
                            static_cast<double>(grid.getWidth());
    return fraction * sourceCell.durationBeats;
}

int NoteDetailView::timeToPx(double beat) const
{
    const auto grid = noteGridBounds();
    const double fraction = beat / juce::jmax(0.001, sourceCell.durationBeats);
    return grid.getX() + static_cast<int>(fraction * grid.getWidth());
}

bool NoteDetailView::isChordTone(int pitch) const
{
    const int pc = pitch % 12;
    return std::find(chordTones.begin(), chordTones.end(), pc) != chordTones.end();
}

bool NoteDetailView::isScaleTone(int pitch) const
{
    const int pc = pitch % 12;
    return std::find(scaleIntervals.begin(), scaleIntervals.end(), pc) != scaleIntervals.end();
}

void NoteDetailView::rebuildNoteRects()
{
    noteRects.clear();
    const auto grid = noteGridBounds();
    const int rowH = juce::jmax(1, getHeight() / kNoteRows);

    for (const auto& note : notes)
    {
        const int row = pitchToRow(note.getPitch());
        if (row < 0 || row >= kNoteRows) { noteRects.push_back({}); continue; }

        const int x = timeToPx(note.getStartBeats());
        const int w = juce::jmax(4, static_cast<int>(
            (note.getDurationBeats() / sourceCell.durationBeats) * grid.getWidth()));
        noteRects.push_back({ x, row * rowH, w, rowH });
    }
}

// ---------------------------------------------------------------
void NoteDetailView::paint(juce::Graphics& g)
{
    const auto grid = noteGridBounds();
    const int rowH  = juce::jmax(1, getHeight() / kNoteRows);

    // ---- Piano keyboard strip ----
    g.setColour(juce::Colour(0xff1a2030));
    g.fillRect(keyBounds());

    static const int blackKeys[] = { 1, 3, 6, 8, 10 }; // semitones within octave
    for (int row = 0; row < kNoteRows; ++row)
    {
        const int pitch = rowToPitch(row);
        const int pc    = pitch % 12;
        const bool isBlack = std::find(std::begin(blackKeys), std::end(blackKeys), pc)
                             != std::end(blackKeys);
        const bool inChord = isChordTone(pitch);
        const bool inScale = isScaleTone(pitch);

        juce::Colour keyCol = isBlack ? juce::Colour(0xff2a2a2a) : juce::Colour(0xffe8e8e8);
        if (inChord)      keyCol = juce::Colour(0xffdd9933);
        else if (inScale) keyCol = juce::Colour(0xff44aa44);

        g.setColour(keyCol);
        g.fillRect(0, row * rowH, kKeyWidth - 1, rowH - 1);
    }

    // ---- Note grid background ----
    for (int row = 0; row < kNoteRows; ++row)
    {
        const int pitch = rowToPitch(row);
        juce::Colour bg(0xff1e2936);
        if      (isChordTone(pitch))  bg = juce::Colour(0x22d4a057);
        else if (isScaleTone(pitch))  bg = juce::Colour(0x1144aa44);
        // outside scale: stays dark

        g.setColour(bg);
        g.fillRect(grid.getX(), row * rowH, grid.getWidth(), rowH - 1);
    }

    // Time grid lines (beat subdivisions)
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    const double subDiv = 0.25; // 16th notes
    for (double t = 0.0; t <= sourceCell.durationBeats; t += subDiv)
    {
        const int x = timeToPx(t);
        g.fillRect(x, 0, 1, getHeight());
    }

    // ---- Notes ----
    const size_t nNotes = notes.size();
    for (size_t i = 0; i < nNotes && i < noteRects.size(); ++i)
    {
        if (noteRects[i].isEmpty()) continue;

        const auto& note = notes[i];
        const int pitch  = note.getPitch();
        const bool chord = isChordTone(pitch);
        const bool scale = isScaleTone(pitch);
        const bool sel   = (static_cast<int>(i) == selectedNoteIdx);

        juce::Colour noteCol;
        if      (chord) noteCol = juce::Colour(0xffdd9933);
        else if (scale) noteCol = juce::Colour(0xff44aa44).withAlpha(0.7f);
        else            noteCol = juce::Colour(0xffcc4444); // out of scale: red warning

        g.setColour(noteCol);
        g.fillRoundedRectangle(noteRects[i].toFloat().reduced(0.5f), 2.0f);

        if (sel)
        {
            g.setColour(juce::Colours::white.withAlpha(0.4f));
            g.drawRoundedRectangle(noteRects[i].toFloat().reduced(0.5f), 2.0f, 1.5f);
        }
    }
}

// ---------------------------------------------------------------
void NoteDetailView::resized()
{
    rebuildNoteRects();
}

// ---------------------------------------------------------------
NoteDetailView::NoteRect NoteDetailView::hitTestGrid(juce::Point<int> pos) const
{
    for (int i = static_cast<int>(noteRects.size()) - 1; i >= 0; --i)
    {
        if (noteRects[i].isEmpty()) continue;
        if (!noteRects[i].contains(pos)) continue;
        const bool resize = pos.getX() >= noteRects[i].getRight() - 6;
        return { i, noteRects[i], resize };
    }
    return { -1, {}, false };
}

// ---------------------------------------------------------------
void NoteDetailView::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragKind = DragKind::None;

    if (!noteGridBounds().contains(e.getPosition()))
        return;

    const auto hit = hitTestGrid(e.getPosition());

    if (e.mods.isRightButtonDown())
    {
        if (hit.noteIndex >= 0)
        {
            selectedNoteIdx = hit.noteIndex;
            repaint();
            showNoteContext(hit.noteIndex, e.getScreenPosition());
        }
        return;
    }

    if (hit.noteIndex < 0)
    {
        // Add note
        const int rowH  = juce::jmax(1, getHeight() / kNoteRows);
        const int row   = e.getPosition().getY() / rowH;
        const int pitch = juce::jlimit(0, 127, rowToPitch(row));
        const double t  = juce::jmax(0.0, pixelToTime(e.getPosition().getX()));
        const double dur = juce::jmax(0.0625, sourceCell.durationBeats / 16.0);
        notes.push_back(model::Note::create(pitch, 0.8f, t, dur));
        selectedNoteIdx = static_cast<int>(notes.size()) - 1;
        rebuildNoteRects();
        repaint();
        if (onNotesChanged) onNotesChanged();
        dragKind = DragKind::None;
        return;
    }

    selectedNoteIdx = hit.noteIndex;
    dragStartPos    = e.getPosition();
    dragNoteIdx     = hit.noteIndex;
    dragOrigStart   = notes[static_cast<size_t>(hit.noteIndex)].getStartBeats();
    dragOrigPitch   = notes[static_cast<size_t>(hit.noteIndex)].getPitch();
    dragOrigDuration = notes[static_cast<size_t>(hit.noteIndex)].getDurationBeats();

    dragKind = hit.resizeHandle ? DragKind::ResizeNote : DragKind::MoveNote;
    repaint();
}

// ---------------------------------------------------------------
void NoteDetailView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragKind == DragKind::None || dragNoteIdx < 0)
        return;

    const auto grid = noteGridBounds();
    const int rowH  = juce::jmax(1, getHeight() / kNoteRows);
    const double secPerPx = sourceCell.durationBeats / juce::jmax(1.0, static_cast<double>(grid.getWidth()));
    const double dxBeats  = (e.getPosition().getX() - dragStartPos.getX()) * secPerPx;
    const int    dyRows   = (e.getPosition().getY() - dragStartPos.getY()) / rowH;
    auto& note = notes[static_cast<size_t>(dragNoteIdx)];

    if (dragKind == DragKind::MoveNote)
    {
        const double newStart = juce::jmax(0.0, dragOrigStart + dxBeats);
        note.setStartBeats(newStart);
        const int newPitch = juce::jlimit(0, 127, dragOrigPitch - dyRows);
        note.setPitch(newPitch);
    }
    else if (dragKind == DragKind::ResizeNote)
    {
        const double newDur = juce::jmax(0.0625, dragOrigDuration + dxBeats);
        note.setDurationBeats(newDur);
    }

    rebuildNoteRects();
    repaint();
}

// ---------------------------------------------------------------
void NoteDetailView::mouseUp(const juce::MouseEvent&)
{
    if (dragKind != DragKind::None)
    {
        if (onNotesChanged) onNotesChanged();
    }
    dragKind     = DragKind::None;
    dragNoteIdx  = -1;
}

// ---------------------------------------------------------------
bool NoteDetailView::keyPressed(const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        && selectedNoteIdx >= 0 && selectedNoteIdx < static_cast<int>(notes.size()))
    {
        notes.erase(notes.begin() + selectedNoteIdx);
        selectedNoteIdx = juce::jmin(selectedNoteIdx, static_cast<int>(notes.size()) - 1);
        rebuildNoteRects();
        repaint();
        if (onNotesChanged) onNotesChanged();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (onCloseRequested) onCloseRequested();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------
void NoteDetailView::showNoteContext(int noteIdx, juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Set Velocity...");
    menu.addItem(2, "Quantize to Grid");
    menu.addSeparator();
    menu.addItem(3, "Delete Note");

    const int result = menu.showAt(juce::Rectangle<int>(screenPos.getX(), screenPos.getY(), 1, 1));

    if (result == 1)
    {
        juce::AlertWindow::showInputBoxAsync(
            "Set Velocity", "Enter velocity (0.0 – 1.0):",
            juce::String(notes[static_cast<size_t>(noteIdx)].getVelocity()), nullptr,
            [this, noteIdx](const juce::String& text)
            {
                if (text.isNotEmpty())
                {
                    notes[static_cast<size_t>(noteIdx)].setVelocity(
                        juce::jlimit(0.0f, 1.0f, text.getFloatValue()));
                    repaint();
                    if (onNotesChanged) onNotesChanged();
                }
            });
    }
    else if (result == 2)
    {
        // Quantize to nearest 1/16
        const double grid = 0.0625;
        auto& n = notes[static_cast<size_t>(noteIdx)];
        n.setStartBeats(std::round(n.getStartBeats() / grid) * grid);
        rebuildNoteRects();
        repaint();
        if (onNotesChanged) onNotesChanged();
    }
    else if (result == 3)
    {
        notes.erase(notes.begin() + noteIdx);
        selectedNoteIdx = juce::jmin(selectedNoteIdx, static_cast<int>(notes.size()) - 1);
        rebuildNoteRects();
        repaint();
        if (onNotesChanged) onNotesChanged();
    }
}

} // namespace setle::gridroll
