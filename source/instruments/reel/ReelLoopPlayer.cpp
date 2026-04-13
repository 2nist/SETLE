#include "ReelLoopPlayer.h"

//==============================================================================
void ReelLoopPlayer::prepare (double sampleRate, int /*blockSize*/) noexcept
{
    m_sampleRate = sampleRate;
    m_playPos    = 0.0;
}

void ReelLoopPlayer::reset() noexcept
{
    m_playPos = startSample();
}

//==============================================================================
double ReelLoopPlayer::startSample() const noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return 0.0;
    return m_params.play.start * static_cast<double> (m_buffer->getNumSamples());
}

double ReelLoopPlayer::endSample() const noexcept
{
    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return 0.0;
    return m_params.play.end * static_cast<double> (m_buffer->getNumSamples());
}

double ReelLoopPlayer::bufferLength() const noexcept
{
    const double s = startSample();
    const double e = endSample();
    return (e > s) ? (e - s) : 0.0;
}

//==============================================================================
void ReelLoopPlayer::renderSample (float& left, float& right) noexcept
{
    left  = 0.0f;
    right = 0.0f;

    if (m_buffer == nullptr || !m_buffer->isLoaded())
        return;

    const double loopLen = bufferLength();
    if (loopLen <= 0.0)
        return;

    // Ensure playPos is within loop region
    const double s = startSample();
    const double e = endSample();

    if (m_playPos < s || m_playPos >= e)
        m_playPos = s;

    // Read sample with linear interpolation
    // TODO: proper pitch shifting independent of speed (phase vocoder)
    const int numCh = m_buffer->getNumChannels();
    if (numCh == 1)
    {
        const float mono = m_buffer->readSample (0, m_playPos);
        left  = mono;
        right = mono;
    }
    else
    {
        left  = m_buffer->readSample (0, m_playPos);
        right = m_buffer->readSample (1, m_playPos);
    }

    // Apply pan (equal-power law: simple linear for now)
    const float pan  = juce::jlimit (-1.0f, 1.0f, m_params.play.pan);
    const float panL = 1.0f - juce::jmax (0.0f, pan);
    const float panR = 1.0f + juce::jmin (0.0f, pan);
    left  *= panL * m_params.play.level;
    right *= panR * m_params.play.level;

    // Reverse flag
    const double playSpeed = m_params.play.reverse
                             ? -static_cast<double> (m_params.loop.speed)
                             :  static_cast<double> (m_params.loop.speed);

    m_playPos += playSpeed;

    // Wrap within loop region
    if (m_playPos >= e)
        m_playPos = s + std::fmod (m_playPos - s, loopLen);
    else if (m_playPos < s)
        m_playPos = e - std::fmod (s - m_playPos, loopLen);
}

void ReelLoopPlayer::setTransportPosition (double beatPosition, double bpm) noexcept
{
    if (!m_params.loop.sync || m_buffer == nullptr || !m_buffer->isLoaded())
        return;

    // Align playhead so loop start falls on bar boundary
    // beatPosition is the current transport beat
    const double loopLen    = bufferLength();
    if (loopLen <= 0.0 || m_sampleRate <= 0.0 || bpm <= 0.0)
        return;

    const double samplesPerBeat = m_sampleRate * 60.0 / bpm;
    const double loopBeats      = loopLen / samplesPerBeat;
    if (loopBeats <= 0.0) return;

    const double beatInLoop = std::fmod (beatPosition, loopBeats);
    m_playPos = startSample() + beatInLoop * samplesPerBeat;
}
