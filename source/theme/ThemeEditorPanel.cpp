#include "ThemeEditorPanel.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>

#include "ThemePresets.h"
#include "ThemeStyleHelpers.h"
#include "../state/AppPreferences.h"

class ThemeEditorPanel::PreviewArea final : public juce::Component
{
public:
    PreviewArea()
    {
        normalButton.setButtonText("Button");

        toggledButton.setButtonText("Toggled");
        toggledButton.setClickingTogglesState(true);
        toggledButton.setToggleState(true, juce::dontSendNotification);

        horizontalSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        horizontalSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
        horizontalSlider.setRange(0.0, 1.0, 0.01);
        horizontalSlider.setValue(0.65, juce::dontSendNotification);

        rotaryKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        rotaryKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 16);
        rotaryKnob.setRange(0.0, 1.0, 0.01);
        rotaryKnob.setValue(0.4, juce::dontSendNotification);

        combo.addItem("Option A", 1);
        combo.addItem("Option B", 2);
        combo.addItem("Option C", 3);
        combo.setSelectedId(2, juce::dontSendNotification);

        addAndMakeVisible(normalButton);
        addAndMakeVisible(toggledButton);
        addAndMakeVisible(horizontalSlider);
        addAndMakeVisible(rotaryKnob);
        addAndMakeVisible(combo);
    }

    void refreshFromTheme()
    {
        const auto& t = ThemeManager::get().theme();
        normalButton.setColour(juce::TextButton::buttonColourId, t.controlBg);
        normalButton.setColour(juce::TextButton::textColourOffId, t.controlText);
        toggledButton.setColour(juce::TextButton::buttonColourId, t.controlOnBg);
        toggledButton.setColour(juce::TextButton::textColourOnId, t.controlTextOn);
        combo.setColour(juce::ComboBox::backgroundColourId, t.controlBg);
        combo.setColour(juce::ComboBox::textColourId, t.controlText);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = ThemeManager::get().theme();
        const auto bounds = getLocalBounds();

        g.setColour(t.surface2.withAlpha(0.85f));
        g.fillRoundedRectangle(bounds.toFloat(), t.radiusMd);
        g.setColour(t.surfaceEdge.withAlpha(0.9f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), t.radiusMd, t.strokeNormal);

        auto contentBounds = bounds.reduced(8);
        g.setColour(t.inkLight);
        g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
        g.drawText("Live Preview", contentBounds.removeFromTop(18), juce::Justification::centredLeft, false);

        auto bottomArea = contentBounds.removeFromBottom(80);
        auto rowCardArea = bottomArea.removeFromTop(34);
        auto sectionArea = bottomArea;

        const auto rowNormal = setle::theme::rowStyle(t, setle::theme::SurfaceState::normal);
        const auto rowSelected = setle::theme::rowStyle(t, setle::theme::SurfaceState::selected);

        auto rowA = rowCardArea.removeFromLeft(rowCardArea.getWidth() / 2).reduced(2, 2);
        auto rowB = rowCardArea.reduced(2, 2);

        g.setColour(rowNormal.fill);
        g.fillRoundedRectangle(rowA.toFloat(), rowNormal.radius);
        g.setColour(rowNormal.outline);
        g.drawRoundedRectangle(rowA.toFloat(), rowNormal.radius, rowNormal.stroke);
        g.setColour(rowNormal.text);
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("Card / Row", rowA.reduced(6, 2), juce::Justification::centredLeft, false);

        g.setColour(rowSelected.fill);
        g.fillRoundedRectangle(rowB.toFloat(), rowSelected.radius);
        g.setColour(rowSelected.outline);
        g.drawRoundedRectangle(rowB.toFloat(), rowSelected.radius, rowSelected.stroke);
        g.setColour(rowSelected.text);
        g.drawText("Selected", rowB.reduced(6, 2), juce::Justification::centredLeft, false);

        auto lane = sectionArea.reduced(2, 1);
        g.setColour(t.surface1.withAlpha(0.9f));
        g.fillRoundedRectangle(lane.toFloat(), t.radiusSm);

        const int beatW = juce::jmax(1, lane.getWidth() / 12);
        for (int beat = 0; beat <= 12; ++beat)
        {
            const bool isBar = (beat % 4) == 0;
            const auto x = lane.getX() + beat * beatW;
            g.setColour(setle::theme::timelineGridLine(t, isBar, beat == 0 || beat == 8));
            g.drawVerticalLine(x, static_cast<float>(lane.getY()), static_cast<float>(lane.getBottom()));
        }

        auto blockA = lane.removeFromLeft(beatW * 4).reduced(3, 3);
        auto blockB = lane.removeFromLeft(beatW * 4).reduced(3, 3);
        auto blockC = lane.reduced(3, 3);

        const auto sA = setle::theme::progressionBlockStyle(t, false, false, t.zoneA.withAlpha(0.25f));
        const auto sB = setle::theme::progressionBlockStyle(t, true, true, t.zoneB.withAlpha(0.35f));
        const auto sC = setle::theme::progressionBlockStyle(t, false, false, t.zoneC.withAlpha(0.25f));

        auto drawBlock = [&g](juce::Rectangle<int> r, const setle::theme::TimelineBlockStyle& s, const juce::String& label)
        {
            g.setColour(s.fill);
            g.fillRoundedRectangle(r.toFloat(), 3.0f);
            g.setColour(s.outline);
            g.drawRoundedRectangle(r.toFloat(), 3.0f, 1.2f);
            g.setColour(s.label);
            g.setFont(juce::FontOptions(9.5f));
            g.drawText(label, r.reduced(4, 2), juce::Justification::centred, true);
        };

        drawBlock(blockA, sA, "I");
        drawBlock(blockB, sB, "V7");
        drawBlock(blockC, sC, "vi");

        auto zonePreview = getLocalBounds().withTrimmedTop(getHeight() - 22).reduced(8, 0);
        const int zoneW = juce::jmax(1, zonePreview.getWidth() / 3);

        auto zIn = zonePreview.removeFromLeft(zoneW).reduced(1, 1);
        auto zWork = zonePreview.removeFromLeft(zoneW).reduced(1, 1);
        auto zOut = zonePreview.reduced(1, 1);

        g.setColour(setle::theme::panelHeaderBackground(t, setle::theme::ZoneRole::inPanel));
        g.fillRoundedRectangle(zIn.toFloat(), 2.0f);
        g.setColour(setle::theme::textForRole(t, setle::theme::TextRole::primary));
        g.drawText("IN", zIn, juce::Justification::centred, false);

        g.setColour(setle::theme::panelHeaderBackground(t, setle::theme::ZoneRole::workPanel));
        g.fillRoundedRectangle(zWork.toFloat(), 2.0f);
        g.setColour(setle::theme::textForRole(t, setle::theme::TextRole::primary));
        g.drawText("WORK", zWork, juce::Justification::centred, false);

        g.setColour(setle::theme::panelHeaderBackground(t, setle::theme::ZoneRole::outPanel));
        g.fillRoundedRectangle(zOut.toFloat(), 2.0f);
        g.setColour(setle::theme::textForRole(t, setle::theme::TextRole::primary));
        g.drawText("OUT", zOut, juce::Justification::centred, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22);

        auto topRow = area.removeFromTop(26);
        normalButton.setBounds(topRow.removeFromLeft(88));
        topRow.removeFromLeft(6);
        toggledButton.setBounds(topRow.removeFromLeft(88));
        topRow.removeFromLeft(6);
        combo.setBounds(topRow.removeFromLeft(110));

        area.removeFromTop(6);
        auto controlRow = area.removeFromTop(54);
        horizontalSlider.setBounds(controlRow.removeFromLeft(200));
        controlRow.removeFromLeft(8);
        rotaryKnob.setBounds(controlRow.removeFromLeft(70));
    }

