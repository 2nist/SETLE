#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ReelBuffer.h"
#include "ReelParams.h"

//==============================================================================
/**
    ReelSampler — MIDI-triggered sample playback for REEL's SAMPLE mode.

    Each incoming note-on triggers a voice pitched relative to sample.root.
    Supports one-shot and looped playback.  Up to MAX_VOICES polyphonic.

    Real-time safe: no allocation in noteOn / renderBlock.
*/
class ReelSampler
{
public:
    static constexpr int MAX_VOICES = 8;

    void prepare (double sampleRate, int blockSize) noexcept;
    void setBuffer (const ReelBuffer* buf) noexcept { m_buffer = buf; }
    void setParams (const ReelParams& p)   noexcept { m_params = p;   }

    void noteOn  (int midiNote, float velocity) noexcept;
    void noteOff (int midiNote)                 noexcept;
    void allNotesOff()                          noexcept;

    void renderBlock (juce::AudioBuffer<float>& buf,
                      int startSample,
                      int numSamples) noexcept;

private:
    struct SampleVoice
    {
        bool   active     { false };
        int    note       { -1 };
        double bufPos     { 0.0 };
        double bufSpeed   { 1.0 };   // pitch-relative read rate
        float  velocity   { 1.0f };
        bool   releasing  { false };
        float  releaseEnv { 1.0f };  // simple linear release
    };

    SampleVoice       m_voices[MAX_VOICES];
    const ReelBuffer* m_buffer      { nullptr };
    ReelParams        m_params;
    double            m_sampleRate  { 44100.0 };

    static constexpr float kReleaseDecay = 0.9995f;   // per-sample release factor
};
