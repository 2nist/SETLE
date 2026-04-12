#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include <memory>
#include <vector>

#include "../capture/GrabSamplerQueue.h"
#include "../capture/HistoryBuffer.h"
#include "../model/SetleSongModel.h"
#include "../timeline/TimelineTracksComponent.h"
#include "../timeline/TrackManager.h"
#include "ProgressionLibraryBrowser.h"
#include "ProgressionChordPalette.h"

namespace te = tracktion::engine;

namespace setle::ui
{

class WorkspaceShellComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    explicit WorkspaceShellComponent(te::Engine& engine);
    ~WorkspaceShellComponent() override;

    bool keyPressed(const juce::KeyPress& key) override;
    void resized() override;

private:
    class LabelPanel;
    class TimelineShell;
    class DragBar;
    class InDevicePanel;

    enum class TheoryMenuTarget
    {
        section,
        chord,
        note,
        progression,
        historyBuffer
    };

    enum class FocusMode
    {
        balanced = 0,
        inFocused = 1,
        outFocused = 2
    };

    void paint(juce::Graphics& g) override;
    void applyFocusMode(FocusMode mode);
    void updateFocusButtonState();
    void handleTimelineTheoryAction(TheoryMenuTarget target, int actionId, const juce::String& actionName);
    juce::String runTheoryAction(TheoryMenuTarget target, int actionId, const juce::String& actionName);
    juce::String runSectionAction(int actionId);
    juce::String runChordAction(int actionId);
    juce::String runNoteAction(int actionId);
    juce::String runProgressionAction(int actionId, const juce::String& targetProgressionId);
    juce::String runHistoryBufferAction(int actionId);
    void handleProgressionAction(const juce::String& progressionId, int actionId);
    juce::PopupMenu buildProgressionContextMenu(const juce::String& progressionId);
    void grabFromBuffer(int beats);
    void refreshCaptureSourceSelector(const juce::String& preferredSourceIdentifier = {});
    void applyCaptureSourceSelection();
    juce::String buildMidiDeviceSignature() const;
    void updateInPanelQueueView();
    void playQueueSlot(int slotIndex);
    void stopQueuePlayback();
    void promoteQueueSlotToLibrary(int slotIndex);
    void dropQueueSlotToTimeline(int slotIndex);
    void clearQueueSlot(int slotIndex);
    void editQueueSlotInTheoryPanel(int slotIndex);
    void renameQueueSlot(int slotIndex);

    void initialiseSongState();
    void loadProgressionToEdit();
    void loadProgressionToEdit(const juce::String& progressionId,
                               double startTimeSeconds,
                               bool preferNonSystemTrack,
                               te::Track* preferredTrack = nullptr);
    juce::File getSongStateFile() const;
    void loadSongState();
    void seedSongStateIfNeeded();
    void saveSongState();
    juce::String createSongSnapshot() const;
    bool restoreSongFromSnapshot(const juce::String& xmlSnapshot);
    void captureUndoStateIfChanged(const juce::String& beforeSnapshot);
    void performUndo();
    void performRedo();
    void updateUndoRedoButtonState();
    void configureTheoryEditorPanel();
    void openTheoryEditor(TheoryMenuTarget target, int actionId, const juce::String& actionName);
    void populateTheoryObjectSelector();
    void populateTheoryFieldsForCurrentSelection();
    void commitTheoryEditorAction();
    juce::String applyTheoryEditorAction();
    void updateSelectionFromSelector();

    std::optional<model::Section> getSelectedSection();
    std::optional<model::Progression> getSelectedProgression();
    std::optional<model::Chord> getSelectedChord();
    std::optional<model::Note> getSelectedNote();
    void ensureSelectionDefaults();

    juce::String summarizeSongState() const;

    void refreshTimelineData();
    void clampLayoutValues(int totalTopWidth, int totalBodyHeight);
    void loadLayoutState();
    void saveLayoutState();
    void timerCallback() override;

    te::Engine& engineRef;
    std::unique_ptr<te::Edit> edit;

