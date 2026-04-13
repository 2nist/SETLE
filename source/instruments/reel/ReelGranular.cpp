#include "ReelGranular.h"

//==============================================================================
void ReelGranular::prepare (double sampleRate, int /*blockSize*/) noexcept
{
    m_sampleRate  = sampleRate;
    m_sampleCount = 0.0;
    m_nextGrainAt = 0.0;
    reset();
}

void ReelGranular::reset() noexcept
{
    for (auto& g : m_grains)
        g.active = false;
    m_sampleCount = 0.0;
    m_nextGrainAt = 0.0;
}

//==============================================================================
float ReelGranular::grainEnvelope (double pos) const noexcept
{
    // pos is 0.0-1.0 through grain lifetime
    switch (m_params.grain.envelope)
    {
        case 1: // linear — triangle
            return static_cast<float> (pos < 0.5 ? pos * 2.0 : (1.0 - pos) * 2.0);

        case 2: // gaussian
        {
            const double x = (pos - 0.5) * 4.0;   // -2..+2
            return static_cast<float> (std::exp (-x * x * 0.5));
        }

        default: // 0 = Hann window
            return 0.5f * (1.0f - static_cast<float> (
                std::cos (2.0 * juce::MathConstants<double>::pi * pos)));
    }
}

//==============================================================================
ReelGranular::Grain* ReelGranular::findFreeGrain() noexcept
{
    for (auto& g : m_grains)
        if (!g.active)
            return &g;
    return nullptr;
}

//==============================================================================
void ReelGranular::triggerGrain() noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return;

    Grain* g = findFreeGrain();
    if (g == nullptr)
        return;   // MAX_GRAINS active — skip

    const double numSamples = static_cast<double> (m_buffer->getNumSamples());

    // Grain size in samples
    const double grainSamples = m_params.grain.size * 0.001 * m_sampleRate;
    if (grainSamples <= 0.0)
        return;

    g->active   = true;
    g->envPos   = 0.0;
    g->envSpeed = 1.0 / grainSamples;

    // Center position with scatter
    const double centerPos   = m_params.grain.position * numSamples;
    const double scatterRange = m_params.grain.scatter * numSamples * 0.5;
    const double scatter = (juce::Random::getSystemRandom().nextDouble() * 2.0 - 1.0)
                           * scatterRange;

    const double startSample = m_params.play.start * numSamples;
    const double endSample   = m_params.play.end   * numSamples;
    g->bufPos = juce::jlimit (startSample, juce::jmax (startSample, endSample - 1.0),
                               centerPos + scatter);

    // Stereo spread
    g->pan = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f)
             * static_cast<float> (m_params.grain.spread);

    // Pitch affects read speed (simple rate change — no independent pitch shifting)
    const float pitchSemitones = static_cast<float> (m_params.grain.pitch);
    g->bufSpeed = static_cast<double> (std::pow (2.0f, pitchSemitones / 12.0f));
}

//==============================================================================
void ReelGranular::renderBlock (juce::AudioBuffer<float>& buf,
                                int startSample,
                                int numSamples) noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return;

    const double numBufSamples = static_cast<double> (m_buffer->getNumSamples());
    const int    numOutCh      = buf.getNumChannels();
    if (numOutCh < 1 || numBufSamples <= 0.0)
        return;

    const double startBuf = m_params.play.start * numBufSamples;
    const double endBuf   = m_params.play.end   * numBufSamples;
    const double loopLen  = (endBuf > startBuf) ? (endBuf - startBuf) : numBufSamples;

    for (int s = 0; s < numSamples; ++s)
    {
        // Time to trigger a new grain?
        m_sampleCount += 1.0;
        const double samplesPerGrain = (m_params.grain.density > 0.0f)
                                       ? (m_sampleRate / static_cast<double> (m_params.grain.density))
                                       : m_sampleRate;
        if (m_sampleCount >= m_nextGrainAt)
        {
            triggerGrain();
            m_nextGrainAt = m_sampleCount + samplesPerGrain;
        }

        // Accumulate all active grains
        float outL = 0.0f, outR = 0.0f;

        for (auto& g : m_grains)
        {
            if (!g.active) continue;

            // Read from buffer (mono mix of available channels)
            const int numBufCh = m_buffer->getNumChannels();
            float sample = 0.0f;
            if (numBufCh == 1)
            {
                sample = m_buffer->readSample (0, g.bufPos);
            }
            else
            {
                sample = (m_buffer->readSample (0, g.bufPos)
                        + m_buffer->readSample (1, g.bufPos)) * 0.5f;
            }

            // Grain envelope
            sample *= grainEnvelope (g.envPos);

            // Pan
            const float panL = 1.0f - juce::jmax (0.0f,  g.pan);
            const float panR = 1.0f + juce::jmin (0.0f,  g.pan);
            outL += sample * panL;
            outR += sample * panR;

            // Advance grain
            g.bufPos  += g.bufSpeed;
            g.envPos  += g.envSpeed;

            // Deactivate when envelope is done
            if (g.envPos >= 1.0)
            {
                g.active = false;
                continue;
            }

            // Wrap buffer position within play region
            if (g.bufPos >= endBuf)
                g.bufPos = startBuf + std::fmod (g.bufPos - startBuf, loopLen);
            else if (g.bufPos < startBuf)
                g.bufPos = endBuf  - std::fmod (startBuf - g.bufPos, loopLen);
        }

        // Write to output
        const float outGain = static_cast<float> (m_params.out.level);
        const int   si      = startSample + s;

        if (numOutCh == 1)
        {
            buf.addSample (0, si, (outL + outR) * 0.5f * outGain);
        }
        else
        {
            buf.addSample (0, si, outL * outGain);
            buf.addSample (1, si, outR * outGain);
        }
    }
}

int ReelGranular::getGrainPositions (float* positions, int maxPositions) const noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded() || positions == nullptr)
        return 0;

    const double numSamples = static_cast<double> (m_buffer->getNumSamples());
    int count = 0;

    for (const auto& g : m_grains)
    {
        if (!g.active) continue;
        if (count >= maxPositions) break;

        positions[count++] = static_cast<float> (g.bufPos / numSamples);
    }

    return count;
}
