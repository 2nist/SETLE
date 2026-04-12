#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace setle::eff
{

enum class BlockType
{
    MidiProbability,
    MidiVelocityCurve,
    MidiHumanize,
    MidiQuantize,
    MidiTranspose,
    MidiArpeggiate,
    MidiBeatRepeat,
    MidiStrumSpread,
    MidiNoteFilter,
    MidiChordVoicing,

    AudioGain,
    AudioFilter,
    AudioSaturator,
    AudioBitCrusher,
    AudioDelay,
    AudioReverb,
    AudioChorus,
    AudioCompressor,
    AudioTremolo,
    AudioStereoWidth,
    AudioDryWet,
};

struct EffParam
{
    enum class ControlHint { Knob, Slider, Toggle, Selector };

    juce::String paramId;
    juce::String label;
    float value { 0.0f };
    float minVal { 0.0f };
    float maxVal { 1.0f };
    float defaultVal { 0.0f };
    ControlHint hint { ControlHint::Knob };
    juce::String unit;
};

struct EffBlock
{
    juce::String blockId;
    BlockType type { BlockType::AudioGain };
    juce::String label;
    bool bypassed { false };
    std::vector<EffParam> params;
};

struct EffDefinition
{
    juce::String effId;
    juce::String name;
    juce::String category;
    juce::String description;
    int schemaVersion { 1 };
    std::vector<EffBlock> blocks;

    struct MacroAssignment
    {
        int macroIndex { 0 };
        juce::String blockId;
        juce::String paramId;
        juce::String macroLabel;
    };

    std::vector<MacroAssignment> macros;
};

} // namespace setle::eff
