#include "NoteDetailView.h"
#include "../theme/ThemeManager.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace setle::gridroll
{

static const std::array<int, 7> kModeIntervals[] = {
    { 0, 2, 4, 5, 7, 9, 11 },
    { 0, 2, 3, 5, 7, 9, 10 },
    { 0, 1, 3, 5, 7, 8, 10 },
    { 0, 2, 4, 6, 7, 9, 11 },
    { 0, 2, 4, 5, 7, 9, 10 },
    { 0, 2, 3, 5, 7, 8, 10 },
    { 0, 1, 3, 5, 6, 8, 10 },
};

static int semitoneFromName(const juce::String& name)
{
    static const juce::String notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        if (notes[i].equalsIgnoreCase(name))
            return i;

    return 0;
}

static std::vector<int> buildScaleIntervals(const juce::String& key, const juce::String& mode)
{
    const int root = semitoneFromName(key);
    const std::array<int, 7>* intervals = &kModeIntervals[0];
    const auto lower = mode.toLowerCase();
    if (lower == "dorian") intervals = &kModeIntervals[1];
    else if (lower == "phrygian") intervals = &kModeIntervals[2];
    else if (lower == "lydian") intervals = &kModeIntervals[3];
    else if (lower == "mixolydian") intervals = &kModeIntervals[4];
    else if (lower == "aeolian" || lower == "minor") intervals = &kModeIntervals[5];
    else if (lower == "locrian") intervals = &kModeIntervals[6];

    std::vector<int> result;
    for (const auto interval : *intervals)
        result.push_back((root + interval) % 12);

    return result;
}

static bool promptTextInput(const juce::String& title,
                            const juce::String& prompt,
                            const juce::String& initialValue,
                            juce::String& outValue)
{
    juce::AlertWindow window(title, prompt, juce::AlertWindow::NoIcon);
    window.addTextEditor("value", initialValue, {});
    window.addButton("OK", 1);
    window.addButton("Cancel", 0);
    if (window.runModalLoop() != 1)
        return false;

    outValue = window.getTextEditorContents("value");
    return true;
}

static juce::String noteNameForPitch(int midiNote)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int pitch = juce::jlimit(0, 127, midiNote);
    const int pc = pitch % 12;
    const int octave = (pitch / 12) - 1;
    return juce::String(names[pc]) + juce::String(octave);
}

NoteDetailView::NoteDetailView(model::Song& songRef)
    : song(songRef)
{
    setWantsKeyboardFocus(true);
    setSize(400, kHeight);

    addAndMakeVisible(scaleLockButton);
    addAndMakeVisible(chordLockButton);
    scaleLockButton.onClick = [this] { setScaleLock(scaleLockButton.getToggleState()); };
    chordLockButton.onClick = [this] { setChordLock(chordLockButton.getToggleState()); };
}

void NoteDetailView::setPlayheadBeat(double beat)
{
    if (std::abs(playheadBeat - beat) < 0.0001)
        return;

    playheadBeat = beat;
    repaint();
}

void NoteDetailView::loadCell(const GridRollCell& cell,
                              const juce::String& key,
                              const juce::String& mode)
{
    sourceCell = cell;
    sessionKey = key;
    sessionMode = mode;
    notes.clear();
    noteRects.clear();
    selectedNoteIdx = -1;

    scaleIntervals = buildScaleIntervals(key, mode);
    chordTones = cell.pitchClasses;
    currentChordSymbol = cell.chordSymbol;

    constraintEngine.setScaleContext(key, mode);
    constraintEngine.setChordSymbol(currentChordSymbol);

    const int root = cell.midiNote;
    lowestPitch = juce::jlimit(0, 127 - kNoteRows, root - 12);

    if (auto progression = song.findProgressionById(cell.sourceId))
    {
        for (const auto& chord : progression->getChords())
        {
            if (chord.getId() == cell.cellId)
            {
                notes = chord.getNotes();
                break;
            }
        }
    }

    if (notes.empty() && !cell.pitchClasses.empty())
    {
        double t = 0.0;
        for (const auto pitchClass : cell.pitchClasses)
        {
            int pitch = lowestPitch + ((pitchClass - lowestPitch % 12 + 12) % 12);
            if (pitch < lowestPitch)
                pitch += 12;

            notes.push_back(model::Note::create(pitch, cell.velocity, t, 0.5));
            t += 0.25;
        }
    }

    rebuildNoteRects();
    repaint();
}

