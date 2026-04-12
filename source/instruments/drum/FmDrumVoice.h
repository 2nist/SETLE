#pragma once

#include <juce_core/juce_core.h>

#include "DrumVoiceDSP.h"
#include "DrumVoiceParams.h"

namespace setle::instruments::drum
{

class FmDrumVoice
{
public:
    void setSampleRate(double newRate)
    {
        sampleRate = newRate > 0.0 ? newRate : 44100.0;
    }

    void trigger(const DrumVoiceParams& p, float velocity, bool accented);
    bool isActive() const noexcept { return active; }
    float renderSample();

private:
    double sampleRate { 44100.0 };
    bool active { false };
    float phaseCarrier { 0.0f };
    float phaseMod { 0.0f };
    float deltaCarrier { 0.0f };
    float deltaMod { 0.0f };
    float amp { 0.0f };
    float decayMul { 0.999f };
    float noiseMix { 0.2f };
    float modIndex { 1.0f };
    juce::Random random { 0x5e7e11 };
};

} // namespace setle::instruments::drum
