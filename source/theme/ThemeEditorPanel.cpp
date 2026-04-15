#include "ThemeEditorPanel.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <functional>

#include "ThemePresets.h"
#include "ThemeStyleHelpers.h"
#include "../state/AppPreferences.h"

namespace
{
juce::String toHexRgb(juce::Colour colour)
{
    return "#" + juce::String::toHexString(colour.getRed()).paddedLeft('0', 2).toUpperCase()
         + juce::String::toHexString(colour.getGreen()).paddedLeft('0', 2).toUpperCase()
         + juce::String::toHexString(colour.getBlue()).paddedLeft('0', 2).toUpperCase();
}

std::optional<juce::Colour> tryParseRgbHex(const juce::String& text)
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

juce::String colourTokenName(juce::Colour ThemeData::* member)
{
    if (member == &ThemeData::surface0) return "surface0";
    if (member == &ThemeData::surface1) return "surface1";
    if (member == &ThemeData::surface2) return "surface2";
    if (member == &ThemeData::surface3) return "surface3";
    if (member == &ThemeData::surface4) return "surface4";
    if (member == &ThemeData::surfaceEdge) return "surfaceEdge";
    if (member == &ThemeData::inkLight) return "inkLight";
    if (member == &ThemeData::inkMid) return "inkMid";
    if (member == &ThemeData::inkMuted) return "inkMuted";
    if (member == &ThemeData::inkGhost) return "inkGhost";
    if (member == &ThemeData::inkDark) return "inkDark";
    if (member == &ThemeData::accent) return "accent";
    if (member == &ThemeData::accentWarm) return "accentWarm";
    if (member == &ThemeData::accentDim) return "accentDim";
    if (member == &ThemeData::headerBg) return "headerBg";
    if (member == &ThemeData::cardBg) return "cardBg";
    if (member == &ThemeData::rowBg) return "rowBg";
    if (member == &ThemeData::rowHover) return "rowHover";
    if (member == &ThemeData::rowSelected) return "rowSelected";
    if (member == &ThemeData::badgeBg) return "badgeBg";
    if (member == &ThemeData::controlBg) return "controlBg";
    if (member == &ThemeData::controlOnBg) return "controlOnBg";
    if (member == &ThemeData::controlText) return "controlText";
    if (member == &ThemeData::controlTextOn) return "controlTextOn";
    if (member == &ThemeData::focusOutline) return "focusOutline";
    if (member == &ThemeData::sliderTrack) return "sliderTrack";
    if (member == &ThemeData::sliderThumb) return "sliderThumb";
    if (member == &ThemeData::zoneA) return "zoneA";
    if (member == &ThemeData::zoneB) return "zoneB";
    if (member == &ThemeData::zoneC) return "zoneC";
    if (member == &ThemeData::zoneD) return "zoneD";
    if (member == &ThemeData::zoneMenu) return "zoneMenu";
    if (member == &ThemeData::zoneBgA) return "zoneBgA";
    if (member == &ThemeData::zoneBgB) return "zoneBgB";
    if (member == &ThemeData::zoneBgC) return "zoneBgC";
    if (member == &ThemeData::zoneBgD) return "zoneBgD";
    if (member == &ThemeData::signalAudio) return "signalAudio";
    if (member == &ThemeData::signalMidi) return "signalMidi";
    if (member == &ThemeData::signalCv) return "signalCv";
    if (member == &ThemeData::signalGate) return "signalGate";
    if (member == &ThemeData::tapeBase) return "tapeBase";
    if (member == &ThemeData::tapeClipBg) return "tapeClipBg";
    if (member == &ThemeData::tapeClipBorder) return "tapeClipBorder";
    if (member == &ThemeData::tapeSeam) return "tapeSeam";
    if (member == &ThemeData::tapeBeatTick) return "tapeBeatTick";
    if (member == &ThemeData::playheadColor) return "playheadColor";
    if (member == &ThemeData::housingEdge) return "housingEdge";
    return {};
}

juce::String floatTokenName(float ThemeData::* member)
{
    if (member == &ThemeData::sizeDisplay) return "sizeDisplay";
    if (member == &ThemeData::sizeHeading) return "sizeHeading";
    if (member == &ThemeData::sizeLabel) return "sizeLabel";
    if (member == &ThemeData::sizeBody) return "sizeBody";
    if (member == &ThemeData::sizeMicro) return "sizeMicro";
    if (member == &ThemeData::sizeValue) return "sizeValue";
    if (member == &ThemeData::sizeTransport) return "sizeTransport";
    if (member == &ThemeData::spaceXs) return "spaceXs";
    if (member == &ThemeData::spaceSm) return "spaceSm";
    if (member == &ThemeData::spaceMd) return "spaceMd";
    if (member == &ThemeData::spaceLg) return "spaceLg";
    if (member == &ThemeData::spaceXl) return "spaceXl";
    if (member == &ThemeData::spaceXxl) return "spaceXxl";
    if (member == &ThemeData::menuBarHeight) return "menuBarHeight";
    if (member == &ThemeData::zoneAWidth) return "zoneAWidth";
    if (member == &ThemeData::zoneCWidth) return "zoneCWidth";
    if (member == &ThemeData::zoneDNormHeight) return "zoneDNormHeight";
    if (member == &ThemeData::moduleSlotHeight) return "moduleSlotHeight";
    if (member == &ThemeData::stepHeight) return "stepHeight";
    if (member == &ThemeData::stepWidth) return "stepWidth";
    if (member == &ThemeData::knobSize) return "knobSize";
    if (member == &ThemeData::radiusXs) return "radiusXs";
    if (member == &ThemeData::radiusChip) return "radiusChip";
    if (member == &ThemeData::radiusSm) return "radiusSm";
    if (member == &ThemeData::radiusMd) return "radiusMd";
    if (member == &ThemeData::radiusLg) return "radiusLg";
    if (member == &ThemeData::strokeSubtle) return "strokeSubtle";
    if (member == &ThemeData::strokeNormal) return "strokeNormal";
    if (member == &ThemeData::strokeAccent) return "strokeAccent";
    if (member == &ThemeData::strokeThick) return "strokeThick";
    if (member == &ThemeData::btnCornerRadius) return "btnCornerRadius";
    if (member == &ThemeData::btnBorderStrength) return "btnBorderStrength";
    if (member == &ThemeData::btnFillStrength) return "btnFillStrength";
    if (member == &ThemeData::btnOnFillStrength) return "btnOnFillStrength";
    if (member == &ThemeData::sliderTrackThickness) return "sliderTrackThickness";
    if (member == &ThemeData::sliderCornerRadius) return "sliderCornerRadius";
    if (member == &ThemeData::sliderThumbSize) return "sliderThumbSize";
    if (member == &ThemeData::knobRingThickness) return "knobRingThickness";
    if (member == &ThemeData::knobCapSize) return "knobCapSize";
    if (member == &ThemeData::knobDotSize) return "knobDotSize";
    if (member == &ThemeData::switchWidth) return "switchWidth";
    if (member == &ThemeData::switchHeight) return "switchHeight";
    if (member == &ThemeData::switchCornerRadius) return "switchCornerRadius";
    if (member == &ThemeData::switchThumbInset) return "switchThumbInset";
    if (member == &ThemeData::materialChassisTexture) return "materialChassisTexture";
    if (member == &ThemeData::materialGlassDepth) return "materialGlassDepth";
    if (member == &ThemeData::materialInsetFuzz) return "materialInsetFuzz";
    if (member == &ThemeData::materialGlowAmount) return "materialGlowAmount";
    return {};
}

juce::String boolTokenName(bool ThemeData::* member)
{
    if (member == &ThemeData::isPebbled) return "isPebbled";
    return {};
}

void setThemeBoolMember(bool ThemeData::* member, bool value)
{
    if (member == &ThemeData::isPebbled)
    {
        ThemeManager::get().setPebbled(value);
        return;
    }

    auto theme = ThemeManager::get().theme();
    theme.*member = value;
    ThemeManager::get().applyTheme(theme);
}
} // namespace

