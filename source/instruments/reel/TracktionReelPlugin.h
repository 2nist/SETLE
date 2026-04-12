#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include "ReelProcessor.h"

namespace te = tracktion::engine;

namespace setle::instruments::reel
{

class TracktionReelPlugin final : public te::Plugin
{
public:
    static const char* xmlTypeName;
    static const char* getPluginName() { return NEEDS_TRANS("SETLE Reel"); }

    explicit TracktionReelPlugin(te::PluginCreationInfo info);
    ~TracktionReelPlugin() override;

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

    ::ReelProcessor& getProcessor() noexcept { return processor; }

private:
    juce::CriticalSection lock;
    ::ReelProcessor processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TracktionReelPlugin)
};

} // namespace setle::instruments::reel
