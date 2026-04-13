#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace setle::mixer
{

class MasterStrip : public juce::Component,
                    private juce::Timer
{
public:
    explicit MasterStrip(te::Edit& edit);
    ~MasterStrip() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& event) override;

    static constexpr int kStripWidth = 96;

private:
    void timerCallback() override;

    te::Edit& edit;

    juce::Slider volumeFader;
    juce::Slider panKnob;
    juce::Label nameLabel;

    float peakLeft { 0.0f };
    float peakRight { 0.0f };
    float peakHoldLeft { 0.0f };
    float peakHoldRight { 0.0f };
    int peakHoldFrames { 0 };
    bool clipLatched { false };

    te::LevelMeasurer::Client meterClient;
    te::LevelMeterPlugin* meterPlugin { nullptr };

    static constexpr int kPeakHoldFrameCount = 60;
};

} // namespace setle::mixer
