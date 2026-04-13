#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ThemeManager.h"

class ThemeEditorPanel final : public juce::Component,
                               private ThemeManager::Listener
{
public:
    ThemeEditorPanel();
    ~ThemeEditorPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshControls();

private:
    void themeChanged() override;
    void populatePresetCombo();

    void addColourControl(const juce::String& name, juce::Colour ThemeData::* member);
    void addFloatControl(const juce::String& name, float ThemeData::* member, float minValue, float maxValue, float step);

    struct ColourControl
    {
        juce::Label label;
        juce::TextButton button;
        juce::Colour ThemeData::* member { nullptr };
    };

    struct FloatControl
    {
        juce::Label label;
        juce::Slider slider;
        float ThemeData::* member { nullptr };
    };

    juce::Label title;
    juce::ComboBox presets;
    juce::TextButton saveAsButton { "Save As" };
    juce::TextButton resetButton { "Reset" };

    juce::Viewport viewport;
    juce::Component content;

    std::vector<std::unique_ptr<ColourControl>> colourControls;
    std::vector<std::unique_ptr<FloatControl>> floatControls;
};
