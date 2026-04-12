#include "ReelProcessor.h"

//==============================================================================
ReelProcessor::ReelProcessor()
{
    // Default params already set by ReelParams constructor
}

//==============================================================================
void ReelProcessor::prepare (double sampleRate, int blockSize)
{
    m_sampleRate = sampleRate;
    m_blockSize  = blockSize;

    m_loopPlayer.prepare (sampleRate, blockSize);
    m_granular.prepare   (sampleRate, blockSize);
    m_sampler.prepare    (sampleRate, blockSize);
    m_slicer.prepare     (sampleRate, blockSize);

    syncParams();
}

void ReelProcessor::reset()
{
    m_loopPlayer.reset();
    m_granular.reset();
}

//==============================================================================
void ReelProcessor::syncParams()
{
    m_loopPlayer.setBuffer (&m_buffer);
    m_loopPlayer.setParams  (m_params);
    m_granular.setBuffer    (&m_buffer);
    m_granular.setParams    (m_params);
    m_sampler.setBuffer     (&m_buffer);
    m_sampler.setParams     (m_params);
    m_slicer.setBuffer      (&m_buffer);
    m_slicer.setParams      (m_params);
}

//==============================================================================
void ReelProcessor::process (juce::AudioBuffer<float>& audio,
                              juce::MidiBuffer&         midi)
{
    if (!m_buffer.isLoaded())
        return;

    // Always sync params at block start (cheap struct copy, no allocation)
    syncParams();

    audio.clear();

    const int numSamples = audio.getNumSamples();

    switch (m_params.mode)
    {
        case ReelMode::loop:
            for (int i = 0; i < numSamples; ++i)
            {
                float l = 0.0f, r = 0.0f;
                m_loopPlayer.renderSample (l, r);
                audio.addSample (0, i, l);
                if (audio.getNumChannels() > 1)
                    audio.addSample (1, i, r);
                else
                    audio.addSample (0, i, r);
            }
            break;

        case ReelMode::grain:
            m_granular.renderBlock (audio, 0, numSamples);
            break;

        case ReelMode::sample:
            // Process MIDI first
            for (const auto meta : midi)
            {
                const auto msg = meta.getMessage();
                if (msg.isNoteOn())
                    m_sampler.noteOn  (msg.getNoteNumber(), msg.getFloatVelocity());
                else if (msg.isNoteOff())
                    m_sampler.noteOff (msg.getNoteNumber());
            }
            m_sampler.renderBlock (audio, 0, numSamples);
            break;

        case ReelMode::slice:
            m_slicer.renderBlock (audio, 0, numSamples);
            break;
    }
}

//==============================================================================
void ReelProcessor::loadFromBuffer (const juce::AudioBuffer<float>& src,
                                    double sampleRate)
{
    m_buffer.loadFromBuffer (src, sampleRate > 0.0 ? sampleRate : m_sampleRate);
    syncParams();
    m_loopPlayer.reset();
    m_granular.reset();
}

bool ReelProcessor::loadFromFile (const juce::File& file)
{
    if (!m_buffer.loadFromFile (file))
        return false;

    syncParams();
    m_loopPlayer.reset();
    m_granular.reset();
    return true;
}

void ReelProcessor::clearBuffer()
{
    m_buffer.clear();
}

//==============================================================================
void ReelProcessor::setMode (ReelMode mode)
{
    m_params.mode = mode;
    syncParams();

    if (mode == ReelMode::loop)
        m_loopPlayer.reset();
    else if (mode == ReelMode::grain)
        m_granular.reset();
}

//==============================================================================
void ReelProcessor::setParam (juce::StringRef paramId, float value) noexcept
{
    m_params.setFloat (paramId, value);
    // syncParams() called at next process() block start
}

float ReelProcessor::getParam (juce::StringRef paramId) const noexcept
{
    return m_params.getFloat (paramId);
}

//==============================================================================
void ReelProcessor::triggerSlice (int sliceIndex, float velocity) noexcept
{
    m_slicer.triggerSlice (sliceIndex, velocity);
}

