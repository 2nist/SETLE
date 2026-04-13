#pragma once

#include <array>

#include "DrumVoiceParams.h"

namespace setle::instruments::drum
{

enum class DrumVoiceId
{
    Kick = 0,
    Snare,
    HatClosed,
    HatOpen,
    Clap,
    Count
};

struct DrumKitPatch
{
    std::array<DrumVoiceParams, static_cast<size_t>(DrumVoiceId::Count)> voices;

    DrumKitPatch()
    {
        voices[0] = { 52.0f, 0.24f, 0.95f, 1.2f, 3.2f };  // kick
        voices[1] = { 180.0f, 0.18f, 0.78f, 2.0f, 1.8f }; // snare
        voices[2] = { 340.0f, 0.08f, 0.62f, 3.0f, 0.9f }; // hh closed
        voices[3] = { 320.0f, 0.22f, 0.58f, 2.8f, 1.1f }; // hh open
        voices[4] = { 250.0f, 0.16f, 0.70f, 1.5f, 2.2f }; // clap
    }
};

} // namespace setle::instruments::drum
