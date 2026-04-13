#include "FmDrumVoice.h"

namespace setle::instruments::drum
{

void FmDrumVoice::trigger(const DrumVoiceParams& p, float velocity, bool accented)
{
    const auto vel = juce::jlimit(0.0f, 1.0f, velocity);
    const auto level = p.level * vel * (accented ? 1.15f : 1.0f);

    const auto carrierHz = juce::jlimit(20.0f, 2000.0f, p.tuneHz);
    const auto modHz = carrierHz * juce::jlimit(0.2f, 8.0f, p.modRatio);

    phaseCarrier = 0.0f;
    phaseMod = 0.0f;
    deltaCarrier = juce::MathConstants<float>::twoPi * carrierHz / static_cast<float>(sampleRate);
    deltaMod = juce::MathConstants<float>::twoPi * modHz / static_cast<float>(sampleRate);

    modIndex = juce::jlimit(0.0f, 8.0f, p.modIndex);
    amp = level;

    const auto decaySeconds = juce::jlimit(0.02f, 1.0f, p.decaySeconds);
    decayMul = std::pow(0.001f, 1.0f / (decaySeconds * static_cast<float>(sampleRate)));

    noiseMix = carrierHz > 280.0f ? 0.55f : 0.18f;
    active = true;
}

float FmDrumVoice::renderSample()
{
    if (!active)
        return 0.0f;

    const auto s = DrumVoiceDSP::fmSample(phaseCarrier, phaseMod, modIndex, noiseMix, random) * amp;

    phaseCarrier += deltaCarrier;
    phaseMod += deltaMod;
    if (phaseCarrier >= juce::MathConstants<float>::twoPi) phaseCarrier -= juce::MathConstants<float>::twoPi;
    if (phaseMod >= juce::MathConstants<float>::twoPi) phaseMod -= juce::MathConstants<float>::twoPi;

    amp *= decayMul;
    modIndex *= 0.9994f;
    if (amp < 0.00008f)
        active = false;

    return s;
}

} // namespace setle::instruments::drum
