#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "EditToolManager.h"
#include "../theme/ThemeManager.h"

namespace setle::ui
{

class ToolPaletteComponent : public juce::Component,
                             public EditToolManager::Listener
{
public:
    ToolPaletteComponent();
    ~ToolPaletteComponent() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void activeToolChanged(EditTool newTool) override;

private:
    std::array<juce::TextButton, 8> toolButtons;
    static constexpr int kButtonWidth = 28;
    static constexpr int kButtonHeight = 22;
};

} // namespace setle::ui
