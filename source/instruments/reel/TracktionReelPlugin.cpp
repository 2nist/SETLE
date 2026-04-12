#include "TracktionReelPlugin.h"

namespace setle::instruments::reel
{

const char* TracktionReelPlugin::xmlTypeName = "setle_reel";

TracktionReelPlugin::TracktionReelPlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
{
}

TracktionReelPlugin::~TracktionReelPlugin()
{
    notifyListenersOfDeletion();
}

void TracktionReelPlugin::initialise(const te::PluginInitialisationInfo& info)
{
    const juce::ScopedLock sl(lock);
    processor.prepare(info.sampleRate, info.blockSizeSamples);
}

void TracktionReelPlugin::deinitialise()
{
    const juce::ScopedLock sl(lock);
    processor.reset();
}

void TracktionReelPlugin::applyToBuffer(const te::PluginRenderContext& fc)
{
    if (!isEnabled() || fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    const juce::ScopedLock sl(lock);

    juce::AudioBuffer<float> temp(fc.destBuffer->getNumChannels(), fc.bufferNumSamples);
    temp.clear();

    juce::MidiBuffer midi;
    if (fc.bufferForMidiMessages != nullptr)
    {
        double bpm = 120.0;
        if (auto* tempo = edit.tempoSequence.getTempo(0))
            bpm = tempo->getBpm();

        const auto secPerBeat = 60.0 / juce::jmax(1.0, bpm);
        const auto beat = fc.editTime.getStart().inSeconds() / secPerBeat;
        processor.setTransportPosition(beat, bpm);

        for (auto& m : *fc.bufferForMidiMessages)
            midi.addEvent(m, 0);
    }

    processor.process(temp, midi);

    for (int ch = 0; ch < juce::jmin(fc.destBuffer->getNumChannels(), temp.getNumChannels()); ++ch)
        fc.destBuffer->addFrom(ch, fc.bufferStartSample, temp, ch, 0, fc.bufferNumSamples);
}

void TracktionReelPlugin::restorePluginStateFromValueTree(const juce::ValueTree&)
{
}

} // namespace setle::instruments::reel
