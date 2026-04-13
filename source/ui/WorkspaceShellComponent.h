#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include <map>
#include <memory>
#include <vector>

#include "../capture/GrabSamplerQueue.h"
#include "../capture/HistoryBuffer.h"
#include "../capture/CircularAudioBuffer.h"
#include "../instruments/InstrumentSlot.h"
#include "../model/SetleSongModel.h"
#include "../timeline/TimelineTracksComponent.h"
#include "../timeline/TrackManager.h"
#include "../gridroll/GridRollComponent.h"
#include "../eff/EffBuilderComponent.h"
#include "../eff/EffSerializer.h"
#include "../state/AppPreferences.h"
#include "../theme/SetleLookAndFeel.h"
#include "../theme/ThemeEditorPanel.h"
#include "../theme/ThemeManager.h"
#include "ProgressionLibraryBrowser.h"
#include "ProgressionChordPalette.h"
#include "ToolPaletteComponent.h"

namespace te = tracktion::engine;

namespace setle::ui
{

class WorkspaceShellComponent final : public juce::Component,
                                      public juce::DragAndDropContainer,
                                      private juce::Timer,
                                      public ThemeManager::Listener
{
public:
    explicit WorkspaceShellComponent(te::Engine& engine);
    ~WorkspaceShellComponent() override;

    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void resized() override;
    void themeChanged() override;
    void themePreviewTargetChanged(const juce::String& token) override;

private:
    class LabelPanel;
    class OutPanelHost;
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
    void ensureProgressionLoadedForNoteMode(const juce::String& progressionId);
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
    void switchWorkTab(int tabIndex); // 0=Theory, 1=GridRoll, 2=FX
    void switchWorkTab(bool showGridRoll) { switchWorkTab(showGridRoll ? 1 : 0); }
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

    // Meter-aware snap calculation helper
    double getSnapBeats(double atBeat) const;

    void refreshTimelineData();
    void refreshTheoryLanes() { refreshTimelineData(); }
    void syncTimeSignaturesToEdit();
    void ensureInstrumentSlots();
    void applyPersistedInstrumentSlotAssignments();
    void persistInstrumentSlotAssignments();
    void rebuildOutPanelStrips();
    void applyDrumPatternToSlots(const std::vector<setle::gridroll::GridRollCell>& cells,
                                 const juce::String& progressionId);
    juce::String getTrackIdForTrack(const te::Track& track) const;
    void clampLayoutValues(int totalTopWidth, int totalBodyHeight);
    void loadLayoutState();
    void saveLayoutState();
    void timerCallback() override;
    void repaintEntireTree();
    void showThemeEditor(bool shouldShow);
    juce::Rectangle<int> highlightBoundsForThemeToken(const juce::String& token) const;
    void drawThemePreviewHighlight(juce::Graphics& g) const;

    // FX chain persistence
    juce::File getEffectsFolder() const;
    juce::File getEffFileForTrack(const juce::String& trackId) const;
    void saveEffChains();
    void loadEffChains();

    te::Engine& engineRef;
    std::unique_ptr<te::Edit> edit;

    juce::Component topStrip;
    juce::Component* inPanel;
    LabelPanel* workPanel;
    std::unique_ptr<OutPanelHost> outPanelHost;
    TimelineShell* timelineShell;
    setle::timeline::TimelineTracksComponent* timelineTracks { nullptr };
    std::unique_ptr<ToolPaletteComponent> toolPalette;

    DragBar* leftResizeBar;
    DragBar* rightResizeBar;
    DragBar* timelineResizeBar;

    juce::TextButton playButton   { juce::CharPointer_UTF8("\xe2\x96\xb6") };  // ▶
    juce::TextButton stopButton   { juce::CharPointer_UTF8("\xe2\x96\xa0") };  // ■
    juce::TextButton recordButton { juce::CharPointer_UTF8("\xe2\x97\x8f") };  // ●
    juce::Label     bpmLabel;
    juce::Label     transportPositionLabel;
    juce::TextEditor bpmEditor;
    juce::Label captureSourceLabel;
    juce::ComboBox captureSourceSelector;
    std::vector<juce::String> captureSourceIdentifiers;
    juce::String midiDeviceSignature;
    float captureSourceActivity { 0.0f };
    int lastHistoryEventCount { 0 };

    juce::TextButton focusInButton { "Focus IN" };
    juce::TextButton focusBalancedButton { "Focus Balanced" };
    juce::TextButton focusOutButton { "Focus OUT" };
    juce::TextButton undoTheoryButton { "Undo Theory" };
    juce::TextButton redoTheoryButton { "Redo Theory" };
    juce::TextButton themeButton { "Theme" };

    juce::Label sessionKeyLabel;
    juce::ComboBox sessionKeySelector;
    juce::Label sessionModeLabel;
    juce::ComboBox sessionModeSelector;

    juce::Label topTitle;
    juce::Label interactionStatus;

    SetleLookAndFeel setleLookAndFeel;
    std::unique_ptr<ThemeEditorPanel> themeEditorPanel;
    juce::Component themeDismissOverlay;
    std::unique_ptr<setle::capture::GrabSamplerQueue> grabSamplerQueue;
    std::unique_ptr<setle::capture::HistoryBuffer> historyBuffer;
    std::unique_ptr<setle::capture::CircularAudioBuffer> circularAudioBuffer;
    std::unique_ptr<juce::AudioIODeviceCallback> outputCaptureTap;
    std::unique_ptr<setle::timeline::TrackManager> trackManager;
    std::map<juce::String, std::unique_ptr<setle::instruments::InstrumentSlot>> instrumentSlots;
    model::Song songState;
    std::vector<juce::String> undoSnapshots;
    std::vector<juce::String> redoSnapshots;
    std::vector<juce::String> theorySelectorObjectIds;
    juce::String selectedSectionId;
    juce::String selectedProgressionId;
    juce::String selectedChordId;
    juce::String selectedNoteId;
    std::shared_ptr<juce::AudioBuffer<float>> pendingGrabCoupledAudio;
    double pendingGrabCoupledSampleRate { 0.0 };

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
    juce::TextButton workTabTheoryButton   { "Theory Editor" };
    juce::TextButton workTabGridRollButton { "GridRoll" };
    juce::TextButton workTabFxButton       { "FX" };
    int workPanelTabIndex { 0 };  // 0=Theory, 1=GridRoll, 2=FX
    bool workPanelShowGridRoll { false };
    std::unique_ptr<ProgressionLibraryBrowser> libraryBrowser;
    std::unique_ptr<ProgressionChordPalette> chordPalette;
    std::unique_ptr<setle::gridroll::GridRollComponent> gridRollComponent;
    std::unique_ptr<setle::eff::EffBuilderComponent> effBuilderComponent;
    juce::String selectedFxTrackId;
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
