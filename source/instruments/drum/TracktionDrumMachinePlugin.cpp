#include "TracktionDrumMachinePlugin.h"

namespace setle::instruments::drum
{

const char* TracktionDrumMachinePlugin::xmlTypeName = "setle_drum_machine";

TracktionDrumMachinePlugin::TracktionDrumMachinePlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
{
}

TracktionDrumMachinePlugin::~TracktionDrumMachinePlugin()
{
    notifyListenersOfDeletion();
}

void TracktionDrumMachinePlugin::initialise(const te::PluginInitialisationInfo& info)
{
    const juce::ScopedLock sl(lock);
    processor.prepareToPlay(info.sampleRate, info.blockSizeSamples);
}

void TracktionDrumMachinePlugin::deinitialise()
{
    const juce::ScopedLock sl(lock);
    processor.releaseResources();
}

void TracktionDrumMachinePlugin::applyToBuffer(const te::PluginRenderContext& fc)
{
    if (!isEnabled() || fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    const juce::ScopedLock sl(lock);

    double beat = externalBeat;
    if (!hasExternalBeat)
    {
        double bpm = 120.0;
        if (auto* tempo = edit.tempoSequence.getTempo(0))
            bpm = tempo->getBpm();

        const auto secPerBeat = 60.0 / juce::jmax(1.0, bpm);
        beat = fc.editTime.getStart().inSeconds() / secPerBeat;
    }
    processor.setTransportBeat(beat);

    juce::AudioBuffer<float> temp(fc.destBuffer->getNumChannels(), fc.bufferNumSamples);
    temp.clear();
    juce::MidiBuffer midi;
    processor.processBlock(temp, midi);

    for (int ch = 0; ch < juce::jmin(fc.destBuffer->getNumChannels(), temp.getNumChannels()); ++ch)
        fc.destBuffer->addFrom(ch, fc.bufferStartSample, temp, ch, 0, fc.bufferNumSamples);
}

void TracktionDrumMachinePlugin::setPattern(const std::vector<setle::gridroll::GridRollCell>& cells)
{
    const juce::ScopedLock sl(lock);
    processor.setPattern(cells);
}

void TracktionDrumMachinePlugin::setTransportBeat(double beat)
{
    const juce::ScopedLock sl(lock);
    hasExternalBeat = true;
    externalBeat = juce::jmax(0.0, beat);
    processor.setTransportBeat(externalBeat);
}

} // namespace setle::instruments::drum
