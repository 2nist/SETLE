#include "ProgressionChordPalette.h"

namespace setle::ui
{

ProgressionChordPalette::ProgressionChordPalette(const juce::String& sessionKey, const juce::String& sessionMode)
    : currentSessionKey(sessionKey), currentSessionMode(sessionMode)
{
    updatePalette();
}

void ProgressionChordPalette::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
}

void ProgressionChordPalette::resized()
{
    auto bounds = getLocalBounds().reduced(8);
    int cellWidth = (bounds.getWidth() - 36) / 7;

    for (int i = 0; i < 7; ++i)
    {
        auto cellBounds = bounds.removeFromLeft(cellWidth + 5);
        if (cells_[i])
            cells_[i]->setBounds(cellBounds);
    }
}

void ProgressionChordPalette::updateSessionKey(const juce::String& newKey)
{
    if (currentSessionKey != newKey)
    {
        currentSessionKey = newKey;
        updatePalette();
    }
}

void ProgressionChordPalette::updateSessionMode(const juce::String& newMode)
{
    if (currentSessionMode != newMode)
    {
        currentSessionMode = newMode;
        updatePalette();
    }
}

void ProgressionChordPalette::setOnCellClicked(std::function<void(int degreeIndex)> callback)
{
    onCellClicked_ = callback;
}

void ProgressionChordPalette::updatePalette()
{
    // Stub palette with 7 diatonic scale degrees
    // In full implementation, would use DiatonicHarmony::buildPalette()
    std::array<PaletteCell, 7> paletteCells = {
        {
            { "I", "C", "T", juce::Colour(0xff8B4513) },      // Tonic (brown)
            { "ii", "D", "SD", juce::Colour(0xff6495ED) },    // Subdominant (blue)
            { "iii", "E", "T", juce::Colour(0xff8B4513) },    // Tonic (brown)
            { "IV", "F", "SD", juce::Colour(0xff6495ED) },    // Subdominant (blue)
            { "V", "G", "D", juce::Colour(0xffDC143C) },      // Dominant (red)
            { "vi", "A", "T", juce::Colour(0xff8B4513) },     // Tonic (brown)
            { "vii°", "B", "D", juce::Colour(0xffDC143C) }    // Dominant (red)
        }
    };

    for (int i = 0; i < 7; ++i)
    {
        // Remove old component if it exists
        if (cells_[i])
            removeChildComponent(cells_[i].get());

        auto cell = std::make_unique<PaletteCellComponent>(paletteCells[i], i);
        cell->setOnClicked([this, degree = i]
        {
            if (onCellClicked_)
                onCellClicked_(degree);
        });
        addAndMakeVisible(*cell);
        cells_[i] = std::move(cell);
    }

    resized();
}

// PaletteCellComponent implementation
ProgressionChordPalette::PaletteCellComponent::PaletteCellComponent(const PaletteCell& cell, int degreeIdx)
    : cell_(cell), degreeIndex_(degreeIdx)
{
}

void ProgressionChordPalette::PaletteCellComponent::paint(juce::Graphics& g)
{
    auto cellBounds = getLocalBounds().reduced(2);
    
    // Draw cell background with function color
    auto bgCol = isHovering ? cell_.tint.brighter(0.3f) : cell_.tint.withAlpha(0.7f);
    g.setColour(bgCol);
    g.fillRoundedRectangle(cellBounds.toFloat(), 4.0f);

    // Draw border
    g.setColour(isHovering ? juce::Colours::white : juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(cellBounds.toFloat(), 4.0f, 2.0f);

    // Draw Roman numeral
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.setFont(juce::FontOptions(16.0f).withStyle("bold"));
    g.drawText(cell_.roman, cellBounds.removeFromTop(cellBounds.getHeight() / 2),
               juce::Justification::centred, true);

    // Draw chord symbol
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(cell_.symbol, cellBounds, juce::Justification::centred, true);
}

void ProgressionChordPalette::PaletteCellComponent::mouseDown(const juce::MouseEvent&)
{
    if (onClicked)
        onClicked();
}

void ProgressionChordPalette::PaletteCellComponent::mouseEnter(const juce::MouseEvent&)
{
    isHovering = true;
    repaint();
}

void ProgressionChordPalette::PaletteCellComponent::mouseExit(const juce::MouseEvent&)
{
    isHovering = false;
    repaint();
}

} // namespace setle::ui
