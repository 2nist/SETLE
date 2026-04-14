#include "CircularAudioBuffer.h"

#include <cmath>

namespace setle::capture
{

CircularAudioBuffer::CircularAudioBuffer(double maxLengthSecondsIn)
    : maxLengthSeconds(juce::jmax(1.0, maxLengthSecondsIn))
{
    rebuildBufferForCurrentRate(sampleRate);
}

void CircularAudioBuffer::setMaxLengthSeconds(double seconds)
{
    const juce::SpinLock::ScopedLockType scopedLock(lock);
    maxLengthSeconds = juce::jmax(1.0, seconds);
    rebuildBufferForCurrentRate(sampleRate);
}

double CircularAudioBuffer::getMaxLengthSeconds() const noexcept
{
    const juce::SpinLock::ScopedLockType scopedLock(lock);
    return maxLengthSeconds;
}

void CircularAudioBuffer::prepare(double sampleRateHz, int)
{
    const juce::SpinLock::ScopedLockType scopedLock(lock);
    const auto newRate = juce::jlimit(8000.0, 384000.0, sampleRateHz);
    if (std::abs(newRate - sampleRate) > 0.5 || ringBuffer.getNumSamples() <= 0)
    {
        sampleRate = newRate;
        rebuildBufferForCurrentRate(sampleRate);
    }
}

void CircularAudioBuffer::pushAudioBlock(const float* const* outputChannelData,
                                         int numOutputChannels,
                                         int numSamples) noexcept
{
    if (outputChannelData == nullptr || numSamples <= 0)
        return;

    const juce::SpinLock::ScopedTryLockType scopedTryLock(lock);
    if (!scopedTryLock.isLocked())
        return;

    const auto capacity = ringBuffer.getNumSamples();
    if (capacity <= 0)
        return;

    const float* left = numOutputChannels > 0 ? outputChannelData[0] : nullptr;
    const float* right = numOutputChannels > 1 ? outputChannelData[1] : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = left != nullptr ? left[i] : 0.0f;
        const float r = right != nullptr ? right[i] : l;

        ringBuffer.setSample(0, writePosition, l);
        ringBuffer.setSample(1, writePosition, r);

        ++writePosition;
        if (writePosition >= capacity)
            writePosition = 0;
    }

    validSamples = juce::jmin(capacity, validSamples + numSamples);
}

juce::AudioBuffer<float> CircularAudioBuffer::getLastSeconds(double seconds) const
{
    const juce::SpinLock::ScopedLockType scopedLock(lock);

    if (ringBuffer.getNumSamples() <= 0 || sampleRate <= 0.0 || validSamples <= 0)
        return {};

    const auto requested = juce::jmax(0, static_cast<int>(std::round(juce::jmax(0.0, seconds) * sampleRate)));
    const auto samplesToCopy = juce::jmin(validSamples, requested);
    if (samplesToCopy <= 0)
        return {};

    juce::AudioBuffer<float> out(2, samplesToCopy);
    out.clear();

    const int capacity = ringBuffer.getNumSamples();
    int readPosition = writePosition - samplesToCopy;
    if (readPosition < 0)
        readPosition += capacity;

    const int firstChunk = juce::jmin(samplesToCopy, capacity - readPosition);
    out.copyFrom(0, 0, ringBuffer, 0, readPosition, firstChunk);
    out.copyFrom(1, 0, ringBuffer, 1, readPosition, firstChunk);

    const int remaining = samplesToCopy - firstChunk;
    if (remaining > 0)
    {
        out.copyFrom(0, firstChunk, ringBuffer, 0, 0, remaining);
        out.copyFrom(1, firstChunk, ringBuffer, 1, 0, remaining);
    }

    return out;
}

juce::AudioBuffer<float> CircularAudioBuffer::getLastNBeats(double bpm, int beats) const
{
    const auto safeBpm = juce::jmax(1.0, bpm);
    const auto seconds = (60.0 / safeBpm) * static_cast<double>(juce::jmax(1, beats));
    return getLastSeconds(seconds);
}

double CircularAudioBuffer::getSampleRate() const noexcept
{
    const juce::SpinLock::ScopedLockType scopedLock(lock);
    return sampleRate;
}

int CircularAudioBuffer::getValidSamples() const noexcept
{
    const juce::SpinLock::ScopedLockType scopedLock(lock);
    return validSamples;
}

bool CircularAudioBuffer::hasAudio() const noexcept
{
    const juce::SpinLock::ScopedLockType scopedLock(lock);
    return validSamples > 0 && sampleRate > 0.0;
}

void CircularAudioBuffer::rebuildBufferForCurrentRate(double sampleRateHz)
{
    const auto newCapacity = juce::jmax(1, static_cast<int>(std::ceil(maxLengthSeconds * sampleRateHz)));
    ringBuffer.setSize(2, newCapacity, false, true, true);
    ringBuffer.clear();
    writePosition = 0;
    validSamples = 0;
}

} // namespace setle::capture

