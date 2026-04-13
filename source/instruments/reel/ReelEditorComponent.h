#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ReelProcessor.h"

namespace setle::instruments::reel
{

class ReelEditorComponent final : public juce::Component,
                                  private juce::Timer
{
public:
    explicit ReelEditorComponent(::ReelProcessor& processorRef);
    ~ReelEditorComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    enum class DragHandle
    {
        none,
        start,
        end
    };

    void timerCallback() override;
    void syncFromProcessor();
    void updateStartFromX(int x);
    void updateEndFromX(int x);
    juce::Rectangle<int> getWaveArea() const;

    ::ReelProcessor& processor;

    juce::ToggleButton loopToggle { "Loop" };
    juce::Slider pitchSlider;
    juce::Slider volumeSlider;
    juce::ComboBox modeSelector;

    juce::Label modeLabel;
    juce::Label pitchLabel;
    juce::Label volumeLabel;

    DragHandle dragHandle { DragHandle::none };
};

} // namespace setle::instruments::reel
