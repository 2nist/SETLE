#include "TracktionPolySynthPlugin.h"

namespace setle::instruments::polysynth
{

const char* TracktionPolySynthPlugin::xmlTypeName = "setle_poly_synth";

TracktionPolySynthPlugin::TracktionPolySynthPlugin(te::PluginCreationInfo info)
    : te::Plugin(info)
{
    auto* um = getUndoManager();
    attackMs.referTo(state, "attackMs", um, 10.0f);
    decayMs.referTo(state, "decayMs", um, 200.0f);
    sustain.referTo(state, "sustain", um, 0.7f);
    releaseMs.referTo(state, "releaseMs", um, 400.0f);
    cutoffHz.referTo(state, "cutoffHz", um, 8000.0f);
    resonance.referTo(state, "resonance", um, 0.2f);
    outputGain.referTo(state, "outputGain", um, 0.8f);
    mono.referTo(state, "mono", um, false);
    patchName.referTo(state, "patchName", um, juce::String("Init Patch"));

    syncProcessorParameters();
}

TracktionPolySynthPlugin::~TracktionPolySynthPlugin()
{
    notifyListenersOfDeletion();
}

void TracktionPolySynthPlugin::initialise(const te::PluginInitialisationInfo& info)
{
    const juce::ScopedLock sl(processLock);
    sampleRate = info.sampleRate > 0.0 ? info.sampleRate : 44100.0;
    processor.prepareToPlay(sampleRate, info.blockSizeSamples);
    syncProcessorParameters();
}

void TracktionPolySynthPlugin::deinitialise()
{
    const juce::ScopedLock sl(processLock);
    processor.releaseResources();
}

void TracktionPolySynthPlugin::applyToBuffer(const te::PluginRenderContext& fc)
{
    if (!isEnabled() || fc.destBuffer == nullptr || fc.bufferNumSamples <= 0)
        return;

    const juce::ScopedLock sl(processLock);
    syncProcessorParameters();

    juce::AudioBuffer<float> temp(fc.destBuffer->getNumChannels(), fc.bufferNumSamples);
    temp.clear();

    juce::MidiBuffer midi;
    if (fc.bufferForMidiMessages != nullptr)
    {
        for (auto& m : *fc.bufferForMidiMessages)
        {
            const auto ts = juce::jlimit(0, fc.bufferNumSamples - 1, juce::roundToInt(m.getTimeStamp() * sampleRate));
            midi.addEvent(m, ts);
        }
    }

    processor.processBlock(temp, midi);

    for (int ch = 0; ch < juce::jmin(fc.destBuffer->getNumChannels(), temp.getNumChannels()); ++ch)
        fc.destBuffer->addFrom(ch, fc.bufferStartSample, temp, ch, 0, fc.bufferNumSamples);
}

void TracktionPolySynthPlugin::restorePluginStateFromValueTree(const juce::ValueTree& v)
{
    copyPropertiesToCachedValues(v,
                                 attackMs,
                                 decayMs,
                                 sustain,
                                 releaseMs,
                                 cutoffHz,
                                 resonance,
                                 outputGain,
                                 mono,
                                 patchName);
    syncProcessorParameters();
}

void TracktionPolySynthPlugin::syncProcessorParameters()
{
    processor.setAttackMs(attackMs.get());
    processor.setDecayMs(decayMs.get());
    processor.setSustain(sustain.get());
    processor.setReleaseMs(releaseMs.get());
    processor.setCutoffHz(cutoffHz.get());
    processor.setResonance(resonance.get());
    processor.setOutputGain(outputGain.get());
    processor.setMonoMode(mono.get());
    processor.setPatchName(patchName.get());
}

} // namespace setle::instruments::polysynth
