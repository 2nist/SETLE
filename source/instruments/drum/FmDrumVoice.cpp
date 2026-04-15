#include "FmDrumVoice.h"

#include <cmath>

namespace setle::instruments::drum
{

float FmDrumVoice::advanceSine(float& phase, float delta) noexcept
{
    const auto s = std::sin(phase);
    phase += delta;
    if (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;
    return s;
}

float FmDrumVoice::highPassNoise(float input, float coeff) noexcept
{
    const float out = coeff * (noisePrevOut + input - noisePrevIn);
    noisePrevIn = input;
    noisePrevOut = out;
    return out;
}

float FmDrumVoice::saturate(float x, float drive) const noexcept
{
    return std::tanh(x * drive);
}

void FmDrumVoice::trigger(DrumVoiceId id,
                          const DrumVoiceParams& p,
                          const DrumMacroState& macros,
                          float velocity,
                          bool accented)
{
    voiceId = id;
    const auto vel = juce::jlimit(0.0f, 1.0f, velocity);
    const auto level = juce::jlimit(0.0f, 1.25f, p.level * vel * (accented ? 1.15f : 1.0f));

    const auto thump = macros.get(DrumMacroId::Thump);
    const auto sizzle = macros.get(DrumMacroId::Sizzle);
    const auto choke = macros.get(DrumMacroId::Choke);
    const auto texture = macros.get(DrumMacroId::Texture);

    phaseCarrier = 0.0f;
    phaseMod = 0.0f;
    phaseAux = 0.0f;
    amp = level;
    modIndex = juce::jlimit(0.0f, 9.0f, p.modIndex);
    textureAmount = juce::jlimit(0.0f, 1.0f, texture);
    pitchEnv = 1.0f;
    clapTapTimer = 0.0f;
    clapTapCount = 0;
    noisePrevIn = 0.0f;
    noisePrevOut = 0.0f;

    float carrierHz = juce::jlimit(20.0f, 4000.0f, p.tuneHz);
    float modHz = carrierHz * juce::jlimit(0.2f, 10.0f, p.modRatio);
    float decaySeconds = juce::jlimit(0.02f, 1.25f, p.decaySeconds);

    switch (voiceId)
    {
        case DrumVoiceId::Kick:
            carrierHz = juce::jlimit(35.0f, 110.0f, p.tuneHz);
            decaySeconds *= juce::jmap(thump, 0.0f, 1.0f, 0.9f, 1.45f);
            driveAmount = 1.15f + thump * 2.1f;
            pitchDecayMul = std::pow(0.0005f, 1.0f / (0.06f * static_cast<float>(sampleRate)));
            hpCoeff = 0.80f;
            break;
        case DrumVoiceId::Sub:
            carrierHz = juce::jlimit(32.0f, 58.0f, p.tuneHz);
            decaySeconds *= juce::jmap(thump, 0.0f, 1.0f, 0.85f, 1.8f);
            driveAmount = 1.0f + thump * 1.1f;
            pitchEnv = 0.0f;
            pitchDecayMul = 1.0f;
            hpCoeff = 0.78f;
            break;
        case DrumVoiceId::Snare:
            carrierHz = juce::jlimit(120.0f, 420.0f, p.tuneHz);
            modHz = carrierHz * juce::jmap(textureAmount, 0.7f, 1.8f);
            decaySeconds *= juce::jmap(sizzle, 0.0f, 1.0f, 0.75f, 1.25f);
            driveAmount = 1.15f + sizzle * 1.8f;
            pitchDecayMul = std::pow(0.002f, 1.0f / (0.04f * static_cast<float>(sampleRate)));
            hpCoeff = 0.91f;
            break;
        case DrumVoiceId::Perc:
            carrierHz = juce::jlimit(160.0f, 1200.0f, p.tuneHz);
            modHz = carrierHz * juce::jmap(textureAmount, 1.2f, 4.4f);
            modIndex = juce::jlimit(0.5f, 8.5f, p.modIndex * juce::jmap(textureAmount, 0.9f, 1.9f));
            decaySeconds *= juce::jmap(textureAmount, 0.0f, 1.0f, 0.75f, 1.35f);
            driveAmount = 1.1f + textureAmount * 1.6f;
            pitchDecayMul = std::pow(0.002f, 1.0f / (0.08f * static_cast<float>(sampleRate)));
            hpCoeff = 0.88f;
            break;
        case DrumVoiceId::HatClosed:
            carrierHz = juce::jlimit(220.0f, 900.0f, p.tuneHz);
            decaySeconds *= juce::jmap(choke, 0.0f, 1.0f, 0.78f, 0.42f);
            driveAmount = 1.2f + sizzle * 1.5f;
            pitchEnv = 0.0f;
            pitchDecayMul = 1.0f;
            hpCoeff = 0.96f;
            break;
        case DrumVoiceId::HatOpen:
            carrierHz = juce::jlimit(210.0f, 860.0f, p.tuneHz);
            decaySeconds *= juce::jmap(choke, 0.0f, 1.0f, 1.45f, 0.58f);
            driveAmount = 1.15f + sizzle * 1.4f;
            pitchEnv = 0.0f;
            pitchDecayMul = 1.0f;
            hpCoeff = 0.95f;
            break;
        case DrumVoiceId::Clap:
            carrierHz = juce::jlimit(160.0f, 420.0f, p.tuneHz);
            decaySeconds *= juce::jmap(sizzle, 0.0f, 1.0f, 0.8f, 1.2f);
            driveAmount = 1.2f + textureAmount * 1.3f;
            pitchDecayMul = std::pow(0.002f, 1.0f / (0.05f * static_cast<float>(sampleRate)));
            hpCoeff = 0.93f;
            break;
        case DrumVoiceId::Count:
            break;
    }

    deltaCarrier = juce::MathConstants<float>::twoPi * carrierHz / static_cast<float>(sampleRate);
    deltaMod = juce::MathConstants<float>::twoPi * modHz / static_cast<float>(sampleRate);
    deltaAux = juce::MathConstants<float>::twoPi * (carrierHz * 0.5f) / static_cast<float>(sampleRate);

    decayMul = std::pow(0.001f, 1.0f / (juce::jmax(0.02f, decaySeconds) * static_cast<float>(sampleRate)));
    active = true;
}

float FmDrumVoice::renderSample()
{
    if (!active)
        return 0.0f;

    float sample = 0.0f;
    const auto noise = random.nextFloat() * 2.0f - 1.0f;
    const auto hpNoise = highPassNoise(noise, hpCoeff);

    switch (voiceId)
    {
        case DrumVoiceId::Kick:
        {
            const auto freq = deltaCarrier * (1.0f + pitchEnv * 1.35f);
            const auto tone = advanceSine(phaseCarrier, freq);
            const auto click = hpNoise * (0.20f + 0.26f * pitchEnv);
            sample = saturate((tone + click) * 0.95f, driveAmount);
            pitchEnv *= pitchDecayMul;
            break;
        }
        case DrumVoiceId::Sub:
            sample = saturate(advanceSine(phaseCarrier, deltaCarrier) * 0.95f, driveAmount * 0.8f);
            break;
        case DrumVoiceId::Snare:
        {
            const auto resonator = advanceSine(phaseCarrier, deltaCarrier * (1.0f + pitchEnv * 0.18f));
            const auto ring = advanceSine(phaseMod, deltaMod) * (0.35f + 0.35f * textureAmount);
            const auto body = resonator * (0.32f + 0.28f * textureAmount);
            const auto burst = hpNoise * (0.72f + 0.18f * textureAmount);
            sample = saturate((burst + body + ring) * 0.78f, driveAmount);
            pitchEnv *= pitchDecayMul;
            break;
        }
        case DrumVoiceId::Perc:
        {
            const auto fm = DrumVoiceDSP::fmSample(phaseCarrier, phaseMod, modIndex, 0.02f, random);
            const auto ping = advanceSine(phaseAux, deltaAux * (1.0f + textureAmount * 0.7f)) * 0.28f;
            sample = saturate((fm * (0.80f + 0.12f * textureAmount) + ping), driveAmount);
            break;
        }
        case DrumVoiceId::HatClosed:
        case DrumVoiceId::HatOpen:
        {
            const auto metallic = advanceSine(phaseCarrier, deltaCarrier * (2.8f + textureAmount * 1.2f));
            sample = saturate(hpNoise * 0.86f + metallic * 0.18f, driveAmount);
            break;
        }
        case DrumVoiceId::Clap:
        {
            if (clapTapTimer <= 0.0f && clapTapCount < 2)
            {
                clapTapTimer = 0.010f * static_cast<float>(sampleRate);
                ++clapTapCount;
                amp = juce::jmax(amp, 0.22f);
            }
            clapTapTimer -= 1.0f;

            const auto body = advanceSine(phaseCarrier, deltaCarrier) * 0.24f;
            sample = saturate(hpNoise * 0.92f + body, driveAmount);
            break;
        }
        case DrumVoiceId::Count:
            break;
    }

    phaseMod += deltaMod;
    if (phaseMod >= juce::MathConstants<float>::twoPi)
        phaseMod -= juce::MathConstants<float>::twoPi;

    sample *= amp;
    amp *= decayMul;
    modIndex *= 0.9988f;
    if (amp < 0.00006f)
        active = false;

    return sample;
}

} // namespace setle::instruments::drum
