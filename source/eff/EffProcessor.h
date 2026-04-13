#pragma once

#include "EffModel.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <map>

namespace setle::eff
{

class EffProcessor
{
public:
    void loadDefinition(const EffDefinition& def);
    void prepare(double sampleRate, int blockSize);

    void processAudio(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void processMidi(juce::MidiBuffer& midiMessages, double currentBeat, double bpm);
    void setDrumSwingForNote(int midiNote, float swingPercent);

    void setParam(const juce::String& blockId, const juce::String& paramId, float value);
    void setMacro(int macroIndex, float value);

    const EffDefinition& getDefinition() const noexcept { return definition; }

    // Factory: create a new EffBlock with sensible defaults for each type
    static EffBlock createDefaultBlock(BlockType type);

private:
    EffDefinition definition;
    double sampleRate { 44100.0 };
    int    blockSize  { 512 };

    std::vector<EffBlock*> midiBlocks;
    std::vector<EffBlock*> audioBlocks;
    std::map<int, float> drumSwingByMidiNote;

    struct BlockState
    {
        juce::dsp::IIR::Filter<float> filterL;
        juce::dsp::IIR::Filter<float> filterR;
        juce::dsp::DelayLine<float>   delayLine { 192000 };
        juce::Reverb                  reverb;
        float lfoPhase    { 0.0f };
        float lfoPhaseR   { 0.0f };   // for stereo chorus spread
        float envFollower { 0.0f };
        juce::Random rng;
    };

    std::map<juce::String, BlockState> blockStates;

    // Internal helpers
    static float getParam(const EffBlock& block, const juce::String& paramId, float fallback = 0.0f);
    void processAudioBlock(EffBlock& block, BlockState& state, juce::AudioBuffer<float>& buf);
    void processMidiBlock(EffBlock& block, BlockState& state,
                          juce::MidiBuffer& midi, double currentBeat, double bpm);
};

} // namespace setle::eff
