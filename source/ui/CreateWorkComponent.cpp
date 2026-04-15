#include "CreateWorkComponent.h"

#include "../theme/ThemeStyleHelpers.h"
#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <algorithm>
#include <set>

namespace setle::ui
{

namespace
{
double eventBeat(const setle::capture::HistoryBuffer::CapturedEvent& event, double bpm)
{
    return (event.wallClockSeconds * juce::jmax(1.0, bpm)) / 60.0;
}

std::vector<setle::capture::HistoryBuffer::CapturedEvent> extractTrimmedMidiEvents(
    const std::vector<setle::capture::HistoryBuffer::CapturedEvent>& events,
    double bpm,
    double trimStartBeat,
    double trimEndBeat)
{
    std::vector<setle::capture::HistoryBuffer::CapturedEvent> result;
    result.reserve(events.size());

    for (const auto& event : events)
    {
        const auto beat = eventBeat(event, bpm);
        if (beat >= trimStartBeat && beat <= trimEndBeat)
            result.push_back(event);
    }
    return result;
}

juce::AudioBuffer<float> extractTrimmedAudioBuffer(const juce::AudioBuffer<float>& source,
                                                   double bpm,
                                                   double sampleRate,
                                                   double trimStartBeat,
                                                   double trimEndBeat)
{
    if (source.getNumSamples() <= 0 || sampleRate <= 0.0 || trimEndBeat <= trimStartBeat)
        return {};

    const auto startSample = juce::jlimit(0,
                                          source.getNumSamples(),
                                          static_cast<int>(std::floor((trimStartBeat / juce::jmax(1.0, bpm)) * 60.0 * sampleRate)));
    const auto endSample = juce::jlimit(startSample,
                                        source.getNumSamples(),
                                        static_cast<int>(std::ceil((trimEndBeat / juce::jmax(1.0, bpm)) * 60.0 * sampleRate)));
    const auto numSamples = juce::jmax(0, endSample - startSample);
    if (numSamples <= 0)
        return {};

    juce::AudioBuffer<float> trimmed(source.getNumChannels(), numSamples);
    for (int ch = 0; ch < source.getNumChannels(); ++ch)
        trimmed.copyFrom(ch, 0, source, ch, startSample, numSamples);
    return trimmed;
}

juce::File writeTempWav(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    if (audio.getNumSamples() <= 0 || sampleRate <= 0.0)
        return {};

    auto cacheDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("setle-cache");
    if (!cacheDir.exists())
        cacheDir.createDirectory();

    auto wavFile = cacheDir.getNonexistentChildFile("create-commit-", ".wav", false);
    auto stream = std::unique_ptr<juce::FileOutputStream>(wavFile.createOutputStream());
    if (stream == nullptr)
        return {};

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(stream.get(),
                                  sampleRate,
                                  static_cast<unsigned int>(juce::jmax(1, audio.getNumChannels())),
                                  24,
                                  {},
                                  0));
    if (writer == nullptr)
        return {};

    stream.release();
    if (!writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()))
        return {};

    return wavFile;
}

std::vector<setle::model::Note> toModelNotes(const std::vector<setle::capture::HistoryBuffer::CapturedEvent>& events,
                                             double bpm,
                                             double trimStartBeat,
                                             double trimEndBeat)
{
    std::vector<setle::model::Note> notes;
    notes.reserve(events.size());

    for (const auto& event : events)
    {
        if (!event.message.isNoteOn())
            continue;

        const auto absoluteBeat = eventBeat(event, bpm);
        const auto startBeat = juce::jlimit(0.0, juce::jmax(0.0, trimEndBeat - trimStartBeat), absoluteBeat - trimStartBeat);
        const auto velocity = juce::jlimit(0.0f, 1.0f, event.message.getFloatVelocity());
        notes.push_back(setle::model::Note::create(event.message.getNoteNumber(),
                                                   velocity,
                                                   startBeat,
                                                   0.5,
                                                   event.message.getChannel()));
    }

    return notes;
}
} // namespace

CreateWorkComponent::CreateWorkComponent(te::Engine& engine_,
                                         setle::model::Song& songModel_,
                                         setle::capture::CircularAudioBuffer& audioBuffer_,
                                         setle::capture::GrabSamplerQueue& grabQueue_)
    : engine(engine_), songModel(songModel_), audioBuffer(audioBuffer_), grabQueue(grabQueue_)
{
    ThemeManager::get().addListener(this);
    startTimerHz(60);  // 60Hz update loop for pulse animation
    buildScenePresets();
}

