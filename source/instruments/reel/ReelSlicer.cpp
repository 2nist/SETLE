#include "ReelSlicer.h"

//==============================================================================
void ReelSlicer::prepare (double sampleRate, int /*blockSize*/) noexcept
{
    m_sampleRate = sampleRate;
    for (auto& v : m_voices)
        v.active = false;
}

void ReelSlicer::setParams (const ReelParams& p) noexcept
{
    m_params = p;
    rebuildShuffleIfNeeded();
}

//==============================================================================
void ReelSlicer::rebuildShuffleIfNeeded() noexcept
{
    const int count = juce::jmax (1, m_params.slice.count);
    const int order = m_params.slice.order;

    if (count == m_lastCount && order == m_lastOrder)
        return;

    m_lastCount  = count;
    m_lastOrder  = order;
    m_shufflePos = 0;

    // Seed deterministically from count so tables are reproducible
    m_rngState = static_cast<uint32_t> (count) * 2654435761u + 1u;

    // Build identity permutation (used by shuffle; forward/reverse/random don't need it)
    for (int i = 0; i < count; ++i)
        m_shuffleTable[i] = i;

    if (order == 3)  // shuffle — Fisher-Yates
    {
        for (int i = count - 1; i > 0; --i)
        {
            m_rngState = m_rngState * 1664525u + 1013904223u;
            const int j = static_cast<int> (m_rngState % static_cast<uint32_t> (i + 1));
            std::swap (m_shuffleTable[i], m_shuffleTable[j]);
        }
    }

    // Re-seed so random-mode LCG starts fresh after any rebuild
    m_rngState = static_cast<uint32_t> (count) * 2654435761u + 1u;
}

//==============================================================================
double ReelSlicer::sliceLength() const noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return 0.0;

    const double totalSamples = static_cast<double> (m_buffer->getNumSamples());
    const double playLen = (m_params.play.end - m_params.play.start) * totalSamples;
    const int    count   = juce::jmax (1, m_params.slice.count);
    return playLen / static_cast<double> (count);
}

double ReelSlicer::sliceStart (int index) const noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return 0.0;
    const double totalSamples = static_cast<double> (m_buffer->getNumSamples());
    return m_params.play.start * totalSamples
           + static_cast<double> (index) * sliceLength();
}

double ReelSlicer::sliceEnd (int index) const noexcept
{
    return sliceStart (index) + sliceLength();
}

//==============================================================================
void ReelSlicer::triggerStep (int stepIndex, float velocity) noexcept
{
    rebuildShuffleIfNeeded();

    const int count   = juce::jmax (1, m_params.slice.count);
    const int wrapped = ((stepIndex % count) + count) % count;
    int sliceIdx      = wrapped;

    switch (m_params.slice.order)
    {
        case 0:  // forward — identity
            sliceIdx = wrapped;
            break;

        case 1:  // reverse
            sliceIdx = count - 1 - wrapped;
            break;

        case 2:  // random — advance LCG each step
            m_rngState = m_rngState * 1664525u + 1013904223u;
            sliceIdx   = static_cast<int> (m_rngState % static_cast<uint32_t> (count));
            break;

        case 3:  // shuffle — cycle pre-computed permutation
            sliceIdx = m_shuffleTable[m_shufflePos];
            if (++m_shufflePos >= count) m_shufflePos = 0;
            break;

        default:
            break;
    }

    triggerSlice (sliceIdx, velocity);
}

//==============================================================================
void ReelSlicer::triggerSlice (int sliceIndex, float velocity) noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return;
    if (sliceIndex < 0 || sliceIndex >= m_params.slice.count)
        return;

    // Find a free voice
    SliceVoice* target = nullptr;
    for (auto& v : m_voices)
    {
        if (!v.active) { target = &v; break; }
    }
    if (target == nullptr)
        target = &m_voices[0];   // steal first

    target->active   = true;
    target->slice    = sliceIndex;
    target->velocity = velocity;
    target->bufPos   = sliceStart (sliceIndex);
    target->bufEnd   = sliceEnd   (sliceIndex);
}

void ReelSlicer::noteOn (int midiNote, float velocity) noexcept
{
    // Map MIDI notes to slices: note 60 = slice 0, 61 = slice 1, etc.
    const int sliceIndex = midiNote - 60;
    if (sliceIndex >= 0 && sliceIndex < m_params.slice.count)
        triggerSlice (sliceIndex, velocity);
}

//==============================================================================
void ReelSlicer::renderBlock (juce::AudioBuffer<float>& buf,
                               int startSample,
                               int numSamples) noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return;

    const int numOutCh = buf.getNumChannels();

    for (int s = 0; s < numSamples; ++s)
    {
        float outL = 0.0f, outR = 0.0f;

        for (auto& v : m_voices)
        {
            if (!v.active) continue;

            const int numBufCh = m_buffer->getNumChannels();
            const float sampleL = m_buffer->readSample (0, v.bufPos);
            const float sampleR = (numBufCh > 1) ? m_buffer->readSample (1, v.bufPos) : sampleL;

            outL += sampleL * v.velocity;
            outR += sampleR * v.velocity;

            v.bufPos += 1.0;   // slice playback always at original speed

            if (v.bufPos >= v.bufEnd)
                v.active = false;
        }

        const float gain = static_cast<float> (m_params.out.level);
        const float pan  = juce::jlimit (-1.0f, 1.0f, m_params.out.pan);
        const float panL = 1.0f - juce::jmax (0.0f, pan);
        const float panR = 1.0f + juce::jmin (0.0f, pan);
        const int   si   = startSample + s;

        if (numOutCh == 1)
            buf.addSample (0, si, (outL + outR) * 0.5f * gain);
        else
        {
            buf.addSample (0, si, outL * panL * gain);
            buf.addSample (1, si, outR * panR * gain);
        }
    }
}

juce::Array<float> ReelSlicer::getSlicePositions() const noexcept
{
    juce::Array<float> positions;
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return positions;

    const double totalSamples = static_cast<double> (m_buffer->getNumSamples());
    if (totalSamples <= 0.0)
        return positions;

    const int count = juce::jmax (2, m_params.slice.count);
    for (int i = 0; i <= count; ++i)
        positions.add (static_cast<float> (sliceStart (i) / totalSamples));

    return positions;
}
