#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include <memory>

#include "drum/TracktionDrumMachinePlugin.h"
#include "polysynth/TracktionPolySynthPlugin.h"
#include "reel/TracktionReelPlugin.h"
#include "../gridroll/GridRollCell.h"
#include "../eff/EffModel.h"
#include "../eff/EffProcessor.h"

namespace te = tracktion::engine;

namespace setle::instruments
{

class InstrumentSlot
{
public:
    enum class SlotType { Empty, PolySynth, DrumMachine, ReelSampler, VST3, MidiOut };

    struct SlotInfo
    {
        SlotType type { SlotType::Empty };
        juce::String displayName;
        juce::String trackId;
        juce::String persistentIdentifier;
        juce::String persistentName;
        bool isActive { false };
    };

    explicit InstrumentSlot(te::AudioTrack& track, te::Edit& edit);

    void loadPolySynth();
    void loadDrumMachine();
    void loadReelSampler();
    void loadVST3(const juce::PluginDescription& desc);
    void loadMidiOut(const juce::String& deviceIdentifier);
    void clear();
    void setDrumPattern(const std::vector<setle::gridroll::GridRollCell>& cells);
    void setTransportBeat(double beat);
    bool getDrumSubSyncHint(float& frequencyHz, float& intensity) const;

    // FX chain
    void loadEffChain(const setle::eff::EffDefinition& def);
    void clearEffChain();
    setle::eff::EffProcessor* getEffProcessor();

    SlotInfo getInfo() const;

    juce::Component* getEditorComponent();

private:
    void setSlot(SlotType type, const juce::String& displayName, const juce::String& detailText);

    te::AudioTrack& track;
    te::Edit& edit;
    SlotInfo info;
    te::Plugin::Ptr loadedPlugin;
    std::unique_ptr<juce::Component> editorComponent;

    std::unique_ptr<setle::eff::EffProcessor> effProcessor;
};

} // namespace setle::instruments
