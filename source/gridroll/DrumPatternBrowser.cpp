#include "DrumPatternBrowser.h"

namespace setle::gridroll
{

DrumPatternBrowser::DrumPatternBrowser(const DrumPatternLibrary& library)
    : patternLibrary(library)
{
    addAndMakeVisible(list);
    list.setModel(this);
    visiblePatterns = patternLibrary.getPatterns();
}

void DrumPatternBrowser::setCategory(const juce::String& category)
{
    visiblePatterns = category.isEmpty() ? patternLibrary.getPatterns()
                                         : patternLibrary.getPatternsForCategory(category);
    list.updateContent();
    repaint();
}

void DrumPatternBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xEE111820));
    g.setColour(juce::Colour(0xFF35424d));
    g.drawRect(getLocalBounds(), 1);
}

void DrumPatternBrowser::resized()
{
    list.setBounds(getLocalBounds().reduced(4));
}

int DrumPatternBrowser::getNumRows()
{
    return static_cast<int>(visiblePatterns.size());
}

void DrumPatternBrowser::paintListBoxItem(int rowNumber,
                                          juce::Graphics& g,
                                          int width,
                                          int height,
                                          bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(visiblePatterns.size()))
        return;

    const auto& p = visiblePatterns[static_cast<size_t>(rowNumber)];
    g.fillAll(rowIsSelected ? juce::Colour(0xFF314659) : juce::Colour(0x00000000));

    g.setColour(juce::Colours::white.withAlpha(0.90f));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(p.name, 6, 0, width - 120, height, juce::Justification::centredLeft, true);

    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(p.style + " / " + p.feel, width - 150, 0, 144, height, juce::Justification::centredRight, true);
}

void DrumPatternBrowser::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected < 0 || lastRowSelected >= static_cast<int>(visiblePatterns.size()))
        return;

    if (onPatternSelected)
        onPatternSelected(visiblePatterns[static_cast<size_t>(lastRowSelected)]);
}

} // namespace setle::gridroll
