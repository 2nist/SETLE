#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "DrumPatternLibrary.h"

namespace setle::gridroll
{

class DrumPatternBrowser : public juce::Component,
                           private juce::ListBoxModel
{
public:
    explicit DrumPatternBrowser(const DrumPatternLibrary& library);

    std::function<void(const DrumPattern&)> onPatternSelected;

    void setCategory(const juce::String& category);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem(int rowNumber,
                          juce::Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;

    const DrumPatternLibrary& patternLibrary;
    std::vector<DrumPattern> visiblePatterns;
    juce::ListBox list;
};

} // namespace setle::gridroll
