#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <array>
#include <map>

namespace te = tracktion::engine;

namespace setle::mixer
{

class BusManager
{
public:
    explicit BusManager(te::Edit& edit);

    struct Bus
    {
        juce::String busId;
        juce::String name;
        bool isActive { false };
        float returnLevel { 1.0f };
    };

    static constexpr int kMaxBuses = 4;

    void enableBus(int busIndex, const juce::String& name);
    void disableBus(int busIndex);
    const Bus& getBus(int busIndex) const;

    void setSendLevel(te::AudioTrack& track, int busIndex, float level);
    float getSendLevel(const te::AudioTrack& track, int busIndex) const;

private:
    static int clampBusIndex(int busIndex) noexcept;
    juce::String makeTrackBusKey(const te::AudioTrack& track, int busIndex) const;

    te::Edit& edit;
    std::array<Bus, kMaxBuses> buses;
    std::map<juce::String, float> sendLevels;
};

} // namespace setle::mixer