CreateWorkComponent::~CreateWorkComponent()
{
    stopTimer();
    ThemeManager::get().removeListener(this);
}

void CreateWorkComponent::paint(juce::Graphics& g)
{
    // Draw pebbled matte background (chassis)
    drawPebbiedMatte(g, getLocalBounds().toFloat());

    const auto bounds = getLocalBounds().toFloat();
    
    // Layout: [Sidebar | Main Examination + Committal]
    const auto sidebarWidth = 120.0f;
    const auto footerHeight = 80.0f;
    const auto separatorThickness = 1.0f;

    // Scene sidebar (left)
    const auto sidebarBounds = bounds.removeFromLeft(sidebarWidth);
    renderSceneSidebar(g, sidebarBounds);

    // Separator line
    g.setColour(ThemeManager::get().theme().surface1.withAlpha(0.3f));
    g.fillRect(bounds.removeFromLeft(separatorThickness));

    // Examination window + committal bridge
    auto contentBounds = bounds.reduced(8.0f);
    const auto examinationBounds = contentBounds.removeFromTop(contentBounds.getHeight() - footerHeight - 8.0f);
    const auto committalBounds = contentBounds.removeFromBottom(footerHeight);

    // Main examination window
    renderExaminationWindow(g, examinationBounds);

    // Committal bridge (three buttons)
    renderCommittalBridge(g, committalBounds);
}

void CreateWorkComponent::resized()
{
    // Components are rendered in paint(), no child components to resize
}

void CreateWorkComponent::timerCallback()
{
    // Update committal button pulse
    if (isReadyToCommit)
    {
        commitPulsePhase += 0.08f;  // ~2.5Hz pulse at 60Hz refresh
        if (commitPulsePhase > juce::MathConstants<float>::twoPi)
            commitPulsePhase -= juce::MathConstants<float>::twoPi;
        
        commitPulseAmount = (std::sin(commitPulsePhase) + 1.0f) * 0.5f;
    }
    else
    {
        commitPulseAmount = 0.0f;
    }
    
    repaint();
}

void CreateWorkComponent::themeChanged()
{
    repaint();
}

void CreateWorkComponent::onNewCapture(const juce::AudioBuffer<float>& audioData,
                                       const std::vector<setle::capture::HistoryBuffer::CapturedEvent>& midiEvents)
{
    // Copy audio data
    capturedAudio = std::make_shared<juce::AudioBuffer<float>>(audioData);
    capturedMidiEvents = midiEvents;
    
    // Reset trim bounds
    const auto bpm = juce::jmax(1.0, songModel.getBpm());
    captureTotalBeats = (capturedAudio->getNumSamples() / audioBuffer.getSampleRate()) * (bpm / 60.0);
    trimStartBeat = 0.0;
    trimEndBeat = juce::jmin(16.0, captureTotalBeats);  // Default 16 beat trim window
    
    // Signal ready to commit
    isReadyToCommit = true;
    commitPulsePhase = 0.0f;
}

// ===== Material Rendering Methods =====

void CreateWorkComponent::drawPebbiedMatte(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    g.setColour(theme.surface0);
    g.fillRect(bounds);

    // Subtle high-frequency noise (3% opacity)
    for (int y = 0; y < static_cast<int>(bounds.getHeight()); y += 4)
    {
        for (int x = 0; x < static_cast<int>(bounds.getWidth()); x += 4)
        {
            const auto noiseVal = getNoiseValue(x * 0.01f, y * 0.013f);
            g.setColour(juce::Colours::white.withAlpha(noiseVal * 0.03f));
            g.fillRect(bounds.getX() + x, bounds.getY() + y, 2.0f, 2.0f);
        }
    }
}