class ThemeEditorPanel::PreviewArea final : public juce::Component
{
public:
    class ScrollContent final : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override
        {
            const auto& t = ThemeManager::get().theme();
            g.fillAll(t.surface2.withAlpha(0.92f));

            auto b = getLocalBounds();
            for (int y = 0; y < b.getHeight(); y += 20)
            {
                g.setColour((y % 40 == 0 ? t.surfaceEdge : t.surfaceEdge.withAlpha(0.5f)).withAlpha(0.55f));
                g.drawHorizontalLine(y, static_cast<float>(b.getX()), static_cast<float>(b.getRight()));
            }

            for (int x = 0; x < b.getWidth(); x += 28)
            {
                g.setColour((x % 112 == 0 ? t.surfaceEdge.withAlpha(0.85f) : t.surfaceEdge.withAlpha(0.38f)));
                g.drawVerticalLine(x, static_cast<float>(b.getY()), static_cast<float>(b.getBottom()));
            }

            g.setColour(t.inkGhost.withAlpha(0.9f));
            g.setFont(juce::FontOptions(9.0f));
            g.drawText("Scrollable content area", b.reduced(8, 6), juce::Justification::topLeft, false);
        }
    };

    PreviewArea()
    {
        normalButton.setButtonText("Button");
        armedButton.setButtonText("Armed");
        armedButton.setClickingTogglesState(true);
        armedButton.setToggleState(true, juce::dontSendNotification);

        disabledButton.setButtonText("Disabled");
        disabledButton.setEnabled(false);

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

        disabledCombo.addItem("Offline", 1);
        disabledCombo.addItem("Bypassed", 2);
        disabledCombo.setSelectedId(1, juce::dontSendNotification);
        disabledCombo.setEnabled(false);

        verticalSlider.setSliderStyle(juce::Slider::LinearVertical);
        verticalSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        verticalSlider.setRange(0.0, 1.0, 0.01);
        verticalSlider.setValue(0.72, juce::dontSendNotification);

        disabledSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        disabledSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
        disabledSlider.setRange(0.0, 1.0, 0.01);
        disabledSlider.setValue(0.28, juce::dontSendNotification);
        disabledSlider.setEnabled(false);

        checkToggle.setButtonText("Snap");
        checkToggle.setToggleState(true, juce::dontSendNotification);

        modeToggle.setButtonText("Latch");
        modeToggle.setToggleState(false, juce::dontSendNotification);

        textEditor.setText("Clip Name", juce::dontSendNotification);
        textEditor.setJustification(juce::Justification::centredLeft);

        infoLabel.setText("Label Preview", juce::dontSendNotification);
        infoLabel.setJustificationType(juce::Justification::centredLeft);
        infoLabel.setColour(juce::Label::backgroundColourId, ThemeManager::get().theme().surface2.withAlpha(0.8f));
        infoLabel.setColour(juce::Label::outlineColourId, ThemeManager::get().theme().surfaceEdge.withAlpha(0.75f));

        tabbed.addTab("Clip", ThemeManager::get().theme().surface2, new juce::Component(), true);
        tabbed.addTab("Mixer", ThemeManager::get().theme().surface2, new juce::Component(), true);
        tabbed.addTab("Routing", ThemeManager::get().theme().surface2, new juce::Component(), true);
        tabbed.setCurrentTabIndex(0);

        menuPreviewButton.setButtonText("Menu Preview");
        menuPreviewButton.onClick = [this]
        {
            juce::PopupMenu menu;
            menu.addItem(1, "Add Note");
            menu.addItem(2, "Quantize");
            menu.addSeparator();
            menu.addItem(3, "Freeze Track");
            menu.addItem(4, "Delete", false, false);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuPreviewButton));
        };

        viewportContent.setSize(720, 220);
        viewport.setViewedComponent(&viewportContent, false);
        viewport.setScrollBarsShown(true, true);

        addAndMakeVisible(normalButton);
        addAndMakeVisible(armedButton);
        addAndMakeVisible(disabledButton);
        addAndMakeVisible(toggledButton);
        addAndMakeVisible(horizontalSlider);
        addAndMakeVisible(disabledSlider);
        addAndMakeVisible(rotaryKnob);
        addAndMakeVisible(verticalSlider);
        addAndMakeVisible(combo);
        addAndMakeVisible(disabledCombo);
        addAndMakeVisible(checkToggle);
        addAndMakeVisible(modeToggle);
        addAndMakeVisible(textEditor);
        addAndMakeVisible(infoLabel);
        addAndMakeVisible(tabbed);
        addAndMakeVisible(menuPreviewButton);
        addAndMakeVisible(viewport);
    }

    void refreshFromTheme()
    {
        const auto& t = ThemeManager::get().theme();
        normalButton.setColour(juce::TextButton::buttonColourId, t.controlBg);
        normalButton.setColour(juce::TextButton::textColourOffId, t.controlText);
        armedButton.setColour(juce::TextButton::buttonColourId, t.accentWarm.withAlpha(0.75f));
        armedButton.setColour(juce::TextButton::textColourOffId, t.inkLight);
        disabledButton.setColour(juce::TextButton::buttonColourId, t.controlBg);
        disabledButton.setColour(juce::TextButton::textColourOffId, t.controlText);
        toggledButton.setColour(juce::TextButton::buttonColourId, t.controlOnBg);
        toggledButton.setColour(juce::TextButton::textColourOnId, t.controlTextOn);
        combo.setColour(juce::ComboBox::backgroundColourId, t.controlBg);
        combo.setColour(juce::ComboBox::textColourId, t.controlText);
        disabledCombo.setColour(juce::ComboBox::backgroundColourId, t.controlBg);
        disabledCombo.setColour(juce::ComboBox::textColourId, t.controlText);
        textEditor.setColour(juce::TextEditor::backgroundColourId, t.controlBg);
        textEditor.setColour(juce::TextEditor::outlineColourId, t.surfaceEdge.withAlpha(0.7f));
        textEditor.setColour(juce::TextEditor::focusedOutlineColourId, t.focusOutline.withAlpha(0.8f));
        textEditor.setColour(juce::TextEditor::textColourId, t.inkLight.withAlpha(0.95f));
        infoLabel.setColour(juce::Label::backgroundColourId, t.surface2.withAlpha(0.8f));
        infoLabel.setColour(juce::Label::outlineColourId, t.surfaceEdge.withAlpha(0.75f));
        infoLabel.setColour(juce::Label::textColourId, t.inkMid.withAlpha(0.95f));
        for (int i = 0; i < tabbed.getNumTabs(); ++i)
            tabbed.setTabBackgroundColour(i, t.surface2.withAlpha(0.85f));
        viewportContent.repaint();
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
        auto scrollerArea = contentBounds.removeFromBottom(68);

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

        auto scrollFrame = scrollerArea.reduced(0, 4);
        g.setColour(t.surface1.withAlpha(0.95f));
        g.fillRoundedRectangle(scrollFrame.toFloat(), t.radiusSm);
        g.setColour(t.surfaceEdge.withAlpha(0.75f));
        g.drawRoundedRectangle(scrollFrame.toFloat(), t.radiusSm, juce::jmax(0.8f, t.strokeNormal));
        g.setFont(juce::FontOptions(9.0f));
        g.setColour(t.inkMuted.withAlpha(0.9f));
        g.drawText("Scrollbar / Viewport Preview", scrollFrame.removeFromTop(14).reduced(4, 1), juce::Justification::centredLeft, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        area.removeFromTop(22);

        auto topRow = area.removeFromTop(26);
        normalButton.setBounds(topRow.removeFromLeft(66));
        topRow.removeFromLeft(4);
        armedButton.setBounds(topRow.removeFromLeft(66));
        topRow.removeFromLeft(4);
        menuPreviewButton.setBounds(topRow.removeFromLeft(92));
        topRow.removeFromLeft(6);
        disabledButton.setBounds(topRow.removeFromLeft(74));
        topRow.removeFromLeft(6);
        toggledButton.setBounds(topRow.removeFromLeft(74));
        topRow.removeFromLeft(6);
        combo.setBounds(topRow.removeFromLeft(110));
        topRow.removeFromLeft(6);
        disabledCombo.setBounds(topRow.removeFromLeft(106));

        area.removeFromTop(6);
        auto controlBlock = area.removeFromTop(92);
        auto sliderCol = controlBlock.removeFromLeft(340);
        horizontalSlider.setBounds(sliderCol.removeFromTop(28));
        sliderCol.removeFromTop(6);
        disabledSlider.setBounds(sliderCol.removeFromTop(26));
        controlBlock.removeFromLeft(10);
        rotaryKnob.setBounds(controlBlock.removeFromLeft(74));
        controlBlock.removeFromLeft(8);
        verticalSlider.setBounds(controlBlock.removeFromLeft(20).withTrimmedTop(4).withTrimmedBottom(8));

        area.removeFromTop(4);
        auto toggleRow = area.removeFromTop(26);
        checkToggle.setBounds(toggleRow.removeFromLeft(70));
        toggleRow.removeFromLeft(6);
        modeToggle.setBounds(toggleRow.removeFromLeft(78));
        toggleRow.removeFromLeft(6);
        infoLabel.setBounds(toggleRow.removeFromLeft(130));
        toggleRow.removeFromLeft(6);
        textEditor.setBounds(toggleRow.removeFromLeft(140));

        area.removeFromTop(6);
        tabbed.setBounds(area.removeFromTop(26));

        area.removeFromTop(6);
        viewport.setBounds(area.removeFromBottom(58));
        viewportContent.setSize(juce::jmax(680, viewport.getWidth() + 260),
                                juce::jmax(200, viewport.getHeight() + 100));
    }

private:
    juce::TextButton normalButton;
    juce::TextButton armedButton;
    juce::TextButton disabledButton;
    juce::TextButton toggledButton;
    juce::TextButton menuPreviewButton;
    juce::Slider horizontalSlider;
    juce::Slider disabledSlider;
    juce::Slider rotaryKnob;
    juce::Slider verticalSlider;
    juce::ComboBox combo;
    juce::ComboBox disabledCombo;
    juce::ToggleButton checkToggle;
    juce::ToggleButton modeToggle;
    juce::TextEditor textEditor;
    juce::Label infoLabel;
    juce::TabbedComponent tabbed { juce::TabbedButtonBar::TabsAtTop };
    juce::Viewport viewport;
    ScrollContent viewportContent;
};