private:
    juce::TextButton normalButton;
    juce::TextButton toggledButton;
    juce::Slider horizontalSlider;
    juce::Slider rotaryKnob;
    juce::ComboBox combo;
};

ThemeEditorPanel::ThemeEditorPanel()
{
    ThemeManager::get().addListener(this);

    title.setText("Theme Console", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(title);

    presets.onChange = [this]
    {
        const auto name = presets.getText();
        auto next = ThemePresets::presetByName(name);
        ThemeManager::get().applyTheme(next);
        setle::state::AppPreferences::get().setActiveThemeName(next.presetName);
        captureBaselineTheme();
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
            captureBaselineTheme();
        }
    };

    duplicateButton.onClick = [this] { duplicateCurrentTheme(); };
    revertButton.onClick = [this] { revertUnsavedChanges(); };
    exportButton.onClick = [this] { exportThemeToFile(); };
    importButton.onClick = [this] { importThemeFromFile(); };
    resetThemeButton.onClick = [this] { resetThemeToDefault(); };

    addAndMakeVisible(saveAsButton);
    addAndMakeVisible(duplicateButton);
    addAndMakeVisible(revertButton);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(importButton);
    addAndMakeVisible(resetThemeButton);

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    previewArea = std::make_unique<PreviewArea>();
    content.addAndMakeVisible(*previewArea);

    const int surfaces = addSection("Surfaces / Ink");
    addColourControl(surfaces, "surface0", &ThemeData::surface0);
    addColourControl(surfaces, "surface1", &ThemeData::surface1);
    addColourControl(surfaces, "surface2", &ThemeData::surface2);
    addColourControl(surfaces, "surface3", &ThemeData::surface3);
    addColourControl(surfaces, "surface4", &ThemeData::surface4);
    addColourControl(surfaces, "surfaceEdge", &ThemeData::surfaceEdge);
    addColourControl(surfaces, "inkLight", &ThemeData::inkLight);
    addColourControl(surfaces, "inkMid", &ThemeData::inkMid);
    addColourControl(surfaces, "inkMuted", &ThemeData::inkMuted);
    addColourControl(surfaces, "inkGhost", &ThemeData::inkGhost);
    addColourControl(surfaces, "inkDark", &ThemeData::inkDark);
    addColourControl(surfaces, "headerBg", &ThemeData::headerBg);
    addColourControl(surfaces, "cardBg", &ThemeData::cardBg);
    addColourControl(surfaces, "rowBg", &ThemeData::rowBg);
    addColourControl(surfaces, "rowHover", &ThemeData::rowHover);
    addColourControl(surfaces, "rowSelected", &ThemeData::rowSelected);

    const int accent = addSection("Accent / Zones");
    addColourControl(accent, "accent", &ThemeData::accent);
    addColourControl(accent, "accentWarm", &ThemeData::accentWarm);
    addColourControl(accent, "accentDim", &ThemeData::accentDim);
    addColourControl(accent, "zoneA", &ThemeData::zoneA);
    addColourControl(accent, "zoneB", &ThemeData::zoneB);
    addColourControl(accent, "zoneC", &ThemeData::zoneC);
    addColourControl(accent, "zoneD", &ThemeData::zoneD);
    addColourControl(accent, "zoneMenu", &ThemeData::zoneMenu);
    addColourControl(accent, "zoneBgA", &ThemeData::zoneBgA);
    addColourControl(accent, "zoneBgB", &ThemeData::zoneBgB);
    addColourControl(accent, "zoneBgC", &ThemeData::zoneBgC);
    addColourControl(accent, "zoneBgD", &ThemeData::zoneBgD);

    const int chrome = addSection("UI Chrome");
    addColourControl(chrome, "badgeBg", &ThemeData::badgeBg);
    addColourControl(chrome, "controlBg", &ThemeData::controlBg);
    addColourControl(chrome, "controlOnBg", &ThemeData::controlOnBg);
    addColourControl(chrome, "controlText", &ThemeData::controlText);
    addColourControl(chrome, "controlTextOn", &ThemeData::controlTextOn);
    addColourControl(chrome, "focusOutline", &ThemeData::focusOutline);
    addColourControl(chrome, "sliderTrack", &ThemeData::sliderTrack);
    addColourControl(chrome, "sliderThumb", &ThemeData::sliderThumb);
    addColourControl(chrome, "signalAudio", &ThemeData::signalAudio);
    addColourControl(chrome, "signalMidi", &ThemeData::signalMidi);
    addColourControl(chrome, "signalCv", &ThemeData::signalCv);
    addColourControl(chrome, "signalGate", &ThemeData::signalGate);

    const int typography = addSection("Typography");
    addFloatControl(typography, "sizeDisplay", &ThemeData::sizeDisplay, 8.0f, 24.0f, 0.5f);
    addFloatControl(typography, "sizeHeading", &ThemeData::sizeHeading, 8.0f, 24.0f, 0.5f);
    addFloatControl(typography, "sizeLabel", &ThemeData::sizeLabel, 8.0f, 20.0f, 0.5f);
    addFloatControl(typography, "sizeBody", &ThemeData::sizeBody, 8.0f, 20.0f, 0.5f);
    addFloatControl(typography, "sizeMicro", &ThemeData::sizeMicro, 7.0f, 16.0f, 0.5f);
    addFloatControl(typography, "sizeValue", &ThemeData::sizeValue, 8.0f, 22.0f, 0.5f);
    addFloatControl(typography, "sizeTransport", &ThemeData::sizeTransport, 10.0f, 28.0f, 0.5f);

    const int spacing = addSection("Spacing");
    addFloatControl(spacing, "spaceXs", &ThemeData::spaceXs, 1.0f, 12.0f, 0.5f);
    addFloatControl(spacing, "spaceSm", &ThemeData::spaceSm, 2.0f, 18.0f, 0.5f);
    addFloatControl(spacing, "spaceMd", &ThemeData::spaceMd, 4.0f, 28.0f, 0.5f);
    addFloatControl(spacing, "spaceLg", &ThemeData::spaceLg, 6.0f, 36.0f, 0.5f);
    addFloatControl(spacing, "spaceXl", &ThemeData::spaceXl, 8.0f, 48.0f, 0.5f);
    addFloatControl(spacing, "spaceXxl", &ThemeData::spaceXxl, 10.0f, 56.0f, 0.5f);

    const int layout = addSection("Layout");
    addFloatControl(layout, "menuBarHeight", &ThemeData::menuBarHeight, 18.0f, 60.0f, 1.0f);
    addFloatControl(layout, "zoneAWidth", &ThemeData::zoneAWidth, 120.0f, 420.0f, 1.0f);
    addFloatControl(layout, "zoneCWidth", &ThemeData::zoneCWidth, 120.0f, 420.0f, 1.0f);
    addFloatControl(layout, "zoneDNormHeight", &ThemeData::zoneDNormHeight, 100.0f, 360.0f, 1.0f);
    addFloatControl(layout, "moduleSlotHeight", &ThemeData::moduleSlotHeight, 36.0f, 180.0f, 1.0f);
    addFloatControl(layout, "stepHeight", &ThemeData::stepHeight, 12.0f, 48.0f, 1.0f);
    addFloatControl(layout, "stepWidth", &ThemeData::stepWidth, 12.0f, 48.0f, 1.0f);
    addFloatControl(layout, "knobSize", &ThemeData::knobSize, 18.0f, 72.0f, 1.0f);

    const int metrics = addSection("Control Metrics");
    addFloatControl(metrics, "radiusXs", &ThemeData::radiusXs, 0.0f, 16.0f, 0.5f);
    addFloatControl(metrics, "radiusChip", &ThemeData::radiusChip, 0.0f, 16.0f, 0.5f);
    addFloatControl(metrics, "radiusSm", &ThemeData::radiusSm, 0.0f, 20.0f, 0.5f);
    addFloatControl(metrics, "radiusMd", &ThemeData::radiusMd, 0.0f, 24.0f, 0.5f);
    addFloatControl(metrics, "radiusLg", &ThemeData::radiusLg, 0.0f, 30.0f, 0.5f);
    addFloatControl(metrics, "strokeSubtle", &ThemeData::strokeSubtle, 0.1f, 2.0f, 0.1f);
    addFloatControl(metrics, "strokeNormal", &ThemeData::strokeNormal, 0.2f, 3.0f, 0.1f);
    addFloatControl(metrics, "strokeAccent", &ThemeData::strokeAccent, 0.4f, 4.0f, 0.1f);
    addFloatControl(metrics, "strokeThick", &ThemeData::strokeThick, 0.5f, 6.0f, 0.1f);
    addFloatControl(metrics, "btnCornerRadius", &ThemeData::btnCornerRadius, 0.0f, 20.0f, 0.5f);
    addFloatControl(metrics, "btnBorderStrength", &ThemeData::btnBorderStrength, 0.0f, 1.0f, 0.02f);
    addFloatControl(metrics, "btnFillStrength", &ThemeData::btnFillStrength, 0.0f, 2.0f, 0.02f);
    addFloatControl(metrics, "btnOnFillStrength", &ThemeData::btnOnFillStrength, 0.0f, 2.0f, 0.02f);
    addFloatControl(metrics, "sliderTrackThickness", &ThemeData::sliderTrackThickness, 1.0f, 12.0f, 0.1f);
    addFloatControl(metrics, "sliderCornerRadius", &ThemeData::sliderCornerRadius, 0.0f, 16.0f, 0.2f);
    addFloatControl(metrics, "sliderThumbSize", &ThemeData::sliderThumbSize, 3.0f, 24.0f, 0.1f);
    addFloatControl(metrics, "knobRingThickness", &ThemeData::knobRingThickness, 0.6f, 8.0f, 0.1f);
    addFloatControl(metrics, "knobCapSize", &ThemeData::knobCapSize, 0.2f, 1.0f, 0.01f);
    addFloatControl(metrics, "knobDotSize", &ThemeData::knobDotSize, 1.0f, 8.0f, 0.1f);
    addFloatControl(metrics, "switchWidth", &ThemeData::switchWidth, 12.0f, 60.0f, 0.5f);
    addFloatControl(metrics, "switchHeight", &ThemeData::switchHeight, 8.0f, 30.0f, 0.5f);
    addFloatControl(metrics, "switchCornerRadius", &ThemeData::switchCornerRadius, 2.0f, 20.0f, 0.5f);
    addFloatControl(metrics, "switchThumbInset", &ThemeData::switchThumbInset, 0.0f, 6.0f, 0.1f);

    const int tape = addSection("Timeline / Tape");
    addColourControl(tape, "tapeBase", &ThemeData::tapeBase);
    addColourControl(tape, "tapeClipBg", &ThemeData::tapeClipBg);
    addColourControl(tape, "tapeClipBorder", &ThemeData::tapeClipBorder);
    addColourControl(tape, "tapeSeam", &ThemeData::tapeSeam);
    addColourControl(tape, "tapeBeatTick", &ThemeData::tapeBeatTick);
    addColourControl(tape, "playheadColor", &ThemeData::playheadColor);
    addColourControl(tape, "housingEdge", &ThemeData::housingEdge);

    populatePresetCombo();
    captureBaselineTheme();
    refreshControls();
}

