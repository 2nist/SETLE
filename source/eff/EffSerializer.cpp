#include "EffSerializer.h"

namespace setle::eff
{

static juce::String blockTypeToString(BlockType t)
{
    switch (t)
    {
        case BlockType::MidiProbability: return "MidiProbability";
        case BlockType::MidiVelocityCurve: return "MidiVelocityCurve";
        case BlockType::MidiHumanize: return "MidiHumanize";
        case BlockType::MidiQuantize: return "MidiQuantize";
        case BlockType::MidiTranspose: return "MidiTranspose";
        case BlockType::MidiArpeggiate: return "MidiArpeggiate";
        case BlockType::MidiBeatRepeat: return "MidiBeatRepeat";
        case BlockType::MidiStrumSpread: return "MidiStrumSpread";
        case BlockType::MidiNoteFilter: return "MidiNoteFilter";
        case BlockType::MidiChordVoicing: return "MidiChordVoicing";

        case BlockType::AudioGain: return "AudioGain";
        case BlockType::AudioFilter: return "AudioFilter";
        case BlockType::AudioSaturator: return "AudioSaturator";
        case BlockType::AudioBitCrusher: return "AudioBitCrusher";
        case BlockType::AudioDelay: return "AudioDelay";
        case BlockType::AudioReverb: return "AudioReverb";
        case BlockType::AudioChorus: return "AudioChorus";
        case BlockType::AudioCompressor: return "AudioCompressor";
        case BlockType::AudioTremolo: return "AudioTremolo";
        case BlockType::AudioStereoWidth: return "AudioStereoWidth";
        case BlockType::AudioDryWet: return "AudioDryWet";
    }
    return "";
}

static BlockType stringToBlockType(const juce::String& s)
{
    if (s == "MidiProbability") return BlockType::MidiProbability;
    if (s == "MidiVelocityCurve") return BlockType::MidiVelocityCurve;
    if (s == "MidiHumanize") return BlockType::MidiHumanize;
    if (s == "MidiQuantize") return BlockType::MidiQuantize;
    if (s == "MidiTranspose") return BlockType::MidiTranspose;
    if (s == "MidiArpeggiate") return BlockType::MidiArpeggiate;
    if (s == "MidiBeatRepeat") return BlockType::MidiBeatRepeat;
    if (s == "MidiStrumSpread") return BlockType::MidiStrumSpread;
    if (s == "MidiNoteFilter") return BlockType::MidiNoteFilter;
    if (s == "MidiChordVoicing") return BlockType::MidiChordVoicing;

    if (s == "AudioGain") return BlockType::AudioGain;
    if (s == "AudioFilter") return BlockType::AudioFilter;
    if (s == "AudioSaturator") return BlockType::AudioSaturator;
    if (s == "AudioBitCrusher") return BlockType::AudioBitCrusher;
    if (s == "AudioDelay") return BlockType::AudioDelay;
    if (s == "AudioReverb") return BlockType::AudioReverb;
    if (s == "AudioChorus") return BlockType::AudioChorus;
    if (s == "AudioCompressor") return BlockType::AudioCompressor;
    if (s == "AudioTremolo") return BlockType::AudioTremolo;
    if (s == "AudioStereoWidth") return BlockType::AudioStereoWidth;
    if (s == "AudioDryWet") return BlockType::AudioDryWet;

    return BlockType::AudioGain;
}

static juce::String hintToString(EffParam::ControlHint h)
{
    switch (h)
    {
        case EffParam::ControlHint::Knob: return "Knob";
        case EffParam::ControlHint::Slider: return "Slider";
        case EffParam::ControlHint::Toggle: return "Toggle";
        case EffParam::ControlHint::Selector: return "Selector";
    }
    return "Knob";
}

static EffParam::ControlHint stringToHint(const juce::String& s)
{
    if (s == "Knob") return EffParam::ControlHint::Knob;
    if (s == "Slider") return EffParam::ControlHint::Slider;
    if (s == "Toggle") return EffParam::ControlHint::Toggle;
    if (s == "Selector") return EffParam::ControlHint::Selector;
    return EffParam::ControlHint::Knob;
}

juce::String EffSerializer::serialize(const EffDefinition& def)
{
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("effId", def.effId);
    root->setProperty("name", def.name);
    root->setProperty("category", def.category);
    root->setProperty("description", def.description);
    root->setProperty("schemaVersion", def.schemaVersion);

    juce::Array<juce::var> blocks;
    for (const auto& b : def.blocks)
    {
        juce::DynamicObject::Ptr bo = new juce::DynamicObject();
        bo->setProperty("blockId", b.blockId);
        bo->setProperty("type", blockTypeToString(b.type));
        bo->setProperty("label", b.label);
        bo->setProperty("bypassed", b.bypassed);

        juce::Array<juce::var> params;
        for (const auto& p : b.params)
        {
            juce::DynamicObject::Ptr po = new juce::DynamicObject();
            po->setProperty("paramId", p.paramId);
            po->setProperty("label", p.label);
            po->setProperty("value", p.value);
            po->setProperty("min", p.minVal);
            po->setProperty("max", p.maxVal);
            po->setProperty("default", p.defaultVal);
            po->setProperty("hint", hintToString(p.hint));
            po->setProperty("unit", p.unit);
            params.add(juce::var(po));
        }

        bo->setProperty("params", juce::var(params));
        blocks.add(juce::var(bo));
    }

    root->setProperty("blocks", juce::var(blocks));

    juce::Array<juce::var> macros;
    for (const auto& m : def.macros)
    {
        juce::DynamicObject::Ptr mo = new juce::DynamicObject();
        mo->setProperty("macroIndex", m.macroIndex);
        mo->setProperty("blockId", m.blockId);
        mo->setProperty("paramId", m.paramId);
        mo->setProperty("macroLabel", m.macroLabel);
        macros.add(juce::var(mo));
    }
    root->setProperty("macros", juce::var(macros));

    return juce::JSON::toString(juce::var(root), true);
}

EffDefinition EffSerializer::deserialize(const juce::String& json)
{
    EffDefinition def;
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
    {
        auto* root = parsed.getDynamicObject();
        def.effId = root->getProperty("effId").toString();
        def.name = root->getProperty("name").toString();
        def.category = root->getProperty("category").toString();
        def.description = root->getProperty("description").toString();
        def.schemaVersion = static_cast<int>(root->getProperty("schemaVersion"));

        auto blocksVar = root->getProperty("blocks");
        if (blocksVar.isArray())
        {
            auto* arr = blocksVar.getArray();
            for (int i = 0; i < arr->size(); ++i)
            {
                const juce::var& bvar = arr->getReference(i);
                if (!bvar.isObject()) continue;
                auto* bo = bvar.getDynamicObject();
                EffBlock block;
                block.blockId = bo->getProperty("blockId").toString();
                block.type = stringToBlockType(bo->getProperty("type").toString());
                block.label = bo->getProperty("label").toString();
                block.bypassed = static_cast<bool>(bo->getProperty("bypassed"));

                auto paramsVar = bo->getProperty("params");
                if (paramsVar.isArray())
                {
                    auto* parr = paramsVar.getArray();
                    for (int pi = 0; pi < parr->size(); ++pi)
                    {
                        const juce::var& pvar = parr->getReference(pi);
                        if (!pvar.isObject()) continue;
                        auto* po = pvar.getDynamicObject();
                        EffParam p;
                        p.paramId = po->getProperty("paramId").toString();
                        p.label = po->getProperty("label").toString();
                        p.value = static_cast<float>(po->getProperty("value"));
                        p.minVal = static_cast<float>(po->getProperty("min"));
                        p.maxVal = static_cast<float>(po->getProperty("max"));
                        p.defaultVal = static_cast<float>(po->getProperty("default"));
                        p.hint = stringToHint(po->getProperty("hint").toString());
                        p.unit = po->getProperty("unit").toString();
                        block.params.push_back(std::move(p));
                    }
                }

                def.blocks.push_back(std::move(block));
            }
        }

        auto macrosVar = root->getProperty("macros");
        if (macrosVar.isArray())
        {
            auto* marr = macrosVar.getArray();
            for (int mi = 0; mi < marr->size(); ++mi)
            {
                const juce::var& mvar = marr->getReference(mi);
                if (!mvar.isObject()) continue;
                auto* mo = mvar.getDynamicObject();
                EffDefinition::MacroAssignment ma;
                ma.macroIndex = static_cast<int>(mo->getProperty("macroIndex"));
                ma.blockId = mo->getProperty("blockId").toString();
                ma.paramId = mo->getProperty("paramId").toString();
                ma.macroLabel = mo->getProperty("macroLabel").toString();
                def.macros.push_back(std::move(ma));
            }
        }
    }
    return def;
}

bool EffSerializer::saveToFile(const EffDefinition& def, const juce::File& file)
{
    const auto text = serialize(def);
    return file.replaceWithText(text, "\n", juce::File::writeText);
}

EffDefinition EffSerializer::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return EffDefinition();
    const auto text = file.loadFileAsString();
    return deserialize(text);
}

} // namespace setle::eff