class ThemeEditorPanel::PreviewWindow final : public juce::DocumentWindow,
                                              private ThemeManager::Listener
{
public:
    explicit PreviewWindow(std::function<void()> onCloseIn)
        : juce::DocumentWindow("SETLE Live Preview",
                               juce::Colours::black,
                               juce::DocumentWindow::closeButton,
                               true),
          onClose(std::move(onCloseIn))
    {
        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(640, 400, 1800, 1400);

        preview = new PreviewArea();
        setContentOwned(preview, true);
        preview->refreshFromTheme();

        ThemeManager::get().addListener(this);
    }

    ~PreviewWindow() override
    {
        ThemeManager::get().removeListener(this);
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        if (onClose != nullptr)
            onClose();
    }

private:
    void themeChanged() override
    {
        if (preview != nullptr)
            preview->refreshFromTheme();
    }

    PreviewArea* preview { nullptr };
    std::function<void()> onClose;
};

class ThemeColourWheelPopup final : public juce::Component,
                                    private juce::ChangeListener
{
public:
    ThemeColourWheelPopup(juce::Colour initial,
                          std::function<void(juce::Colour)> onColourChangedIn)
        : selector(juce::ColourSelector::showColourAtTop
                   | juce::ColourSelector::editableColour
                   | juce::ColourSelector::showSliders
                   | juce::ColourSelector::showColourspace),
          onColourChanged(std::move(onColourChangedIn))
    {
        addAndMakeVisible(selector);
        selector.addChangeListener(this);
        selector.setCurrentColour(initial, juce::dontSendNotification);
        selector.setColour(juce::ColourSelector::backgroundColourId, ThemeManager::get().theme().surface1);
        selector.setColour(juce::ColourSelector::labelTextColourId, ThemeManager::get().theme().inkMid);

        hexEditor.setText(toHexRgb(initial), juce::dontSendNotification);
        hexEditor.setInputRestrictions(7, "#0123456789abcdefABCDEF");
        hexEditor.onReturnKey = [this] { applyHexEditor(); };
        hexEditor.onFocusLost = [this] { applyHexEditor(); };
        addAndMakeVisible(hexEditor);

        setSize(320, 370);
    }

    ~ThemeColourWheelPopup() override
    {
        selector.removeChangeListener(this);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        selector.setBounds(area.removeFromTop(getHeight() - 54));
        area.removeFromTop(4);
        hexEditor.setBounds(area.removeFromTop(24));
    }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        const auto colour = selector.getCurrentColour();
        hexEditor.setText(toHexRgb(colour), juce::dontSendNotification);
        if (onColourChanged != nullptr)
            onColourChanged(colour);
    }

    void applyHexEditor()
    {
        if (auto parsed = tryParseRgbHex(hexEditor.getText()))
        {
            selector.setCurrentColour(*parsed, juce::dontSendNotification);
            if (onColourChanged != nullptr)
                onColourChanged(*parsed);
            return;
        }

        hexEditor.setText(toHexRgb(selector.getCurrentColour()), juce::dontSendNotification);
    }

    juce::ColourSelector selector;
    juce::TextEditor hexEditor;
    std::function<void(juce::Colour)> onColourChanged;
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
    previewWindowButton.onClick = [this]
    {
        showPreviewWindow(previewWindow == nullptr);
    };
    resetThemeButton.onClick = [this] { resetThemeToDefault(); };

    addAndMakeVisible(saveAsButton);
    addAndMakeVisible(duplicateButton);
    addAndMakeVisible(revertButton);
    addAndMakeVisible(exportButton);
    addAndMakeVisible(importButton);
    addAndMakeVisible(previewWindowButton);
    addAndMakeVisible(resetThemeButton);

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    previewArea = std::make_unique<PreviewArea>();
    addAndMakeVisible(*previewArea);

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

    const int material = addSection("Material Toggles");
    addBoolControl(material, "isPebbled", &ThemeData::isPebbled);
    addFloatControl(material, "materialChassisTexture", &ThemeData::materialChassisTexture, 0.0f, 1.0f, 0.01f);
    addFloatControl(material, "materialGlassDepth", &ThemeData::materialGlassDepth, 0.0f, 1.0f, 0.01f);
    addFloatControl(material, "materialInsetFuzz", &ThemeData::materialInsetFuzz, 0.0f, 1.0f, 0.01f);
    addFloatControl(material, "materialGlowAmount", &ThemeData::materialGlowAmount, 0.0f, 1.0f, 0.01f);

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
    showPreviewWindow(false);
    clearPreviewTargetToken();
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
    const auto canonicalName = colourTokenName(member);
    control->member = member;
    control->name = canonicalName.isNotEmpty() ? canonicalName : name;
    control->sectionIndex = sectionIndex;
    control->label.setText(control->name, juce::dontSendNotification);
    control->label.setJustificationType(juce::Justification::centredLeft);

    control->hexEditor.setInputRestrictions(7, "#0123456789abcdefABCDEF");
    control->hexEditor.setJustification(juce::Justification::centredLeft);
    control->hexEditor.setIndents(2, 2);

    auto* c = control.get();

    auto applyHex = [this, c]
    {
        if (c == nullptr)
            return;

        setPreviewTargetToken(c->name);
        if (auto parsed = parseHexColour(c->hexEditor.getText()))
            ThemeManager::get().setColour(c->member, *parsed);
        else
            refreshControls();
    };

    control->swatchButton.onClick = [this, c]
    {
        if (c == nullptr)
            return;

        setPreviewTargetToken(c->name);

        auto popup = std::make_unique<ThemeColourWheelPopup>(ThemeManager::get().theme().*(c->member),
                                                             [c](juce::Colour colour)
                                                             {
                                                                 if (c != nullptr)
                                                                     ThemeManager::get().setColour(c->member, colour);
                                                             });
        juce::CallOutBox::launchAsynchronously(std::move(popup),
                                               c->swatchButton.getScreenBounds(),
                                               nullptr);
    };

    control->hexEditor.onReturnKey = applyHex;
    control->hexEditor.onFocusLost = applyHex;
    control->hexEditor.onTextChange = [this, c]
    {
        if (c != nullptr)
            setPreviewTargetToken(c->name);
    };

    control->copyButton.onClick = [this, c]
    {
        if (c != nullptr)
        {
            setPreviewTargetToken(c->name);
            juce::SystemClipboard::copyTextToClipboard(c->hexEditor.getText());
        }
    };

    control->pasteButton.onClick = [this, c]
    {
        if (c == nullptr)
            return;

        setPreviewTargetToken(c->name);
        c->hexEditor.setText(juce::SystemClipboard::getTextFromClipboard(), juce::dontSendNotification);
        if (auto parsed = parseHexColour(c->hexEditor.getText()))
            ThemeManager::get().setColour(c->member, *parsed);
        else
            refreshControls();
    };

    control->resetButton.onClick = [this, c]
    {
        if (c != nullptr)
        {
            setPreviewTargetToken(c->name);
            ThemeManager::get().setColour(c->member, baselineTheme.*(c->member));
        }
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
    const auto canonicalName = floatTokenName(member);
    control->member = member;
    control->name = canonicalName.isNotEmpty() ? canonicalName : name;
    control->sectionIndex = sectionIndex;
    control->label.setText(control->name, juce::dontSendNotification);
    control->label.setJustificationType(juce::Justification::centredLeft);

    control->slider.setSliderStyle(juce::Slider::LinearHorizontal);
    control->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    control->slider.setRange(minValue, maxValue, step);

    auto* c = control.get();
    control->slider.onValueChange = [this, c]
    {
        if (suppressCallbacks || c == nullptr)
            return;

        setPreviewTargetToken(c->name);
        ThemeManager::get().setFloat(c->member, static_cast<float>(c->slider.getValue()));
    };
    control->slider.onDragStart = [this, c]
    {
        if (c != nullptr)
            setPreviewTargetToken(c->name);
    };

    control->resetButton.onClick = [this, c]
    {
        if (c != nullptr)
        {
            setPreviewTargetToken(c->name);
            ThemeManager::get().setFloat(c->member, baselineTheme.*(c->member));
        }
    };

    content.addAndMakeVisible(control->label);
    content.addAndMakeVisible(control->slider);
    content.addAndMakeVisible(control->resetButton);

    sections[static_cast<size_t>(sectionIndex)]->floatIndices.push_back(floatControls.size());
    floatControls.push_back(std::move(control));
}

