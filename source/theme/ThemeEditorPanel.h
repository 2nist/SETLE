#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <optional>
#include <vector>

#include "ThemeManager.h"

class ThemeEditorPanel final : public juce::Component,
                               private ThemeManager::Listener
{
public:
    ThemeEditorPanel();
    ~ThemeEditorPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent&) override;

    void refreshControls();

private:
    class PreviewArea;
    class PreviewWindow;

    void themeChanged() override;

    void populatePresetCombo();
    void captureBaselineTheme();

    int addSection(const juce::String& sectionName);
    void addColourControl(int sectionIndex,
                          const juce::String& name,
                          juce::Colour ThemeData::* member);
    void addFloatControl(int sectionIndex,
                         const juce::String& name,
                         float ThemeData::* member,
                         float minValue,
                         float maxValue,
                         float step);
    void addBoolControl(int sectionIndex,
                        const juce::String& name,
                        bool ThemeData::* member);

    void resetSectionToBaseline(int sectionIndex);
    void resetThemeToDefault();
    void duplicateCurrentTheme();
    void revertUnsavedChanges();
    void exportThemeToFile();
    void importThemeFromFile();
    void showPreviewWindow(bool shouldShow);
    void setPreviewTargetToken(const juce::String& token);
    void clearPreviewTargetToken();

    static juce::String colourToHexRgb(juce::Colour colour);
    static std::optional<juce::Colour> parseHexColour(const juce::String& text);

    struct ColourControl
    {
        juce::Label label;
        juce::TextButton swatchButton;
        juce::TextEditor hexEditor;
        juce::TextButton copyButton { "Copy" };
        juce::TextButton pasteButton { "Paste" };
        juce::TextButton resetButton { "Reset" };
        juce::Colour ThemeData::* member { nullptr };
        int sectionIndex { -1 };
        juce::String name;
    };

    struct FloatControl
    {
        juce::Label label;
        juce::Slider slider;
        juce::TextButton resetButton { "Reset" };
        float ThemeData::* member { nullptr };
        int sectionIndex { -1 };
        juce::String name;
    };

    struct BoolControl
    {
        juce::Label label;
        juce::ToggleButton toggle;
        juce::TextButton resetButton { "Reset" };
        bool ThemeData::* member { nullptr };
        int sectionIndex { -1 };
        juce::String name;
    };

    struct SectionControl
    {
        juce::Label label;
        juce::TextButton resetButton { "Reset Group" };
        juce::String name;
        std::vector<size_t> colourIndices;
        std::vector<size_t> floatIndices;
        std::vector<size_t> boolIndices;
    };

    juce::Label title;
    juce::ComboBox presets;
    juce::TextButton saveAsButton { "Save As" };
    juce::TextButton duplicateButton { "Duplicate" };
    juce::TextButton revertButton { "Revert" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton importButton { "Import" };
    juce::TextButton previewWindowButton { "Live Preview" };
    juce::TextButton resetThemeButton { "Reset" };

    juce::Viewport viewport;
    juce::Component content;
    std::unique_ptr<PreviewArea> previewArea;
    std::unique_ptr<PreviewWindow> previewWindow;

    ThemeData baselineTheme;
    bool suppressCallbacks { false };

    std::vector<std::unique_ptr<SectionControl>> sections;
    std::vector<std::unique_ptr<ColourControl>> colourControls;
    std::vector<std::unique_ptr<FloatControl>> floatControls;
    std::vector<std::unique_ptr<BoolControl>> boolControls;
};
