#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include <functional>

#include "BusManager.h"
#include "../ui/EditToolManager.h"

namespace te = tracktion::engine;

namespace setle::mixer
{

class MixerStrip : public juce::Component,
                   private juce::Timer,
                   private setle::ui::EditToolManager::Listener
{
public:
    explicit MixerStrip(te::AudioTrack& track,
                        te::Edit& edit,
                        const juce::String& instrumentType,
                        BusManager& busManager);
    ~MixerStrip() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

    std::function<void(te::AudioTrack&)> onSoloChanged;
    std::function<void(te::AudioTrack&)> onMuteChanged;

    static constexpr int kStripWidth = 72;

private:
    void timerCallback() override;
    float getPeakLevel(int channel);
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void activeToolChanged(setle::ui::EditTool newTool) override;

    te::AudioTrack& track;
    te::Edit& edit;
    BusManager& busManager;

    juce::Slider volumeFader;
    juce::Slider panKnob;
    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::Label nameLabel;
    juce::Label slotTypeLabel;

    juce::Slider sendKnobs[BusManager::kMaxBuses];

    float peakLeft { 0.0f };
    float peakRight { 0.0f };
    float peakHoldLeft { 0.0f };
    float peakHoldRight { 0.0f };
    int peakHoldFrames { 0 };

    te::LevelMeasurer::Client meterClient;
    te::LevelMeterPlugin* meterPlugin { nullptr };
    bool faderDragging { false };

    static constexpr int kMeterWidth = 6;
    static constexpr int kFaderHeight = 120;
    static constexpr int kPeakHoldFrameCount = 60;
};

} // namespace setle::mixer