void ThemeEditorPanel::addBoolControl(int sectionIndex,
                                      const juce::String& name,
                                      bool ThemeData::* member)
{
    auto control = std::make_unique<BoolControl>();
    const auto canonicalName = boolTokenName(member);
    control->member = member;
    control->name = canonicalName.isNotEmpty() ? canonicalName : name;
    control->sectionIndex = sectionIndex;
    control->label.setText(control->name, juce::dontSendNotification);
    control->label.setJustificationType(juce::Justification::centredLeft);
    control->toggle.setButtonText("Enabled");

    auto* c = control.get();
    control->toggle.onClick = [this, c]
    {
        if (suppressCallbacks || c == nullptr)
            return;

        setPreviewTargetToken(c->name);
        setThemeBoolMember(c->member, c->toggle.getToggleState());
    };

    control->resetButton.onClick = [this, c]
    {
        if (c == nullptr)
            return;

        setPreviewTargetToken(c->name);
        setThemeBoolMember(c->member, baselineTheme.*(c->member));
    };

    content.addAndMakeVisible(control->label);
    content.addAndMakeVisible(control->toggle);
    content.addAndMakeVisible(control->resetButton);

    sections[static_cast<size_t>(sectionIndex)]->boolIndices.push_back(boolControls.size());
    boolControls.push_back(std::move(control));
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

    for (const auto index : section.boolIndices)
    {
        if (index < boolControls.size())
        {
            auto& control = *boolControls[index];
            setThemeBoolMember(control.member, baselineTheme.*(control.member));
        }
    }
}

