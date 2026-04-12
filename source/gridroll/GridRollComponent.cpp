#include "GridRollComponent.h"

namespace setle::gridroll
{

// ---------------------------------------------------------------
GridRollComponent::GridRollComponent(model::Song& songRef, te::Edit& editRef)
    : song(songRef)
    , edit(editRef)
{
    // ---- create sub-views ----
    chordGrid  = std::make_unique<ChordGridView>(song);
    drumGrid   = std::make_unique<DrumGridView>();
    noteDetail = std::make_unique<NoteDetailView>(song);

    drumGrid->buildDefaultRows();

    // ---- chord callbacks ----
    chordGrid->onCellDoubleClicked = [this](int idx)   { openNoteDetail(idx); };
    chordGrid->onCellSelected      = [this](int /*idx*/) { repaint(); };
    chordGrid->onCellsChanged      = [this]
    {
        syncChordCellsToModel();
    };
    chordGrid->onStatusMessage = [this](const juce::String& msg)
    {
        if (onStatusMessage) onStatusMessage(msg);
    };

    // ---- drum callbacks ----
    drumGrid->onCellsChanged = [this]
    {
        if (onProgressionEdited) onProgressionEdited(progressionId);
    };
    drumGrid->onStatusMessage = [this](const juce::String& msg)
    {
        if (onStatusMessage) onStatusMessage(msg);
    };

    // ---- note detail callbacks ----
    noteDetail->onNotesChanged = [this]
    {
        syncChordCellsToModel();
    };
    noteDetail->onCloseRequested = [this] { closeNoteDetail(); };
    noteDetail->onStatusMessage  = [this](const juce::String& msg)
    {
        if (onStatusMessage) onStatusMessage(msg);
    };

    // ---- make visible ----
    addAndMakeVisible(*chordGrid);
    addAndMakeVisible(*drumGrid);
    addChildComponent(*noteDetail);

    // ---- header buttons ----
    auto setActiveModeButton = [this](Mode m)
    {
        return [this, m] { setMode(m); };
    };
    chordModeButton.onClick = setActiveModeButton(Mode::ChordGrid);
    drumModeButton.onClick  = setActiveModeButton(Mode::DrumGrid);
    splitModeButton.onClick = setActiveModeButton(Mode::Split);

    for (auto* btn : { &chordModeButton, &drumModeButton, &splitModeButton })
    {
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a3a4a));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3a5a7a));
        addAndMakeVisible(*btn);
    }

    progressionName.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    progressionName.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    progressionName.setEditable(false);
    progressionName.setText("—", juce::dontSendNotification);
    addAndMakeVisible(progressionName);

    sessionKeyDisplay.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.55f));
    sessionKeyDisplay.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(sessionKeyDisplay);

    zoomInButton.onClick  = [this]
    {
        zoomBeatsPerPx = juce::jmax(0.05, zoomBeatsPerPx * 0.75);
        repaint();
    };
    zoomOutButton.onClick = [this]
    {
        zoomBeatsPerPx = juce::jmin(4.0, zoomBeatsPerPx * 1.33);
        repaint();
    };
    for (auto* b : { &zoomInButton, &zoomOutButton })
    {
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a3040));
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
        addAndMakeVisible(*b);
    }

    applyMode();
    setSize(800, 300);
}

// ---------------------------------------------------------------
void GridRollComponent::setTargetProgression(const juce::String& id)
{
    progressionId = id;
    closeNoteDetail();

    auto progOpt = song.findProgressionById(id);
    if (progOpt.has_value())
    {
        progressionName.setText(progOpt->getName(), juce::dontSendNotification);
        const juce::String keyMode = progOpt->getKey() + " " + progOpt->getMode();
        sessionKeyDisplay.setText(keyMode, juce::dontSendNotification);
    }
    else
    {
        progressionName.setText("—", juce::dontSendNotification);
        sessionKeyDisplay.setText("", juce::dontSendNotification);
    }

    chordGrid->loadProgression(id);
}

// ---------------------------------------------------------------
void GridRollComponent::setMode(Mode mode)
{
    currentMode = mode;
    closeNoteDetail();
    applyMode();
    resized();
    repaint();
}

// ---------------------------------------------------------------
void GridRollComponent::setPlayheadBeat(double beat)
{
    playheadBeat = beat;
    chordGrid->setPlayheadBeat(beat);
    drumGrid->setPlayheadBeat(beat);
}

// ---------------------------------------------------------------
void GridRollComponent::setTheorySnap(const juce::String& snap)
{
    chordGrid->setTheorySnap(snap);
}

// ---------------------------------------------------------------
void GridRollComponent::applyMode()
{
    // Update button tint to indicate active mode
    chordModeButton.setToggleState(currentMode == Mode::ChordGrid, juce::dontSendNotification);
    drumModeButton.setToggleState (currentMode == Mode::DrumGrid,  juce::dontSendNotification);
    splitModeButton.setToggleState(currentMode == Mode::Split,     juce::dontSendNotification);

    chordGrid->setVisible(currentMode == Mode::ChordGrid || currentMode == Mode::Split);
    drumGrid->setVisible(currentMode == Mode::DrumGrid  || currentMode == Mode::Split);
}

