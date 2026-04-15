#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include "ChordGridView.h"
#include "DrumGridView.h"
#include "NoteModeView.h"
#include "../model/SetleSongModel.h"
#include "../theory/MeterContext.h"
#include "../theme/ThemeManager.h"

#include <optional>

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

    struct SelectionMetadata
    {
        enum class Target
        {
            none,
            chord,
            note
        };

        Target target { Target::none };
        bool valid { false };
        juce::String chordId;
        juce::String noteId;
        juce::String root;
        juce::String extension;
        int inversion { 0 };
        float velocity { 0.8f };
    };

    struct CoupledMutation
    {
        juce::String root;
        juce::String extension;
        int inversion { 0 };
        float velocity { 0.8f };
    };

    static constexpr int kHeaderHeight = 32; // mode buttons + name + zoom

    explicit GridRollComponent(model::Song& song, te::Edit& edit);

    /** Point the GridRoll at a specific progression id. */
    void setTargetProgression(const juce::String& progressionId);

    /** Switch between ChordGrid / DrumGrid / Split display modes. */
    void setMode(Mode mode);

    /** Drive the playhead from the transport (30 Hz). */
    void setPlayheadBeat(double beat);
    bool isNoteMode() const noexcept { return currentMode == Mode::NoteMode || currentMode == Mode::Split; }
    void refreshNoteModeCache();

    /** Propagate the theory snap setting from the workspace. */
    void setTheorySnap(const juce::String& snap);
    void setMeterContext(const setle::theory::MeterContext& meter);
    std::optional<SelectionMetadata> getSelectionMetadata() const;
    bool applyTheoryMutationToSelection(const CoupledMutation& mutation);
    bool applyHumanizeToSelection(int timingJitterMs, int velocityDeviation);
    void toggleHumanizePopover();
    void setScaleLock(bool enabled);
    void setChordLock(bool enabled);
    bool getScaleLock() const noexcept { return scaleLockEnabled; }
    bool getChordLock() const noexcept { return chordLockEnabled; }

    /** Called when a chord edit should be persisted back to the model. */
    std::function<void(const juce::String& progressionId)> onProgressionEdited;

    /** Called before Note Mode cache binding, so host can ensure TE clip data exists. */
    std::function<void(const juce::String& progressionId)> onPrepareProgressionForNoteMode;

    /** Called when a status message should be shown. */
    std::function<void(const juce::String&)> onStatusMessage;

    /** Called when drum pattern cells change and should be pushed to drum instruments. */
    std::function<void(const std::vector<GridRollCell>&, const juce::String& progressionId)> onDrumPatternEdited;
    std::function<void(const SelectionMetadata&)> onSelectionMetadataChanged;
    std::function<void(bool scaleLock, bool chordLock)> onConstraintLockChanged;

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
    void updateSelectionFromChordCell(int cellIndex);
    void updateSelectionFromNoteMetadata(const NoteModeView::SelectionMetadata& metadata);
    void emitSelectionChanged();
    void updateLockButtonStyles();
    void showHumanizePopover(bool shouldShow);
    bool applyChordMutationToSelection(const CoupledMutation& mutation);

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
    int selectedChordCellIndex { -1 };
    std::optional<SelectionMetadata> currentSelection;
    bool scaleLockEnabled { false };
    bool chordLockEnabled { false };

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
    juce::TextButton scaleLockButton { "Scale Lock" };
    juce::TextButton chordLockButton { "Chord Lock" };
    juce::TextButton humanizeButton  { "Humanize" };

    juce::Component humanizePopover;
    juce::Slider timingJitterSlider;
    juce::Slider velocityDeviationSlider;
    juce::Label timingJitterLabel;
    juce::Label velocityDeviationLabel;
    juce::TextButton applyHumanizeButton { "Apply" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridRollComponent)
};

} // namespace setle::gridroll