void ThemeEditorPanel::resetThemeToDefault()
{
    auto defaultTheme = ThemeManager::loadDefaultTheme();
    ThemeManager::get().applyTheme(defaultTheme);
    setle::state::AppPreferences::get().setActiveThemeName(defaultTheme.presetName);
    presets.setText(defaultTheme.presetName, juce::dontSendNotification);
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
    return toHexRgb(colour);
}

std::optional<juce::Colour> ThemeEditorPanel::parseHexColour(const juce::String& text)
{
    return tryParseRgbHex(text);
}

void ThemeEditorPanel::showPreviewWindow(bool shouldShow)
{
    if (!shouldShow)
    {
        if (previewWindow != nullptr)
        {
            previewWindow->setVisible(false);
            previewWindow.reset();
        }

        previewWindowButton.setButtonText("Live Preview");
        return;
    }

    if (previewWindow == nullptr)
    {
        previewWindow = std::make_unique<PreviewWindow>([this]
        {
            previewWindow.reset();
            previewWindowButton.setButtonText("Live Preview");
        });
    }

    previewWindowButton.setButtonText("Close Preview");
    auto target = getScreenBounds().translated(getWidth() + 8, 0);
    target.setSize(juce::jmax(720, getWidth()), juce::jmax(460, getHeight() - 40));
    previewWindow->setBounds(target);
    previewWindow->setVisible(true);
    previewWindow->toFront(true);
}

