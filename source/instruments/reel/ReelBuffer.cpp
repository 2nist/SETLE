#include "ReelBuffer.h"

//==============================================================================
void ReelBuffer::loadFromBuffer (const juce::AudioBuffer<float>& src, double sampleRate)
{
    m_sampleRate = sampleRate;
    m_buffer.makeCopyOf (src);
    m_loaded = (m_buffer.getNumSamples() > 0 && m_buffer.getNumChannels() > 0);
}

bool ReelBuffer::loadFromFile (const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));

    if (reader == nullptr)
        return false;

    // TODO: streaming for large files — for now load entire file into memory
    const int numSamples  = static_cast<int> (reader->lengthInSamples);
    const int numChannels = static_cast<int> (reader->numChannels);

    if (numSamples <= 0 || numChannels <= 0)
        return false;

    m_buffer.setSize (numChannels, numSamples);
    reader->read (&m_buffer, 0, numSamples, 0, true, numChannels > 1);

    m_sampleRate = reader->sampleRate;
    m_loaded     = true;
    return true;
}

void ReelBuffer::clear()
{
    m_buffer.setSize (0, 0);
    m_loaded = false;
}

double ReelBuffer::getDuration() const noexcept
{
    if (!m_loaded || m_sampleRate <= 0.0)
        return 0.0;
    return static_cast<double> (m_buffer.getNumSamples()) / m_sampleRate;
}

float ReelBuffer::readSample (int channel, double pos) const noexcept
{
    if (!m_loaded)
        return 0.0f;

    const int numCh  = m_buffer.getNumChannels();
    const int numSmp = m_buffer.getNumSamples();
    if (numCh <= 0 || numSmp <= 0)
        return 0.0f;

    const int ch = juce::jlimit (0, numCh - 1, channel);

    // Clamp to valid range
    if (pos < 0.0 || pos >= static_cast<double> (numSmp - 1))
        return 0.0f;

    // Linear interpolation
    const int   i0 = static_cast<int> (pos);
    const int   i1 = juce::jmin (i0 + 1, numSmp - 1);
    const float t  = static_cast<float> (pos - static_cast<double> (i0));

    const float* data = m_buffer.getReadPointer (ch);
    return data[i0] + t * (data[i1] - data[i0]);
}
