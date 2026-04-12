#include "BusManager.h"

namespace setle::mixer
{

BusManager::BusManager(te::Edit& e)
    : edit(e)
{
    (void) edit;

    for (int i = 0; i < kMaxBuses; ++i)
    {
        buses[static_cast<size_t>(i)].busId = "bus_" + juce::String(i + 1);
        buses[static_cast<size_t>(i)].name = "Bus " + juce::String(i + 1);
        buses[static_cast<size_t>(i)].isActive = false;
        buses[static_cast<size_t>(i)].returnLevel = 1.0f;
    }

    enableBus(0, "Reverb");
    enableBus(1, "Delay");
}

void BusManager::enableBus(int busIndex, const juce::String& name)
{
    const auto i = clampBusIndex(busIndex);
    auto& bus = buses[static_cast<size_t>(i)];
    bus.name = name.isNotEmpty() ? name : ("Bus " + juce::String(i + 1));
    bus.isActive = true;
}

void BusManager::disableBus(int busIndex)
{
    const auto i = clampBusIndex(busIndex);
    buses[static_cast<size_t>(i)].isActive = false;
}

const BusManager::Bus& BusManager::getBus(int busIndex) const
{
    return buses[static_cast<size_t>(clampBusIndex(busIndex))];
}

void BusManager::setSendLevel(te::AudioTrack& track, int busIndex, float level)
{
    const auto i = clampBusIndex(busIndex);
    if (!buses[static_cast<size_t>(i)].isActive)
        return;

    sendLevels[makeTrackBusKey(track, i)] = juce::jlimit(0.0f, 1.0f, level);
}

float BusManager::getSendLevel(const te::AudioTrack& track, int busIndex) const
{
    const auto i = clampBusIndex(busIndex);
    if (!buses[static_cast<size_t>(i)].isActive)
        return 0.0f;

    const auto key = makeTrackBusKey(track, i);
    const auto it = sendLevels.find(key);
    if (it == sendLevels.end())
        return 0.0f;

    return it->second;
}

int BusManager::clampBusIndex(int busIndex) noexcept
{
    return juce::jlimit(0, kMaxBuses - 1, busIndex);
}

juce::String BusManager::makeTrackBusKey(const te::AudioTrack& track, int busIndex) const
{
    return track.itemID.toString() + "::" + juce::String(busIndex);
}

} // namespace setle::mixer
