#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ReelBuffer.h"
#include "ReelParams.h"

//==============================================================================
/**
    ReelLoopPlayer — continuous loop playback for REEL's LOOP mode.

    Plays the buffer as a loop between play.start and play.end at a speed
    determined by loop.speed.  Wraps seamlessly at the end point.

    Real-time safe: no allocation in renderSample().
    TODO: proper pitch-independent time-stretching (phase vocoder).
          Current implementation ties pitch to speed.
*/
class ReelLoopPlayer
{
public:
    void prepare (double sampleRate, int blockSize) noexcept;

    void setBuffer (const ReelBuffer* buf) noexcept { m_buffer = buf; }
    void setParams (const ReelParams& p)   noexcept { m_params = p;   }

    /** Render one interleaved stereo sample pair into left / right. */
    void renderSample (float& left, float& right) noexcept;

    /** Sync to transport position (beat, bpm) when loop.sync is active. */
    void setTransportPosition (double beatPosition, double bpm) noexcept;

    /** Reset playhead to start point. */
    void reset() noexcept;

private:
    const ReelBuffer* m_buffer     { nullptr };
    ReelParams        m_params;
    double            m_playPos    { 0.0 };   // current read position in samples
    double            m_sampleRate { 44100.0 };

    double startSample()  const noexcept;
    double endSample()    const noexcept;
    double bufferLength() const noexcept;
};
