#pragma once

#include <juce_core/juce_core.h>

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
};

} // namespace setle::capture