// ---------------------------------------------------------------
void GridRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141e2a));

    // Header background
    g.setColour(juce::Colour(0xff1c2c3c));
    g.fillRect(0, 0, getWidth(), kHeaderHeight);
}

// ---------------------------------------------------------------
void GridRollComponent::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(kHeaderHeight);

    // Mode buttons
    chordModeButton.setBounds(header.removeFromLeft(72).reduced(4, 6));
    drumModeButton.setBounds (header.removeFromLeft(66).reduced(4, 6));
    splitModeButton.setBounds(header.removeFromLeft(60).reduced(4, 6));
    header.removeFromLeft(8);

    // Progression name
    progressionName.setBounds(header.removeFromLeft(200).reduced(4, 4));
    sessionKeyDisplay.setBounds(header.removeFromLeft(120).reduced(4, 4));

    // Zoom controls (right side)
    zoomOutButton.setBounds(header.removeFromRight(28).reduced(4, 6));
    zoomInButton.setBounds (header.removeFromRight(28).reduced(4, 6));

    // Content area
    if (currentMode == Mode::ChordGrid)
    {
        chordGrid->setBounds(area);
        if (noteDetail->isVisible())
        {
            const int chordH  = ChordGridView::kHeight;
            const int detailH = NoteDetailView::kHeight;
            chordGrid->setBounds(area.removeFromTop(chordH));
            noteDetail->setBounds(area.removeFromTop(detailH));
        }
    }
    else if (currentMode == Mode::DrumGrid)
    {
        drumGrid->setBounds(area);
    }
    else  // Split
    {
        const int totalH  = area.getHeight();
        const int chordH  = noteDetail->isVisible()
                                ? ChordGridView::kHeight + NoteDetailView::kHeight
                                : static_cast<int>(totalH * 0.40f);
        const int drumH   = totalH - chordH;

        auto chordArea = area.removeFromTop(chordH);
        if (noteDetail->isVisible())
        {
            chordGrid->setBounds(chordArea.removeFromTop(ChordGridView::kHeight));
            noteDetail->setBounds(chordArea.removeFromTop(NoteDetailView::kHeight));
        }
        else
        {
            chordGrid->setBounds(chordArea);
        }
        drumGrid->setBounds(area.removeFromTop(drumH));
    }
}

// ---------------------------------------------------------------
void GridRollComponent::openNoteDetail(int cellIndex)
{
    const auto& cells = chordGrid->getCells();
    if (cellIndex < 0 || cellIndex >= static_cast<int>(cells.size()))
        return;

    auto progOpt = song.findProgressionById(progressionId);
    const juce::String key  = progOpt.has_value() ? progOpt->getKey()  : "C";
    const juce::String mode = progOpt.has_value() ? progOpt->getMode() : "ionian";

    expandedCellIdx = cellIndex;
    noteDetail->loadCell(cells[static_cast<size_t>(cellIndex)], key, mode);
    noteDetail->setVisible(true);
    noteDetail->grabKeyboardFocus();
    resized();
    repaint();
}

// ---------------------------------------------------------------
void GridRollComponent::closeNoteDetail()
{
    expandedCellIdx = -1;
    noteDetail->setVisible(false);
    resized();
    repaint();
}

// ---------------------------------------------------------------
void GridRollComponent::syncChordCellsToModel()
{
    // Write updated cells back into the model Progression
    auto progOpt = song.findProgressionById(progressionId);
    if (!progOpt.has_value())
        return;

    Progression& prog = *progOpt;
    // Clear existing chords and rebuild from cell list
    // (ValueTree-backed: create new Chord objects for each updated cell)
    const auto& cells = chordGrid->getCells();

    // Remove all current chords from prog
    const auto existing = prog.getChords();
    // We rebuild by mutating the backing ValueTree directly via the model API:
    // For simplicity, delete-all-and-re-add via existing addChord interface
    // Note: this is the approved model-mutation path per the spec.

    // Build replacement progression state using the chord API
    auto progTree = prog.valueTree();
    // Remove child chord nodes
    for (auto child : existing)
        progTree.removeChild(child.valueTree(), nullptr);

    double cursor = 0.0;
    for (const auto& cell : cells)
    {
        auto chord = model::Chord::create(cell.chordSymbol,
                                          "unknown",
                                          cell.midiNote);
        chord.setFunction(cell.chordFunction);
        chord.setName(cell.romanNumeral);
        chord.setStartBeats(cell.startBeat);
        chord.setDurationBeats(cell.durationBeats);

        // If note detail edited notes for this cell, write them in
        if (expandedCellIdx >= 0 &&
            static_cast<size_t>(expandedCellIdx) < cells.size() &&
            cells[static_cast<size_t>(expandedCellIdx)].cellId == cell.cellId &&
            noteDetail != nullptr)
        {
            for (const auto& note : noteDetail->getNotes())
                chord.addNote(note);
        }

        prog.addChord(chord);
        cursor += cell.durationBeats;
    }

    if (onProgressionEdited)
        onProgressionEdited(progressionId);
}

} // namespace setle::gridroll