void ThemeEditorPanel::setPreviewTargetToken(const juce::String& token)
{
    ThemeManager::get().setPreviewHighlightToken(token);
}

void ThemeEditorPanel::clearPreviewTargetToken()
{
    ThemeManager::get().setPreviewHighlightToken({});
}

void ThemeEditorPanel::mouseExit(const juce::MouseEvent&)
{
    clearPreviewTargetToken();
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
    previewWindowButton.setBounds(row2.removeFromLeft(92));
    row2.removeFromLeft(4);
    resetThemeButton.setBounds(row2.removeFromLeft(60));

    area.removeFromTop(6);
    const int previewHeight = juce::jlimit(170, 280, area.getHeight() / 3);
    previewArea->setBounds(area.removeFromTop(previewHeight));
    area.removeFromTop(6);
    viewport.setBounds(area);

    int contentWidth = juce::jmax(420, viewport.getWidth() - 10);
    if (contentWidth <= 0)
        contentWidth = 420;

    int y = 8;

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

        for (const auto idx : section.boolIndices)
        {
            if (idx >= boolControls.size())
                continue;

            auto& c = *boolControls[idx];
            auto row = juce::Rectangle<int>(10, y, contentWidth - 20, 24);
            c.label.setBounds(row.removeFromLeft(130));
            c.resetButton.setBounds(row.removeFromRight(50));
            row.removeFromRight(6);
            c.toggle.setBounds(row.removeFromRight(92));
            y += 26;
        }

        y += 8;
    }

    content.setSize(contentWidth, y + 8);
}