//==============================================================================
void ReelProcessor::setTransportPosition (double beat, double bpm) noexcept
{
    m_loopPlayer.setTransportPosition (beat, bpm);
}

//==============================================================================
int ReelProcessor::getGrainPositions (float* positions, int maxPositions) const noexcept
{
    return m_granular.getGrainPositions (positions, maxPositions);
}

//==============================================================================
// Binary layout (version 1):
//   uint32  magic  = 0x5245454C ('REEL')
//   uint32  version = 1
//   int32   mode
//   float   play.start, play.end, play.level, play.pan
//   uint8   play.reverse
//   float   loop.speed, loop.pitch
//   uint8   loop.sync
//   int32   sample.root
//   uint8   sample.oneshot
//   float   grain.position, grain.size, grain.density, grain.pitch, grain.spread, grain.scatter
//   int32   grain.envelope
//   int32   slice.count, slice.order
//   uint8   slice.quantize
//   float   out.level, out.pan

static constexpr juce::uint32 kReelStateMagic   = 0x5245454Cu;
static constexpr juce::uint32 kReelStateVersion = 1u;

void ReelProcessor::getState (juce::MemoryBlock& destData) const
{
    juce::MemoryOutputStream s (destData, false);
    s.writeInt  (static_cast<int> (kReelStateMagic));
    s.writeInt  (static_cast<int> (kReelStateVersion));
    s.writeInt  (static_cast<int> (m_params.mode));
    s.writeFloat (m_params.play.start);
    s.writeFloat (m_params.play.end);
    s.writeFloat (m_params.play.level);
    s.writeFloat (m_params.play.pan);
    s.writeByte  (m_params.play.reverse ? 1 : 0);
    s.writeFloat (m_params.loop.speed);
    s.writeFloat (m_params.loop.pitch);
    s.writeByte  (m_params.loop.sync ? 1 : 0);
    s.writeInt   (m_params.sample.root);
    s.writeByte  (m_params.sample.oneshot ? 1 : 0);
    s.writeFloat (m_params.grain.position);
    s.writeFloat (m_params.grain.size);
    s.writeFloat (m_params.grain.density);
    s.writeFloat (m_params.grain.pitch);
    s.writeFloat (m_params.grain.spread);
    s.writeFloat (m_params.grain.scatter);
    s.writeInt   (m_params.grain.envelope);
    s.writeInt   (m_params.slice.count);
    s.writeInt   (m_params.slice.order);
    s.writeByte  (m_params.slice.quantize ? 1 : 0);
    s.writeFloat (m_params.out.level);
    s.writeFloat (m_params.out.pan);
}

bool ReelProcessor::setState (const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes < 8)
        return false;

    juce::MemoryInputStream s (data, static_cast<size_t> (sizeInBytes), false);
    if (static_cast<uint32_t> (s.readInt()) != kReelStateMagic)   return false;
    if (static_cast<uint32_t> (s.readInt()) != kReelStateVersion)  return false;

    m_params.mode               = static_cast<ReelMode> (s.readInt());
    m_params.play.start         = s.readFloat();
    m_params.play.end           = s.readFloat();
    m_params.play.level         = s.readFloat();
    m_params.play.pan           = s.readFloat();
    m_params.play.reverse       = s.readByte() != 0;
    m_params.loop.speed         = s.readFloat();
    m_params.loop.pitch         = s.readFloat();
    m_params.loop.sync          = s.readByte() != 0;
    m_params.sample.root        = s.readInt();
    m_params.sample.oneshot     = s.readByte() != 0;
    m_params.grain.position     = s.readFloat();
    m_params.grain.size         = s.readFloat();
    m_params.grain.density      = s.readFloat();
    m_params.grain.pitch        = s.readFloat();
    m_params.grain.spread       = s.readFloat();
    m_params.grain.scatter      = s.readFloat();
    m_params.grain.envelope     = s.readInt();
    m_params.slice.count        = s.readInt();
    m_params.slice.order        = s.readInt();
    m_params.slice.quantize     = s.readByte() != 0;
    m_params.out.level          = s.readFloat();
    m_params.out.pan            = s.readFloat();

    syncParams();
    return true;
}
