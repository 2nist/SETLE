#include "GridRollComponent.h"

#include <cmath>
#include <map>

#include "../theme/ThemeStyleHelpers.h"

namespace setle::gridroll
{

GridRollComponent::GridRollComponent(model::Song& songRef, te::Edit& editRef)
    : song(songRef)
    , edit(editRef)
{
    chordGrid = std::make_unique<ChordGridView>(song);
    drumGrid = std::make_unique<DrumGridView>();
    noteModeView = std::make_unique<NoteModeView>(song, edit);

    chordViewport.setViewedComponent(chordGrid.get(), false);
    chordViewport.setScrollBarsShown(true, false, true, false);

    drumViewport.setViewedComponent(drumGrid.get(), false);
    drumViewport.setScrollBarsShown(false, true, true, false);

    drumGrid->buildDefaultRows();

    chordGrid->onCellDoubleClicked = [this](int idx) { jumpToChordInNoteMode(idx); };
    chordGrid->onOpenNoteDetail = [this](int idx) { jumpToChordInNoteMode(idx); };
    chordGrid->onCellSelected = [this](int) { repaint(); };
    chordGrid->onCellsChanged = [this]
    {
        syncChordCellsToModel();
        noteModeView->setTargetProgression(progressionId);
        setNoteBeatRange(noteVisibleStartBeat, noteVisibleEndBeat);
        setNotePitchRange(noteLowestMidi, noteHighestMidi);
    };
    chordGrid->onStatusMessage = [this](const juce::String& msg)
    {
        if (onStatusMessage)
            onStatusMessage(msg);
    };

    drumGrid->onCellsChanged = [this]
    {
        syncDrumCellsToBackend();
        if (onProgressionEdited)
            onProgressionEdited(progressionId);
    };
    drumGrid->onStatusMessage = [this](const juce::String& msg)
    {
        if (onStatusMessage)
            onStatusMessage(msg);
    };

    noteModeView->onNotesChanged = [this]
    {
        chordGrid->loadProgression(progressionId);
        if (onProgressionEdited)
            onProgressionEdited(progressionId);
    };
    noteModeView->onVisibleBeatRangeChanged = [this](double startBeat, double endBeat)
    {
        noteVisibleStartBeat = startBeat;
        noteVisibleEndBeat = endBeat;
        const auto span = juce::jmax(0.001, noteVisibleEndBeat - noteVisibleStartBeat);
        horizontalZoomFactor = juce::jlimit(1.0, 8.0, totalBeats() / span);
        syncChordViewportToNoteRange();
    };
    noteModeView->onVisiblePitchRangeChanged = [this](int lowMidi, int highMidi)
    {
        noteLowestMidi = lowMidi;
        noteHighestMidi = highMidi;
    };
    noteModeView->onStatusMessage = [this](const juce::String& msg)
    {
        if (onStatusMessage)
            onStatusMessage(msg);
    };

    addAndMakeVisible(chordViewport);
    addAndMakeVisible(drumViewport);
    addAndMakeVisible(*noteModeView);

    auto setActiveModeButton = [this](Mode mode)
    {
        return [this, mode] { setMode(mode); };
    };
    chordModeButton.onClick = setActiveModeButton(Mode::ChordGrid);
    noteModeButton.onClick = setActiveModeButton(Mode::NoteMode);
    drumModeButton.onClick = setActiveModeButton(Mode::DrumGrid);
    splitModeButton.onClick = setActiveModeButton(Mode::Split);

    for (auto* btn : { &chordModeButton, &noteModeButton, &drumModeButton, &splitModeButton,
                       &zoomInButton, &zoomOutButton, &zoomResetButton, &pitchInButton, &pitchOutButton })
    {
        const auto& theme = ThemeManager::get().theme();
        btn->setColour(juce::TextButton::buttonColourId, theme.controlBg);
        btn->setColour(juce::TextButton::textColourOffId, theme.controlText.withAlpha(0.9f));
        btn->setColour(juce::TextButton::buttonOnColourId, theme.controlOnBg);
        btn->setClickingTogglesState(false);
        addAndMakeVisible(*btn);
    }

    progressionName.setColour(juce::Label::textColourId,
                              setle::theme::textForRole(ThemeManager::get().theme(),
                                                         setle::theme::TextRole::primary)
                                  .withAlpha(0.96f));
    progressionName.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    progressionName.setEditable(false);
    progressionName.setText("—", juce::dontSendNotification);
    addAndMakeVisible(progressionName);

    sessionKeyDisplay.setColour(juce::Label::textColourId,
                                setle::theme::textForRole(ThemeManager::get().theme(),
                                                           setle::theme::TextRole::muted)
                                    .withAlpha(0.90f));
    sessionKeyDisplay.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(sessionKeyDisplay);

    zoomLabel.setColour(juce::Label::textColourId,
                        setle::theme::textForRole(ThemeManager::get().theme(),
                                                   setle::theme::TextRole::muted)
                            .withAlpha(0.96f));
    zoomLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(zoomLabel);

    pitchLabel.setColour(juce::Label::textColourId,
                         setle::theme::textForRole(ThemeManager::get().theme(),
                                                    setle::theme::TextRole::muted)
                             .withAlpha(0.96f));
    pitchLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(pitchLabel);

    zoomInButton.onClick = [this] { adjustHorizontalZoom(1.25); };
    zoomOutButton.onClick = [this] { adjustHorizontalZoom(0.8); };
    zoomResetButton.onClick = [this] { resetHorizontalZoom(); };
    pitchInButton.onClick = [this] { adjustPitchZoom(-4); };
    pitchOutButton.onClick = [this] { adjustPitchZoom(4); };

    applyMode();
    setSize(900, 320);
}

void GridRollComponent::setTargetProgression(const juce::String& id)
{
    progressionId = id;

    if (onPrepareProgressionForNoteMode != nullptr && id.isNotEmpty())
        onPrepareProgressionForNoteMode(id);

    auto progOpt = song.findProgressionById(id);
    if (progOpt.has_value())
    {
        progressionName.setText(progOpt->getName(), juce::dontSendNotification);
        sessionKeyDisplay.setText((progOpt->getKey() + " " + progOpt->getMode()).trim(), juce::dontSendNotification);
    }
    else
    {
        progressionName.setText("—", juce::dontSendNotification);
        sessionKeyDisplay.setText({}, juce::dontSendNotification);
    }

    chordGrid->loadProgression(id);
    noteModeView->setTargetProgression(id);

    const auto beats = totalBeats();
    noteVisibleStartBeat = 0.0;
    noteVisibleEndBeat = beats;
    noteLowestMidi = 48;
    noteHighestMidi = 72;
    horizontalZoomFactor = 1.0;

    setNoteBeatRange(noteVisibleStartBeat, noteVisibleEndBeat);
    setNotePitchRange(noteLowestMidi, noteHighestMidi);

    setMeterContext(setle::theory::MeterContext::forBeat(playheadBeat, song));
    resized();
    repaint();
}

void GridRollComponent::setMode(Mode mode)
{
    if (currentMode == mode)
        return;

    currentMode = mode;
    applyMode();
    resized();
    repaint();
}

void GridRollComponent::setPlayheadBeat(double beat)
{
    playheadBeat = beat;

    const auto meterAtBeat = setle::theory::MeterContext::forBeat(playheadBeat, song);
    setMeterContext(meterAtBeat);

    chordGrid->setPlayheadBeat(beat);
    drumGrid->setPlayheadBeat(beat);
    noteModeView->setPlayheadBeat(beat);
}

void GridRollComponent::refreshNoteModeCache()
{
    if (noteModeView != nullptr)
        noteModeView->refreshFromEdit();
}

void GridRollComponent::setTheorySnap(const juce::String& snap)
{
    chordGrid->setTheorySnap(snap);
}

void GridRollComponent::setMeterContext(const setle::theory::MeterContext& meter)
{
    if (currentMeter.numerator == meter.numerator
        && currentMeter.denominator == meter.denominator)
        return;

    currentMeter = meter;
    chordGrid->setMeterContext(currentMeter);
    drumGrid->setMeterContext(currentMeter);
}

void GridRollComponent::applyMode()
{
    chordModeButton.setToggleState(currentMode == Mode::ChordGrid, juce::dontSendNotification);
    noteModeButton.setToggleState(currentMode == Mode::NoteMode, juce::dontSendNotification);
    drumModeButton.setToggleState(currentMode == Mode::DrumGrid, juce::dontSendNotification);
    splitModeButton.setToggleState(currentMode == Mode::Split, juce::dontSendNotification);

    chordViewport.setScrollBarsShown(currentMode != Mode::Split, false, true, false);
    chordViewport.setVisible(currentMode == Mode::ChordGrid || currentMode == Mode::Split);
    noteModeView->setVisible(currentMode == Mode::NoteMode || currentMode == Mode::Split);
    drumViewport.setVisible(currentMode == Mode::DrumGrid);
}

void GridRollComponent::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    g.fillAll(setle::theme::panelBackground(theme, setle::theme::ZoneRole::timeline));

