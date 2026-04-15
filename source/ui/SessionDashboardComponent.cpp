#include "SessionDashboardComponent.h"

#include "../theme/ThemeStyleHelpers.h"
#include "../theory/DiatonicHarmony.h"

#include <cmath>
#include <algorithm>
#include <array>

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace setle::ui
{

SessionDashboardComponent::SessionDashboardComponent(te::Engine& engine_,
                                                     setle::model::Song& songModel_,
                                                     setle::capture::CircularAudioBuffer& audioBuffer_,
                                                     te::Edit* activeEdit)
    : engine(engine_), songModel(songModel_), audioBuffer(audioBuffer_), edit(activeEdit)
{
    ThemeManager::get().addListener(this);
    
    // Start timer at ~60Hz (16ms)
    startTimerHz(60);
}

SessionDashboardComponent::~SessionDashboardComponent()
{
    stopTimer();
    ThemeManager::get().removeListener(this);
}

void SessionDashboardComponent::pushAudioCallbackMetrics(float blockPeak, int numSamples) noexcept
{
    callbackPeak.store(juce::jmax(0.0f, blockPeak), std::memory_order_relaxed);
    callbackSampleCount.fetch_add(juce::jmax(0, numSamples), std::memory_order_relaxed);
}

void SessionDashboardComponent::paint(juce::Graphics& g)
{
    // Draw pebbled matte background
    drawPebbiedMatte(g, getLocalBounds().toFloat());

    // 3x3 Grid layout
    const auto gridPadding = 12.0f;
    const auto gridGap = 10.0f;
    const auto bounds = getLocalBounds().toFloat().reduced(gridPadding);
    
    const auto cellWidth = (bounds.getWidth() - gridGap * 2.0f) / 3.0f;
    const auto cellHeight = (bounds.getHeight() - gridGap * 2.0f) / 3.0f;

    // Top row
    const auto topY = bounds.getY();
    const auto leftX = bounds.getX();
    const auto centerX = leftX + cellWidth + gridGap;
    const auto rightX = centerX + cellWidth + gridGap;
    
    // Middle row
    const auto midY = topY + cellHeight + gridGap;
    
    // Bottom row
    const auto bottomY = midY + cellHeight + gridGap;

    // Top-Left: BPM Module
    renderBpmModule(g, juce::Rectangle<float>(leftX, topY, cellWidth, cellHeight));

    // Top-Center: ToneScope (Hero)
    renderToneScope(g, juce::Rectangle<float>(centerX, topY, cellWidth, cellHeight));

    // Top-Right: Key Intelligence
    renderKeyIntelligence(g, juce::Rectangle<float>(rightX, topY, cellWidth, cellHeight));

    // Bottom: Health Grid (spanning all 3 columns)
    const auto healthGridBounds = juce::Rectangle<float>(leftX, bottomY, bounds.getWidth(), cellHeight);
    renderHealthGrid(g, healthGridBounds);
}

void SessionDashboardComponent::resized()
{
    // Components are rendered in paint(), so no child components to resize
}

void SessionDashboardComponent::timerCallback()
{
    // Update at 60Hz
    updateFFTAnalysis();
    updateKeyDetection();
    updateHealthMetrics();
    updateDiatoniHealthPulse();
    
    repaint();
}

void SessionDashboardComponent::themeChanged()
{
    repaint();
}

// ===== Material Rendering =====

void SessionDashboardComponent::drawPebbiedMatte(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    auto& themeManager = ThemeManager::get();
    const auto& theme = themeManager.theme();
    // Fill with windowBackground (surface0)
    g.setColour(theme.windowBackground);
    g.fillRect(bounds);

    // Apply subtle high-frequency noise (3% opacity) using a simple pattern
    // For efficiency, we'll use a gradient-based noise pattern instead of full pixel manipulation
    for (int y = 0; y < static_cast<int>(bounds.getHeight()); y += 4)
    {
        for (int x = 0; x < static_cast<int>(bounds.getWidth()); x += 4)
        {
            const auto noiseVal = getNoiseValue(x * 0.01f, y * 0.013f);
            g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(noiseVal * 0.03f));
            g.fillRect(bounds.getX() + x, bounds.getY() + y, 2.0f, 2.0f);
        }
    }
}

