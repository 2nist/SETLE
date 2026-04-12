#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include "ChordGridView.h"
#include "DrumGridView.h"
#include "NoteDetailView.h"
#include "../model/SetleSongModel.h"

namespace te = tracktion::engine;

namespace setle::gridroll
{

/**
 * Host component for the GridRoll editor (Phase 14D).
 *
 * Contains ChordGridView and DrumGridView and manages mode / zoom state.
 * Supports three layout modes:
 *   ChordGrid — chord row only
 *   DrumGrid  — drum sequencer only
 *   Split     — chord (top 40%) + drums (bottom 60%), locked horizontal scroll
 */
class GridRollComponent : public juce::Component
{
public:
    enum class Mode { ChordGrid, DrumGrid, Split };

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

    /** Called when a chord edit should be persisted back to the model. */
    std::function<void(const juce::String& progressionId)> onProgressionEdited;

    /** Called when a status message should be shown. */
    std::function<void(const juce::String&)> onStatusMessage;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    // ---------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------
    void applyMode();
    void syncChordCellsToModel();
    void openNoteDetail(int cellIndex);
    void closeNoteDetail();

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    model::Song& song;
    te::Edit&    edit;

    std::unique_ptr<ChordGridView> chordGrid;
    std::unique_ptr<DrumGridView>  drumGrid;
    std::unique_ptr<NoteDetailView> noteDetail;

    Mode   currentMode     { Mode::ChordGrid };
    int    expandedCellIdx { -1 };   // -1 = no detail open
    double playheadBeat    { 0.0 };
    double zoomBeatsPerPx  { 1.0 };

    juce::String progressionId;

    // Header controls
    juce::TextButton chordModeButton { "Chords" };
    juce::TextButton drumModeButton  { "Drums" };
    juce::TextButton splitModeButton { "Split" };
    juce::Label      progressionName;
    juce::Label      sessionKeyDisplay;
    juce::TextButton zoomInButton    { "+" };
    juce::TextButton zoomOutButton   { "-" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridRollComponent)
};

} // namespace setle::gridroll
