#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace setle::ui
{

class ProgressionChordPalette final : public juce::Component
{
public:
    explicit ProgressionChordPalette(const juce::String& sessionKey, const juce::String& sessionMode);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void updateSessionKey(const juce::String& newKey);
    void updateSessionMode(const juce::String& newMode);
    void setOnCellClicked(std::function<void(int degreeIndex)> callback);

private:
    struct PaletteCell
    {
        juce::String roman;
        juce::String symbol;
        juce::String function;
        juce::Colour tint;
    };

    class PaletteCellComponent final : public juce::Component
    {
    public:
        explicit PaletteCellComponent(const PaletteCell& cell, int degreeIdx);
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseEnter(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        void setOnClicked(std::function<void()> callback) { onClicked = callback; }

    private:
        PaletteCell cell_;
        int degreeIndex_;
        std::function<void()> onClicked;
        bool isHovering { false };
    };

    void updatePalette();

    std::array<std::unique_ptr<PaletteCellComponent>, 7> cells_;
    std::function<void(int)> onCellClicked_;
    
    juce::String currentSessionKey { "C" };
    juce::String currentSessionMode { "ionian" };
};

} // namespace setle::ui
