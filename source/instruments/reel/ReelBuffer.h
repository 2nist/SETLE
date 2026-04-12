#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
/**
    ReelBuffer — holds one loaded audio clip for REEL playback.

    Audio can be loaded from a grabbed circular-buffer region or from a file.
    readSample() uses linear interpolation for sub-sample accuracy.
    Thread safety: load methods must be called from the message thread.
                   readSample / getBuffer are read-only and safe from audio thread
                   once loading is complete.
*/
class ReelBuffer
{
public:
    ReelBuffer() = default;

    //==========================================================================
    // Loading (message thread only)

    /** Load audio from a pre-captured juce::AudioBuffer (circular-buffer grab). */
    void loadFromBuffer (const juce::AudioBuffer<float>& src, double sampleRate = 44100.0);

    /** Load from an audio file. Returns false on failure.
        Supports WAV, AIFF, FLAC, MP3 via JUCE AudioFormatManager.
        NOTE: loads entire file into memory. TODO: streaming for large files. */
    bool loadFromFile (const juce::File& file);

    void clear();

    //==========================================================================
    // Queries (audio-thread safe after load completes)

    bool   isLoaded()       const noexcept { return m_loaded; }
    double getDuration()    const noexcept;
    int    getNumSamples()  const noexcept { return m_buffer.getNumSamples(); }
    double getSampleRate()  const noexcept { return m_sampleRate; }
    int    getNumChannels() const noexcept { return m_buffer.getNumChannels(); }

    /** Read one sample with linear interpolation.
        channel:  0 = L, 1 = R (clamped to available channels).
        pos:      sub-sample read position in samples.
        Returns 0 if not loaded or pos out of range. */
    float readSample (int channel, double pos) const noexcept;

    const juce::AudioBuffer<float>& getBuffer() const noexcept { return m_buffer; }

private:
    juce::AudioBuffer<float>  m_buffer;
    double                    m_sampleRate { 44100.0 };
    bool                      m_loaded     { false };
};