    g.setColour(setle::theme::panelHeaderBackground(theme, setle::theme::ZoneRole::timeline));
    g.fillRect(0, 0, getWidth(), kHeaderHeight);
    g.setColour(setle::theme::timelineGridLine(theme, true).withAlpha(0.85f));
    g.drawLine(0.0f, static_cast<float>(kHeaderHeight), static_cast<float>(getWidth()), static_cast<float>(kHeaderHeight),
               setle::theme::stroke(theme, setle::theme::StrokeRole::normal));
}

void GridRollComponent::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(kHeaderHeight);

    chordModeButton.setBounds(header.removeFromLeft(74).reduced(4, 6));
    noteModeButton.setBounds(header.removeFromLeft(66).reduced(4, 6));
    drumModeButton.setBounds(header.removeFromLeft(66).reduced(4, 6));
    splitModeButton.setBounds(header.removeFromLeft(62).reduced(4, 6));
    header.removeFromLeft(8);

    progressionName.setBounds(header.removeFromLeft(180).reduced(4, 4));
    sessionKeyDisplay.setBounds(header.removeFromLeft(96).reduced(4, 4));

    pitchInButton.setBounds(header.removeFromRight(28).reduced(4, 6));
    pitchOutButton.setBounds(header.removeFromRight(28).reduced(4, 6));
    pitchLabel.setBounds(header.removeFromRight(46).reduced(2, 7));

    header.removeFromRight(8);

    zoomInButton.setBounds(header.removeFromRight(28).reduced(4, 6));
    zoomResetButton.setBounds(header.removeFromRight(42).reduced(4, 6));
    zoomOutButton.setBounds(header.removeFromRight(28).reduced(4, 6));
    zoomLabel.setBounds(header.removeFromRight(46).reduced(2, 7));

    if (currentMode == Mode::ChordGrid)
    {
        chordViewport.setBounds(area);
        syncChordViewportToNoteRange();
        return;
    }

    if (currentMode == Mode::NoteMode)
    {
        noteModeView->setBounds(area);
        syncChordViewportToNoteRange();
        return;
    }

    if (currentMode == Mode::DrumGrid)
    {
        drumViewport.setBounds(area);
        drumGrid->setSize(drumViewport.getWidth(), juce::jmax(drumGrid->getHeight(), drumViewport.getHeight()));
        return;
    }

    auto chordArea = area.removeFromTop(static_cast<int>(std::round(area.getHeight() * 0.30f)));
    auto noteArea = area;

    chordViewport.setBounds(chordArea);
    noteModeView->setBounds(noteArea);
    syncChordViewportToNoteRange();
}