juce::Rectangle<int> NoteDetailView::keyBounds() const
{
    return { 0, kHeaderHeight, kKeyWidth, getHeight() - kHeaderHeight };
}

juce::Rectangle<int> NoteDetailView::noteGridBounds() const
{
    return { kKeyWidth, kHeaderHeight, getWidth() - kKeyWidth, getHeight() - kHeaderHeight };
}

int NoteDetailView::pitchToRow(int pitch) const
{
    return (lowestPitch + kNoteRows - 1) - pitch;
}

int NoteDetailView::rowToPitch(int row) const
{
    return (lowestPitch + kNoteRows - 1) - row;
}

double NoteDetailView::pixelToTime(int px) const
{
    const auto grid = noteGridBounds();
    const double fraction = static_cast<double>(px - grid.getX()) / static_cast<double>(grid.getWidth());
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
    return constraintEngine.analyzeNote(pitch).inChord;
}

bool NoteDetailView::isScaleTone(int pitch) const
{
    return constraintEngine.analyzeNote(pitch).inScale;
}

void NoteDetailView::rebuildNoteRects()
{
    noteRects.clear();
    const auto grid = noteGridBounds();
    const int rowHeight = juce::jmax(1, grid.getHeight() / kNoteRows);

    for (const auto& note : notes)
    {
        const int row = pitchToRow(note.getPitch());
        if (row < 0 || row >= kNoteRows)
        {
            noteRects.push_back({});
            continue;
        }

        const int x = timeToPx(note.getStartBeats());
        const int width = juce::jmax(4, static_cast<int>((note.getDurationBeats() / sourceCell.durationBeats) * grid.getWidth()));
        noteRects.push_back({ x, grid.getY() + row * rowHeight, width, rowHeight });
    }
}

