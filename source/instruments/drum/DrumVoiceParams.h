#pragma once

namespace setle::instruments::drum
{

struct DrumVoiceParams
{
    float tuneHz { 120.0f };
    float decaySeconds { 0.18f };
    float level { 0.8f };
    float modRatio { 1.8f };
    float modIndex { 2.0f };
};

} // namespace setle::instruments::drum