    juce::Component topStrip;
    juce::Component* inPanel;
    LabelPanel* workPanel;
    LabelPanel* outPanel;
    TimelineShell* timelineShell;
    setle::timeline::TimelineTracksComponent* timelineTracks { nullptr };

    DragBar* leftResizeBar;
    DragBar* rightResizeBar;
    DragBar* timelineResizeBar;

    juce::TextButton playButton   { juce::CharPointer_UTF8("\xe2\x96\xb6") };  // ▶
    juce::TextButton stopButton   { juce::CharPointer_UTF8("\xe2\x96\xa0") };  // ■
    juce::TextButton recordButton { juce::CharPointer_UTF8("\xe2\x97\x8f") };  // ●
    juce::Label     bpmLabel;
    juce::TextEditor bpmEditor;
    juce::Label captureSourceLabel;
    juce::ComboBox captureSourceSelector;
    std::vector<juce::String> captureSourceIdentifiers;
    juce::String midiDeviceSignature;

    juce::TextButton focusInButton { "Focus IN" };
    juce::TextButton focusBalancedButton { "Focus Balanced" };
    juce::TextButton focusOutButton { "Focus OUT" };
    juce::TextButton undoTheoryButton { "Undo Theory" };
    juce::TextButton redoTheoryButton { "Redo Theory" };

    juce::Label sessionKeyLabel;
    juce::ComboBox sessionKeySelector;
    juce::Label sessionModeLabel;
    juce::ComboBox sessionModeSelector;

    juce::Label topTitle;
    juce::Label interactionStatus;

    juce::ApplicationProperties appProperties;
    std::unique_ptr<setle::capture::GrabSamplerQueue> grabSamplerQueue;
    std::unique_ptr<setle::capture::HistoryBuffer> historyBuffer;
    std::unique_ptr<setle::timeline::TrackManager> trackManager;
    model::Song songState;
    std::vector<juce::String> undoSnapshots;
    std::vector<juce::String> redoSnapshots;
    std::vector<juce::String> theorySelectorObjectIds;
    juce::String selectedSectionId;
    juce::String selectedProgressionId;
    juce::String selectedChordId;
    juce::String selectedNoteId;

    // TRANSIENT UI STATE — never persist these to SetleSongModel.
    // They reset on app launch. Keep them on the component, not the model.
    int theoryCurrentRepeatIndex { 1 };
    juce::String theorySelectedRepeatIndices;
    juce::String theoryScope;
    juce::String theorySnap { "1/16" };

    juce::Component theoryEditorPanel;
    juce::Label theoryEditorTitle;
    juce::Label theoryEditorHint;
    juce::Label theoryObjectLabel;
    juce::ComboBox theoryObjectSelector;
    juce::Label theoryFieldLabel1;
    juce::Label theoryFieldLabel2;
    juce::Label theoryFieldLabel3;
    juce::Label theoryFieldLabel4;
    juce::Label theoryFieldLabel5;
    juce::TextEditor theoryFieldEditor1;
    juce::TextEditor theoryFieldEditor2;
    juce::TextEditor theoryFieldEditor3;
    juce::TextEditor theoryFieldEditor4;
    juce::TextEditor theoryFieldEditor5;
    juce::TextButton applyTheoryEditorButton { "Apply Edit" };
    juce::TextButton reloadTheoryEditorButton { "Reload" };
    std::unique_ptr<ProgressionLibraryBrowser> libraryBrowser;
    std::unique_ptr<ProgressionChordPalette> chordPalette;
    TheoryMenuTarget activeEditorTarget { TheoryMenuTarget::section };
    int activeEditorActionId = 0;

    FocusMode focusMode { FocusMode::balanced };

    int leftPanelWidth { 280 };
    int rightPanelWidth { 300 };
    int timelineHeight { 280 };

    static constexpr int topStripHeight = 44;
    static constexpr int splitterThickness = 6;
    static constexpr int minSideWidth = 72;
    static constexpr int maxSideWidth = 700;
    static constexpr int minCenterWidth = 360;
    static constexpr int minTimelineHeight = 248;
    static constexpr int minTopWorkHeight = 220;
    static constexpr int maxUndoDepth = 64;
};

} // namespace setle::ui