void SessionDashboardComponent::drawUnderdogGlassPanel(juce::Graphics& g, 
                                                       const juce::Rectangle<float>& bounds,
                                                       juce::Colour panelColour,
                                                       bool drawBorder)
{
    auto& themeManager = ThemeManager::get();
    const auto& theme = themeManager.theme();

    // Fill panel with 0.3 alpha
    g.setColour(panelColour.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds, 4.0f);

    // Draw thin 0.5px border with primaryAccent (low alpha)
    if (drawBorder)
    {
        g.setColour(themeManager.getColor(ThemeManager::ThemeKey::primaryAccent).withAlpha(0.2f));
        g.drawRoundedRectangle(bounds, 4.0f, 0.5f);
    }

    // Inner glow: subtle highlight on top-left
    juce::ColourGradient innerGlow(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.1f),
                                   bounds.getTopLeft(),
                                   juce::Colours::transparentWhite,
                                   bounds.getCentre(),
                                   false);
    g.setGradientFill(innerGlow);
    g.fillRoundedRectangle(bounds.reduced(0.5f), 3.5f);
}

void SessionDashboardComponent::drawFuzzyInset(juce::Graphics& g,
                                               const juce::Rectangle<float>& bounds,
                                               juce::Colour textColour,
                                               const juce::String& text,
                                               float fontSize)
{
    auto& themeManager = ThemeManager::get();
    const auto& theme = themeManager.theme();
    
    // Fill with surfaceVariant (surface3)
    g.setColour(theme.surface3);
    g.fillRoundedRectangle(bounds, 3.0f);

    // Inner shadow (top/left) - simulates recessed depth
    juce::ColourGradient innerShadow(themeManager.getColor(ThemeManager::ThemeKey::inkDark).withAlpha(0.2f),
                                     bounds.getTopLeft(),
                                     juce::Colours::transparentBlack,
                                     bounds.getBottomRight(),
                                     true);
    g.setGradientFill(innerShadow);
    g.fillRoundedRectangle(bounds, 2.5f);

    // Draw text
    g.setColour(textColour);
    g.setFont(juce::FontOptions(fontSize).withStyle("Bold"));
    g.drawFittedText(text, bounds.toNearestInt().reduced(4), juce::Justification::centred, 1);
}

void SessionDashboardComponent::drawOrganicGlow(juce::Graphics& g,
                                               const juce::Point<float>& centre,
                                               float radius,
                                               juce::Colour glowColour,
                                               bool isActive,
                                               float pulseAmount)
{
    if (!isActive)
    {
        // Draw inactive state: darker version
        g.setColour(glowColour.withAlpha(0.4f));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        return;
    }

    // Active state with pulsing glow
    const auto effectiveRadius = radius + (pulseAmount * 3.0f);  // Pulse 3px max
    
    // Outer glow with bleed (5-10px beyond bounds)
    const auto glowBleedRadius = radius + 8.0f;
    juce::ColourGradient outerGlow(glowColour.withAlpha(0.6f),
                                   centre,
                                   glowColour.withAlpha(0.0f),
                                   centre.translated(glowBleedRadius, 0),
                                   false);
    g.setGradientFill(outerGlow);
    g.fillEllipse(centre.x - glowBleedRadius, centre.y - glowBleedRadius,
                  glowBleedRadius * 2.0f, glowBleedRadius * 2.0f);

    // Core light
    g.setColour(glowColour.withAlpha(0.8f));
    g.fillEllipse(centre.x - effectiveRadius, centre.y - effectiveRadius,
                 effectiveRadius * 2.0f, effectiveRadius * 2.0f);
}

void SessionDashboardComponent::drawToneScopeNeedle(juce::Graphics& g,
                                                    const juce::Point<float>& centre,
                                                    float radius,
                                                    float angleRadians,
                                                    juce::Colour needleColour)
{
    // Calculate needle endpoints
    const auto needleLength = radius * 0.85f;
    const auto endX = centre.x + std::cos(angleRadians - juce::MathConstants<float>::halfPi) * needleLength;
    const auto endY = centre.y + std::sin(angleRadians - juce::MathConstants<float>::halfPi) * needleLength;

    // Draw needle with gradient glow
    juce::Path needlePath;
    needlePath.startNewSubPath(centre);
    needlePath.lineTo(endX, endY);

    // Glow effect
    g.setColour(needleColour.withAlpha(0.4f));
    g.strokePath(needlePath, juce::PathStrokeType(4.0f));

    // Core needle
    g.setColour(needleColour.withAlpha(0.9f));
    g.strokePath(needlePath, juce::PathStrokeType(2.0f));
}

