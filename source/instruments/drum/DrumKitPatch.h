#pragma once

#include <array>

#include "DrumVoiceParams.h"

namespace setle::instruments::drum
{

enum class DrumVoiceId
{
    Kick = 0,
    Sub,
    Snare,
    Perc,
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
        voices[static_cast<size_t>(DrumVoiceId::Kick)]      = { 52.0f, 0.24f, 0.95f, 1.2f, 3.2f };
        voices[static_cast<size_t>(DrumVoiceId::Sub)]       = { 44.0f, 0.42f, 0.90f, 1.0f, 0.2f };
        voices[static_cast<size_t>(DrumVoiceId::Snare)]     = { 190.0f, 0.20f, 0.78f, 2.0f, 1.8f };
        voices[static_cast<size_t>(DrumVoiceId::Perc)]      = { 260.0f, 0.16f, 0.70f, 2.3f, 2.8f };
        voices[static_cast<size_t>(DrumVoiceId::HatClosed)] = { 340.0f, 0.08f, 0.62f, 3.0f, 0.9f };
        voices[static_cast<size_t>(DrumVoiceId::HatOpen)]   = { 320.0f, 0.24f, 0.58f, 2.8f, 1.1f };
        voices[static_cast<size_t>(DrumVoiceId::Clap)]      = { 250.0f, 0.16f, 0.70f, 1.5f, 2.2f };
    }
};

} // namespace setle::instruments::drum
