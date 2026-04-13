#include "DrumMachineProcessor.h"

#include <algorithm>

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

    std::fill(voiceMeters.begin(), voiceMeters.end(), 0.0f);

    {
        const juce::ScopedLock sl(lock);

        for (const auto& cell : pattern)
        {
            if (cell.type != setle::gridroll::GridRollCell::CellType::DrumHit || cell.muted)
                continue;

            const auto cellStart = static_cast<double>(cell.startBeat);
            const bool crossed = (previousBeat <= cellStart && cellStart < transportBeat)
                              || (transportBeat < previousBeat && (cellStart >= previousBeat || cellStart < transportBeat));
            if (crossed)
            {
                const juce::int64 seed = static_cast<juce::int64>(loopSeed + static_cast<uint32_t>(cellStart * 1000.0f));
                auto random = juce::Random(seed);
                if (cell.probability >= 1.0f || random.nextFloat() <= cell.probability)
                    triggerVoice(cell.midiNote, cell.velocity, juce::jmax(1, cell.ratchetCount));
            }
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float mix = 0.0f;

        for (auto it = activeHits.begin(); it != activeHits.end();)
        {
            auto& hit = *it;
            const auto s = hit.renderSample();
            mix += s;

            if (!hit.isActive())
                it = activeHits.erase(it);
            else
                ++it;
        }

        mix *= moduleState.masterVolume;
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.addSample(ch, i, mix);
    }

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

    const auto params = moduleState.patch.voices[static_cast<size_t>(voiceIndex)];

    for (int i = 0; i < count; ++i)
    {
        FmDrumVoice hit;
        hit.setSampleRate(sampleRate);
        hit.trigger(params,
                    juce::jmax(0.0f, vel * (1.0f - 0.12f * static_cast<float>(i))),
                    i == 0 && velocity > 0.9f);
        activeHits.push_back(hit);
    }

    voiceMeters[static_cast<size_t>(voiceIndex)] = juce::jmax(voiceMeters[static_cast<size_t>(voiceIndex)], vel);
}

int DrumMachineProcessor::voiceIndexForMidi(int midiNote) const
{
    switch (midiNote)
    {
        case 36: return 0;
        case 38: return 1;
        case 42: return 2;
        case 46: return 3;
        case 39: return 4;
        default: return 1;
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

} // namespace setle::instruments::drum
