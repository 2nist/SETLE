#include "EffTemplates.h"
#include "EffProcessor.h"

namespace setle::eff
{

namespace
{

EffDefinition makeTemplate(const juce::String& name,
                           const juce::String& category,
                           const juce::String& description,
                           std::initializer_list<BlockType> blockTypes)
{
    EffDefinition def;
    def.effId       = juce::Uuid().toString();
    def.name        = name;
    def.category    = category;
    def.description = description;
    def.schemaVersion = 1;

    for (const auto t : blockTypes)
        def.blocks.push_back(EffProcessor::createDefaultBlock(t));

    return def;
}

} // namespace

std::vector<EffDefinition> EffTemplates::getBuiltInTemplates()
{
    return {
        makeTemplate("Clean Boost",
                     "Utility",
                     "Transparent level trim or boost.",
                     { BlockType::AudioGain }),

        makeTemplate("Warm Lo-Fi",
                     "Texture",
                     "Bit-crushed texture with LP filter roll-off.",
                     { BlockType::AudioBitCrusher, BlockType::AudioFilter }),

        makeTemplate("Filter Sweep",
                     "Modulation",
                     "Resonant filter into parallel dry/wet blend.",
                     { BlockType::AudioFilter, BlockType::AudioDryWet }),

        makeTemplate("Room Verb",
                     "Ambience",
                     "Algorithmic room reverb for natural depth.",
                     { BlockType::AudioReverb }),

        makeTemplate("Tempo Delay",
                     "Time",
                     "Tempo-synced delay with feedback.",
                     { BlockType::AudioDelay }),

        makeTemplate("Chorus Pad",
                     "Modulation",
                     "Lush chorus into reverb tail for wide pad sound.",
                     { BlockType::AudioChorus, BlockType::AudioReverb }),

        makeTemplate("Compress + Sat",
                     "Dynamics",
                     "Compressor into soft saturation for glue and heat.",
                     { BlockType::AudioCompressor, BlockType::AudioSaturator }),

        makeTemplate("Humanize",
                     "MIDI",
                     "Subtle timing and velocity randomisation for a live feel.",
                     { BlockType::MidiHumanize, BlockType::MidiVelocityCurve }),

        makeTemplate("Beat Repeat",
                     "MIDI",
                     "Triggered MIDI loop-repeat of the last N beats.",
                     { BlockType::MidiBeatRepeat }),

        makeTemplate("Strum Chord",
                     "MIDI",
                     "Micro-time strum spread flowing into an arpeggiated pattern.",
                     { BlockType::MidiStrumSpread, BlockType::MidiArpeggiate }),

        makeTemplate("Probability Gate",
                     "MIDI",
                     "Stochastic note gating — thinning dense patterns.",
                     { BlockType::MidiProbability }),

        makeTemplate("Full Mix Bus",
                     "Mastering",
                     "Compressor → saturator → stereo width for a mix bus chain.",
                     { BlockType::AudioCompressor, BlockType::AudioSaturator, BlockType::AudioStereoWidth }),
    };
}

EffDefinition EffTemplates::findByName(const juce::String& name)
{
    for (const auto& t : getBuiltInTemplates())
        if (t.name.equalsIgnoreCase(name))
            return t;
    return {};
}

} // namespace setle::eff
