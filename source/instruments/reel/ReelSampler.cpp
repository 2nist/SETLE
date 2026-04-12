#include "ReelSampler.h"

//==============================================================================
void ReelSampler::prepare (double sampleRate, int /*blockSize*/) noexcept
{
    m_sampleRate = sampleRate;
    allNotesOff();
}

void ReelSampler::allNotesOff() noexcept
{
    for (auto& v : m_voices)
        v.active = false;
}

//==============================================================================
void ReelSampler::noteOn (int midiNote, float velocity) noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return;

    // Steal oldest voice if all busy
    SampleVoice* target = nullptr;
    for (auto& v : m_voices)
    {
        if (!v.active) { target = &v; break; }
    }
    if (target == nullptr)
        target = &m_voices[0];   // steal first

    // Pitch ratio: semitone difference from root
    const int   semitones = midiNote - m_params.sample.root;
    const float pitchRatio = std::pow (2.0f, static_cast<float> (semitones) / 12.0f);

    target->active     = true;
    target->note       = midiNote;
    target->velocity   = velocity;
    target->releasing  = false;
    target->releaseEnv = 1.0f;
    target->bufPos     = m_params.play.start
                         * static_cast<double> (m_buffer->getNumSamples());
    target->bufSpeed   = static_cast<double> (pitchRatio);
}

void ReelSampler::noteOff (int midiNote) noexcept
{
    for (auto& v : m_voices)
    {
        if (v.active && v.note == midiNote && !v.releasing)
        {
            if (m_params.sample.oneshot)
                return;   // let one-shot play to completion
            v.releasing = true;
        }
    }
}

//==============================================================================
void ReelSampler::renderBlock (juce::AudioBuffer<float>& buf,
                                int startSample,
                                int numSamples) noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return;

    const int    numOutCh  = buf.getNumChannels();
    const double endPos    = m_params.play.end
                             * static_cast<double> (m_buffer->getNumSamples());
    const double startPos  = m_params.play.start
                             * static_cast<double> (m_buffer->getNumSamples());
    const double loopLen   = (endPos > startPos) ? (endPos - startPos) : 0.0;

    for (int s = 0; s < numSamples; ++s)
    {
        float outL = 0.0f, outR = 0.0f;

        for (auto& v : m_voices)
        {
            if (!v.active) continue;

            // Read
            const int numBufCh = m_buffer->getNumChannels();
            float sampleL = m_buffer->readSample (0, v.bufPos);
            float sampleR = (numBufCh > 1) ? m_buffer->readSample (1, v.bufPos) : sampleL;

            float env = v.releasing ? v.releaseEnv : 1.0f;
            sampleL *= env * v.velocity;
            sampleR *= env * v.velocity;

            outL += sampleL;
            outR += sampleR;

            // Advance
            const double speed = m_params.play.reverse ? -v.bufSpeed : v.bufSpeed;
            v.bufPos += speed;

            // Release decay
            if (v.releasing)
            {
                v.releaseEnv *= kReleaseDecay;
                if (v.releaseEnv < 0.0001f)
                {
                    v.active = false;
                    continue;
                }
            }

            // Loop or one-shot
            if (v.bufPos >= endPos)
            {
                if (m_params.sample.oneshot)
                {
                    v.active = false;
                }
                else if (loopLen > 0.0)
                {
                    v.bufPos = startPos + std::fmod (v.bufPos - startPos, loopLen);
                }
            }
            else if (v.bufPos < startPos)
            {
                if (m_params.sample.oneshot)
                    v.active = false;
                else if (loopLen > 0.0)
                    v.bufPos = endPos - std::fmod (startPos - v.bufPos, loopLen);
            }
        }

        // Write
        const float gain  = static_cast<float> (m_params.out.level);
        const float pan   = juce::jlimit (-1.0f, 1.0f, m_params.out.pan);
        const float panL  = 1.0f - juce::jmax (0.0f, pan);
        const float panR  = 1.0f + juce::jmin (0.0f, pan);
        const int   si    = startSample + s;

        if (numOutCh == 1)
            buf.addSample (0, si, (outL + outR) * 0.5f * gain);
        else
        {
            buf.addSample (0, si, outL * panL * gain);
            buf.addSample (1, si, outR * panR * gain);
        }
    }
}
