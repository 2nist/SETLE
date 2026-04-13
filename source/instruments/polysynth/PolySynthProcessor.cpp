#include "PolySynthProcessor.h"

namespace setle::instruments::polysynth
{

PolySynthProcessor::PolySynthProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void PolySynthProcessor::setMonoMode(bool enabled) { monoMode.store(enabled); }
bool PolySynthProcessor::isMonoMode() const noexcept { return monoMode.load(); }

void PolySynthProcessor::playChord(const std::vector<int>& midiNotes, float velocity)
{
    if (midiNotes.empty())
        return;

    if (monoMode.load())
    {
        playNote(midiNotes.back(), velocity);
        return;
    }

    for (const auto note : midiNotes)
        playNote(note, velocity);
}

void PolySynthProcessor::playNote(int midiNote, float velocity)
{
    const juce::ADSR::Parameters adsr { attackSec.load(), decaySec.load(), sustainLevel.load(), releaseSec.load() };

    const juce::ScopedLock sl(stateLock);
    PolyVoice* voice = nullptr;

    if (monoMode.load())
    {
        voice = &voices[0];
        for (size_t i = 1; i < voices.size(); ++i)
            voices[i].stop(false);
        voice->stop(false);
    }
    else
    {
        voice = findFreeVoice();
        if (voice == nullptr)
            voice = &voices[0];
    }

    if (voice == nullptr)
        return;

    voice->setADSR(adsr);
    voice->start(midiNote, velocity);
}

void PolySynthProcessor::allNotesOff()
{
    const juce::ScopedLock sl(stateLock);
    for (auto& voice : voices)
        voice.stop(false);
}

void PolySynthProcessor::setAttackMs(float value) { attackSec.store(juce::jlimit(0.001f, 5.0f, value * 0.001f)); }
void PolySynthProcessor::setDecayMs(float value) { decaySec.store(juce::jlimit(0.001f, 5.0f, value * 0.001f)); }
void PolySynthProcessor::setSustain(float value) { sustainLevel.store(juce::jlimit(0.0f, 1.0f, value)); }
void PolySynthProcessor::setReleaseMs(float value) { releaseSec.store(juce::jlimit(0.001f, 8.0f, value * 0.001f)); }
void PolySynthProcessor::setCutoffHz(float value) { cutoffHz.store(juce::jlimit(20.0f, 20000.0f, value)); }
void PolySynthProcessor::setResonance(float value) { resonance.store(juce::jlimit(0.0f, 0.98f, value)); }
void PolySynthProcessor::setOutputGain(float value) { outputGain.store(juce::jlimit(0.0f, 1.5f, value)); }
void PolySynthProcessor::setPatchName(const juce::String& name) { patchName = name; }

float PolySynthProcessor::getAttackMs() const noexcept { return attackSec.load() * 1000.0f; }
float PolySynthProcessor::getDecayMs() const noexcept { return decaySec.load() * 1000.0f; }
float PolySynthProcessor::getSustain() const noexcept { return sustainLevel.load(); }
float PolySynthProcessor::getReleaseMs() const noexcept { return releaseSec.load() * 1000.0f; }
float PolySynthProcessor::getCutoffHz() const noexcept { return cutoffHz.load(); }
float PolySynthProcessor::getResonance() const noexcept { return resonance.load(); }
float PolySynthProcessor::getOutputGain() const noexcept { return outputGain.load(); }
juce::String PolySynthProcessor::getPatchName() const { return patchName; }

const juce::String PolySynthProcessor::getName() const { return "SETLE PolySynth"; }

void PolySynthProcessor::prepareToPlay(double sampleRate, int)
{
    const juce::ScopedLock sl(stateLock);
    for (auto& voice : voices)
    {
        voice.setSampleRate(sampleRate);
        voice.setADSR({ attackSec.load(), decaySec.load(), sustainLevel.load(), releaseSec.load() });
        voice.stop(false);
    }
}

void PolySynthProcessor::releaseResources()
{
    allNotesOff();
}

bool PolySynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void PolySynthProcessor::processBlock(juce::AudioBuffer<float>& output, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const juce::ScopedLock sl(stateLock);

    output.clear();
    if (output.getNumSamples() <= 0 || output.getNumChannels() <= 0)
        return;

    handleMidi(midi);

    for (auto& voice : voices)
    {
        voice.setADSR({ attackSec.load(), decaySec.load(), sustainLevel.load(), releaseSec.load() });
        voice.render(output, 0, output.getNumSamples(), cutoffHz.load(), resonance.load());
    }

    output.applyGain(outputGain.load());
}

void PolySynthProcessor::handleMidi(const juce::MidiBuffer& midi)
{
    for (const auto& event : midi)
    {
        const auto msg = event.getMessage();
        if (msg.isNoteOn() && msg.getVelocity() > 0)
            playNote(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            if (auto* voice = findVoiceForNote(msg.getNoteNumber()); voice != nullptr)
                voice->stop(true);
    }
}

PolyVoice* PolySynthProcessor::findFreeVoice()
{
    for (auto& voice : voices)
        if (!voice.isActive())
            return &voice;
    return nullptr;
}

PolyVoice* PolySynthProcessor::findVoiceForNote(int note)
{
    for (auto& voice : voices)
        if (voice.matchesMidi(note))
            return &voice;
    return nullptr;
}

juce::AudioProcessorEditor* PolySynthProcessor::createEditor() { return nullptr; }
bool PolySynthProcessor::hasEditor() const { return false; }
double PolySynthProcessor::getTailLengthSeconds() const { return 0.0; }
int PolySynthProcessor::getNumPrograms() { return 1; }
int PolySynthProcessor::getCurrentProgram() { return 0; }
void PolySynthProcessor::setCurrentProgram(int) {}
const juce::String PolySynthProcessor::getProgramName(int) { return {}; }
void PolySynthProcessor::changeProgramName(int, const juce::String&) {}

void PolySynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("setlePolySynth");
    state.setProperty("attackMs", getAttackMs(), nullptr);
    state.setProperty("decayMs", getDecayMs(), nullptr);
    state.setProperty("sustain", getSustain(), nullptr);
    state.setProperty("releaseMs", getReleaseMs(), nullptr);
    state.setProperty("cutoff", getCutoffHz(), nullptr);
    state.setProperty("res", getResonance(), nullptr);
    state.setProperty("gain", getOutputGain(), nullptr);
    state.setProperty("mono", monoMode.load(), nullptr);
    state.setProperty("patch", patchName, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PolySynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml(*xmlState);
    if (!state.isValid())
        return;

    setAttackMs(static_cast<float>(state.getProperty("attackMs", 10.0f)));
    setDecayMs(static_cast<float>(state.getProperty("decayMs", 200.0f)));
    setSustain(static_cast<float>(state.getProperty("sustain", 0.7f)));
    setReleaseMs(static_cast<float>(state.getProperty("releaseMs", 400.0f)));
    setCutoffHz(static_cast<float>(state.getProperty("cutoff", 8000.0f)));
    setResonance(static_cast<float>(state.getProperty("res", 0.2f)));
    setOutputGain(static_cast<float>(state.getProperty("gain", 0.8f)));
    setMonoMode(static_cast<bool>(state.getProperty("mono", false)));
    setPatchName(state.getProperty("patch", "Init Patch").toString());
}

} // namespace setle::instruments::polysynth
