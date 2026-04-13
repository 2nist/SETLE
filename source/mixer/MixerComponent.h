#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include <map>
#include <memory>
#include <vector>

#include "BusManager.h"
#include "MasterStrip.h"
#include "MixerStrip.h"
#include "../instruments/InstrumentSlot.h"
#include "../timeline/TrackManager.h"

namespace te = tracktion::engine;

namespace setle::mixer
{

class MixerComponent : public juce::Component
{
public:
    explicit MixerComponent(te::Edit& edit,
                            setle::timeline::TrackManager& trackManager,
                            std::map<juce::String, std::unique_ptr<setle::instruments::InstrumentSlot>>& instrumentSlots);

    void refreshStrips();

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    te::Edit& edit;
    setle::timeline::TrackManager& trackManager;
    std::map<juce::String, std::unique_ptr<setle::instruments::InstrumentSlot>>& instrumentSlots;

    juce::Viewport stripsViewport;
    juce::Component stripsContent;
    std::vector<std::unique_ptr<setle::mixer::MixerStrip>> strips;
    std::unique_ptr<setle::mixer::MasterStrip> masterStrip;
    std::unique_ptr<setle::mixer::BusManager> busManager;
};

} // namespace setle::mixer
