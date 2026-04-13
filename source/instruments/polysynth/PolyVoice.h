#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "SineOscNode.h"

namespace setle::instruments::polysynth
{

class PolyVoice
{
public:
    void setSampleRate(double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        osc.setSampleRate(sampleRate);
    }

    void setADSR(const juce::ADSR::Parameters& p)
    {
        adsrParams = p;
        adsr.setParameters(adsrParams);
    }

    void start(int midiNote, float noteVelocity)
    {
        noteNumber = midiNote;
        velocity = juce::jlimit(0.0f, 1.0f, noteVelocity);
        osc.setFrequency(juce::MidiMessage::getMidiNoteInHertz(midiNote));
        osc.reset();
        adsr.noteOn();
        active = true;
    }

    void stop(bool allowTailOff)
    {
        if (allowTailOff)
        {
            adsr.noteOff();
            return;
        }

        adsr.reset();
        active = false;
    }

    bool matchesMidi(int midiNote) const noexcept
    {
        return active && noteNumber == midiNote;
    }

    bool isActive() const noexcept
    {
        return active;
    }

    int getMidiNote() const noexcept
    {
        return noteNumber;
    }

    void render(juce::AudioBuffer<float>& output,
                int startSample,
                int numSamples,
                float cutoffHz,
                float resonance)
    {
        if (!active)
            return;

        auto* left = output.getWritePointer(0);
        auto* right = output.getNumChannels() > 1 ? output.getWritePointer(1) : nullptr;

        const auto cut = juce::jlimit(40.0f, 18000.0f, cutoffHz);
        const auto x = juce::MathConstants<float>::twoPi * cut / static_cast<float>(sampleRate);
        const auto alpha = x / (x + 1.0f);
        const auto feedback = juce::jlimit(0.0f, 0.98f, resonance);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto env = adsr.getNextSample();
            if (!adsr.isActive())
            {
                active = false;
                break;
            }

            const auto raw = osc.nextSample() * velocity * env;
            const auto input = raw - (feedback * filterState);
            filterState += alpha * (input - filterState);
            const auto s = filterState;

            left[startSample + i] += s;
            if (right != nullptr)
                right[startSample + i] += s;
        }
    }

private:
    double sampleRate { 44100.0 };
    int noteNumber { -1 };
    float velocity { 0.0f };
    float filterState { 0.0f };
    bool active { false };
    juce::ADSR::Parameters adsrParams;
    juce::ADSR adsr;
    SineOscNode osc;
};

} // namespace setle::instruments::polysynth