void NoteDetailView::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    const auto header = getLocalBounds().removeFromTop(kHeaderHeight);
    g.setColour(theme.surface2);
    g.fillRect(header);

    const auto grid = noteGridBounds();
    const int rowHeight = juce::jmax(1, grid.getHeight() / kNoteRows);

    g.setColour(theme.surface1);
    g.fillRect(keyBounds());

    const auto localPlayhead = playheadBeat - sourceCell.startBeat;
    auto pitchIsActive = [this, localPlayhead](int pitch)
    {
        if (localPlayhead < 0.0)
            return false;
        for (const auto& note : notes)
        {
            if (note.getPitch() != pitch)
                continue;
            if (localPlayhead >= note.getStartBeats()
                && localPlayhead <= note.getStartBeats() + note.getDurationBeats())
                return true;
        }
        return false;
    };

    static const int blackKeys[] = { 1, 3, 6, 8, 10 };
    for (int row = 0; row < kNoteRows; ++row)
    {
        const int pitch = rowToPitch(row);
        const int pitchClass = pitch % 12;
        const bool black = std::find(std::begin(blackKeys), std::end(blackKeys), pitchClass) != std::end(blackKeys);
        const auto analysis = constraintEngine.analyzeNote(pitch, 0.0);

        juce::Colour key = black ? theme.surface0 : theme.inkLight;
        if (analysis.inScale)
            key = key.interpolatedWith(theme.zoneC, 0.16f);
        if (analysis.inChord)
            key = key.interpolatedWith(theme.zoneB, 0.22f);
        if (pitchIsActive(pitch))
            key = theme.accent;

        const auto keyRow = juce::Rectangle<int>(0, grid.getY() + row * rowHeight, kKeyWidth - 1, rowHeight - 1);
        g.setColour(key);
        g.fillRect(keyRow);

        if (pitchClass == 0 && rowHeight >= 8)
        {
            g.setColour(theme.inkGhost.withAlpha(0.9f));
            g.setFont(juce::FontOptions(9.0f));
            g.drawText(noteNameForPitch(pitch), keyRow.reduced(2, 0), juce::Justification::centredLeft, false);
        }
    }

    for (int row = 0; row < kNoteRows; ++row)
    {
        const int pitch = rowToPitch(row);
        const auto analysis = constraintEngine.analyzeNote(pitch, 0.0);
        juce::Colour background = theme.surface1.darker(0.10f);
        if (analysis.inScale)
            background = theme.zoneC.withAlpha(0.08f);
        if (analysis.inChord)
            background = theme.zoneB.withAlpha(0.12f);

        g.setColour(background);
        g.fillRect(grid.getX(), grid.getY() + row * rowHeight, grid.getWidth(), rowHeight - 1);
    }

    const double beatStep = 0.25;
    const double barStep = 1.0;
    g.setColour(theme.surfaceEdge.withAlpha(0.20f));
    for (double t = 0.0; t <= sourceCell.durationBeats + 0.0001; t += beatStep)
        g.fillRect(timeToPx(t), grid.getY(), 1, grid.getHeight());

    g.setColour(theme.surfaceEdge.withAlpha(0.40f));
    for (double t = 0.0; t <= sourceCell.durationBeats + 0.0001; t += barStep)
        g.fillRect(timeToPx(t), grid.getY(), 1, grid.getHeight());

    for (size_t i = 0; i < notes.size() && i < noteRects.size(); ++i)
    {
        if (noteRects[i].isEmpty())
            continue;

        const auto& note = notes[i];
        const auto analysis = constraintEngine.analyzeNote(note.getPitch(), note.getStartBeats());
        const bool selected = static_cast<int>(i) == selectedNoteIdx;

        juce::Colour colour;
        if (analysis.inChord) colour = theme.accent;
        else if (analysis.inScale) colour = theme.zoneC;
        else colour = theme.accentWarm;

        const auto brightness = juce::jmap(note.getVelocity(), 0.0f, 1.0f, 0.52f, 1.0f);
        colour = colour.withMultipliedBrightness(brightness);

        auto rect = noteRects[i].toFloat().reduced(0.5f);
        g.setColour(colour);
        g.fillRoundedRectangle(rect, 3.0f);

        if (selected)
        {
            g.setColour(theme.inkLight.withAlpha(0.95f));
            g.drawRoundedRectangle(rect, 3.0f, 1.5f);
        }

        // Resize handle strip (4 px) on right edge.
        g.setColour(colour.darker(0.35f));
        g.fillRect(noteRects[i].getRight() - 4, noteRects[i].getY(), 4, noteRects[i].getHeight());

        if (noteRects[i].getWidth() >= 28)
        {
            g.setColour(theme.inkDark.withAlpha(0.92f));
            g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
            g.drawText(noteNameForPitch(note.getPitch()),
                       noteRects[i].reduced(4, 0),
                       juce::Justification::centredLeft,
                       true);
        }
    }
}

void NoteDetailView::resized()
{
    auto header = getLocalBounds().removeFromTop(kHeaderHeight).reduced(4, 2);
    chordLockButton.setBounds(header.removeFromRight(96));
    header.removeFromRight(4);
    scaleLockButton.setBounds(header.removeFromRight(96));

    rebuildNoteRects();
}

NoteDetailView::NoteRect NoteDetailView::hitTestGrid(juce::Point<int> pos) const
{
    for (int i = static_cast<int>(noteRects.size()) - 1; i >= 0; --i)
    {
        if (noteRects[i].isEmpty() || !noteRects[i].contains(pos))
            continue;

        const bool resize = pos.getX() >= noteRects[i].getRight() - 6;
        return { i, noteRects[i], resize };
    }

    return { -1, {}, false };
}

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
        const auto grid = noteGridBounds();
        const int rowH = juce::jmax(1, grid.getHeight() / kNoteRows);
        const int row = (e.getPosition().getY() - grid.getY()) / rowH;
        const int rawPitch = juce::jlimit(0, 127, rowToPitch(row));
        const int constrainedPitch = constraintEngine.processNote(rawPitch, 0.0);
        const double start = juce::jmax(0.0, pixelToTime(e.getPosition().getX()));
        const double duration = juce::jmax(0.0625, sourceCell.durationBeats / 16.0);

        notes.push_back(model::Note::create(constrainedPitch, 0.8f, start, duration));
        selectedNoteIdx = static_cast<int>(notes.size()) - 1;
        rebuildNoteRects();
        repaint();
        if (onNotesChanged)
            onNotesChanged();
        return;
    }

    selectedNoteIdx = hit.noteIndex;
    dragStartPos = e.getPosition();
    dragNoteIdx = hit.noteIndex;
    dragOrigStart = notes[static_cast<size_t>(hit.noteIndex)].getStartBeats();
    dragOrigPitch = notes[static_cast<size_t>(hit.noteIndex)].getPitch();
    dragOrigDuration = notes[static_cast<size_t>(hit.noteIndex)].getDurationBeats();

    dragKind = hit.resizeHandle ? DragKind::ResizeNote : DragKind::MoveNote;
    repaint();
}