void CreateWorkComponent::drawUnderdogGlassPanel(juce::Graphics& g,
                                                const juce::Rectangle<float>& bounds,
                                                juce::Colour panelColour,
                                                bool drawBorder)
{
    g.setColour(panelColour.withAlpha(0.3f));
    g.fillRoundedRectangle(bounds, 4.0f);

    if (drawBorder)
    {
        const auto& theme = ThemeManager::get().theme();
        g.setColour(theme.zoneB.withAlpha(0.2f));
        g.drawRoundedRectangle(bounds, 4.0f, 0.5f);
    }

    // Inner glow
    juce::ColourGradient innerGlow(juce::Colours::white.withAlpha(0.1f),
                                   bounds.getTopLeft(),
                                   juce::Colours::transparentWhite,
                                   bounds.getCentre(),
                                   false);
    g.setGradientFill(innerGlow);
    g.fillRoundedRectangle(bounds.reduced(0.5f), 3.5f);
}

void CreateWorkComponent::drawFuzzyInset(juce::Graphics& g,
                                         const juce::Rectangle<float>& bounds,
                                         juce::Colour textColour,
                                         const juce::String& text,
                                         float fontSize)
{
    const auto& theme = ThemeManager::get().theme();
    
    g.setColour(theme.surface3);
    g.fillRoundedRectangle(bounds, 3.0f);

    juce::ColourGradient innerShadow(juce::Colours::black.withAlpha(0.2f),
                                     bounds.getTopLeft(),
                                     juce::Colours::transparentBlack,
                                     bounds.getBottomRight(),
                                     true);
    g.setGradientFill(innerShadow);
    g.fillRoundedRectangle(bounds, 2.5f);

    g.setColour(textColour);
    g.setFont(juce::FontOptions(fontSize).withStyle("Bold"));
    g.drawFittedText(text, bounds.toNearestInt().reduced(4), juce::Justification::centred, 1);
}

void CreateWorkComponent::drawOrganicGlow(juce::Graphics& g,
                                         const juce::Point<float>& centre,
                                         float radius,
                                         juce::Colour glowColour,
                                         bool isActive,
                                         float pulseAmount)
{
    if (!isActive)
    {
        g.setColour(glowColour.withAlpha(0.4f));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        return;
    }

    const auto effectiveRadius = radius + (pulseAmount * 3.0f);
    const auto glowBleedRadius = radius + 8.0f;
    
    juce::ColourGradient outerGlow(glowColour.withAlpha(0.6f),
                                   centre,
                                   glowColour.withAlpha(0.0f),
                                   centre.translated(glowBleedRadius, 0),
                                   false);
    g.setGradientFill(outerGlow);
    g.fillEllipse(centre.x - glowBleedRadius, centre.y - glowBleedRadius,
                  glowBleedRadius * 2.0f, glowBleedRadius * 2.0f);

    g.setColour(glowColour.withAlpha(0.8f));
    g.fillEllipse(centre.x - effectiveRadius, centre.y - effectiveRadius,
                 effectiveRadius * 2.0f, effectiveRadius * 2.0f);
}

// ===== Examination Window Components =====

void CreateWorkComponent::renderSceneSidebar(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    // Background
    g.setColour(theme.surface1);
    g.fillRect(bounds);

    // Title
    g.setColour(theme.inkMuted);
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    auto titleBounds = bounds.removeFromTop(24.0f);
    g.drawFittedText("SCENES", titleBounds.toNearestInt().reduced(4), juce::Justification::centred, 1);

    // Scene preset buttons (simplified - stack vertically)
    const auto buttonHeight = 32.0f;
    const auto buttonGap = 4.0f;
    
    for (size_t i = 0; i < scenePresets.size() && i < 4; ++i)
    {
        auto buttonBounds = bounds.removeFromTop(buttonHeight + buttonGap).reduced(2.0f);
        
        const auto isSelected = static_cast<int>(i) == selectedPresetIndex;
        const auto buttonColour = isSelected ? theme.zoneB : theme.surface2;
        
        drawFuzzyInset(g, buttonBounds, theme.inkLight, scenePresets[i].name, 9.0f);
    }
}

