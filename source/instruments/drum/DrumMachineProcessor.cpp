#include "DrumMachineProcessor.h"

#include <algorithm>
#include <cmath>

namespace setle::instruments::drum
{

DrumMachineProcessor::DrumMachineProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void DrumMachineProcessor::setPattern(const std::vector<setle::gridroll::GridRollCell>& cells)
{
    const juce::ScopedLock sl(lock);
    pattern = cells;
}

void DrumMachineProcessor::setTransportBeat(double beat)
{
    const juce::ScopedLock sl(lock);
    previousBeat = transportBeat;
    transportBeat = juce::jmax(0.0, beat);
    if (transportBeat < previousBeat)
    {
        ++loopCounter;
        loopSeed = 0x12345678u + static_cast<uint32_t>(loopCounter * 7919);
    }
}

const juce::String DrumMachineProcessor::getName() const { return "SETLE DrumMachine"; }

void DrumMachineProcessor::prepareToPlay(double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    activeHits.clear();
    glossLp = 0.0f;
    subSyncFrequencyHz.store(0.0f, std::memory_order_relaxed);
    subSyncIntensity.store(0.0f, std::memory_order_relaxed);
}

void DrumMachineProcessor::releaseResources()
{
    activeHits.clear();
}

bool DrumMachineProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void DrumMachineProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    for (auto& meter : voiceMeters)
        meter *= 0.80f;

    {
        const juce::ScopedLock sl(lock);
        const auto arc = moduleState.macros.get(DrumMacroId::Arc);

        for (const auto& cell : pattern)
        {
            if (cell.type != setle::gridroll::GridRollCell::CellType::DrumHit || cell.muted)
                continue;

            const auto cellStart = static_cast<double>(cell.startBeat);
            const auto sixteenthIndex = juce::jmax(0, juce::roundToInt(cellStart * 4.0));
            const auto swingOffset = (sixteenthIndex % 2 == 1)
                                         ? static_cast<double>(arc) * 0.085
                                         : 0.0;
            const auto adjustedStart = cellStart + swingOffset;
            const bool crossed = (previousBeat <= adjustedStart && adjustedStart < transportBeat)
                              || (transportBeat < previousBeat && (adjustedStart >= previousBeat || adjustedStart < transportBeat));
            if (crossed)
            {
                const juce::int64 seed = static_cast<juce::int64>(loopSeed + static_cast<uint32_t>(cellStart * 1000.0f));
                auto random = juce::Random(seed);
                const auto complexityBoost = juce::jmap(arc, 0.0f, 1.0f, 0.0f, 0.08f);
                if (cell.probability >= 1.0f || random.nextFloat() <= juce::jlimit(0.0f, 1.0f, cell.probability + complexityBoost))
                    triggerVoice(cell.midiNote,
                                 juce::jlimit(0.0f, 1.0f, cell.velocity + random.nextFloat() * 0.08f * arc),
                                 juce::jmax(1, cell.ratchetCount));
            }
        }
    }

    std::array<float, kVoiceCount> blockPeaks {};
    const auto glossAmount = moduleState.macros.get(DrumMacroId::Gloss);
    const auto glossAlpha = std::exp(-2.0f * juce::MathConstants<float>::pi * 3500.0f / static_cast<float>(sampleRate));
    float subEnergy = subSyncIntensity.load(std::memory_order_relaxed) * 0.92f;

    for (int i = 0; i < numSamples; ++i)
    {
        float mix = 0.0f;

        for (auto it = activeHits.begin(); it != activeHits.end();)
        {
            auto& hit = *it;
            const auto s = hit.renderSample();
            mix += s;

            const auto voiceIndex = static_cast<int>(hit.getVoiceId());
            if (voiceIndex >= 0 && voiceIndex < kVoiceCount)
                blockPeaks[static_cast<size_t>(voiceIndex)] = juce::jmax(blockPeaks[static_cast<size_t>(voiceIndex)], std::abs(s));

            if (hit.getVoiceId() == DrumVoiceId::Sub)
                subEnergy = juce::jmax(subEnergy, juce::jlimit(0.0f, 1.0f, std::abs(s) * 2.1f));

            if (!hit.isActive())
                it = activeHits.erase(it);
            else
                ++it;
        }

        glossLp = glossLp * glossAlpha + mix * (1.0f - glossAlpha);
        const auto highBand = mix - glossLp;
        mix += highBand * (0.15f + 0.95f * glossAmount);
        mix *= moduleState.masterVolume;

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addSample(ch, i, mix);
    }

