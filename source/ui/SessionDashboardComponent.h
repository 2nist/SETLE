#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <tracktion_engine/tracktion_engine.h>

#include <atomic>
#include <vector>

#include "../capture/CircularAudioBuffer.h"
#include "../model/SetleSongModel.h"
#include "../theme/ThemeManager.h"

namespace te = tracktion::engine;

namespace setle::ui
{

class SessionDashboardComponent final : public juce::Component,
                                        public juce::Timer,
                                        public ThemeManager::Listener
{
public:
    SessionDashboardComponent(te::Engine& engine,
                             setle::model::Song& songModel,
                             setle::capture::CircularAudioBuffer& audioBuffer,
                             te::Edit* activeEdit = nullptr);
    ~SessionDashboardComponent() override;

    // juce::Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::Timer - runs at 60Hz for dashboard updates
    void timerCallback() override;

    // ThemeManager::Listener
    void themeChanged() override;

    float getDominantFrequencyHz() const noexcept { return dominantFrequency; }
    float getDominantMagnitude() const noexcept { return dominantMagnitude; }
    void pushAudioCallbackMetrics(float blockPeak, int numSamples) noexcept;

private:
    // ===== Material Rendering Methods =====
    /**
     * Draws the pebbled matte background with high-frequency noise.
     */
    void drawPebbiedMatte(juce::Graphics& g, const juce::Rectangle<float>& bounds);

    /**
     * Draws the underdog glass panel effect with smoked acrylic appearance.
     */
    void drawUnderdogGlassPanel(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                                juce::Colour panelColour, bool drawBorder = true);

    /**
     * Draws the fuzzy inset readout with inner shadow depth.
     */
    void drawFuzzyInset(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                       juce::Colour textColour, const juce::String& text, float fontSize);

    /**
     * Draws an organic glow indicator light with radial gradient.
     */
    void drawOrganicGlow(juce::Graphics& g, const juce::Point<float>& centre, float radius,
                        juce::Colour glowColour, bool isActive, float pulseAmount = 0.0f);

    /**
     * Draws a circular needle for the ToneScope, with gradient glow.
     */
    void drawToneScopeNeedle(juce::Graphics& g, const juce::Point<float>& centre, float radius,
                            float angleRadians, juce::Colour needleColour);

    // ===== Sub-Component Rendering =====
    void renderBpmModule(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void renderToneScope(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void renderKeyIntelligence(juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void renderHealthGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds);

    // ===== Logic Methods =====
    void updateFFTAnalysis();
    void updateKeyDetection();
    void updateHealthMetrics();
    void updateDiatoniHealthPulse();
    std::vector<int> collectActiveMidiNotesAtPlayhead() const;
    double getCurrentPlayheadBeat() const;
    float querySystemRamUsageRatio() const;

    // ===== Audio FFT Analysis =====
    void performFFTAnalysis(const juce::AudioBuffer<float>& buffer);
    float getFrequencyForBin(int bin) const noexcept;

    // ===== Utility =====
    juce::Colour getThemeColour(const juce::String& colourName) const;
    float getNoiseValue(float x, float y) const noexcept;

    // ===== Members =====
    te::Engine& engine;
    setle::model::Song& songModel;
    setle::capture::CircularAudioBuffer& audioBuffer;
    te::Edit* edit { nullptr };

    // FFT Analysis state
    static constexpr int kFFTOrder = 11;              // 2^11 = 2048
    static constexpr int kFFTSize  = 1 << kFFTOrder;
    // Regression guard: juce::dsp::FFT constructor takes the log2 order, not the
    // raw size. Passing a size (e.g. 1024) as order allocates 2^1024 bytes and
    // crashes immediately. kFFTOrder must be a small exponent in [1, 20].
    static_assert (kFFTOrder >= 1 && kFFTOrder <= 20,
                   "kFFTOrder must be a log2 order in [1,20], not a raw FFT size");
    juce::dsp::FFT fftProcessor { kFFTOrder };
    // performRealOnlyForwardTransform writes 2*size interleaved complex floats.
    // A buffer of only kFFTSize floats causes a heap overrun after a few seconds.
    std::array<float, kFFTSize * 2> fftData {};   // must be 2*kFFTSize
    std::array<float, kFFTSize / 2 + 1> fftMagnitudes {};
    float dominantFrequency { 0.0f };
    float dominantMagnitude { 0.0f };

    // ToneScope rendering state
    float scopeNeedleAngle { 0.0f };
    float scopeNeedleTargetAngle { 0.0f };

    // Health metrics
    struct HealthMetrics
    {
        float diatonicHealth { 0.5f };      // 0.0-1.0 in-key percentage
        float creativeVelocity { 0.3f };    // 0.0-1.0 activity density
        float cpuLoad { 0.1f };             // 0.0-1.0
        float ramUsage { 0.15f };           // 0.0-1.0
    };
    HealthMetrics currentMetrics;

    // Diatonic Health pulsing
    float diatonicHealthPulsePhase { 0.0f };
    float diatonicHealthPulseAmount { 0.0f };
    bool shouldPulseDiatonicHealth { false };

    // Key comparison
    juce::String detectedKey { "C" };
    bool keyMismatch { false };

    // BPM display
    double currentBpm { 120.0 };

    // Last sample rate for FFT calibration
    double lastSampleRate { 44100.0 };

    // Audio callback-fed metrics (written on audio thread, read on UI timer thread)
    std::atomic<float> callbackPeak { 0.0f };
    std::atomic<int> callbackSampleCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionDashboardComponent)
};

} // namespace setle::ui