void NoteDetailView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragKind == DragKind::None || dragNoteIdx < 0)
        return;

    const auto grid = noteGridBounds();
    const int rowH = juce::jmax(1, grid.getHeight() / kNoteRows);
    const double beatPerPixel = sourceCell.durationBeats / juce::jmax(1.0, static_cast<double>(grid.getWidth()));
    const double dxBeats = (e.getPosition().getX() - dragStartPos.getX()) * beatPerPixel;
    const int dyRows = (e.getPosition().getY() - dragStartPos.getY()) / rowH;

    auto& note = notes[static_cast<size_t>(dragNoteIdx)];
    if (dragKind == DragKind::MoveNote)
    {
        const double newStart = juce::jmax(0.0, dragOrigStart + dxBeats);
        note.setStartBeats(newStart);
        const int newPitch = constraintEngine.processNote(juce::jlimit(0, 127, dragOrigPitch - dyRows), newStart);
        note.setPitch(newPitch);
    }
    else
    {
        note.setDurationBeats(juce::jmax(0.0625, dragOrigDuration + dxBeats));
    }

    rebuildNoteRects();
    repaint();
}

void NoteDetailView::mouseUp(const juce::MouseEvent&)
{
    if (dragKind != DragKind::None && onNotesChanged)
        onNotesChanged();

    dragKind = DragKind::None;
    dragNoteIdx = -1;
}

bool NoteDetailView::keyPressed(const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        && selectedNoteIdx >= 0 && selectedNoteIdx < static_cast<int>(notes.size()))
    {
        notes.erase(notes.begin() + selectedNoteIdx);
        selectedNoteIdx = juce::jmin(selectedNoteIdx, static_cast<int>(notes.size()) - 1);
        rebuildNoteRects();
        repaint();
        if (onNotesChanged)
            onNotesChanged();
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (onCloseRequested)
            onCloseRequested();
        return true;
    }

    return false;
}

void NoteDetailView::showNoteContext(int noteIdx, juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.addItem(1, "Set Velocity...");
    menu.addItem(2, "Quantize to Grid");
    menu.addItem(4, "Set Duration...");
    menu.addItem(5, "Set Pitch...");
    menu.addItem(6, "Toggle Legato");
    {
        juce::PopupMenu transposeMenu;
        transposeMenu.addItem(701, "+1 semitone");
        transposeMenu.addItem(702, "-1 semitone");
        transposeMenu.addItem(703, "+12 semitones");
        transposeMenu.addItem(704, "-12 semitones");
        menu.addSubMenu("Transpose", transposeMenu);
    }
    menu.addItem(8, "Flip Velocity");
    menu.addItem(9, "Select Same Pitch");
    menu.addItem(10, "Quantize to Scale");
    menu.addSeparator();
    menu.addItem(3, "Delete Note");

    auto safeThis = juce::Component::SafePointer<NoteDetailView>(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(
            juce::Rectangle<int>(screenPos.getX(), screenPos.getY(), 1, 1)),
        [safeThis, noteIdx](int result)
        {
            if (safeThis != nullptr)
                safeThis->handleNoteContextResult(noteIdx, result);
        });
}

