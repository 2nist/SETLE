#pragma once

#include "EffModel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace setle::eff
{

/**
 * Modal overlay that lists all BlockType values grouped into MIDI / Audio columns.
 * Clicking a block type fires onBlockChosen(BlockType), then the overlay removes itself.
 *
 * Usage:
 *   auto* picker = new EffBlockPickerOverlay();
 *   picker->onBlockChosen = [this](BlockType t) { insertBlock(t); };
 *   addAndMakeVisible(picker);
 *   picker->setBounds(getLocalBounds());
 */
class EffBlockPickerOverlay final : public juce::Component
{
public:
    EffBlockPickerOverlay();

    std::function<void(BlockType)> onBlockChosen;

    void resized() override;
    void paint(juce::Graphics& g) override;

    // Clicking outside the panel dismisses
    void mouseDown(const juce::MouseEvent& e) override;

private:
    struct Entry
    {
        BlockType   type;
        juce::String label;
    };

    std::vector<Entry> midiEntries;
    std::vector<Entry> audioEntries;

    juce::Rectangle<int> panelBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffBlockPickerOverlay)
};

} // namespace setle::eff