void CreateWorkComponent::renderExaminationWindow(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    // Main panel with underdog glass effect
    drawUnderdogGlassPanel(g, bounds.reduced(2.0f), theme.surface2, true);

    if (!capturedAudio || capturedAudio->getNumSamples() == 0)
    {
        // Empty state
        g.setColour(theme.inkMuted);
        g.setFont(juce::FontOptions(16.0f));
        g.drawFittedText("No capture loaded. Try triggering a 'Grab' from the nav bar.",
                        bounds.toNearestInt().reduced(12),
                        juce::Justification::centred, 2);
        return;
    }

    // Split the examination window into top (MIDI) and bottom (Audio)
    const auto innerBounds = bounds.reduced(8.0f);
    const auto midpoint = innerBounds.getHeight() / 2.0f;

    auto midiStripBounds = innerBounds.withHeight(midpoint - 2.0f);
    auto audioStripBounds = innerBounds.withY(innerBounds.getY() + midpoint + 2.0f)
                                      .withHeight(midpoint - 2.0f);

    // MIDI History Strip (top)
    renderMidiHistoryStrip(g, midiStripBounds);

    // Audio History Strip (bottom)
    renderAudioHistoryStrip(g, audioStripBounds);

    // Trim handles
    renderTrimHandles(g, bounds);
}

void CreateWorkComponent::renderMidiHistoryStrip(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    // Background
    g.setColour(theme.surface0.withAlpha(0.5f));
    g.fillRect(bounds);

    // Grid lines (beats)
    const auto beatsPerDivision = 4.0f;
    const auto pixelsPerBeat = bounds.getWidth() / (trimEndBeat - trimStartBeat);
    
    g.setColour(theme.surface2.withAlpha(0.2f));
    for (double beat = std::ceil(trimStartBeat / beatsPerDivision) * beatsPerDivision;
         beat <= trimEndBeat; beat += beatsPerDivision)
    {
        const auto x = bounds.getX() + (beat - trimStartBeat) * pixelsPerBeat;
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Draw ghosted MIDI notes from capturedMidiEvents
    g.setColour(theme.zoneC.withAlpha(0.4f));
    g.setFont(juce::FontOptions(8.0f));
    
    const auto bpm = songModel.getBpm();
    for (const auto& event : capturedMidiEvents)
    {
        if (!event.message.isNoteOn())
            continue;

        // Convert event time to beats
        const auto eventBeats = eventBeat(event, bpm);
        
        if (eventBeats < trimStartBeat || eventBeats > trimEndBeat)
            continue;

        const auto xPos = bounds.getX() + (eventBeats - trimStartBeat) * pixelsPerBeat;
        const auto midiNote = event.message.getNoteNumber();
        
        // Normalize pitch to y position (assuming 48-84 MIDI range spans the height)
        const auto noteRange = 84.0f - 48.0f;
        const auto normalizedNote = static_cast<float>(midiNote - 48) / noteRange;
        const auto yPos = bounds.getBottom() - (normalizedNote * bounds.getHeight());
        
        // Draw a small circle for each note
        g.fillEllipse(xPos - 3.0f, yPos - 3.0f, 6.0f, 6.0f);
    }

    // Label
    g.setColour(theme.inkMuted);
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.drawFittedText("MIDI", bounds.removeFromTop(14.0f).toNearestInt().reduced(4),
                    juce::Justification::topLeft, 1);
}

void CreateWorkComponent::renderAudioHistoryStrip(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    // Background
    g.setColour(theme.surface0.withAlpha(0.5f));
    g.fillRect(bounds);

    // Grid lines (beats)
    const auto beatsPerDivision = 4.0f;
    const auto pixelsPerBeat = bounds.getWidth() / (trimEndBeat - trimStartBeat);
    
    g.setColour(theme.surface2.withAlpha(0.2f));
    for (double beat = std::ceil(trimStartBeat / beatsPerDivision) * beatsPerDivision;
         beat <= trimEndBeat; beat += beatsPerDivision)
    {
        const auto x = bounds.getX() + (beat - trimStartBeat) * pixelsPerBeat;
        g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());
    }

    // Draw waveform
    if (capturedAudio && capturedAudio->getNumSamples() > 0)
    {
        const auto* audioData = capturedAudio->getReadPointer(0);
        const auto sampleRate = audioBuffer.getSampleRate();
        const auto bpm = songModel.getBpm();
        
        const auto startSample = static_cast<int>((trimStartBeat / bpm) * 60.0 * sampleRate);
        const auto endSample = static_cast<int>((trimEndBeat / bpm) * 60.0 * sampleRate);
        const auto numSamplesToDraw = juce::jmin(endSample - startSample, 
                                                 capturedAudio->getNumSamples() - startSample);

        const auto centerY = bounds.getCentreY();
        const auto halfHeight = bounds.getHeight() / 2.0f;
        
        g.setColour(theme.zoneB.withAlpha(0.7f));
        
        // Draw waveform peaks (simple peak detection per pixel)
        const auto pixelsToRender = bounds.getWidth();
        for (int px = 0; px < static_cast<int>(pixelsToRender); ++px)
        {
            const auto sampleIndex = startSample + (px * numSamplesToDraw) / static_cast<int>(pixelsToRender);
            
            if (sampleIndex >= 0 && sampleIndex < capturedAudio->getNumSamples())
            {
                const auto sample = audioData[sampleIndex];
                const auto peakHeight = std::abs(sample) * halfHeight;
                
                g.drawVerticalLine(static_cast<int>(bounds.getX() + px),
                                  static_cast<int>(centerY - peakHeight),
                                  static_cast<int>(centerY + peakHeight));
            }
        }
    }

    // Label
    g.setColour(theme.inkMuted);
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.drawFittedText("AUDIO", bounds.removeFromTop(14.0f).toNearestInt().reduced(4),
                    juce::Justification::topLeft, 1);
}

