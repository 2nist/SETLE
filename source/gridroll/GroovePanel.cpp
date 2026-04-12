#include "GroovePanel.h"

namespace setle::gridroll
{

void GroovePanel::setRow(int rowIndex, const RowGroove& groove)
{
    grooves[rowIndex] = groove;
    if (rowIndex == activeRowIndex)
    {
        swingSlider.setValue(groove.swingPercent, juce::dontSendNotification);
        feelBox.setSelectedId(static_cast<int>(groove.feel) + 1, juce::dontSendNotification);
    }
}

GroovePanel::RowGroove GroovePanel::getRow(int rowIndex) const
{
    if (const auto it = grooves.find(rowIndex); it != grooves.end())
        return it->second;

    return {};
}

void GroovePanel::openForRow(int rowIndex)
{
    activeRowIndex = rowIndex;

    title.setText("Groove", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    swingLabel.setText("Swing", juce::dontSendNotification);
    addAndMakeVisible(swingLabel);

    swingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    swingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 20);
    swingSlider.setRange(0.0, 1.0, 0.01);
    swingSlider.onValueChange = [this]
    {
        if (activeRowIndex < 0)
            return;

        auto groove = getRow(activeRowIndex);
        groove.swingPercent = static_cast<float>(swingSlider.getValue());
        setRow(activeRowIndex, groove);
        if (onGrooveChanged)
            onGrooveChanged(activeRowIndex, groove);
    };
    addAndMakeVisible(swingSlider);

    feelBox.clear();
    feelBox.addItem("Straight", 1);
    feelBox.addItem("Swing", 2);
    feelBox.addItem("Pushed", 3);
    feelBox.addItem("Lazy", 4);
    feelBox.onChange = [this]
    {
        if (activeRowIndex < 0)
            return;

        auto groove = getRow(activeRowIndex);
        groove.feel = static_cast<RowGroove::Feel>(juce::jlimit(0, 3, feelBox.getSelectedId() - 1));
        setRow(activeRowIndex, groove);
        if (onGrooveChanged)
            onGrooveChanged(activeRowIndex, groove);
    };
    addAndMakeVisible(feelBox);

    const auto groove = getRow(rowIndex);
    swingSlider.setValue(groove.swingPercent, juce::dontSendNotification);
    feelBox.setSelectedId(static_cast<int>(groove.feel) + 1, juce::dontSendNotification);

    repaint();
}

void GroovePanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xEE101418));
    g.setColour(juce::Colour(0xFF405060));
    g.drawRect(getLocalBounds(), 1);
}

void GroovePanel::resized()
{
    auto area = getLocalBounds().reduced(10);
    title.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);

    auto swingRow = area.removeFromTop(26);
    swingLabel.setBounds(swingRow.removeFromLeft(48));
    swingSlider.setBounds(swingRow);

    area.removeFromTop(6);
    feelBox.setBounds(area.removeFromTop(24));
}

} // namespace setle::gridroll