void GridRollComponent::jumpToChordInNoteMode(int cellIndex)
{
    const auto& cells = chordGrid->getCells();
    if (cellIndex < 0 || cellIndex >= static_cast<int>(cells.size()))
        return;

    setMode(Mode::NoteMode);
    noteModeView->scrollToBeat(cells[static_cast<size_t>(cellIndex)].startBeat, true);
    noteVisibleStartBeat = noteModeView->getVisibleStartBeat();
    noteVisibleEndBeat = noteModeView->getVisibleEndBeat();
    syncChordViewportToNoteRange();
}

double GridRollComponent::totalBeats() const
{
    return juce::jmax(2.0, chordGrid->getTotalBeats());
}

void GridRollComponent::setNoteBeatRange(double startBeat, double endBeat)
{
    noteModeView->setVisibleBeatRange(startBeat, endBeat);
    noteVisibleStartBeat = noteModeView->getVisibleStartBeat();
    noteVisibleEndBeat = noteModeView->getVisibleEndBeat();

    const auto span = juce::jmax(0.001, noteVisibleEndBeat - noteVisibleStartBeat);
    horizontalZoomFactor = juce::jlimit(1.0, 8.0, totalBeats() / span);
    syncChordViewportToNoteRange();
}

void GridRollComponent::setNotePitchRange(int lowMidi, int highMidi)
{
    noteModeView->setVisiblePitchRange(lowMidi, highMidi);
    noteLowestMidi = noteModeView->getLowestVisibleMidi();
    noteHighestMidi = noteModeView->getHighestVisibleMidi();
}

void GridRollComponent::syncChordViewportToNoteRange()
{
    if (chordGrid == nullptr || chordViewport.getWidth() <= 0 || chordViewport.getHeight() <= 0)
        return;

    const auto total = totalBeats();
    const auto span = juce::jmax(0.5, noteVisibleEndBeat - noteVisibleStartBeat);
    const auto ppb = static_cast<double>(chordViewport.getWidth()) / span;
    const int contentWidth = juce::jmax(chordViewport.getWidth(), static_cast<int>(std::round(total * ppb)) + 32);

    chordGrid->setSize(contentWidth, chordViewport.getHeight());

    const int maxViewX = juce::jmax(0, contentWidth - chordViewport.getWidth());
    const int viewX = juce::jlimit(0, maxViewX, static_cast<int>(std::round(noteVisibleStartBeat * ppb)));
    chordViewport.setViewPosition(viewX, 0);
}

