#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "../capture/GrabSamplerQueue.h"
#include "../capture/HistoryBuffer.h"
#include "../capture/CircularAudioBuffer.h"
#include "../instruments/InstrumentSlot.h"
#include "../model/SetleSongModel.h"
#include "../timeline/TimelineStampManager.h"
#include "../timeline/TimelineTracksComponent.h"
#include "../timeline/TrackManager.h"
#include "../gridroll/GridRollComponent.h"
#include "../eff/EffBuilderComponent.h"
#include "SessionDashboardComponent.h"
#include "../eff/EffSerializer.h"
#include "../state/AppPreferences.h"
#include "../theme/SetleLookAndFeel.h"
#include "../theme/ThemeEditorPanel.h"
#include "../theme/ThemeManager.h"
#include "ProgressionLibraryBrowser.h"
#include "ProgressionChordPalette.h"
#include "ArrangeWorkComponent.h"
#include "SoundWorkComponent.h"
#include "ToolPaletteComponent.h"
#include "AppCommands.h"
#include "AppMenuBarModel.h"
#include "LeftNavComponent.h"
#include "NavSection.h"
#include "ZoneHeaderComponent.h"
#include "QuesoLogoComponent.h"

namespace te = tracktion::engine;

namespace setle::ui
{

class WorkspaceShellComponent final : public juce::Component,
                                      public juce::DragAndDropContainer,
                                      public juce::ApplicationCommandTarget,
                                      private juce::Timer,
                                      public ThemeManager::Listener
{
public:
    explicit WorkspaceShellComponent(te::Engine& engine);
    ~WorkspaceShellComponent() override;

    std::optional<setle::timeline::TimelineStampManager::GhostTrail> getGhostTrail(const juce::String& sectionId) const;

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

    // juce::ApplicationCommandTarget overrides
    juce::ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands(juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo(juce::CommandID commandID,
                        juce::ApplicationCommandInfo& result) override;
    bool perform(const juce::ApplicationCommandTarget::InvocationInfo& info) override;

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
    void switchNavSection(NavSection section);
    void populateTheoryObjectSelector();
    void populateTheoryFieldsForCurrentSelection();
    void commitTheoryEditorAction();
    juce::String applyTheoryEditorAction();
    void updateSelectionFromSelector();
    void handleGridRollSelectionChanged(const setle::gridroll::GridRollComponent::SelectionMetadata& metadata);
    void applyLiveCoupledTheoryMutation();

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
    std::vector<SoundWorkComponent::InstrumentRosterEntry> buildSoundRosterEntries() const;
    void refreshSoundRoster();
    void refreshArrangeWorkspace();
    void focusSoundTrack(const juce::String& trackId);
    SoundWorkComponent::SidechainSuggestion buildSmartSidechainSuggestion(const juce::String& focusedTrackId) const;
    void applySmartSidechainSuggestion(const SoundWorkComponent::SidechainSuggestion& suggestion);
    std::optional<float> runAutoGainForTrack(const juce::String& trackId);
    void onSectionReordered(const juce::StringArray& orderedSectionIds);
    juce::String smartDuplicateSection(const juce::String& sectionId, ArrangeWorkComponent::DuplicateMode mode);
    double getSectionLengthBeats(const model::Section& section) const;
    double getSongStructureLengthBeats() const;
    juce::String beginUndoableAction(const juce::String& actionLabel);
    void performUpdate(const juce::String& beforeSnapshot, const juce::String& statusMessage);
    void showVesselSelectionModal();
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

    // ---- Nav / menu layer ----
    juce::ApplicationCommandManager commandManager;
    std::unique_ptr<AppMenuBarModel>     appMenuBarModel;
    std::unique_ptr<juce::MenuBarComponent> menuBarComponent;
    std::unique_ptr<LeftNavComponent>    leftNavComponent;

    NavSection activeNavSection { NavSection::edit };

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
    std::unique_ptr<QuesoLogoComponent> quesoLogoComponent;
    juce::Label interactionStatus;

    SetleLookAndFeel setleLookAndFeel;
    std::unique_ptr<ThemeEditorPanel> themeEditorPanel;
    juce::Component themeDismissOverlay;
    std::unique_ptr<setle::capture::GrabSamplerQueue> grabSamplerQueue;
    std::unique_ptr<setle::capture::HistoryBuffer> historyBuffer;
    std::unique_ptr<setle::capture::CircularAudioBuffer> circularAudioBuffer;
    std::unique_ptr<juce::AudioIODeviceCallback> outputCaptureTap;
    std::unique_ptr<setle::timeline::TrackManager> trackManager;
    std::unique_ptr<setle::timeline::TimelineStampManager> timelineStampManager;
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
    int workPanelTabIndex { 0 };  // 0=Theory, 1=GridRoll, 2=FX, 3=SESSION, 4=ARRANGE
    bool workPanelShowGridRoll { false };
    std::unique_ptr<ProgressionLibraryBrowser> libraryBrowser;
    std::unique_ptr<ProgressionChordPalette> chordPalette;
    std::unique_ptr<setle::gridroll::GridRollComponent> gridRollComponent;
    std::unique_ptr<setle::eff::EffBuilderComponent> effBuilderComponent;
    std::unique_ptr<ArrangeWorkComponent> arrangeWorkComponent;
    std::unique_ptr<SoundWorkComponent> soundWorkComponent;
    std::unique_ptr<SessionDashboardComponent> sessionDashboardComponent;
    std::optional<setle::gridroll::GridRollComponent::SelectionMetadata> liveGridRollSelection;
    bool suppressLiveCoupledFieldEvents { false };
    juce::String liveCoupledUndoBaseSnapshot;
    juce::uint32 liveCoupledLastMutationMs { 0 };
    juce::String selectedFxTrackId;
    float arrangeLogoMeltTarget { 0.0f };
    float arrangeLogoMeltCurrent { 0.0f };
    TheoryMenuTarget activeEditorTarget { TheoryMenuTarget::section };
    int activeEditorActionId = 0;

    // ---- Zone header (inside WORK panel) ----
    std::unique_ptr<ZoneHeaderComponent> zoneHeader;

    FocusMode focusMode { FocusMode::balanced };

    int leftPanelWidth { 280 };
    int rightPanelWidth { 300 };
    int timelineHeight { 280 };

    static constexpr int menuBarHeight    = 22;
    static constexpr int navBarHeight     = 30;
    static constexpr int topStripHeight   = 44;
    static constexpr int splitterThickness = 6;
    static constexpr int minSideWidth = 72;
    static constexpr int maxSideWidth = 700;
    static constexpr int minCenterWidth = 360;
    static constexpr int minTimelineHeight = 248;
    static constexpr int minTopWorkHeight = 220;
    static constexpr int maxUndoDepth = 64;
};

} // namespace setle::ui
