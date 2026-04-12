#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "ReelBuffer.h"
#include "ReelParams.h"
#include "ReelLoopPlayer.h"
#include "ReelGranular.h"
#include "ReelSampler.h"
#include "ReelSlicer.h"
#include "../../module/ModuleProcessor.h"

//==============================================================================
/**
    ReelProcessor — top-level instrument processor for one REEL slot.

    Owns the buffer, params, and all mode sub-processors.
    Dispatches process() to the active mode's engine.

    Thread safety:
      - loadFromBuffer / loadFromFile: message thread only
      - setParam / setMode:            message thread only (uses simple copy)
      - process:                       audio thread only
      - getBuffer / getParams:         read-only, safe after load
*/
class ReelProcessor : public ModuleProcessor
{
public:
    ReelProcessor();

    //==========================================================================
    // ModuleProcessor identity

    const char* getModuleId()   const noexcept override { return "com.spool.reel"; }
    const char* getModuleName() const noexcept override { return "Reel"; }

    // Reel params are accessed via setParam/getParam string IDs (ReelParams scheme)
    // rather than a manifest; expose empty manifest for now.
    const ParamDef* getParamDefs()  const noexcept override { return nullptr; }
    int             getNumParams()  const noexcept override { return 0; }

    //==========================================================================
    // Lifecycle

    void prepare (double sampleRate, int blockSize);
    void reset();

    //==========================================================================
    // Audio processing

    void process (juce::AudioBuffer<float>& audio,
                  juce::MidiBuffer&         midi);

    //==========================================================================
    // Buffer loading (message thread)

    void loadFromBuffer (const juce::AudioBuffer<float>& src,
                         double sampleRate = 44100.0);
    bool loadFromFile   (const juce::File& file);
    void clearBuffer();

    bool isLoaded() const noexcept { return m_buffer.isLoaded(); }

    //==========================================================================
    // Mode

    void     setMode (ReelMode mode);
    ReelMode getMode() const noexcept { return m_params.mode; }

    //==========================================================================
    // Params

    void  setParam (juce::StringRef paramId, float value) noexcept override;
    float getParam (juce::StringRef paramId) const noexcept override;

    //==========================================================================
    // Direct slice trigger (from step sequencer — audio thread safe)

    void triggerSlice (int sliceIndex, float velocity) noexcept;

    //==========================================================================
    // Transport sync

    void setTransportPosition (double beat, double bpm) noexcept;

    //==========================================================================
    // Read-only access for display

    const ReelBuffer& getBuffer() const noexcept { return m_buffer; }
    const ReelParams& getParams() const noexcept { return m_params; }

    /** Grain positions snapshot for Zone A grain cloud display. */
    int getGrainPositions (float* positions, int maxPositions) const noexcept;

    //==========================================================================
    // State persistence

    void getState (juce::MemoryBlock& destData) const override;
    bool setState (const void* data, int sizeInBytes) override;

private:
    ReelBuffer     m_buffer;
    ReelParams     m_params;
    ReelLoopPlayer m_loopPlayer;
    ReelGranular   m_granular;
    ReelSampler    m_sampler;
    ReelSlicer     m_slicer;

    double m_sampleRate { 44100.0 };
    int    m_blockSize  { 256 };

    /** Push current params to all sub-processors. */
    void syncParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReelProcessor)
};