// ===== Sub-Component Rendering =====

void SessionDashboardComponent::renderBpmModule(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    // Draw background panel
    drawUnderdogGlassPanel(g, bounds.reduced(4.0f), ThemeManager::get().theme().surface2);

    // BPM Display (Fuzzy Inset)
    const auto displayBounds = bounds.reduced(8.0f).withHeight(bounds.getHeight() * 0.6f);
    const auto bpmText = juce::String(static_cast<int>(currentBpm)) + " BPM";
    drawFuzzyInset(g, displayBounds, ThemeManager::get().theme().inkLight, bpmText, 24.0f);

    // Tap Button below
    const auto& theme = ThemeManager::get().theme();
    const auto tapButtonBounds = bounds.reduced(8.0f).withY(displayBounds.getBottom() + 4.0f);
    drawOrganicGlow(g, tapButtonBounds.getCentre(), 12.0f, theme.zoneB, true, 0.0f);
    
    g.setColour(theme.inkLight);
    g.setFont(juce::FontOptions(11.0f));
    g.drawFittedText("TAP", tapButtonBounds.toNearestInt(), juce::Justification::centred, 1);
}

void SessionDashboardComponent::renderToneScope(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    // Draw background panel
    drawUnderdogGlassPanel(g, bounds.reduced(4.0f), theme.surface2, true);

    // Inner circle for scope
    const auto scopeBounds = bounds.reduced(12.0f);
    const auto centre = scopeBounds.getCentre();
    const auto radius = scopeBounds.getWidth() / 2.5f;

    // Draw frequency circle background
    g.setColour(theme.surface0.withAlpha(0.4f));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    // Draw concentric rings (frequency bands)
    g.setColour(theme.zoneB.withAlpha(0.15f));
    for (int i = 1; i < 4; ++i)
    {
        const auto ringRadius = radius * (i / 4.0f);
        g.drawEllipse(centre.x - ringRadius, centre.y - ringRadius,
                     ringRadius * 2.0f, ringRadius * 2.0f, 0.5f);
    }

    // Update needle angle based on dominant frequency
    const auto freqRange = 4000.0f;  // 0-4kHz mapped to circle
    const auto normalizedFreq = juce::jlimit(0.0f, 1.0f, dominantFrequency / freqRange);
    scopeNeedleTargetAngle = normalizedFreq * juce::MathConstants<float>::twoPi;
    
    // Smooth needle animation
    const auto angleDiff = scopeNeedleTargetAngle - scopeNeedleAngle;
    scopeNeedleAngle += angleDiff * 0.15f;  // Smooth easing

    // Draw needle
    drawToneScopeNeedle(g, centre, radius, scopeNeedleAngle, theme.zoneB);

    // Frequency label
    g.setColour(theme.inkMuted);
    g.setFont(juce::FontOptions(10.0f));
    g.drawFittedText(juce::String(static_cast<int>(dominantFrequency)) + " Hz",
                    bounds.reduced(8.0f).withHeight(16.0f).withY(bounds.getBottom() - 20.0f).toNearestInt(),
                    juce::Justification::centred, 1);
}