void ThemeEditorPanel::mouseMove(const juce::MouseEvent& event)
{
    const auto screenPos = event.getScreenPosition();

    auto containsScreen = [&screenPos](const juce::Component& c)
    {
        return c.getScreenBounds().contains(screenPos);
    };

    juce::String hoveredToken;

    for (const auto& control : colourControls)
    {
        if (containsScreen(control->label)
            || containsScreen(control->swatchButton)
            || containsScreen(control->hexEditor)
            || containsScreen(control->copyButton)
            || containsScreen(control->pasteButton)
            || containsScreen(control->resetButton))
        {
            hoveredToken = control->name;
            break;
        }
    }

    if (hoveredToken.isEmpty())
    {
        for (const auto& control : floatControls)
        {
            if (containsScreen(control->label)
                || containsScreen(control->slider)
                || containsScreen(control->resetButton))
            {
                hoveredToken = control->name;
                break;
            }
        }
    }

    if (hoveredToken.isEmpty())
    {
        for (const auto& control : boolControls)
        {
            if (containsScreen(control->label)
                || containsScreen(control->toggle)
                || containsScreen(control->resetButton))
            {
                hoveredToken = control->name;
                break;
            }
        }
    }

    if (hoveredToken.isEmpty())
    {
        for (const auto& section : sections)
        {
            if (containsScreen(section->label) || containsScreen(section->resetButton))
            {
                if (!section->colourIndices.empty() && section->colourIndices.front() < colourControls.size())
                    hoveredToken = colourControls[section->colourIndices.front()]->name;
                else if (!section->floatIndices.empty() && section->floatIndices.front() < floatControls.size())
                    hoveredToken = floatControls[section->floatIndices.front()]->name;
                else if (!section->boolIndices.empty() && section->boolIndices.front() < boolControls.size())
                    hoveredToken = boolControls[section->boolIndices.front()]->name;
                break;
            }
        }
    }

    if (hoveredToken.isEmpty() && previewArea != nullptr && containsScreen(*previewArea))
        hoveredToken = "controlBg";

    if (hoveredToken.isNotEmpty())
        setPreviewTargetToken(hoveredToken);
    else
        clearPreviewTargetToken();
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

    for (auto& control : boolControls)
    {
        const auto value = t.*(control->member);
        control->toggle.setToggleState(value, juce::dontSendNotification);

        const bool changed = value != baselineTheme.*(control->member);
        control->label.setText(control->name + (changed ? " *" : ""), juce::dontSendNotification);
        control->label.setColour(juce::Label::textColourId, changed ? t.accentWarm : t.inkMid);
        control->toggle.setColour(juce::ToggleButton::tickColourId, t.accent.withAlpha(value ? 0.95f : 0.7f));
        control->toggle.setColour(juce::ToggleButton::textColourId, value ? t.inkLight : t.inkMuted);
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

        if (!sectionChanged)
        {
            for (const auto idx : section->boolIndices)
            {
                if (idx < boolControls.size())
                {
                    auto& c = *boolControls[idx];
                    if ((t.*(c.member)) != (baselineTheme.*(c.member)))
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
    previewWindowButton.setButtonText(previewWindow != nullptr ? "Close Preview" : "Live Preview");

    suppressCallbacks = false;
    repaint();
}

void ThemeEditorPanel::themeChanged()
{
    refreshControls();
}