    for (int i = 0; i < kVoiceCount; ++i)
        voiceMeters[static_cast<size_t>(i)] = juce::jmax(voiceMeters[static_cast<size_t>(i)],
                                                         juce::jlimit(0.0f, 1.0f, blockPeaks[static_cast<size_t>(i)] * 2.8f));

    subSyncIntensity.store(juce::jlimit(0.0f, 1.0f, subEnergy), std::memory_order_relaxed);

    if (transportBeat >= previousBeat)
        previousBeat = transportBeat;
}

juce::AudioProcessorEditor* DrumMachineProcessor::createEditor() { return nullptr; }
bool DrumMachineProcessor::hasEditor() const { return false; }
double DrumMachineProcessor::getTailLengthSeconds() const { return 0.0; }
int DrumMachineProcessor::getNumPrograms() { return 1; }
int DrumMachineProcessor::getCurrentProgram() { return 0; }
void DrumMachineProcessor::setCurrentProgram(int) {}
const juce::String DrumMachineProcessor::getProgramName(int) { return {}; }
void DrumMachineProcessor::changeProgramName(int, const juce::String&) {}
void DrumMachineProcessor::getStateInformation(juce::MemoryBlock&) {}
void DrumMachineProcessor::setStateInformation(const void*, int) {}

void DrumMachineProcessor::triggerVoice(int midiNote, float velocity, int ratchetCount)
{
    const auto vel = juce::jlimit(0.0f, 1.0f, velocity);
    const auto count = juce::jmax(1, ratchetCount);
    const auto voiceIndex = voiceIndexForMidi(midiNote);
    if (voiceIndex < 0 || voiceIndex >= kVoiceCount)
        return;

    auto params = moduleState.patch.voices[static_cast<size_t>(voiceIndex)];
    const auto voiceId = static_cast<DrumVoiceId>(voiceIndex);

    const auto thump = moduleState.macros.get(DrumMacroId::Thump);
    const auto sizzle = moduleState.macros.get(DrumMacroId::Sizzle);
    const auto choke = moduleState.macros.get(DrumMacroId::Choke);
    const auto texture = moduleState.macros.get(DrumMacroId::Texture);

    if (voiceId == DrumVoiceId::Kick)
    {
        params.level *= juce::jmap(thump, 0.0f, 1.0f, 0.78f, 1.35f);
        params.modIndex *= juce::jmap(thump, 0.0f, 1.0f, 0.75f, 1.25f);
    }
    else if (voiceId == DrumVoiceId::Sub)
    {
        params.tuneHz = juce::jlimit(32.0f, 58.0f, params.tuneHz);
        params.level *= juce::jmap(thump, 0.0f, 1.0f, 0.70f, 1.45f);
        params.decaySeconds *= juce::jmap(thump, 0.0f, 1.0f, 0.88f, 1.55f);
    }
    else if (voiceId == DrumVoiceId::Snare)
    {
        params.modIndex *= juce::jmap(sizzle, 0.0f, 1.0f, 0.72f, 1.55f);
        params.decaySeconds *= juce::jmap(sizzle, 0.0f, 1.0f, 0.80f, 1.24f);
    }
    else if (voiceId == DrumVoiceId::Perc)
    {
        params.modRatio *= juce::jmap(texture, 0.0f, 1.0f, 0.75f, 1.85f);
        params.modIndex *= juce::jmap(texture, 0.0f, 1.0f, 0.85f, 1.95f);
    }
    else if (voiceId == DrumVoiceId::HatClosed)
    {
        params.decaySeconds *= juce::jmap(choke, 0.0f, 1.0f, 0.84f, 0.40f);
        for (auto& hit : activeHits)
        {
            if (hit.getVoiceId() == DrumVoiceId::HatOpen)
                hit.forceStop();
        }
    }
    else if (voiceId == DrumVoiceId::HatOpen)
    {
        params.decaySeconds *= juce::jmap(choke, 0.0f, 1.0f, 1.36f, 0.60f);
    }

    for (int i = 0; i < count; ++i)
    {
        FmDrumVoice hit;
        hit.setSampleRate(sampleRate);
        hit.trigger(voiceId,
                    params,
                    moduleState.macros,
                    juce::jmax(0.0f, vel * (1.0f - 0.12f * static_cast<float>(i))),
                    i == 0 && velocity > 0.9f);
        activeHits.push_back(hit);
    }

    voiceMeters[static_cast<size_t>(voiceIndex)] = juce::jmax(voiceMeters[static_cast<size_t>(voiceIndex)], vel * 0.9f);
    if (voiceId == DrumVoiceId::Sub)
    {
        subSyncFrequencyHz.store(params.tuneHz, std::memory_order_relaxed);
        subSyncIntensity.store(juce::jmax(subSyncIntensity.load(std::memory_order_relaxed), vel), std::memory_order_relaxed);
    }
}