void CreateWorkComponent::renderTrimHandles(juce::Graphics& g, const juce::Rectangle<float>& panelBounds)
{
    const auto& theme = ThemeManager::get().theme();
    const auto innerBounds = panelBounds.reduced(8.0f);
    const auto totalBeats = trimEndBeat - trimStartBeat;
    const auto pixelsPerBeat = innerBounds.getWidth() / totalBeats;

    // Start handle (left)
    const auto startHandleX = innerBounds.getX() + (0.0f * pixelsPerBeat);
    const auto handleWidth = 8.0f;
    const auto handleHeight = 12.0f;
    
    auto startHandleBounds = juce::Rectangle<float>(startHandleX - handleWidth / 2.0f,
                                                    panelBounds.getCentreY() - handleHeight / 2.0f,
                                                    handleWidth, handleHeight);
    
    g.setColour(theme.zoneB.withAlpha(0.7f));
    g.fillRect(startHandleBounds);
    g.setColour(theme.zoneB);
    g.drawRect(startHandleBounds, 1.0f);

    // End handle (right)
    const auto endHandleX = innerBounds.getX() + (totalBeats * pixelsPerBeat);
    auto endHandleBounds = juce::Rectangle<float>(endHandleX - handleWidth / 2.0f,
                                                  panelBounds.getCentreY() - handleHeight / 2.0f,
                                                  handleWidth, handleHeight);
    
    g.setColour(theme.zoneB.withAlpha(0.7f));
    g.fillRect(endHandleBounds);
    g.setColour(theme.zoneB);
    g.drawRect(endHandleBounds, 1.0f);
}

void CreateWorkComponent::renderCommittalBridge(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    const auto& theme = ThemeManager::get().theme();
    
    // Background
    drawUnderdogGlassPanel(g, bounds.reduced(2.0f), theme.surface2, false);

    const auto innerBounds = bounds.reduced(8.0f);
    const auto buttonWidth = (innerBounds.getWidth() - 16.0f) / 3.0f;
    const auto buttonHeight = innerBounds.getHeight() - 16.0f;

    // Three buttons with organic glow
    struct CommitButton
    {
        const char* label;
        juce::Colour glowColour;
        std::function<void()> onPressed;
    };

    // Button 1: Commit to EDIT (Saffron Glow)
    auto editButtonBounds = innerBounds.removeFromLeft(buttonWidth);
    drawOrganicGlow(g, editButtonBounds.getCentre(), editButtonBounds.getWidth() / 3.0f,
                   theme.zoneB, isReadyToCommit, isReadyToCommit ? commitPulseAmount : 0.0f);
    
    g.setColour(theme.inkLight);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawFittedText("Commit\nto EDIT", editButtonBounds.toNearestInt(),
                    juce::Justification::centred, 2);

    // Spacer
    innerBounds.removeFromLeft(8.0f);

    // Button 2: Commit to SOUND (Turquoise Glow)
    auto soundButtonBounds = innerBounds.removeFromLeft(buttonWidth);
    drawOrganicGlow(g, soundButtonBounds.getCentre(), soundButtonBounds.getWidth() / 3.0f,
                   theme.zoneC, isReadyToCommit, isReadyToCommit ? commitPulseAmount : 0.0f);
    
    g.setColour(theme.inkLight);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawFittedText("Commit\nto SOUND", soundButtonBounds.toNearestInt(),
                    juce::Justification::centred, 2);

    // Spacer
    innerBounds.removeFromLeft(8.0f);

    // Button 3: Commit to ARRANGE (Orange Red Glow)
    auto arrangeButtonBounds = innerBounds;
    drawOrganicGlow(g, arrangeButtonBounds.getCentre(), arrangeButtonBounds.getWidth() / 3.0f,
                   theme.signalMidi, isReadyToCommit, isReadyToCommit ? commitPulseAmount : 0.0f);
    
    g.setColour(theme.inkLight);
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawFittedText("Commit\nto ARRANGE", arrangeButtonBounds.toNearestInt(),
                    juce::Justification::centred, 2);
}

