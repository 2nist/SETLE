#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_core/juce_core.h>

#include <memory>
#include <vector>

namespace setle::capture
{

class HistoryBuffer : private juce::MidiInputCallback
{
public:
    explicit HistoryBuffer(int maxBeats = 64);
    ~HistoryBuffer() override;

    // Source management
    void setSource(const juce::String& deviceIdentifier);   // "" = Off
    void setAllSources(bool enabled);
    void clearSource();
    juce::String getCurrentSourceIdentifier() const;

    // Buffer access
    struct CapturedEvent {
        juce::MidiMessage message;
        double wallClockSeconds;   // time since capture started
    };

    std::vector<CapturedEvent> getLastNBeats(double bpm, int beats) const;
    std::vector<CapturedEvent> getAll() const;
    void clear();

    // Buffer configuration
    void setMaxBeats(int beats);
    int getMaxBeats() const noexcept;

    bool isCapturing() const noexcept;

private:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage&) override;
    void trimToMaxLength(double bpm);

    mutable juce::CriticalSection lock;
    std::vector<CapturedEvent> events;
    std::vector<std::unique_ptr<juce::MidiInput>> openInputs;
    double captureStartTime { 0.0 };
    int maxBeats { 64 };
    bool allSources { false };
    juce::String currentDeviceId;
};

} // namespace setle::capture
