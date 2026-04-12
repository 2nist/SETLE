#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include <memory>

namespace setle::capture
{

struct GrabSlot
{
    enum class State
    {
        Empty,
        Loading,
        Ready,
        Playing
    };

    juce::String slotId;          // stable UUID for this slot
    juce::String progressionId;   // references model::Progression in songState
    juce::String displayName;     // editable label
    State state { State::Empty };
    float confidence { 0.0f };    // from ChordIdentifier
    bool looping { false };
    int repeatCount { 1 };

    // Optional coupled audio payload for Reel playback.
    std::shared_ptr<juce::AudioBuffer<float>> coupledAudio;
    double coupledSampleRate { 0.0 };

    bool hasCoupledAudio() const noexcept
    {
        return coupledAudio != nullptr && coupledAudio->getNumSamples() > 0 && coupledSampleRate > 0.0;
    }
};

} // namespace setle::capture
