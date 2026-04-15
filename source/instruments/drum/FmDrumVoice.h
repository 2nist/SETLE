#pragma once

#include <juce_core/juce_core.h>

#include "DrumKitPatch.h"
#include "DrumModuleState.h"
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

    void trigger(DrumVoiceId id,
                 const DrumVoiceParams& p,
                 const DrumMacroState& macros,
                 float velocity,
                 bool accented);
    bool isActive() const noexcept { return active; }
    DrumVoiceId getVoiceId() const noexcept { return voiceId; }
    void forceStop() noexcept { active = false; amp = 0.0f; }
    float renderSample();

private:
    float advanceSine(float& phase, float delta) noexcept;
    float highPassNoise(float input, float hpCoeff) noexcept;
    float saturate(float x, float drive) const noexcept;

    DrumVoiceId voiceId { DrumVoiceId::Kick };
    double sampleRate { 44100.0 };
    bool active { false };
    float phaseCarrier { 0.0f };
    float phaseMod { 0.0f };
    float phaseAux { 0.0f };
    float deltaCarrier { 0.0f };
    float deltaMod { 0.0f };
    float deltaAux { 0.0f };
    float amp { 0.0f };
    float decayMul { 0.999f };
    float pitchEnv { 0.0f };
    float pitchDecayMul { 0.995f };
    float modIndex { 1.0f };
    float textureAmount { 0.5f };
    float driveAmount { 1.5f };
    float hpCoeff { 0.82f };
    float noisePrevIn { 0.0f };
    float noisePrevOut { 0.0f };
    float clapTapTimer { 0.0f };
    int clapTapCount { 0 };
    juce::Random random { 0x5e7e11 };
};

} // namespace setle::instruments::drum