ThemeEditorPanel::~ThemeEditorPanel()
{
    ThemeManager::get().removeListener(this);
}

int ThemeEditorPanel::addSection(const juce::String& sectionName)
{
    auto section = std::make_unique<SectionControl>();
    section->name = sectionName;
    section->label.setText(sectionName, juce::dontSendNotification);
    section->label.setJustificationType(juce::Justification::centredLeft);

    const auto sectionIndex = static_cast<int>(sections.size());
    section->resetButton.onClick = [this, sectionIndex]
    {
        resetSectionToBaseline(sectionIndex);
    };

    content.addAndMakeVisible(section->label);
    content.addAndMakeVisible(section->resetButton);
    sections.push_back(std::move(section));
    return sectionIndex;
}

void ThemeEditorPanel::addColourControl(int sectionIndex,
                                        const juce::String& name,
                                        juce::Colour ThemeData::* member)
{
    auto control = std::make_unique<ColourControl>();
    control->member = member;
    control->name = name;
    control->sectionIndex = sectionIndex;
    control->label.setText(name, juce::dontSendNotification);
    control->label.setJustificationType(juce::Justification::centredLeft);

    control->hexEditor.setInputRestrictions(7, "#0123456789abcdefABCDEF");
    control->hexEditor.setJustification(juce::Justification::centredLeft);
    control->hexEditor.setIndents(2, 2);

    auto* c = control.get();

    auto applyHex = [this, c]
    {
        if (c == nullptr)
            return;

        if (auto parsed = parseHexColour(c->hexEditor.getText()))
            ThemeManager::get().setColour(c->member, *parsed);
        else
            refreshControls();
    };

    control->swatchButton.onClick = [c]
    {
        if (c == nullptr)
            return;

        juce::AlertWindow window("Set Colour", "Hex RGB (RRGGBB)", juce::AlertWindow::NoIcon);
        window.addTextEditor("hex", ThemeEditorPanel::colourToHexRgb(ThemeManager::get().theme().*(c->member)).removeCharacters("#"), "Hex");
        window.addButton("Set", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            if (auto parsed = ThemeEditorPanel::parseHexColour(window.getTextEditorContents("hex")))
                ThemeManager::get().setColour(c->member, *parsed);
        }
    };

    control->hexEditor.onReturnKey = applyHex;
    control->hexEditor.onFocusLost = applyHex;

    control->copyButton.onClick = [c]
    {
        if (c != nullptr)
            juce::SystemClipboard::copyTextToClipboard(c->hexEditor.getText());
    };

    control->pasteButton.onClick = [this, c]
    {
        if (c == nullptr)
            return;

        c->hexEditor.setText(juce::SystemClipboard::getTextFromClipboard(), juce::dontSendNotification);
        if (auto parsed = parseHexColour(c->hexEditor.getText()))
            ThemeManager::get().setColour(c->member, *parsed);
        else
            refreshControls();
    };

    control->resetButton.onClick = [this, c]
    {
        if (c != nullptr)
            ThemeManager::get().setColour(c->member, baselineTheme.*(c->member));
    };

    content.addAndMakeVisible(control->label);
    content.addAndMakeVisible(control->swatchButton);
    content.addAndMakeVisible(control->hexEditor);
    content.addAndMakeVisible(control->copyButton);
    content.addAndMakeVisible(control->pasteButton);
    content.addAndMakeVisible(control->resetButton);

    sections[static_cast<size_t>(sectionIndex)]->colourIndices.push_back(colourControls.size());
    colourControls.push_back(std::move(control));
}

