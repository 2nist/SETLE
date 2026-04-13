#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include "DrumMachineProcessor.h"

namespace te = tracktion::engine;

namespace setle::instruments::drum
{

class TracktionDrumMachinePlugin final : public te::Plugin
{
public:
    static const char* xmlTypeName;
    static const char* getPluginName() { return NEEDS_TRANS("SETLE DrumMachine"); }

    explicit TracktionDrumMachinePlugin(te::PluginCreationInfo info);
    ~TracktionDrumMachinePlugin() override;

    juce::String getName() const override { return getPluginName(); }
    juce::String getPluginType() override { return xmlTypeName; }
    juce::String getSelectableDescription() override { return getName(); }

    void initialise(const te::PluginInitialisationInfo& info) override;
    void deinitialise() override;
    void applyToBuffer(const te::PluginRenderContext& fc) override;
    void restorePluginStateFromValueTree(const juce::ValueTree&) override {}

    bool takesMidiInput() override { return false; }
    bool isSynth() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return true; }
    int getNumOutputChannelsGivenInputs(int) override { return 2; }

    void setPattern(const std::vector<setle::gridroll::GridRollCell>& cells);
    void setTransportBeat(double beat);
    DrumMachineProcessor& getProcessor() noexcept { return processor; }

private:
    juce::CriticalSection lock;
    DrumMachineProcessor processor;
    double externalBeat { 0.0 };
    bool hasExternalBeat { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TracktionDrumMachinePlugin)
};

} // namespace setle::instruments::drum