void GridRollComponent::setHorizontalZoomFactor(double factor)
{
    const auto total = totalBeats();
    const auto clampedFactor = juce::jlimit(1.0, 8.0, factor);
    const auto newSpan = juce::jlimit(2.0, total, total / clampedFactor);
    const auto centre = noteVisibleStartBeat + (noteVisibleEndBeat - noteVisibleStartBeat) * 0.5;
    setNoteBeatRange(centre - newSpan * 0.5, centre + newSpan * 0.5);
}

void GridRollComponent::adjustHorizontalZoom(double factorMultiplier)
{
    setHorizontalZoomFactor(horizontalZoomFactor * factorMultiplier);
}

void GridRollComponent::resetHorizontalZoom()
{
    setHorizontalZoomFactor(1.0);
}

void GridRollComponent::adjustPitchZoom(int semitoneDelta)
{
    const auto currentSpan = noteHighestMidi - noteLowestMidi;
    const auto newSpan = juce::jlimit(12, 72, currentSpan + semitoneDelta);
    const auto centre = (noteLowestMidi + noteHighestMidi) / 2;
    auto low = centre - newSpan / 2;
    auto high = low + newSpan;

    if (low < 0)
    {
        high -= low;
        low = 0;
    }
    if (high > 127)
    {
        const auto excess = high - 127;
        low -= excess;
        high = 127;
    }

    setNotePitchRange(juce::jmax(0, low), juce::jmin(127, high));
}

void GridRollComponent::syncChordCellsToModel()
{
    auto progOpt = song.findProgressionById(progressionId);
    if (!progOpt.has_value())
        return;

    auto progTree = progOpt->valueTree();
    if (!progTree.isValid())
        return;

    std::map<juce::String, juce::ValueTree> existingById;
    for (const auto& child : progTree)
    {
        if (!child.hasType(model::Schema::chordType))
            continue;
        const auto id = child.getProperty(model::Schema::idProp).toString();
        if (id.isNotEmpty())
            existingById[id] = child.createCopy();
    }

    for (int i = progTree.getNumChildren(); --i >= 0;)
    {
        if (progTree.getChild(i).hasType(model::Schema::chordType))
            progTree.removeChild(i, nullptr);
    }

    double lengthBeats = 0.0;
    const auto& cells = chordGrid->getCells();
    for (const auto& cell : cells)
    {
        juce::ValueTree chordTree;

        auto id = cell.cellId;
        if (id.isEmpty())
            id = juce::Uuid().toString();

        const auto existing = existingById.find(id);
        if (existing != existingById.end())
        {
            chordTree = existing->second.createCopy();
        }
        else
        {
            chordTree = model::Chord::create(cell.chordSymbol,
                                             "unknown",
                                             cell.midiNote).valueTree();
        }

        chordTree.setProperty(model::Schema::idProp, id, nullptr);
        chordTree.setProperty(model::Schema::nameProp, cell.romanNumeral, nullptr);
        chordTree.setProperty(model::Schema::symbolProp, cell.chordSymbol, nullptr);
        chordTree.setProperty(model::Schema::functionProp, cell.chordFunction, nullptr);
        chordTree.setProperty(model::Schema::sourceProp, cell.source, nullptr);
        chordTree.setProperty(model::Schema::confidenceProp, cell.confidence, nullptr);
        chordTree.setProperty(model::Schema::rootMidiProp, cell.midiNote, nullptr);
        chordTree.setProperty(model::Schema::startBeatsProp, cell.startBeat, nullptr);
        chordTree.setProperty(model::Schema::durationBeatsProp, cell.durationBeats, nullptr);

        progTree.appendChild(chordTree, nullptr);
        lengthBeats = juce::jmax(lengthBeats, cell.startBeat + cell.durationBeats);
    }

    progTree.setProperty(model::Schema::lengthBeatsProp, lengthBeats, nullptr);

    if (onProgressionEdited)
        onProgressionEdited(progressionId);
}

void GridRollComponent::syncDrumCellsToBackend()
{
    if (onDrumPatternEdited == nullptr || drumGrid == nullptr)
        return;

    onDrumPatternEdited(drumGrid->getActivePatternCells(), progressionId);
}

} // namespace setle::gridroll