void ThemeEditorPanel::addFloatControl(int sectionIndex,
                                       const juce::String& name,
                                       float ThemeData::* member,
                                       float minValue,
                                       float maxValue,
                                       float step)
{
    auto control = std::make_unique<FloatControl>();
    control->member = member;
    control->name = name;
    control->sectionIndex = sectionIndex;
    control->label.setText(name, juce::dontSendNotification);
    control->label.setJustificationType(juce::Justification::centredLeft);

    control->slider.setSliderStyle(juce::Slider::LinearHorizontal);
    control->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    control->slider.setRange(minValue, maxValue, step);

    auto* c = control.get();
    control->slider.onValueChange = [this, c]
    {
        if (suppressCallbacks || c == nullptr)
            return;

        ThemeManager::get().setFloat(c->member, static_cast<float>(c->slider.getValue()));
    };

    control->resetButton.onClick = [this, c]
    {
        if (c != nullptr)
            ThemeManager::get().setFloat(c->member, baselineTheme.*(c->member));
    };

    content.addAndMakeVisible(control->label);
    content.addAndMakeVisible(control->slider);
    content.addAndMakeVisible(control->resetButton);

    sections[static_cast<size_t>(sectionIndex)]->floatIndices.push_back(floatControls.size());
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

void ThemeEditorPanel::captureBaselineTheme()
{
    baselineTheme = ThemeManager::get().theme();
}

void ThemeEditorPanel::resetSectionToBaseline(int sectionIndex)
{
    if (sectionIndex < 0 || sectionIndex >= static_cast<int>(sections.size()))
        return;

    auto& section = *sections[static_cast<size_t>(sectionIndex)];

    for (const auto index : section.colourIndices)
    {
        if (index < colourControls.size())
        {
            auto& control = *colourControls[index];
            ThemeManager::get().setColour(control.member, baselineTheme.*(control.member));
        }
    }

    for (const auto index : section.floatIndices)
    {
        if (index < floatControls.size())
        {
            auto& control = *floatControls[index];
            ThemeManager::get().setFloat(control.member, baselineTheme.*(control.member));
        }
    }
}

void ThemeEditorPanel::resetThemeToDefault()
{
    ThemeManager::get().applyTheme(ThemePresets::presetByName("Slate Dark"));
    setle::state::AppPreferences::get().setActiveThemeName("Slate Dark");
    presets.setText("Slate Dark", juce::dontSendNotification);
    captureBaselineTheme();
}

void ThemeEditorPanel::duplicateCurrentTheme()
{
    juce::AlertWindow window("Duplicate Theme", "New theme name", juce::AlertWindow::NoIcon);
    window.addTextEditor("name", ThemeManager::get().theme().presetName + " Copy", "Name");
    window.addButton("Duplicate", 1);
    window.addButton("Cancel", 0);

    if (window.runModalLoop() != 1)
        return;

    auto newName = window.getTextEditorContents("name").trim();
    if (newName.isEmpty())
        return;

    auto duplicate = ThemeManager::get().theme();
    duplicate.presetName = newName;
    ThemePresets::saveUserPreset(duplicate);
    ThemeManager::get().applyTheme(duplicate);
    setle::state::AppPreferences::get().setActiveThemeName(duplicate.presetName);
    populatePresetCombo();
    presets.setText(duplicate.presetName, juce::dontSendNotification);
    captureBaselineTheme();
}

void ThemeEditorPanel::revertUnsavedChanges()
{
    ThemeManager::get().applyTheme(baselineTheme);
    setle::state::AppPreferences::get().setActiveThemeName(baselineTheme.presetName);
    presets.setText(baselineTheme.presetName, juce::dontSendNotification);
}

void ThemeEditorPanel::exportThemeToFile()
{
    const auto current = ThemeManager::get().theme();
    const auto safeName = current.presetName.replaceCharacters("/\\:*?\"<>|", "_________");

    juce::FileChooser chooser("Export theme",
                              ThemeManager::get().getUserThemeDir().getChildFile(safeName + ".setle-theme"),
                              "*.setle-theme");

    if (!chooser.browseForFileToSave(true))
        return;

    ThemeManager::get().saveToFile(chooser.getResult());
}

void ThemeEditorPanel::importThemeFromFile()
{
    juce::FileChooser chooser("Import theme", ThemeManager::get().getUserThemeDir(), "*.setle-theme;*.xml");
    if (!chooser.browseForFileToOpen())
        return;

    const auto file = chooser.getResult();
    if (!file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse(file))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (!tree.isValid())
            return;

        auto imported = ThemeManager::themeFromValueTree(tree);
        if (imported.presetName.trim().isEmpty())
            imported.presetName = file.getFileNameWithoutExtension();

        ThemePresets::saveUserPreset(imported);
        ThemeManager::get().applyTheme(imported);
        setle::state::AppPreferences::get().setActiveThemeName(imported.presetName);

        populatePresetCombo();
        presets.setText(imported.presetName, juce::dontSendNotification);
        captureBaselineTheme();
    }
}

