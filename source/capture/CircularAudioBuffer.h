#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace setle::capture
{

/**
 * Lock-aware stereo ring buffer for continuously capturing recent master output.
 * Written from the audio callback, read from the message thread.
 */
class CircularAudioBuffer
{
public:
    explicit CircularAudioBuffer(double maxLengthSeconds = 240.0);

    void setMaxLengthSeconds(double seconds);
    double getMaxLengthSeconds() const noexcept;

    void prepare(double sampleRateHz, int numOutputChannels);
    void pushAudioBlock(const float* const* outputChannelData,
                        int numOutputChannels,
                        int numSamples) noexcept;

    juce::AudioBuffer<float> getLastSeconds(double seconds) const;
    juce::AudioBuffer<float> getLastNBeats(double bpm, int beats) const;

    double getSampleRate() const noexcept;
    int getValidSamples() const noexcept;
    bool hasAudio() const noexcept;

private:
    void rebuildBufferForCurrentRate(double sampleRateHz);

    mutable juce::SpinLock lock;
    juce::AudioBuffer<float> ringBuffer;
    int writePosition { 0 };
    int validSamples { 0 };
    double sampleRate { 44100.0 };
    double maxLengthSeconds { 240.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularAudioBuffer)
};

} // namespace setle::capture

