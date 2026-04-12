#include "HistoryBuffer.h"

#include <algorithm>

namespace setle::capture
{
namespace
{
double wallClockNowSeconds()
{
    return juce::Time::getMillisecondCounterHiRes() * 0.001;
}
} // namespace

HistoryBuffer::HistoryBuffer(int maxBeatsToKeep)
    : maxBeats(juce::jmax(1, maxBeatsToKeep))
{
}

HistoryBuffer::~HistoryBuffer()
{
    clearSource();
}

void HistoryBuffer::setSource(const juce::String& deviceIdentifier)
{
    const juce::ScopedLock scopedLock(lock);

    openInputs.clear();
    allSources = false;
    currentDeviceId = deviceIdentifier;

    if (deviceIdentifier.isEmpty())
        return;

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (device.identifier != deviceIdentifier)
            continue;

        if (auto input = juce::MidiInput::openDevice(device.identifier, this))
        {
            input->start();
            openInputs.push_back(std::move(input));
            captureStartTime = wallClockNowSeconds();
        }
        break;
    }
}

void HistoryBuffer::setAllSources(bool enabled)
{
    const juce::ScopedLock scopedLock(lock);

    openInputs.clear();
    allSources = enabled;

    if (!enabled)
    {
        currentDeviceId.clear();
        return;
    }

    currentDeviceId = "__all__";
    captureStartTime = wallClockNowSeconds();

    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        if (auto input = juce::MidiInput::openDevice(device.identifier, this))
        {
            input->start();
            openInputs.push_back(std::move(input));
        }
    }
}

void HistoryBuffer::clearSource()
{
    const juce::ScopedLock scopedLock(lock);
    openInputs.clear();
    allSources = false;
    currentDeviceId.clear();
}

juce::String HistoryBuffer::getCurrentSourceIdentifier() const
{
    const juce::ScopedLock scopedLock(lock);
    return currentDeviceId;
}

std::vector<HistoryBuffer::CapturedEvent> HistoryBuffer::getLastNBeats(double bpm, int beats) const
{
    const juce::ScopedLock scopedLock(lock);

    if (events.empty())
        return {};

    const auto clampedBpm = juce::jmax(1.0, bpm);
    const auto windowSeconds = (60.0 / clampedBpm) * static_cast<double>(juce::jmax(1, beats));
    const auto lastWallTime = events.back().wallClockSeconds;
    const auto cutoff = lastWallTime - windowSeconds;

    std::vector<CapturedEvent> result;
    result.reserve(events.size());

    for (const auto& event : events)
    {
        if (event.wallClockSeconds >= cutoff)
            result.push_back(event);
    }

    return result;
}

std::vector<HistoryBuffer::CapturedEvent> HistoryBuffer::getAll() const
{
    const juce::ScopedLock scopedLock(lock);
    return events;
}

void HistoryBuffer::clear()
{
    const juce::ScopedLock scopedLock(lock);
    events.clear();
}

void HistoryBuffer::setMaxBeats(int beats)
{
    const juce::ScopedLock scopedLock(lock);
    maxBeats = juce::jlimit(1, 128, beats);
}

int HistoryBuffer::getMaxBeats() const noexcept
{
    return maxBeats;
}

bool HistoryBuffer::isCapturing() const noexcept
{
    const juce::ScopedLock scopedLock(lock);
    return !openInputs.empty();
}

void HistoryBuffer::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    const juce::ScopedLock scopedLock(lock);

    if (captureStartTime <= 0.0)
        captureStartTime = wallClockNowSeconds();

    events.push_back({ message, wallClockNowSeconds() - captureStartTime });

    // Keep memory bounded in real-time capture.
    trimToMaxLength(120.0);
}

void HistoryBuffer::trimToMaxLength(double bpm)
{
    if (events.empty())
        return;

    const auto clampedBpm = juce::jmax(1.0, bpm);
    const auto maxWindowSeconds = (60.0 / clampedBpm) * static_cast<double>(juce::jmax(1, maxBeats));
    const auto latestTime = events.back().wallClockSeconds;
    const auto cutoff = latestTime - maxWindowSeconds;

    auto eraseBegin = std::find_if(events.begin(), events.end(), [cutoff](const auto& event)
    {
        return event.wallClockSeconds >= cutoff;
    });

    if (eraseBegin != events.begin())
        events.erase(events.begin(), eraseBegin);
}

} // namespace setle::capture