void SessionDashboardComponent::renderKeyIntelligence(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    // Draw background panel
    drawUnderdogGlassPanel(g, bounds.reduced(4.0f), theme.surface2, true);

    const auto innerBounds = bounds.reduced(8.0f);
    const auto halfHeight = innerBounds.getHeight() / 2.0f;

    // Target Key display (top half)
    auto targetBounds = innerBounds.withHeight(halfHeight - 2.0f);
    g.setColour(theme.inkMuted);
    g.setFont(juce::FontOptions(9.0f));
    g.drawFittedText("TARGET", targetBounds.reduced(2.0f).withHeight(10.0f).toNearestInt(),
                    juce::Justification::centred, 1);

    // Get target key from song model
    const auto sessionKey = songModel.getSessionKey();
    drawFuzzyInset(g, targetBounds.reduced(2.0f).withY(targetBounds.getY() + 12.0f),
                  theme.inkLight, sessionKey, 16.0f);

    // Detected Key display (bottom half) with mismatch indicator
    const auto detectedBounds = innerBounds.withY(innerBounds.getY() + halfHeight + 2.0f).withHeight(halfHeight - 2.0f);
    g.setColour(theme.inkMuted);
    g.drawFittedText("DETECTED", detectedBounds.reduced(2.0f).withHeight(10.0f).toNearestInt(),
                    juce::Justification::centred, 1);

    const auto mismatchColour = keyMismatch ? theme.signalMidi : theme.zoneC;
    drawFuzzyInset(g, detectedBounds.reduced(2.0f).withY(detectedBounds.getY() + 12.0f),
                  mismatchColour, detectedKey, 16.0f);

    // Mismatch indicator light
    if (keyMismatch)
    {
        const auto indicatorPos = detectedBounds.getBottomRight().translated(-8.0f, -8.0f);
        drawOrganicGlow(g, indicatorPos, 3.0f, theme.signalMidi, true, 0.5f);
    }
}

void SessionDashboardComponent::renderHealthGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    // 4 equal-width indicator lights
    const auto cellWidth = (bounds.getWidth() - 12.0f) / 4.0f;
    auto cellHeight = bounds.getHeight() - 8.0f;
    
    struct HealthIndicator
    {
        const char* label;
        float value;
        bool isPulsing;
        juce::Colour colour;
    };

    const std::array<HealthIndicator, 4> indicators { {
        { "Diatonic", currentMetrics.diatonicHealth, shouldPulseDiatonicHealth, theme.zoneC },
        { "Creative", currentMetrics.creativeVelocity, false, theme.zoneB },
        { "CPU", currentMetrics.cpuLoad, currentMetrics.cpuLoad > 0.8f, theme.signalAudio },
        { "RAM", currentMetrics.ramUsage, false, theme.signalMidi }
    } };

    for (size_t i = 0; i < indicators.size(); ++i)
    {
        const auto x = bounds.getX() + (i * (cellWidth + 4.0f));
        auto cellBounds = juce::Rectangle<float>(x, bounds.getY() + 4.0f, cellWidth, cellHeight);

        const auto& indicator = indicators[i];
        
        // Draw panel background
        drawUnderdogGlassPanel(g, cellBounds, theme.surface2, false);

        // Draw indicator light at top
        const auto lightBoundsRect = cellBounds.removeFromTop(cellHeight * 0.6f).reduced(8.0f);
        const auto pulseAmount = indicator.isPulsing ? diatonicHealthPulseAmount : 0.0f;
        drawOrganicGlow(g, lightBoundsRect.getCentre(), lightBoundsRect.getWidth() / 2.5f,
                       indicator.colour, indicator.value > 0.1f, pulseAmount);

        // Draw label at bottom
        g.setColour(theme.inkMuted);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawFittedText(indicator.label, cellBounds.toNearestInt(),
                        juce::Justification::centred, 1);

        // Draw percentage
        g.setColour(theme.inkLight);
        g.setFont(juce::FontOptions(8.0f));
        const auto percentText = juce::String(static_cast<int>(indicator.value * 100.0f)) + "%";
        auto percentBounds = cellBounds.removeFromBottom(12.0f);
        g.drawFittedText(percentText, percentBounds.toNearestInt(),
                        juce::Justification::centred, 1);
    }
}

// ===== Logic Methods =====

void SessionDashboardComponent::updateFFTAnalysis()
{
    if (!audioBuffer.hasAudio())
        return;

    // Get last beat of audio
    const auto fb = audioBuffer.getLastNBeats(currentBpm, 1);
    
    if (fb.getNumSamples() == 0)
        return;

    lastSampleRate = audioBuffer.getSampleRate();
    performFFTAnalysis(fb);
}

