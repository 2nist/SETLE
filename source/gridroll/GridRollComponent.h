#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include "ChordGridView.h"
#include "DrumGridView.h"
#include "NoteModeView.h"
#include "../model/SetleSongModel.h"
#include "../theory/MeterContext.h"
#include "../theme/ThemeManager.h"

namespace te = tracktion::engine;

namespace setle::gridroll
{

/**
 * Host component for the GridRoll editor (Phase 14D).
 *
 * Contains ChordGridView, NoteModeView, and DrumGridView and manages mode / zoom state.
 */
class GridRollComponent : public juce::Component
{
public:
    enum class Mode { ChordGrid, NoteMode, DrumGrid, Split };

    static constexpr int kHeaderHeight = 32; // mode buttons + name + zoom

    explicit GridRollComponent(model::Song& song, te::Edit& edit);

    /** Point the GridRoll at a specific progression id. */
    void setTargetProgression(const juce::String& progressionId);

    /** Switch between ChordGrid / DrumGrid / Split display modes. */
    void setMode(Mode mode);

    /** Drive the playhead from the transport (30 Hz). */
    void setPlayheadBeat(double beat);

    /** Propagate the theory snap setting from the workspace. */
    void setTheorySnap(const juce::String& snap);
    void setMeterContext(const setle::theory::MeterContext& meter);

    /** Called when a chord edit should be persisted back to the model. */
    std::function<void(const juce::String& progressionId)> onProgressionEdited;

    /** Called when a status message should be shown. */
    std::function<void(const juce::String&)> onStatusMessage;

    /** Called when drum pattern cells change and should be pushed to drum instruments. */
    std::function<void(const std::vector<GridRollCell>&, const juce::String& progressionId)> onDrumPatternEdited;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    // ---------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------
    void applyMode();
    void syncChordCellsToModel();
    void syncDrumCellsToBackend();
    void jumpToChordInNoteMode(int cellIndex);
    double totalBeats() const;
    void setNoteBeatRange(double startBeat, double endBeat);
    void setNotePitchRange(int lowMidi, int highMidi);
    void syncChordViewportToNoteRange();
    void setHorizontalZoomFactor(double factor);
    void adjustHorizontalZoom(double factorMultiplier);
    void resetHorizontalZoom();
    void adjustPitchZoom(int semitoneDelta);

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    model::Song& song;
    te::Edit&    edit;

    std::unique_ptr<ChordGridView> chordGrid;
    std::unique_ptr<DrumGridView>  drumGrid;
    std::unique_ptr<NoteModeView> noteModeView;
    juce::Viewport chordViewport;
    juce::Viewport drumViewport;

    Mode   currentMode     { Mode::ChordGrid };
    double playheadBeat    { 0.0 };
    double horizontalZoomFactor { 1.0 };
    setle::theory::MeterContext currentMeter;
    double noteVisibleStartBeat { 0.0 };
    double noteVisibleEndBeat { 16.0 };
    int noteLowestMidi { 48 };
    int noteHighestMidi { 72 };

    juce::String progressionId;

    // Header controls
    juce::TextButton chordModeButton { "Chords" };
    juce::TextButton noteModeButton  { "Notes" };
    juce::TextButton drumModeButton  { "Drums" };
    juce::TextButton splitModeButton { "Split" };
    juce::Label      progressionName;
    juce::Label      sessionKeyDisplay;
    juce::Label      zoomLabel { {}, "Zoom:" };
    juce::TextButton zoomInButton    { "+" };
    juce::TextButton zoomOutButton   { "-" };
    juce::TextButton zoomResetButton { "1:1" };
    juce::Label      pitchLabel { {}, "Pitch:" };
    juce::TextButton pitchInButton   { "+" };
    juce::TextButton pitchOutButton  { "-" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridRollComponent)
};

} // namespace setle::gridroll