void NoteDetailView::handleNoteContextResult(int noteIdx, int result)
{
    if (result == 0 || noteIdx < 0 || noteIdx >= static_cast<int>(notes.size()))
        return;

    if (result == 1)
    {
        juce::String text;
        if (promptTextInput("Set Velocity",
                            "Enter velocity (0.0 - 1.0):",
                            juce::String(notes[static_cast<size_t>(noteIdx)].getVelocity()),
                            text)
            && text.isNotEmpty())
        {
            notes[static_cast<size_t>(noteIdx)].setVelocity(juce::jlimit(0.0f, 1.0f, text.getFloatValue()));
            repaint();
            if (onNotesChanged)
                onNotesChanged();
        }
    }
    else if (result == 2)
    {
        auto& note = notes[static_cast<size_t>(noteIdx)];
        constexpr double grid = 0.0625;
        note.setStartBeats(std::round(note.getStartBeats() / grid) * grid);
        rebuildNoteRects();
        repaint();
        if (onNotesChanged)
            onNotesChanged();
    }
    else if (result == 4)
    {
        juce::String text;
        if (promptTextInput("Set Duration",
                            "Enter duration in beats:",
                            juce::String(notes[static_cast<size_t>(noteIdx)].getDurationBeats()),
                            text)
            && text.isNotEmpty())
        {
            notes[static_cast<size_t>(noteIdx)].setDurationBeats(juce::jmax(0.03125, text.getDoubleValue()));
            rebuildNoteRects();
            repaint();
            if (onNotesChanged)
                onNotesChanged();
        }
    }
    else if (result == 5)
    {
        juce::String text;
        if (promptTextInput("Set Pitch",
                            "Enter MIDI note (0-127):",
                            juce::String(notes[static_cast<size_t>(noteIdx)].getPitch()),
                            text)
            && text.isNotEmpty())
        {
            notes[static_cast<size_t>(noteIdx)].setPitch(juce::jlimit(0, 127, text.getIntValue()));
            rebuildNoteRects();
            repaint();
            if (onNotesChanged)
                onNotesChanged();
        }
    }
    else if (result == 6)
    {
        auto& note = notes[static_cast<size_t>(noteIdx)];
        constexpr double legatoDur = 1.0;
        note.setDurationBeats(note.getDurationBeats() >= legatoDur ? 0.25 : legatoDur);
        rebuildNoteRects();
        repaint();
        if (onNotesChanged)
            onNotesChanged();
    }
    else if (result >= 701 && result <= 704)
    {
        const int delta = (result == 701) ? 1 : (result == 702) ? -1 : (result == 703) ? 12 : -12;
        auto& note = notes[static_cast<size_t>(noteIdx)];
        note.setPitch(juce::jlimit(0, 127, note.getPitch() + delta));
        rebuildNoteRects();
        repaint();
        if (onNotesChanged)
            onNotesChanged();
    }
    else if (result == 8)
    {
        auto& note = notes[static_cast<size_t>(noteIdx)];
        note.setVelocity(juce::jlimit(0.0f, 1.0f, 1.0f - note.getVelocity()));
        repaint();
        if (onNotesChanged)
            onNotesChanged();
    }
    else if (result == 9)
    {
        selectedNoteIdx = noteIdx;
        repaint();
    }
    else if (result == 10)
    {
        auto& note = notes[static_cast<size_t>(noteIdx)];
        note.setPitch(constraintEngine.processNote(note.getPitch(), note.getStartBeats()));
        rebuildNoteRects();
        repaint();
        if (onNotesChanged)
            onNotesChanged();
    }
    else if (result == 3)
    {
        notes.erase(notes.begin() + static_cast<size_t>(noteIdx));
        selectedNoteIdx = juce::jmin(selectedNoteIdx, static_cast<int>(notes.size()) - 1);
        rebuildNoteRects();
        repaint();
        if (onNotesChanged)
            onNotesChanged();
    }
}

void NoteDetailView::setScaleLock(bool enabled)
{
    scaleLockButton.setToggleState(enabled, juce::dontSendNotification);
    constraintEngine.setScaleLock(enabled);
    repaint();
}

void NoteDetailView::setChordLock(bool enabled)
{
    chordLockButton.setToggleState(enabled, juce::dontSendNotification);
    constraintEngine.setChordLock(enabled);
    repaint();
}

bool NoteDetailView::getScaleLock() const noexcept
{
    return scaleLockButton.getToggleState();
}

bool NoteDetailView::getChordLock() const noexcept
{
    return chordLockButton.getToggleState();
}

} // namespace setle::gridroll
