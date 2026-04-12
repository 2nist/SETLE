#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <map>

namespace setle::gridroll
{

class GroovePanel : public juce::Component
{
public:
    struct RowGroove
    {
        float swingPercent { 0.0f };
        enum class Feel { Straight, Swing, Pushed, Lazy } feel { Feel::Straight };
    };

    void setRow(int rowIndex, const RowGroove& groove);
    RowGroove getRow(int rowIndex) const;

    std::function<void(int rowIndex, RowGroove)> onGrooveChanged;

    void openForRow(int rowIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    int activeRowIndex { -1 };
    std::map<int, RowGroove> grooves;

    juce::Label title;
    juce::Label swingLabel;
    juce::Slider swingSlider;
    juce::ComboBox feelBox;
};

} // namespace setle::gridroll
