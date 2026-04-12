#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include "PolySynthEditorComponent.h"
#include "PolySynthProcessor.h"

namespace te = tracktion::engine;

namespace setle::instruments::polysynth
{

class TracktionPolySynthPlugin final : public te::Plugin
{
public:
    static const char* xmlTypeName;
    static const char* getPluginName() { return NEEDS_TRANS("SETLE PolySynth"); }

    explicit TracktionPolySynthPlugin(te::PluginCreationInfo info);
    ~TracktionPolySynthPlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise(const te::PluginInitialisationInfo& info) override;
    void deinitialise() override;
    void applyToBuffer(const te::PluginRenderContext& fc) override;
    void restorePluginStateFromValueTree(const juce::ValueTree& v) override;

    bool takesMidiInput() override { return true; }
    bool isSynth() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return true; }
    int getNumOutputChannelsGivenInputs(int) override { return 2; }

    PolySynthProcessor& getProcessor() noexcept { return processor; }

private:
    void syncProcessorParameters();

    juce::CriticalSection processLock;
    PolySynthProcessor processor;
    double sampleRate { 44100.0 };

    juce::CachedValue<float> attackMs;
    juce::CachedValue<float> decayMs;
    juce::CachedValue<float> sustain;
    juce::CachedValue<float> releaseMs;
    juce::CachedValue<float> cutoffHz;
    juce::CachedValue<float> resonance;
    juce::CachedValue<float> outputGain;
    juce::CachedValue<bool> mono;
    juce::CachedValue<juce::String> patchName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TracktionPolySynthPlugin)
};

} // namespace setle::instruments::polysynth
