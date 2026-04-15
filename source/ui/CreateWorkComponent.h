#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include "../capture/CircularAudioBuffer.h"
#include "../capture/GrabSamplerQueue.h"
#include "../model/SetleSongModel.h"
#include "../theme/ThemeManager.h"

namespace te = tracktion::engine;

namespace setle::ui
{

/**
 * CreateWorkComponent - The "Reel-In" Manager (Sorting Desk Build)
 *
 * This station acts as the bridge between raw history captures ("The Spool")
 * and the formal DAW sections (EDIT, SOUND, ARRANGE).
 *
 * Features:
 * - Split examination window (MIDI + Audio history strips with sync'd scroll/zoom)
 * - Trimming handles to crop the capture before committing
 * - Three committal paths: EDIT (MIDI clip), SOUND (audio load), ARRANGE (both to timeline)
 * - Scene sidebar with capture presets
 * - Subscribes to GrabManager events for auto-focus and population
 */
class CreateWorkComponent final : public juce::Component,
                                   public juce::Timer,
                                   public ThemeManager::Listener
{
public:
    CreateWorkComponent(te::Engine& engine,
                        setle::model::Song& songModel,
                        setle::capture::CircularAudioBuffer& audioBuffer,
                        setle::capture::GrabSamplerQueue& grabQueue);
    ~CreateWorkComponent() override;

    // juce::Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::Timer - runs at 60Hz for animation/UI updates
    void timerCallback() override;

    // ThemeManager::Listener
    void themeChanged() override;

    // Called when a new capture is grabbed (from external listener)
    void onNewCapture(const juce::AudioBuffer<float>& midiOrAudioData,
                     const std::vector<setle::capture::HistoryBuffer::CapturedEvent>& midiEvents);

    // Returns current trim bounds in beats (relative to capture start)
    std::pair<double, double> getTrimBounds() const noexcept { return { trimStartBeat, trimEndBeat }; }

    // Backend bridge callbacks (wired by host shell when CREATE station is mounted)
    std::function<void()> onGridRefreshRequested;
    std::function<bool(const juce::File& sampleFile)> onLoadSampleRequested;
    std::function<void(const juce::String& progressionId, const juce::File& sampleFile, double playheadBeat)> onStampToArrangeRequested;
    std::function<double()> onRequestPlayheadBeat;
    void setSelectedTrackId(juce::String trackId) { selectedTrackId = std::move(trackId); }

private:
    // ===== Material Rendering Methods (Shared with SessionDashboardComponent) =====
    void drawPebbiedMatte(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void drawUnderdogGlassPanel(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                                juce::Colour panelColour, bool drawBorder = true);
    void drawFuzzyInset(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                       juce::Colour textColour, const juce::String& text, float fontSize);
    void drawOrganicGlow(juce::Graphics& g, const juce::Point<float>& centre, float radius,
                        juce::Colour glowColour, bool isActive, float pulseAmount = 0.0f);

    // ===== Examination Window Components =====
    void renderSceneSidebar(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void renderExaminationWindow(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void renderMidiHistoryStrip(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void renderAudioHistoryStrip(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void renderTrimHandles(juce::Graphics& g, const juce::Rectangle<float>& panelBounds);
    void renderCommittalBridge(juce::Graphics& g, const juce::Rectangle<float>& bounds);

    // ===== Committal Logic =====
    void commitToEdit();       // Extract MIDI from trim bounds → create clip in GridRoll
    void commitToSound();      // Extract audio from trim bounds → load into ReelSampler
    void commitToArrange();    // Stamp both MIDI and audio into ARRANGE timeline

    // ===== Utility =====
    float getNoiseValue(float x, float y) const noexcept;
    juce::Colour getThemeColour(const juce::String& colourName) const;

    // ===== Scene Presets =====
    struct CapturePreset
    {
        juce::String name;
        juce::String inputSource;
        int bufferLengthBeats { 16 };
        float recordingThreshold { 0.05f };
    };
    void buildScenePresets();
    void applyScenePreset(const CapturePreset& preset);

    // ===== Mouse Input =====
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // ===== Members =====
    te::Engine& engine;
    setle::model::Song& songModel;
    setle::capture::CircularAudioBuffer& audioBuffer;
    setle::capture::GrabSamplerQueue& grabQueue;

    // Capture data
    std::shared_ptr<juce::AudioBuffer<float>> capturedAudio;
    std::vector<setle::capture::HistoryBuffer::CapturedEvent> capturedMidiEvents;
    double captureStartBeat { 0.0 };
    double captureTotalBeats { 0.0 };
    juce::String selectedTrackId;

    // Trim state (in beats, relative to capture start)
    double trimStartBeat { 0.0 };
    double trimEndBeat { 16.0 };
    enum class TrimDragMode { None, StartHandle, EndHandle, Scroll };
    TrimDragMode trimDragMode { TrimDragMode::None };
    double scrollOffsetBeats { 0.0 };

    // Zoom/Scroll sync between MIDI and audio strips
    double zoomLevel { 1.0 };  // pixels per beat

    // Committal button pulse animation
    float commitPulseAmount { 0.0f };
    float commitPulsePhase { 0.0f };
    bool isReadyToCommit { false };

    // Scene presets
    std::vector<CapturePreset> scenePresets;
    int selectedPresetIndex { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CreateWorkComponent)
};

} // namespace setle::ui