void SessionDashboardComponent::updateKeyDetection()
{
    static constexpr std::array<const char*, 12> pitchClassNames { "C", "C#", "D", "Eb", "E", "F",
                                                                    "F#", "G", "Ab", "A", "Bb", "B" };

    const auto activeNotes = collectActiveMidiNotesAtPlayhead();
    if (activeNotes.empty())
    {
        detectedKey = songModel.getSessionKey();
        keyMismatch = false;
        return;
    }

    std::array<int, 12> pcHistogram {};
    for (const auto midiNote : activeNotes)
        ++pcHistogram[static_cast<size_t>(((midiNote % 12) + 12) % 12)];

    int dominantPc = 0;
    int dominantCount = 0;
    for (int pc = 0; pc < 12; ++pc)
    {
        if (pcHistogram[static_cast<size_t>(pc)] > dominantCount)
        {
            dominantCount = pcHistogram[static_cast<size_t>(pc)];
            dominantPc = pc;
        }
    }

    detectedKey = pitchClassNames[static_cast<size_t>(dominantPc)];
    keyMismatch = !detectedKey.equalsIgnoreCase(songModel.getSessionKey());
}

void SessionDashboardComponent::updateHealthMetrics()
{
    const auto activeNotes = collectActiveMidiNotesAtPlayhead();
    const auto tonicPc = setle::theory::DiatonicHarmony::pitchClassForRoot(songModel.getSessionKey());
    const auto scale = setle::theory::DiatonicHarmony::modeIntervals(songModel.getSessionMode());

    float diatonicTarget = 1.0f;
    if (!activeNotes.empty() && !scale.empty())
    {
        int inKey = 0;
        for (const auto midiNote : activeNotes)
        {
            const auto relativePc = ((midiNote % 12) - tonicPc + 12) % 12;
            if (std::find(scale.begin(), scale.end(), relativePc) != scale.end())
                ++inKey;
        }

        diatonicTarget = static_cast<float>(inKey) / static_cast<float>(activeNotes.size());
    }

    const auto callbackPeakNow = callbackPeak.exchange(0.0f, std::memory_order_relaxed);
    const auto callbackSamplesNow = callbackSampleCount.exchange(0, std::memory_order_relaxed);
    const auto noteDensity = juce::jlimit(0.0f, 1.0f, static_cast<float>(activeNotes.size()) / 12.0f);
    const auto callbackActivity = callbackSamplesNow > 0
                                      ? juce::jlimit(0.0f, 1.0f, callbackPeakNow * 1.8f)
                                      : 0.0f;
    const auto creativeTarget = juce::jlimit(0.0f, 1.0f, noteDensity * 0.6f + callbackActivity * 0.4f);

    currentMetrics.diatonicHealth = juce::jlimit(0.0f, 1.0f,
                                                 currentMetrics.diatonicHealth * 0.65f
                                                     + diatonicTarget * 0.35f);
    currentMetrics.creativeVelocity = juce::jlimit(0.0f, 1.0f,
                                                   currentMetrics.creativeVelocity * 0.72f
                                                       + creativeTarget * 0.28f);
    currentMetrics.cpuLoad = juce::jlimit(0.0f, 1.0f, engine.getDeviceManager().getCpuUsage());
    currentMetrics.ramUsage = juce::jlimit(0.0f, 1.0f,
                                           currentMetrics.ramUsage * 0.7f + querySystemRamUsageRatio() * 0.3f);

    currentBpm = songModel.getBpm();
    shouldPulseDiatonicHealth = keyMismatch || currentMetrics.diatonicHealth < 0.75f;
}

void SessionDashboardComponent::updateDiatoniHealthPulse()
{
    if (shouldPulseDiatonicHealth)
    {
        diatonicHealthPulsePhase += 0.06f;  // ~2 Hz pulse at 60Hz refresh
        if (diatonicHealthPulsePhase > juce::MathConstants<float>::twoPi)
            diatonicHealthPulsePhase -= juce::MathConstants<float>::twoPi;

        diatonicHealthPulseAmount = (std::sin(diatonicHealthPulsePhase) + 1.0f) * 0.5f;
    }
    else
    {
        diatonicHealthPulseAmount = 0.0f;
    }
}

// ===== Audio FFT Analysis =====