juce::String ThemeEditorPanel::colourToHexRgb(juce::Colour colour)
{
    return "#" + juce::String::toHexString(colour.getRed()).paddedLeft('0', 2).toUpperCase()
         + juce::String::toHexString(colour.getGreen()).paddedLeft('0', 2).toUpperCase()
         + juce::String::toHexString(colour.getBlue()).paddedLeft('0', 2).toUpperCase();
}

std::optional<juce::Colour> ThemeEditorPanel::parseHexColour(const juce::String& text)
{
    auto hex = text.trim().retainCharacters("0123456789abcdefABCDEF");
    if (hex.length() == 3)
    {
        juce::String expanded;
        for (auto ch : hex)
            expanded << juce::String::charToString(ch) << juce::String::charToString(ch);
        hex = expanded;
    }

    if (hex.length() != 6)
        return std::nullopt;

    return juce::Colour::fromString("FF" + hex.toUpperCase());
}

void ThemeEditorPanel::paint(juce::Graphics& g)
{
    const auto& t = ThemeManager::get().theme();
    g.fillAll(t.surface1);

    auto headerArea = getLocalBounds().reduced(6).removeFromTop(56);
    g.setColour(t.headerBg.withAlpha(0.72f));
    g.fillRoundedRectangle(headerArea.toFloat(), t.radiusSm);
    g.setColour(t.surfaceEdge.withAlpha(0.85f));
    g.drawRoundedRectangle(headerArea.toFloat().reduced(0.5f), t.radiusSm, t.strokeNormal);

    g.setColour(t.surfaceEdge.withAlpha(0.95f));
    g.drawRect(getLocalBounds(), 1);
}

