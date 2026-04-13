#include "EffProcessor.h"

#include <cmath>
#include <algorithm>

namespace setle::eff
{

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool isMidiBlockType(BlockType t)
{
    switch (t)
    {
        case BlockType::MidiProbability:
        case BlockType::MidiVelocityCurve:
        case BlockType::MidiHumanize:
        case BlockType::MidiQuantize:
        case BlockType::MidiTranspose:
        case BlockType::MidiArpeggiate:
        case BlockType::MidiBeatRepeat:
        case BlockType::MidiStrumSpread:
        case BlockType::MidiNoteFilter:
        case BlockType::MidiChordVoicing:
            return true;
        default:
            return false;
    }
}

static juce::String blockTypeToDisplayName(BlockType t)
{
    switch (t)
    {
        case BlockType::MidiProbability:  return "Probability";
        case BlockType::MidiVelocityCurve:return "Velocity Curve";
        case BlockType::MidiHumanize:     return "Humanize";
        case BlockType::MidiQuantize:     return "Quantize";
        case BlockType::MidiTranspose:    return "Transpose";
        case BlockType::MidiArpeggiate:   return "Arpeggiate";
        case BlockType::MidiBeatRepeat:   return "Beat Repeat";
        case BlockType::MidiStrumSpread:  return "Strum Spread";
        case BlockType::MidiNoteFilter:   return "Note Filter";
        case BlockType::MidiChordVoicing: return "Chord Voicing";
        case BlockType::AudioGain:        return "Gain";
        case BlockType::AudioFilter:      return "Filter";
        case BlockType::AudioSaturator:   return "Saturator";
        case BlockType::AudioBitCrusher:  return "Bit Crusher";
        case BlockType::AudioDelay:       return "Delay";
        case BlockType::AudioReverb:      return "Reverb";
        case BlockType::AudioChorus:      return "Chorus";
        case BlockType::AudioCompressor:  return "Compressor";
        case BlockType::AudioTremolo:     return "Tremolo";
        case BlockType::AudioStereoWidth: return "Stereo Width";
        case BlockType::AudioDryWet:      return "Dry/Wet";
    }
    return "Unknown";
}

static EffParam makeParam(const juce::String& id, const juce::String& label,
                          float val, float mn, float mx, float def,
                          EffParam::ControlHint hint, const juce::String& unit = "")
{
    EffParam p;
    p.paramId = id;
    p.label = label;
    p.value = val;
    p.minVal = mn;
    p.maxVal = mx;
    p.defaultVal = def;
    p.hint = hint;
    p.unit = unit;
    return p;
}

using K = EffParam::ControlHint;

// ─────────────────────────────────────────────────────────────────────────────
// createDefaultBlock — factory
// ─────────────────────────────────────────────────────────────────────────────

EffBlock EffProcessor::createDefaultBlock(BlockType type)
{
    EffBlock b;
    b.type = type;
    b.label = blockTypeToDisplayName(type);
    b.blockId = juce::Uuid().toString();
    b.bypassed = false;

    switch (type)
    {
        case BlockType::MidiProbability:
            b.params = {
                makeParam("prob",  "Probability", 1.0f, 0.0f, 1.0f, 1.0f, K::Knob, "%"),
            };
            break;

        case BlockType::MidiVelocityCurve:
            b.params = {
                makeParam("curve", "Curve",  0.0f, -1.0f, 1.0f, 0.0f, K::Slider, ""),
                makeParam("min",   "Min Vel",  0.0f, 0.0f, 127.0f, 0.0f, K::Knob, ""),
                makeParam("max",   "Max Vel", 127.0f, 0.0f, 127.0f, 127.0f, K::Knob, ""),
            };
            break;

        case BlockType::MidiHumanize:
            b.params = {
                makeParam("timingAmt", "Timing",  10.0f, 0.0f, 100.0f, 10.0f, K::Knob, "ms"),
                makeParam("velocityAmt","Velocity", 10.0f, 0.0f, 40.0f, 10.0f, K::Knob, ""),
            };
            break;

        case BlockType::MidiQuantize:
            b.params = {
                makeParam("grid",     "Grid",   0.25f, 0.0f, 1.0f, 0.25f, K::Selector, ""),
                makeParam("strength", "Strength", 1.0f, 0.0f, 1.0f, 1.0f, K::Knob, "%"),
            };
            break;

        case BlockType::MidiTranspose:
            b.params = {
                makeParam("semitones", "Semitones", 0.0f, -24.0f, 24.0f, 0.0f, K::Slider, "st"),
            };
            break;

        case BlockType::MidiArpeggiate:
            b.params = {
                makeParam("rate",    "Rate",    0.25f, 0.0625f, 1.0f, 0.25f, K::Knob, "beats"),
                makeParam("pattern", "Pattern", 0.0f, 0.0f, 3.0f, 0.0f, K::Selector, ""),
                makeParam("octaves", "Octaves", 1.0f, 1.0f, 4.0f, 1.0f, K::Knob, ""),
            };
            break;

        case BlockType::MidiBeatRepeat:
            b.params = {
                makeParam("length",   "Length",  1.0f, 0.25f, 4.0f, 1.0f, K::Knob, "beats"),
                makeParam("trigger",  "Trigger", 0.0f, 0.0f, 1.0f, 0.0f, K::Toggle, ""),
            };
            break;

        case BlockType::MidiStrumSpread:
            b.params = {
                makeParam("spread", "Spread", 20.0f, 0.0f, 200.0f, 20.0f, K::Knob, "ms"),
            };
            break;

        case BlockType::MidiNoteFilter:
            b.params = {
                makeParam("allowMask", "Allow Mask", 0xFFF, 0.0f, 0xFFF, 0xFFF, K::Selector, ""),
            };
            break;

        case BlockType::MidiChordVoicing:
            b.params = {
                makeParam("inversion", "Inversion", 0.0f, 0.0f, 3.0f, 0.0f, K::Selector, ""),
                makeParam("spread",    "Spread",    0.0f, 0.0f, 1.0f, 0.0f, K::Toggle, ""),
            };
            break;

        case BlockType::AudioGain:
            b.params = {
                makeParam("gain", "Gain", 0.0f, -24.0f, 12.0f, 0.0f, K::Knob, "dB"),
            };
            break;

        case BlockType::AudioFilter:
            b.params = {
                makeParam("type",   "Type",    0.0f, 0.0f, 2.0f, 0.0f, K::Selector, ""),
                makeParam("cutoff", "Cutoff", 1000.0f, 20.0f, 20000.0f, 1000.0f, K::Knob, "Hz"),
                makeParam("res",    "Resonance", 0.5f, 0.1f, 10.0f, 0.5f, K::Knob, ""),
            };
            break;

        case BlockType::AudioSaturator:
            b.params = {
                makeParam("drive",  "Drive",  0.5f, 0.0f, 1.0f, 0.5f, K::Knob, ""),
                makeParam("output", "Output", 0.0f, -12.0f, 0.0f, 0.0f, K::Knob, "dB"),
            };
            break;

        case BlockType::AudioBitCrusher:
            b.params = {
                makeParam("bits",       "Bit Depth",   12.0f, 2.0f, 16.0f, 12.0f, K::Knob, "bits"),
                makeParam("sampleRate", "Sample Rate", 1.0f, 0.0f, 1.0f, 1.0f, K::Slider, "%"),
            };
            break;

        case BlockType::AudioDelay:
            b.params = {
                makeParam("time",     "Time",      0.25f, 0.0625f, 2.0f, 0.25f, K::Knob, "beats"),
                makeParam("feedback", "Feedback",  0.4f, 0.0f, 0.95f, 0.4f, K::Knob, "%"),
                makeParam("mix",      "Mix",       0.5f, 0.0f, 1.0f, 0.5f, K::Knob, "%"),
            };
            break;

        case BlockType::AudioReverb:
            b.params = {
                makeParam("mix",   "Mix",        0.3f, 0.0f, 1.0f, 0.3f, K::Knob, "%"),
                makeParam("size",  "Size",       0.5f, 0.0f, 1.0f, 0.5f, K::Knob, ""),
                makeParam("damp",  "Damping",    0.5f, 0.0f, 1.0f, 0.5f, K::Knob, ""),
                makeParam("width", "Width",      1.0f, 0.0f, 1.0f, 1.0f, K::Knob, ""),
            };
            break;

        case BlockType::AudioChorus:
            b.params = {
                makeParam("rate",  "Rate",  0.5f, 0.1f, 5.0f, 0.5f, K::Knob, "Hz"),
                makeParam("depth", "Depth", 0.5f, 0.0f, 1.0f, 0.5f, K::Knob, ""),
                makeParam("mix",   "Mix",   0.5f, 0.0f, 1.0f, 0.5f, K::Knob, "%"),
            };
            break;

        case BlockType::AudioCompressor:
            b.params = {
                makeParam("threshold", "Threshold", -12.0f, -60.0f, 0.0f, -12.0f, K::Knob, "dB"),
                makeParam("ratio",     "Ratio",      4.0f,  1.0f, 20.0f,  4.0f,  K::Knob, ":1"),
                makeParam("attack",    "Attack",    10.0f,  0.1f, 200.0f, 10.0f,  K::Knob, "ms"),
                makeParam("release",   "Release",  100.0f,  5.0f, 2000.0f,100.0f, K::Knob, "ms"),
                makeParam("makeupGain","Make-up",    0.0f, -6.0f, 24.0f,  0.0f,  K::Knob, "dB"),
            };
            break;

        case BlockType::AudioTremolo:
            b.params = {
                makeParam("rate",  "Rate",  4.0f, 0.1f, 20.0f, 4.0f, K::Knob, "Hz"),
                makeParam("depth", "Depth", 0.8f, 0.0f, 1.0f,  0.8f, K::Knob, ""),
            };
            break;

        case BlockType::AudioStereoWidth:
            b.params = {
                makeParam("width", "Width", 1.0f, 0.0f, 2.0f, 1.0f, K::Slider, ""),
            };
            break;

        case BlockType::AudioDryWet:
            b.params = {
                makeParam("mix", "Mix", 0.5f, 0.0f, 1.0f, 0.5f, K::Knob, "%"),
            };
            break;
    }

    return b;
}

// ─────────────────────────────────────────────────────────────────────────────
// getParam helper
// ─────────────────────────────────────────────────────────────────────────────

float EffProcessor::getParam(const EffBlock& block, const juce::String& paramId, float fallback)
{
    for (const auto& p : block.params)
        if (p.paramId == paramId)
            return p.value;
    return fallback;
}

// ─────────────────────────────────────────────────────────────────────────────
// loadDefinition
// ─────────────────────────────────────────────────────────────────────────────

void EffProcessor::loadDefinition(const EffDefinition& def)
{
    definition = def;

    midiBlocks.clear();
    audioBlocks.clear();
    blockStates.clear();

    // Sort: MIDI blocks first, audio blocks after
    for (auto& block : definition.blocks)
    {
        if (isMidiBlockType(block.type))
            midiBlocks.push_back(&block);
        else
            audioBlocks.push_back(&block);

        // Allocate state
        blockStates.try_emplace(block.blockId);
    }

    // Re-prepare if already prepared
    if (sampleRate > 0.0 && blockSize > 0)
        prepare(sampleRate, blockSize);
}

// ─────────────────────────────────────────────────────────────────────────────
// prepare
// ─────────────────────────────────────────────────────────────────────────────

void EffProcessor::prepare(double sr, int bs)
{
    sampleRate = sr;
    blockSize  = bs;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sr;
    spec.maximumBlockSize = static_cast<juce::uint32>(bs);
    spec.numChannels      = 1;

    for (auto& [id, state] : blockStates)
    {
        state.filterL.reset();
        state.filterR.reset();
        state.filterL.prepare(spec);
        state.filterR.prepare(spec);
        state.delayLine.reset();
        state.delayLine.prepare(spec);
        state.reverb.setSampleRate(sr);
        state.lfoPhase  = 0.0f;
        state.lfoPhaseR = 0.3f; // slight phase offset for stereo spread
        state.envFollower = 0.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setParam
// ─────────────────────────────────────────────────────────────────────────────

void EffProcessor::setParam(const juce::String& blockId,
                            const juce::String& paramId,
                            float value)
{
    for (auto& block : definition.blocks)
    {
        if (block.blockId != blockId)
            continue;
        for (auto& p : block.params)
        {
            if (p.paramId == paramId)
            {
                p.value = juce::jlimit(p.minVal, p.maxVal, value);
                return;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setMacro
// ─────────────────────────────────────────────────────────────────────────────

void EffProcessor::setMacro(int macroIndex, float normalizedValue)
{
    const float clamped = juce::jlimit(0.0f, 1.0f, normalizedValue);

    for (const auto& macro : definition.macros)
    {
        if (macro.macroIndex != macroIndex)
            continue;

        for (auto& block : definition.blocks)
        {
            if (block.blockId != macro.blockId)
                continue;
            for (auto& p : block.params)
            {
                if (p.paramId == macro.paramId)
                {
                    p.value = p.minVal + clamped * (p.maxVal - p.minVal);
                    return;
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// processMidi — dispatches per MIDI block type
// ─────────────────────────────────────────────────────────────────────────────

void EffProcessor::processMidiBlock(EffBlock& block, BlockState& state,
                                    juce::MidiBuffer& midiIn,
                                    double /*currentBeat*/, double bpm)
{
    if (block.bypassed)
        return;

    juce::MidiBuffer out;

    switch (block.type)
    {
        // ── MidiProbability ──────────────────────────────────────────────────
        case BlockType::MidiProbability:
        {
            const float prob = getParam(block, "prob", 1.0f);
            for (const auto meta : midiIn)
            {
                auto msg = meta.getMessage();
                if (msg.isNoteOn())
                {
                    if (state.rng.nextFloat() <= prob)
                        out.addEvent(msg, meta.samplePosition);
                    // matching NoteOff is passed through regardless
                }
                else
                {
                    out.addEvent(msg, meta.samplePosition);
                }
            }
            midiIn = out;
            break;
        }

        // ── MidiVelocityCurve ────────────────────────────────────────────────
        case BlockType::MidiVelocityCurve:
        {
            const float curve   = getParam(block, "curve",  0.0f);
            const float velMin  = getParam(block, "min",    0.0f);
            const float velMax  = getParam(block, "max",   127.0f);

            for (const auto meta : midiIn)
            {
                auto msg = meta.getMessage();
                if (msg.isNoteOn())
                {
                    const float v = static_cast<float>(msg.getVelocity()) / 127.0f;
                    float shaped = v;
                    if (curve > 0.0f)
                        shaped = std::pow(v, 1.0f + curve * 2.0f);
                    else if (curve < 0.0f)
                        shaped = 1.0f - std::pow(1.0f - v, 1.0f - curve * 2.0f);

                    const float mapped = velMin + shaped * (velMax - velMin);
                    msg = juce::MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(),
                                                   static_cast<juce::uint8>(juce::jlimit(0.0f, 127.0f, mapped)));
                }
                out.addEvent(msg, meta.samplePosition);
            }
            midiIn = out;
            break;
        }

        // ── MidiHumanize ─────────────────────────────────────────────────────
        case BlockType::MidiHumanize:
        {
            const float timingAmt  = getParam(block, "timingAmt",  10.0f); // ms
            const float velocityAmt= getParam(block, "velocityAmt",10.0f);

            const int maxTimingSamples = static_cast<int>(timingAmt * 0.001 * sampleRate);

            for (const auto meta : midiIn)
            {
                auto msg = meta.getMessage();
                int pos  = meta.samplePosition;

                if (msg.isNoteOn())
                {
                    // Random timing offset
                    pos += static_cast<int>((state.rng.nextFloat() * 2.0f - 1.0f) * maxTimingSamples);
                    pos = std::max(0, pos);

                    // Random velocity offset
                    const int velOff = static_cast<int>((state.rng.nextFloat() * 2.0f - 1.0f) * velocityAmt);
                    const int vel = juce::jlimit(1, 127, msg.getVelocity() + velOff);
                    msg = juce::MidiMessage::noteOn(msg.getChannel(), msg.getNoteNumber(),
                                                   static_cast<juce::uint8>(vel));
                }
                out.addEvent(msg, pos);
            }
            midiIn = out;
            break;
        }

        // ── MidiQuantize ──────────────────────────────────────────────────────
        case BlockType::MidiQuantize:
        {
            const float grid     = getParam(block, "grid",     0.25f); // beats
            const float strength = getParam(block, "strength", 1.0f);

            const double samplesPerBeat = sampleRate * 60.0 / bpm;
            const double gridSamples    = grid * samplesPerBeat;

            for (const auto meta : midiIn)
            {
                auto msg = meta.getMessage();
                int pos  = meta.samplePosition;

                if (msg.isNoteOn() && gridSamples > 0.0)
                {
                    const double nearest = std::round(pos / gridSamples) * gridSamples;
                    pos = static_cast<int>(pos + strength * (nearest - pos));
                    pos = std::max(0, pos);
                }
                out.addEvent(msg, pos);
            }
            midiIn = out;
            break;
        }

        // ── MidiTranspose ─────────────────────────────────────────────────────
        case BlockType::MidiTranspose:
        {
            const int semitones = static_cast<int>(getParam(block, "semitones", 0.0f));

            for (const auto meta : midiIn)
            {
                auto msg = meta.getMessage();
                if (msg.isNoteOn() || msg.isNoteOff())
                {
                    const int note = juce::jlimit(0, 127, msg.getNoteNumber() + semitones);
                    if (msg.isNoteOn())
                        msg = juce::MidiMessage::noteOn(msg.getChannel(), note,
                                                        static_cast<juce::uint8>(msg.getVelocity()));
                    else
                        msg = juce::MidiMessage::noteOff(msg.getChannel(), note);
                }
                out.addEvent(msg, meta.samplePosition);
            }
            midiIn = out;
            break;
        }

        // ── MidiArpeggiate ───────────────────────────────────────────────────
        // Simplified: just pass through (full arp needs persistent note state across blocks)
        case BlockType::MidiArpeggiate:
            break;

        // ── MidiBeatRepeat ────────────────────────────────────────────────────
        // Simplified: pass through (requires cross-buffer state)
        case BlockType::MidiBeatRepeat:
            break;

        // ── MidiStrumSpread ───────────────────────────────────────────────────
        case BlockType::MidiStrumSpread:
        {
            const float spreadMs = getParam(block, "spread", 20.0f);
            const int spreadSamples = static_cast<int>(spreadMs * 0.001 * sampleRate);

            int noteCount = 0;
            for (const auto meta : midiIn)
            {
                auto msg = meta.getMessage();
                int pos  = meta.samplePosition;
                if (msg.isNoteOn())
                    pos += noteCount++ * (spreadSamples / std::max(1, noteCount + 1));
                out.addEvent(msg, pos);
            }
            midiIn = out;
            break;
        }

        // ── MidiNoteFilter ────────────────────────────────────────────────────
        case BlockType::MidiNoteFilter:
        {
            const int mask = static_cast<int>(getParam(block, "allowMask", 0xFFF));
            for (const auto meta : midiIn)
            {
                auto msg = meta.getMessage();
                if (msg.isNoteOn() || msg.isNoteOff())
                {
                    const int pitchClass = ((msg.getNoteNumber() % 12) + 12) % 12;
                    if ((mask >> pitchClass) & 1)
                        out.addEvent(msg, meta.samplePosition);
                }
                else
                {
                    out.addEvent(msg, meta.samplePosition);
                }
            }
            midiIn = out;
            break;
        }

        // ── MidiChordVoicing ──────────────────────────────────────────────────
        case BlockType::MidiChordVoicing:
        {
            const int inversion = static_cast<int>(getParam(block, "inversion", 0.0f));

            // Collect note-on events per position, apply inversion by octave shifting
            for (const auto meta : midiIn)
            {
                auto msg = meta.getMessage();
                if (msg.isNoteOn() && inversion > 0)
                {
                    int note = msg.getNoteNumber();
                    // Raise lowest note by octave for each inversion step
                    note = juce::jlimit(0, 127, note + inversion * 12);
                    msg = juce::MidiMessage::noteOn(msg.getChannel(), note,
                                                   static_cast<juce::uint8>(msg.getVelocity()));
                }
                out.addEvent(msg, meta.samplePosition);
            }
            midiIn = out;
            break;
        }

        default:
            break;
    }

    (void) state;
}

void EffProcessor::processMidi(juce::MidiBuffer& midiMessages,
                               double currentBeat, double bpm)
{
    if (!drumSwingByMidiNote.empty())
    {
        juce::MidiBuffer swung;
        const double stepDurationMs = (60.0 / juce::jmax(1.0, bpm)) * 1000.0 * 0.25;
        const int stepIndex = static_cast<int>(std::floor(currentBeat * 4.0));
        const bool offbeat = (stepIndex % 2) == 1;

        for (const auto metadata : midiMessages)
        {
            auto message = metadata.getMessage();
            auto sampleOffset = metadata.samplePosition;

            if (offbeat && message.isNoteOn())
            {
                const auto note = message.getNoteNumber();
                if (const auto it = drumSwingByMidiNote.find(note); it != drumSwingByMidiNote.end())
                {
                    const auto swingPercent = juce::jlimit(0.0f, 1.0f, it->second);
                    const auto delayMs = static_cast<double>(swingPercent) * (stepDurationMs * 0.5);
                    const auto delaySamples = static_cast<int>((delayMs / 1000.0) * sampleRate);
                    sampleOffset += delaySamples;
                }
            }

            swung.addEvent(message, sampleOffset);
        }

        midiMessages.swapWith(swung);
    }

    for (auto* block : midiBlocks)
    {
        if (block == nullptr)
            continue;
        auto it = blockStates.find(block->blockId);
        if (it == blockStates.end())
            continue;
        processMidiBlock(*block, it->second, midiMessages, currentBeat, bpm);
    }
}

void EffProcessor::setDrumSwingForNote(int midiNote, float swingPercent)
{
    drumSwingByMidiNote[midiNote] = juce::jlimit(0.0f, 1.0f, swingPercent);
}

// ─────────────────────────────────────────────────────────────────────────────
// processAudio — dispatches per audio block type
// ─────────────────────────────────────────────────────────────────────────────

void EffProcessor::processAudioBlock(EffBlock& block, BlockState& state,
                                     juce::AudioBuffer<float>& buf)
{
    if (block.bypassed)
        return;

    const int numCh  = buf.getNumChannels();
    const int numSmp = buf.getNumSamples();
    if (numSmp == 0 || numCh == 0)
        return;

    switch (block.type)
    {
        // ── AudioGain ────────────────────────────────────────────────────────
        case BlockType::AudioGain:
        {
            const float dB   = getParam(block, "gain", 0.0f);
            const float gain = juce::Decibels::decibelsToGain(dB);
            buf.applyGain(gain);
            break;
        }

        // ── AudioFilter ───────────────────────────────────────────────────────
        case BlockType::AudioFilter:
        {
            const int   type   = static_cast<int>(getParam(block, "type",   0.0f));
            const float cutoff = juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.49),
                                              getParam(block, "cutoff", 1000.0f));
            const float res    = juce::jlimit(0.1f, 10.0f, getParam(block, "res", 0.5f));

            juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
            if (type == 1)
                coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, cutoff, res);
            else if (type == 2)
                coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, cutoff, res);
            else
                coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoff, res);

            *state.filterL.coefficients = *coeffs;
            *state.filterR.coefficients = *coeffs;

            auto* L = buf.getWritePointer(0);
            for (int i = 0; i < numSmp; ++i)
                L[i] = state.filterL.processSample(L[i]);

            if (numCh > 1)
            {
                auto* R = buf.getWritePointer(1);
                for (int i = 0; i < numSmp; ++i)
                    R[i] = state.filterR.processSample(R[i]);
            }
            break;
        }

        // ── AudioSaturator ───────────────────────────────────────────────────
        case BlockType::AudioSaturator:
        {
            const float drive  = getParam(block, "drive",  0.5f);
            const float outGdB = getParam(block, "output", 0.0f);
            const float outG   = juce::Decibels::decibelsToGain(outGdB);
            const float pre    = 1.0f + drive * 9.0f; // 1..10

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = buf.getWritePointer(ch);
                for (int i = 0; i < numSmp; ++i)
                    data[i] = std::tanh(data[i] * pre) * outG;
            }
            break;
        }

        // ── AudioBitCrusher ───────────────────────────────────────────────────
        case BlockType::AudioBitCrusher:
        {
            const float bits   = juce::jlimit(2.0f, 16.0f, getParam(block, "bits",       12.0f));
            const float srRatio= juce::jlimit(0.01f, 1.0f, getParam(block, "sampleRate",  1.0f));

            const float levels = std::pow(2.0f, bits);
            const int   step   = std::max(1, static_cast<int>(1.0f / srRatio));

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = buf.getWritePointer(ch);
                float held = data[0];
                for (int i = 0; i < numSmp; ++i)
                {
                    if (i % step == 0)
                        held = std::floor(data[i] * levels) / levels;
                    data[i] = held;
                }
            }
            break;
        }

        // ── AudioDelay ────────────────────────────────────────────────────────
        case BlockType::AudioDelay:
        {
            const float beats    = getParam(block, "time",     0.25f);
            const float feedback = juce::jlimit(0.0f, 0.95f, getParam(block, "feedback", 0.4f));
            const float mix      = getParam(block, "mix",      0.5f);

            const float delaySec    = static_cast<float>(60.0 / 120.0 * beats); // default BPM 120 if bpm not threaded here
            const int   delaySamples= std::min(192000 - 1,
                                               static_cast<int>(delaySec * sampleRate));

            state.delayLine.setDelay(static_cast<float>(delaySamples));

            // Process L (mono-shared for simplicity)
            auto* L = buf.getWritePointer(0);
            for (int i = 0; i < numSmp; ++i)
            {
                const float delayed = state.delayLine.popSample(0);
                state.delayLine.pushSample(0, L[i] + delayed * feedback);
                L[i] = L[i] * (1.0f - mix) + delayed * mix;
            }
            if (numCh > 1)
                juce::FloatVectorOperations::copy(buf.getWritePointer(1), L, numSmp);
            break;
        }

        // ── AudioReverb ───────────────────────────────────────────────────────
        case BlockType::AudioReverb:
        {
            juce::Reverb::Parameters p;
            p.wetLevel  = getParam(block, "mix",   0.3f);
            p.dryLevel  = 1.0f - p.wetLevel;
            p.roomSize  = getParam(block, "size",  0.5f);
            p.damping   = getParam(block, "damp",  0.5f);
            p.width     = getParam(block, "width", 1.0f);
            state.reverb.setParameters(p);

            if (numCh >= 2)
                state.reverb.processStereo(buf.getWritePointer(0),
                                           buf.getWritePointer(1),
                                           numSmp);
            else
                state.reverb.processMono(buf.getWritePointer(0), numSmp);
            break;
        }

        // ── AudioChorus ───────────────────────────────────────────────────────
        case BlockType::AudioChorus:
        {
            const float rate  = getParam(block, "rate",  0.5f);
            const float depth = getParam(block, "depth", 0.5f);
            const float mix   = getParam(block, "mix",   0.5f);

            const float maxDepthSamples = static_cast<float>(sampleRate * 0.02); // 20ms max depth
            const float lfoIncL = static_cast<float>(rate * juce::MathConstants<float>::twoPi / sampleRate);
            const float lfoIncR = lfoIncL;

            auto process = [&](int ch, float& phase)
            {
                auto* data = buf.getWritePointer(ch);
                for (int i = 0; i < numSmp; ++i)
                {
                    const float lfo    = (std::sin(phase) * 0.5f + 0.5f);
                    const float del    = 1.0f + lfo * depth * maxDepthSamples;
                    state.delayLine.setDelay(del);
                    const float wet    = state.delayLine.popSample(0);
                    state.delayLine.pushSample(0, data[i]);
                    data[i] = data[i] * (1.0f - mix) + wet * mix;
                    phase  += lfoIncL;
                    if (phase > juce::MathConstants<float>::twoPi)
                        phase -= juce::MathConstants<float>::twoPi;
                }
            };

            process(0, state.lfoPhase);
            if (numCh > 1)
            {
                state.lfoPhaseR = state.lfoPhase + juce::MathConstants<float>::pi * 0.5f;
                process(1, state.lfoPhaseR);
            }
            break;
        }

        // ── AudioCompressor ───────────────────────────────────────────────────
        case BlockType::AudioCompressor:
        {
            const float threshDb   = getParam(block, "threshold", -12.0f);
            const float ratio      = juce::jlimit(1.0f, 20.0f, getParam(block, "ratio",   4.0f));
            const float attackMs   = getParam(block, "attack",   10.0f);
            const float releaseMs  = getParam(block, "release", 100.0f);
            const float makeupDb   = getParam(block, "makeupGain", 0.0f);

            const float threshLin  = juce::Decibels::decibelsToGain(threshDb);
            const float makeupGain = juce::Decibels::decibelsToGain(makeupDb);
            const float attackCoef = 1.0f - std::exp(-1.0f / static_cast<float>(attackMs  * 0.001 * sampleRate));
            const float releaseCoef= 1.0f - std::exp(-1.0f / static_cast<float>(releaseMs * 0.001 * sampleRate));

            for (int i = 0; i < numSmp; ++i)
            {
                // Peak detection across channels
                float peak = 0.0f;
                for (int ch = 0; ch < numCh; ++ch)
                    peak = std::max(peak, std::abs(buf.getSample(ch, i)));

                // Envelope follower
                const float coef = peak > state.envFollower ? attackCoef : releaseCoef;
                state.envFollower += coef * (peak - state.envFollower);

                // Gain reduction
                float gainReduction = 1.0f;
                if (state.envFollower > threshLin && ratio > 1.0f)
                {
                    const float overDb = juce::Decibels::gainToDecibels(state.envFollower) - threshDb;
                    gainReduction = juce::Decibels::decibelsToGain(-(overDb * (1.0f - 1.0f / ratio)));
                }

                for (int ch = 0; ch < numCh; ++ch)
                    buf.setSample(ch, i, buf.getSample(ch, i) * gainReduction * makeupGain);
            }
            break;
        }

        // ── AudioTremolo ──────────────────────────────────────────────────────
        case BlockType::AudioTremolo:
        {
            const float rate  = getParam(block, "rate",  4.0f);
            const float depth = getParam(block, "depth", 0.8f);
            const float lfoInc = static_cast<float>(rate * juce::MathConstants<float>::twoPi / sampleRate);

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* data = buf.getWritePointer(ch);
                float phase = state.lfoPhase;
                for (int i = 0; i < numSmp; ++i)
                {
                    const float gain = 1.0f - depth * (0.5f + 0.5f * std::sin(phase));
                    data[i] *= gain;
                    phase += lfoInc;
                    if (phase > juce::MathConstants<float>::twoPi)
                        phase -= juce::MathConstants<float>::twoPi;
                }
            }
            state.lfoPhase += lfoInc * numSmp;
            while (state.lfoPhase > juce::MathConstants<float>::twoPi)
                state.lfoPhase -= juce::MathConstants<float>::twoPi;
            break;
        }

        // ── AudioStereoWidth ──────────────────────────────────────────────────
        case BlockType::AudioStereoWidth:
        {
            if (numCh < 2) break;
            const float width = getParam(block, "width", 1.0f);
            const float mid   = (1.0f + width) * 0.5f;
            const float side  = (1.0f - width) * 0.5f;

            auto* L = buf.getWritePointer(0);
            auto* R = buf.getWritePointer(1);
            for (int i = 0; i < numSmp; ++i)
            {
                const float m = (L[i] + R[i]) * 0.5f;
                const float s = (L[i] - R[i]) * 0.5f;
                L[i] = m + s * width;
                R[i] = m - s * width;
                (void) mid; (void) side;
            }
            break;
        }

        // ── AudioDryWet ───────────────────────────────────────────────────────
        case BlockType::AudioDryWet:
        {
            // DryWet is a meta-control: store dry, process remaining chain, blend back.
            // In a single-block pass, this is a no-op; it controls overall wet mix
            // via the mix param accessed by the host.
            const float mix = getParam(block, "mix", 0.5f);
            buf.applyGain(mix);
            break;
        }

        default:
            break;
    }
}

void EffProcessor::processAudio(juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer& /*midiMessages*/)
{
    for (auto* block : audioBlocks)
    {
        if (block == nullptr)
            continue;
        auto it = blockStates.find(block->blockId);
        if (it == blockStates.end())
            continue;
        processAudioBlock(*block, it->second, buffer);
    }
}

} // namespace setle::eff
