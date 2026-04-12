#pragma once

#include "DrumKitPatch.h"

namespace setle::instruments::drum
{

struct DrumModuleState
{
    DrumKitPatch patch;
    float masterVolume { 0.85f };
};

} // namespace setle::instruments::drum
