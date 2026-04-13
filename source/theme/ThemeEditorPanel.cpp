#include "ThemeEditorPanel.h"

#include "ThemePresets.h"
#include "../state/AppPreferences.h"

ThemeEditorPanel::ThemeEditorPanel()
{
    ThemeManager::get().addListener(this);

    title.setText("Theme", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    presets.onChange = [this]
    {
        const auto name = presets.getText();
        auto next = ThemePresets::presetByName(name);
        ThemeManager::get().applyTheme(next);
        setle::state::AppPreferences::get().setActiveThemeName(next.presetName);
    };
    addAndMakeVisible(presets);

    saveAsButton.onClick = [this]
    {
        juce::AlertWindow window("Save Theme", "Name", juce::AlertWindow::NoIcon);
        window.addTextEditor("name", ThemeManager::get().theme().presetName, "Name");
        window.addButton("Save", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            const auto trimmed = window.getTextEditorContents("name").trim();
            if (trimmed.isEmpty())
                return;

            auto theme = ThemeManager::get().theme();
            theme.presetName = trimmed;
            ThemePresets::saveUserPreset(theme);
            ThemeManager::get().applyTheme(theme);
            setle::state::AppPreferences::get().setActiveThemeName(theme.presetName);
            populatePresetCombo();
            presets.setText(trimmed, juce::dontSendNotification);
        }
    };
    addAndMakeVisible(saveAsButton);

    resetButton.onClick = [this]
    {
        ThemeManager::get().applyTheme(ThemePresets::presetByName("Slate Dark"));
        setle::state::AppPreferences::get().setActiveThemeName("Slate Dark");
        populatePresetCombo();
        presets.setText("Slate Dark", juce::dontSendNotification);
    };
    addAndMakeVisible(resetButton);

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    addColourControl("Surface 1", &ThemeData::surface1);
    addColourControl("Surface 2", &ThemeData::surface2);
    addColourControl("Surface 3", &ThemeData::surface3);
    addColourControl("Edge", &ThemeData::surfaceEdge);
    addColourControl("Ink", &ThemeData::inkLight);
    addColourControl("Accent", &ThemeData::accent);
    addColourControl("IN", &ThemeData::zoneA);
    addColourControl("WORK", &ThemeData::zoneB);
    addColourControl("OUT", &ThemeData::zoneC);
    addColourControl("Timeline", &ThemeData::zoneD);

    addFloatControl("Radius", &ThemeData::radiusMd, 2.0f, 14.0f, 0.5f);
    addFloatControl("Button Radius", &ThemeData::btnCornerRadius, 0.0f, 12.0f, 0.5f);
    addFloatControl("Slider Size", &ThemeData::sliderThumbSize, 4.0f, 18.0f, 0.5f);

    populatePresetCombo();
    refreshControls();
}

ThemeEditorPanel::~ThemeEditorPanel()
{
    ThemeManager::get().removeListener(this);
}

void ThemeEditorPanel::addColourControl(const juce::String& name, juce::Colour ThemeData::* member)
{
    auto control = std::make_unique<ColourControl>();
    control->member = member;
    control->label.setText(name, juce::dontSendNotification);
    content.addAndMakeVisible(control->label);

    control->button.onClick = [this, member]
    {
        const auto current = ThemeManager::get().theme().*member;
        juce::AlertWindow window("Set Colour", "Hex RGB (e.g. 4A9EFF)", juce::AlertWindow::NoIcon);
        window.addTextEditor("hex", current.toString().substring(2), "Hex");
        window.addButton("Set", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            auto cleaned = window.getTextEditorContents("hex").retainCharacters("0123456789aAbBcCdDeEfF");
            if (cleaned.length() < 6)
                return;
            if (cleaned.length() > 6)
                cleaned = cleaned.substring(cleaned.length() - 6);
            const auto colour = juce::Colour::fromString("FF" + cleaned.toUpperCase());
            ThemeManager::get().setColour(member, colour);
        }
    };

    content.addAndMakeVisible(control->button);
    colourControls.push_back(std::move(control));
}

void ThemeEditorPanel::addFloatControl(const juce::String& name, float ThemeData::* member, float minValue, float maxValue, float step)
{
    auto control = std::make_unique<FloatControl>();
    control->member = member;
    control->label.setText(name, juce::dontSendNotification);
    content.addAndMakeVisible(control->label);

    control->slider.setSliderStyle(juce::Slider::LinearHorizontal);
    control->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    control->slider.setRange(minValue, maxValue, step);
    control->slider.onValueChange = [member, slider = &control->slider]
    {
        ThemeManager::get().setFloat(member, static_cast<float>(slider->getValue()));
    };

    content.addAndMakeVisible(control->slider);
    floatControls.push_back(std::move(control));
}

void ThemeEditorPanel::populatePresetCombo()
{
    presets.clear(juce::dontSendNotification);
    int id = 1;
    for (const auto& theme : ThemePresets::builtInPresets())
        presets.addItem(theme.presetName, id++);

    for (const auto& theme : ThemePresets::userPresets())
        presets.addItem(theme.presetName, id++);
}

void ThemeEditorPanel::paint(juce::Graphics& g)
{
    const auto& t = ThemeManager::get().theme();
    g.fillAll(t.surface1);
    g.setColour(t.surfaceEdge);
    g.drawRect(getLocalBounds(), 1);
}

void ThemeEditorPanel::resized()
{
    auto area = getLocalBounds().reduced(8);

    auto header = area.removeFromTop(28);
    title.setBounds(header.removeFromLeft(60));
    presets.setBounds(header.removeFromLeft(130));
    saveAsButton.setBounds(header.removeFromLeft(70));
    header.removeFromLeft(4);
    resetButton.setBounds(header.removeFromLeft(60));

    area.removeFromTop(6);
    viewport.setBounds(area);

    int y = 6;
    const int rowH = 24;

    for (auto& control : colourControls)
    {
        control->label.setBounds(6, y, 120, rowH);
        control->button.setBounds(130, y + 2, 120, rowH - 4);
        y += rowH + 4;
    }

    for (auto& control : floatControls)
    {
        control->label.setBounds(6, y, 120, rowH);
        control->slider.setBounds(130, y, 160, rowH);
        y += rowH + 4;
    }

    content.setSize(300, y + 8);
}

void ThemeEditorPanel::refreshControls()
{
    const auto& t = ThemeManager::get().theme();
    for (auto& control : colourControls)
        control->button.setColour(juce::TextButton::buttonColourId, t.*(control->member));

    for (auto& control : floatControls)
        control->slider.setValue(t.*(control->member), juce::dontSendNotification);

    title.setColour(juce::Label::textColourId, t.inkLight);

    if (presets.getText() != t.presetName)
        presets.setText(t.presetName, juce::dontSendNotification);

    repaint();
}

void ThemeEditorPanel::themeChanged()
{
    refreshControls();
}