void ThemeEditorPanel::resized()
{
    auto area = getLocalBounds().reduced(8);

    auto row1 = area.removeFromTop(26);
    title.setBounds(row1.removeFromLeft(108));
    presets.setBounds(row1.removeFromLeft(132));
    row1.removeFromLeft(4);
    saveAsButton.setBounds(row1.removeFromLeft(68));
    row1.removeFromLeft(4);
    duplicateButton.setBounds(row1.removeFromLeft(74));
    row1.removeFromLeft(4);
    revertButton.setBounds(row1.removeFromLeft(60));

    auto row2 = area.removeFromTop(24);
    importButton.setBounds(row2.removeFromLeft(70));
    row2.removeFromLeft(4);
    exportButton.setBounds(row2.removeFromLeft(70));
    row2.removeFromLeft(4);
    resetThemeButton.setBounds(row2.removeFromLeft(60));

    area.removeFromTop(6);
    viewport.setBounds(area);

    int contentWidth = juce::jmax(420, viewport.getWidth() - 10);
    if (contentWidth <= 0)
        contentWidth = 420;

    int y = 8;
    previewArea->setBounds(8, y, contentWidth - 16, 236);
    y += 244;

    for (size_t sectionIndex = 0; sectionIndex < sections.size(); ++sectionIndex)
    {
        auto& section = *sections[sectionIndex];

        section.label.setBounds(10, y, contentWidth - 120, 20);
        section.resetButton.setBounds(contentWidth - 96, y - 1, 84, 22);
        y += 24;

        for (const auto idx : section.colourIndices)
        {
            if (idx >= colourControls.size())
                continue;

            auto& c = *colourControls[idx];
            auto row = juce::Rectangle<int>(10, y, contentWidth - 20, 24);
            c.label.setBounds(row.removeFromLeft(120));
            c.swatchButton.setBounds(row.removeFromLeft(28).reduced(1, 1));
            row.removeFromLeft(4);
            c.hexEditor.setBounds(row.removeFromLeft(84).reduced(0, 1));
            row.removeFromLeft(4);
            c.copyButton.setBounds(row.removeFromLeft(42));
            row.removeFromLeft(2);
            c.pasteButton.setBounds(row.removeFromLeft(44));
            row.removeFromLeft(2);
            c.resetButton.setBounds(row.removeFromLeft(50));
            y += 26;
        }

        for (const auto idx : section.floatIndices)
        {
            if (idx >= floatControls.size())
                continue;

            auto& c = *floatControls[idx];
            auto row = juce::Rectangle<int>(10, y, contentWidth - 20, 24);
            c.label.setBounds(row.removeFromLeft(130));
            c.resetButton.setBounds(row.removeFromRight(50));
            row.removeFromRight(4);
            c.slider.setBounds(row);
            y += 26;
        }

        y += 8;
    }

    content.setSize(contentWidth, y + 8);
}