int DrumMachineProcessor::voiceIndexForMidi(int midiNote) const
{
    switch (midiNote)
    {
        case 36: return static_cast<int>(DrumVoiceId::Kick);
        case 35: return static_cast<int>(DrumVoiceId::Sub);
        case 38: return static_cast<int>(DrumVoiceId::Snare);
        case 45: return static_cast<int>(DrumVoiceId::Perc);
        case 42: return static_cast<int>(DrumVoiceId::HatClosed);
        case 46: return static_cast<int>(DrumVoiceId::HatOpen);
        case 39: return static_cast<int>(DrumVoiceId::Clap);
        default: return static_cast<int>(DrumVoiceId::Snare);
    }
}

void DrumMachineProcessor::setVoiceTune(int voiceIndex, float valueHz)
{
    if (voiceIndex < 0 || voiceIndex >= kVoiceCount)
        return;
    moduleState.patch.voices[static_cast<size_t>(voiceIndex)].tuneHz = juce::jlimit(30.0f, 2000.0f, valueHz);
}

void DrumMachineProcessor::setVoiceDecay(int voiceIndex, float valueSeconds)
{
    if (voiceIndex < 0 || voiceIndex >= kVoiceCount)
        return;
    moduleState.patch.voices[static_cast<size_t>(voiceIndex)].decaySeconds = juce::jlimit(0.02f, 1.0f, valueSeconds);
}

void DrumMachineProcessor::setVoiceLevel(int voiceIndex, float value)
{
    if (voiceIndex < 0 || voiceIndex >= kVoiceCount)
        return;
    moduleState.patch.voices[static_cast<size_t>(voiceIndex)].level = juce::jlimit(0.0f, 1.0f, value);
}

float DrumMachineProcessor::getVoiceTune(int voiceIndex) const
{
    if (voiceIndex < 0 || voiceIndex >= kVoiceCount)
        return 120.0f;
    return moduleState.patch.voices[static_cast<size_t>(voiceIndex)].tuneHz;
}

float DrumMachineProcessor::getVoiceDecay(int voiceIndex) const
{
    if (voiceIndex < 0 || voiceIndex >= kVoiceCount)
        return 0.18f;
    return moduleState.patch.voices[static_cast<size_t>(voiceIndex)].decaySeconds;
}

float DrumMachineProcessor::getVoiceLevel(int voiceIndex) const
{
    if (voiceIndex < 0 || voiceIndex >= kVoiceCount)
        return 0.7f;
    return moduleState.patch.voices[static_cast<size_t>(voiceIndex)].level;
}

void DrumMachineProcessor::setMacro(int macroIndex, float value)
{
    if (macroIndex < 0 || macroIndex >= static_cast<int>(DrumMacroId::Count))
        return;

    moduleState.macros.set(static_cast<DrumMacroId>(macroIndex), value);
}

float DrumMachineProcessor::getMacro(int macroIndex) const
{
    if (macroIndex < 0 || macroIndex >= static_cast<int>(DrumMacroId::Count))
        return 0.0f;

    return moduleState.macros.get(static_cast<DrumMacroId>(macroIndex));
}

void DrumMachineProcessor::setMasterVolume(float value)
{
    moduleState.masterVolume = juce::jlimit(0.0f, 1.2f, value);
}

float DrumMachineProcessor::getMasterVolume() const
{
    return moduleState.masterVolume;
}

float DrumMachineProcessor::getVoiceMeter(int voiceIndex) const
{
    if (voiceIndex < 0 || voiceIndex >= kVoiceCount)
        return 0.0f;
    return voiceMeters[static_cast<size_t>(voiceIndex)];
}

float DrumMachineProcessor::getSubSyncFrequencyHz() const noexcept
{
    return subSyncFrequencyHz.load(std::memory_order_relaxed);
}

float DrumMachineProcessor::getSubSyncIntensity() const noexcept
{
    return subSyncIntensity.load(std::memory_order_relaxed);
}

} // namespace setle::instruments::drum