void SessionDashboardComponent::performFFTAnalysis(const juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = juce::jmin(buffer.getNumSamples(), kFFTSize);
    const auto* audioData = buffer.getReadPointer(0);  // Use left channel

    // Copy audio to FFT buffer (zero-pad if needed)
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    std::copy_n(audioData, numSamples, fftData.begin());

    // Apply Hann window
    for (int i = 0; i < numSamples; ++i)
    {
        const auto w = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * i / (numSamples - 1)));
        fftData[i] *= w;
    }

    // Perform FFT
    fftProcessor.performRealOnlyForwardTransform(fftData.data());

    // Calculate magnitudes and find dominant frequency
    dominantMagnitude = 0.0f;
    int dominantBin = 0;

    for (int i = 0; i < kFFTSize / 2; ++i)
    {
        const auto re = fftData[i * 2];
        const auto im = fftData[i * 2 + 1];
        const auto magnitude = std::sqrt(re * re + im * im);
        fftMagnitudes[i] = magnitude;

        if (magnitude > dominantMagnitude)
        {
            dominantMagnitude = magnitude;
            dominantBin = i;
        }
    }

    dominantFrequency = getFrequencyForBin(dominantBin);
}

float SessionDashboardComponent::getFrequencyForBin(int bin) const noexcept
{
    return (bin * static_cast<float>(lastSampleRate)) / static_cast<float>(kFFTSize);
}

std::vector<int> SessionDashboardComponent::collectActiveMidiNotesAtPlayhead() const
{
    std::vector<int> notes;
    if (edit == nullptr)
        return notes;

    const auto playheadBeat = getCurrentPlayheadBeat();

    for (auto* track : te::getAudioTracks(*edit))
    {
        if (track == nullptr)
            continue;

        for (auto* clipBase : track->getClips())
        {
            auto* midiClip = dynamic_cast<te::MidiClip*>(clipBase);
            if (midiClip == nullptr)
                continue;

            const auto clipStartBeat = edit->tempoSequence.toBeats(midiClip->getPosition().getStart()).inBeats();
            for (auto* note : midiClip->getSequence().getNotes())
            {
                if (note == nullptr)
                    continue;

                const auto noteStart = clipStartBeat + note->getStartBeat().inBeats();
                const auto noteEnd = noteStart + note->getLengthBeats().inBeats();
                if (playheadBeat >= noteStart - 1.0e-4 && playheadBeat < noteEnd + 1.0e-4)
                    notes.push_back(note->getNoteNumber());
            }
        }
    }

    return notes;
}

double SessionDashboardComponent::getCurrentPlayheadBeat() const
{
    if (edit == nullptr)
        return 0.0;

    const auto* tempo = edit->tempoSequence.getTempo(0);
    const auto bpm = tempo != nullptr ? tempo->getBpm() : juce::jmax(1.0, songModel.getBpm());
    const auto secPerBeat = 60.0 / juce::jmax(1.0, bpm);
    return edit->getTransport().getPosition().inSeconds() / secPerBeat;
}

float SessionDashboardComponent::querySystemRamUsageRatio() const
{
#if JUCE_WINDOWS
    MEMORYSTATUSEX memoryStatus {};
    memoryStatus.dwLength = sizeof(memoryStatus);
    if (::GlobalMemoryStatusEx(&memoryStatus) == FALSE || memoryStatus.ullTotalPhys == 0)
        return currentMetrics.ramUsage;

    const auto usedBytes = memoryStatus.ullTotalPhys - memoryStatus.ullAvailPhys;
    return static_cast<float>(static_cast<double>(usedBytes) / static_cast<double>(memoryStatus.ullTotalPhys));
#else
    return currentMetrics.ramUsage;
#endif
}

// ===== Utility =====

juce::Colour SessionDashboardComponent::getThemeColour(const juce::String& colourName) const
{
    const auto& theme = ThemeManager::get().theme();

    if (colourName == "windowBackground") return theme.surface0;
    if (colourName == "surfaceLow") return theme.surface1;
    if (colourName == "surfaceVariant") return theme.surface3;
    if (colourName == "primaryAccent") return theme.zoneB;
    if (colourName == "secondaryAccent") return theme.zoneC;

    return theme.surface1;
}

float SessionDashboardComponent::getNoiseValue(float x, float y) const noexcept
{
    // Simple Perlin-like noise approximation using sine
    const auto sx = std::sin(x * 12.9898f) * 43758.5453f;
    const auto sy = std::sin(y * 78.233f) * 43758.5453f;
    const auto val = std::fmod(sx + sy, 1.0f);
    return val < 0.0f ? val + 1.0f : val;
}

} // namespace setle::ui