void ThemeEditorPanel::refreshControls()
{
    const auto& t = ThemeManager::get().theme();
    suppressCallbacks = true;

    for (auto& control : colourControls)
    {
        const auto colour = t.*(control->member);
        control->swatchButton.setButtonText({});
        control->swatchButton.setColour(juce::TextButton::buttonColourId, colour);
        control->hexEditor.setText(colourToHexRgb(colour), juce::dontSendNotification);

        const bool changed = colour != baselineTheme.*(control->member);
        control->label.setText(control->name + (changed ? " *" : ""), juce::dontSendNotification);
        control->label.setColour(juce::Label::textColourId, changed ? t.accentWarm : t.inkMid);
        control->hexEditor.setColour(juce::TextEditor::backgroundColourId, t.surface3.withAlpha(0.65f));
        control->hexEditor.setColour(juce::TextEditor::textColourId, t.inkLight.withAlpha(0.95f));
        control->hexEditor.setColour(juce::TextEditor::outlineColourId, t.surfaceEdge.withAlpha(0.7f));
    }

    for (auto& control : floatControls)
    {
        const auto value = t.*(control->member);
        control->slider.setValue(value, juce::dontSendNotification);

        const bool changed = std::abs(value - baselineTheme.*(control->member)) > 0.0001f;
        control->label.setText(control->name + (changed ? " *" : ""), juce::dontSendNotification);
        control->label.setColour(juce::Label::textColourId, changed ? t.accentWarm : t.inkMid);
    }

    for (auto& section : sections)
    {
        bool sectionChanged = false;

        for (const auto idx : section->colourIndices)
        {
            if (idx < colourControls.size())
            {
                auto& c = *colourControls[idx];
                if ((t.*(c.member)) != (baselineTheme.*(c.member)))
                {
                    sectionChanged = true;
                    break;
                }
            }
        }

        if (!sectionChanged)
        {
            for (const auto idx : section->floatIndices)
            {
                if (idx < floatControls.size())
                {
                    auto& c = *floatControls[idx];
                    if (std::abs((t.*(c.member)) - (baselineTheme.*(c.member))) > 0.0001f)
                    {
                        sectionChanged = true;
                        break;
                    }
                }
            }
        }

        section->label.setText(section->name + (sectionChanged ? "  (changed)" : ""), juce::dontSendNotification);
        section->label.setColour(juce::Label::textColourId, sectionChanged ? t.inkLight : t.inkMuted);
    }

    title.setColour(juce::Label::textColourId, t.inkLight);

    if (presets.getText() != t.presetName)
        presets.setText(t.presetName, juce::dontSendNotification);

    previewArea->refreshFromTheme();

    suppressCallbacks = false;
    repaint();
}

void ThemeEditorPanel::themeChanged()
{
    refreshControls();
}
