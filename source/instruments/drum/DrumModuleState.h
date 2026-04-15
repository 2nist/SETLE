#pragma once

#include <array>

#include <juce_core/juce_core.h>

#include "DrumKitPatch.h"

namespace setle::instruments::drum
{

enum class DrumMacroId
{
    Thump = 0,
    Sizzle,
    Choke,
    Texture,
    Arc,
    Gloss,
    Count
};

struct DrumMacroState
{
    std::array<float, static_cast<size_t>(DrumMacroId::Count)> values
    {
        0.62f, // THUMP
        0.55f, // SIZZLE
        0.64f, // CHOKE
        0.50f, // TEXTURE
        0.42f, // ARC
        0.36f  // GLOSS
    };

    float get(DrumMacroId id) const noexcept
    {
        return values[static_cast<size_t>(id)];
    }

    void set(DrumMacroId id, float value) noexcept
    {
        values[static_cast<size_t>(id)] = juce::jlimit(0.0f, 1.0f, value);
    }
};

struct DrumModuleState
{
    DrumKitPatch patch;
    DrumMacroState macros;
    float masterVolume { 0.85f };
};

} // namespace setle::instruments::drum
