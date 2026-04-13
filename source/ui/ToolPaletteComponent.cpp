#include "ToolPaletteComponent.h"

namespace setle::ui
{

ToolPaletteComponent::ToolPaletteComponent()
{
    const std::array<const char*, 7> labels { "Sel", "Draw", "Ers", "Splt", "Str", "Lst", "Mrq" };

    for (size_t i = 0; i < toolButtons.size(); ++i)
    {
        auto& b = toolButtons[i];
        b.setButtonText(labels[i]);
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.onClick = [i]
        {
            EditToolManager::get().setActiveTool(static_cast<EditTool>(i));
        };
        addAndMakeVisible(b);
    }

    EditToolManager::get().addListener(this);
}

ToolPaletteComponent::~ToolPaletteComponent()
{
    EditToolManager::get().removeListener(this);
}

void ToolPaletteComponent::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop((r.getHeight() - kButtonHeight) / 2);
    for (auto& b : toolButtons)
    {
        b.setBounds(r.removeFromLeft(kButtonWidth).reduced(4, 3));
    }
}

void ToolPaletteComponent::paint(juce::Graphics& g)
{
    // subtle background
    g.fillAll(juce::Colours::transparentBlack);
}

void ToolPaletteComponent::activeToolChanged(EditTool newTool)
{
    const auto& theme = ThemeManager::get().theme();
    for (size_t i = 0; i < toolButtons.size(); ++i)
    {
        auto& b = toolButtons[i];
        if (static_cast<int>(newTool) == static_cast<int>(i))
            b.setColour(juce::TextButton::buttonColourId, theme.accent);
        else
            b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    }
    repaint();
}

} // namespace setle::ui