// ===== Committal Logic =====

void CreateWorkComponent::commitToEdit()
{
    const auto bpm = juce::jmax(1.0, songModel.getBpm());
    const auto trimmedEvents = extractTrimmedMidiEvents(capturedMidiEvents, bpm, trimStartBeat, trimEndBeat);
    const auto notes = toModelNotes(trimmedEvents, bpm, trimStartBeat, trimEndBeat);
    if (notes.empty())
        return;

    const auto clipLengthBeats = juce::jmax(0.25, trimEndBeat - trimStartBeat);
    const auto clipName = "Create " + juce::Time::getCurrentTime().toString(true, true, false, true);
    const auto progressionId = songModel.createNewMidiClip(clipName, notes, clipLengthBeats, selectedTrackId);
    if (progressionId.isEmpty())
        return;

    grabQueue.loadProgression(progressionId, clipName, 0.85f);
    if (onGridRefreshRequested)
        onGridRefreshRequested();
}

void CreateWorkComponent::commitToSound()
{
    if (!capturedAudio)
        return;

    const auto bpm = juce::jmax(1.0, songModel.getBpm());
    const auto sampleRate = audioBuffer.getSampleRate();
    auto trimmedAudio = extractTrimmedAudioBuffer(*capturedAudio, bpm, sampleRate, trimStartBeat, trimEndBeat);
    if (trimmedAudio.getNumSamples() <= 0)
        return;

    const auto sampleFile = writeTempWav(trimmedAudio, sampleRate);
    if (sampleFile == juce::File())
        return;

    if (onLoadSampleRequested)
        onLoadSampleRequested(sampleFile);
}

void CreateWorkComponent::commitToArrange()
{
    const auto bpm = juce::jmax(1.0, songModel.getBpm());
    const auto clipLengthBeats = juce::jmax(0.25, trimEndBeat - trimStartBeat);
    const auto playheadBeat = onRequestPlayheadBeat ? onRequestPlayheadBeat() : 0.0;

    const auto trimmedEvents = extractTrimmedMidiEvents(capturedMidiEvents, bpm, trimStartBeat, trimEndBeat);
    const auto notes = toModelNotes(trimmedEvents, bpm, trimStartBeat, trimEndBeat);
    juce::String progressionId;
    if (!notes.empty())
    {
        const auto clipName = "Arrange " + juce::Time::getCurrentTime().toString(true, true, false, true);
        progressionId = songModel.createNewMidiClip(clipName, notes, clipLengthBeats, selectedTrackId);
    }

    juce::File sampleFile;
    if (capturedAudio && capturedAudio->getNumSamples() > 0)
    {
        const auto sampleRate = audioBuffer.getSampleRate();
        auto trimmedAudio = extractTrimmedAudioBuffer(*capturedAudio, bpm, sampleRate, trimStartBeat, trimEndBeat);
        sampleFile = writeTempWav(trimmedAudio, sampleRate);
        if (sampleFile != juce::File() && onLoadSampleRequested)
            onLoadSampleRequested(sampleFile);
    }

    if (progressionId.isNotEmpty())
    {
        auto section = setle::model::Section::create("Stamped " + juce::Time::getCurrentTime().toString(true, true, false, true), 1);
        section.valueTree().setProperty("startBeat", playheadBeat, nullptr);
        section.addProgressionRef(setle::model::SectionProgressionRef::create(progressionId, 0, "createStamp"));
        songModel.addSection(section);
    }

    if (onStampToArrangeRequested)
        onStampToArrangeRequested(progressionId, sampleFile, playheadBeat);

    if (onGridRefreshRequested)
        onGridRefreshRequested();
}

