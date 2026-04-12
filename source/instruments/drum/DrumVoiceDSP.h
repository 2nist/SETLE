#pragma once

#include <juce_core/juce_core.h>

namespace setle::instruments::drum
{

struct DrumVoiceDSP
{
    static inline float fmSample(float carrierPhase,
                                 float modPhase,
                                 float modIndex,
                                 float noiseMix,
                                 juce::Random& random)
    {
        const auto mod = std::sin(modPhase) * modIndex;
        const auto tone = std::sin(carrierPhase + mod);
        const auto noise = (random.nextFloat() * 2.0f - 1.0f);
        return tone * (1.0f - noiseMix) + noise * noiseMix;
    }
};

} // namespace setle::instruments::drum
