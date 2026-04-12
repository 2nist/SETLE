#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace setle::instruments::polysynth
{

class SineOscNode
{
public:
    void setSampleRate(double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    }

    void setFrequency(float newFrequency)
    {
        frequency = newFrequency > 0.0f ? newFrequency : 0.0f;
        phaseDelta = static_cast<float>((2.0 * juce::MathConstants<double>::pi * frequency) / sampleRate);
    }

    float nextSample()
    {
        auto out = std::sin(phase);
        phase += phaseDelta;
        if (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
        return out;
    }

    void reset()
    {
        phase = 0.0f;
    }

private:
    double sampleRate { 44100.0 };
    float frequency { 440.0f };
    float phase { 0.0f };
    float phaseDelta { 0.0f };
};

} // namespace setle::instruments::polysynth
