#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ReelBuffer.h"
#include "ReelParams.h"

//==============================================================================
/**
    ReelGranular — granular synthesis engine for REEL's GRAIN mode.

    Maintains a fixed pool of MAX_GRAINS grain voices (no allocation at runtime).
    Grains are triggered at rate = grain.density grains/sec.
    Each grain reads from the buffer at grain.position +/- scatter offset,
    shaped by a Hann, linear, or Gaussian envelope.

    Real-time safe: renderBlock() performs no allocation.
    MAX_GRAINS enforced — if all grains are active, triggerGrain() is a no-op.
*/
class ReelGranular
{
public:
    static constexpr int MAX_GRAINS = 64;

    void prepare (double sampleRate, int blockSize) noexcept;
    void setBuffer (const ReelBuffer* buf) noexcept { m_buffer = buf; }
    void setParams (const ReelParams& p)   noexcept { m_params = p;   }
    void reset()                           noexcept;

    /** Render numSamples of granular audio into buf starting at startSample.
        Adds to existing buffer content. */
    void renderBlock (juce::AudioBuffer<float>& buf,
                      int startSample,
                      int numSamples) noexcept;

    /** Read-only snapshot of active grain positions (0-1 normalised) for display.
        Returns number of active grains written into positions[]. */
    int getGrainPositions (float* positions, int maxPositions) const noexcept;

private:
    struct Grain
    {
        bool   active   { false };
        double bufPos   { 0.0 };   // read position in samples
        double bufSpeed { 1.0 };   // samples per output sample
        double envPos   { 0.0 };   // 0-1 through grain lifetime
        double envSpeed { 0.0 };   // increment per sample
        float  pan      { 0.0f };
    };

    Grain             m_grains[MAX_GRAINS];
    const ReelBuffer* m_buffer      { nullptr };
    ReelParams        m_params;
    double            m_sampleRate  { 44100.0 };
    double            m_sampleCount { 0.0 };
    double            m_nextGrainAt { 0.0 };

    float grainEnvelope (double pos) const noexcept;
    void  triggerGrain()                   noexcept;
    Grain* findFreeGrain()                 noexcept;
};