// ===== Utility =====

float CreateWorkComponent::getNoiseValue(float x, float y) const noexcept
{
    const auto sx = std::sin(x * 12.9898f) * 43758.5453f;
    const auto sy = std::sin(y * 78.233f) * 43758.5453f;
    const auto val = std::fmod(sx + sy, 1.0f);
    return val < 0.0f ? val + 1.0f : val;
}

juce::Colour CreateWorkComponent::getThemeColour(const juce::String& colourName) const
{
    const auto& theme = ThemeManager::get().theme();
    
    if (colourName == "windowBackground") return theme.surface0;
    if (colourName == "surfaceLow") return theme.surface1;
    if (colourName == "surfaceVariant") return theme.surface3;
    if (colourName == "primaryAccent") return theme.zoneB;
    if (colourName == "secondaryAccent") return theme.zoneC;
    
    return theme.surface1;
}

// ===== Scene Presets =====

void CreateWorkComponent::buildScenePresets()
{
    scenePresets.clear();
    
    scenePresets.push_back({ "Late Night Piano", "MIDI In", 32, 0.03f });
    scenePresets.push_back({ "Drum Loop", "Instrument In", 16, 0.1f });
    scenePresets.push_back({ "Vocal Capture", "Line In", 24, 0.05f });
    scenePresets.push_back({ "Synth Jam", "USB Audio", 64, 0.08f });
    
    selectedPresetIndex = 0;
}

void CreateWorkComponent::applyScenePreset(const CapturePreset& preset)
{
    // Update global CaptureManager settings
    // This would integrate with a CaptureManager singleton or passed-in reference
    
    // TODO: Integrate with capture settings API
    selectedPresetIndex = -1;
    for (size_t i = 0; i < scenePresets.size(); ++i)
    {
        if (scenePresets[i].name == preset.name)
        {
            selectedPresetIndex = static_cast<int>(i);
            break;
        }
    }
}

// ===== Mouse Input =====

void CreateWorkComponent::mouseDrag(const juce::MouseEvent& e)
{
    // Handle trim handle dragging
    const auto innerBounds = getLocalBounds().toFloat().reduced(8.0f);
    const auto totalBeats = trimEndBeat - trimStartBeat;
    const auto pixelsPerBeat = innerBounds.getWidth() / totalBeats;
    
    const auto startHandleX = innerBounds.getX();
    const auto endHandleX = innerBounds.getX() + (totalBeats * pixelsPerBeat);
    
    // Check if near start handle
    if (std::abs(e.getPosition().x - startHandleX) < 8.0f && trimDragMode == TrimDragMode::None)
    {
        trimDragMode = TrimDragMode::StartHandle;
    }
    // Check if near end handle
    else if (std::abs(e.getPosition().x - endHandleX) < 8.0f && trimDragMode == TrimDragMode::None)
    {
        trimDragMode = TrimDragMode::EndHandle;
    }
    // Otherwise, allow scrolling
    else if (trimDragMode == TrimDragMode::None)
    {
        trimDragMode = TrimDragMode::Scroll;
    }

    // Apply drag
    const auto deltaPixels = e.getDistanceFromDragStartX();
    const auto deltaBeats = deltaPixels / pixelsPerBeat;
    
    if (trimDragMode == TrimDragMode::StartHandle)
    {
        trimStartBeat = juce::jlimit(0.0, trimEndBeat - 0.25, trimStartBeat + deltaBeats);
    }
    else if (trimDragMode == TrimDragMode::EndHandle)
    {
        trimEndBeat = juce::jlimit(trimStartBeat + 0.25, captureTotalBeats, trimEndBeat + deltaBeats);
    }
    else if (trimDragMode == TrimDragMode::Scroll)
    {
        scrollOffsetBeats += deltaBeats;
    }
    
    repaint();
}

void CreateWorkComponent::mouseUp(const juce::MouseEvent&)
{
    trimDragMode = TrimDragMode::None;
}

} // namespace setle::ui
