#include "ZoneHeaderComponent.h"

#include "../theme/ThemeStyleHelpers.h"

namespace setle::ui
{

ZoneHeaderComponent::ZoneHeaderComponent()
{
    // Zone label
    zoneLabel.setFont(juce::FontOptions(13.5f).withStyle("Bold"));
    zoneLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(zoneLabel);

    // Overflow/hamburger button
    overflowButton.setTooltip("Zone options");
    overflowButton.setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
    overflowButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    overflowButton.onClick = [this]
    {
        if (onOverflowRequested)
            onOverflowRequested(overflowButton);
    };
    addAndMakeVisible(overflowButton);

    ThemeManager::get().addListener(this);

    // Build initial tab set for the default section (edit)
    rebuildTabs();
    rebuildControls();
    updateTabAppearance();
    updateControlAppearance();
}

ZoneHeaderComponent::~ZoneHeaderComponent()
{
    ThemeManager::get().removeListener(this);
}

void ZoneHeaderComponent::setSection(NavSection section)
{
    if (activeSection == section)
        return;

    activeSection = section;
    rebuildTabs();
    rebuildControls();
    updateTabAppearance();
    updateControlAppearance();
    resized();
    repaint();
}

void ZoneHeaderComponent::setActiveTab(int tabIndex)
{
    if (activeTabIndex == tabIndex)
        return;

    activeTabIndex = tabIndex;
    updateTabAppearance();
    repaint();
}

void ZoneHeaderComponent::resized()
{
    auto area = getLocalBounds().reduced(4, 4);

    // Overflow button on the far right (square)
    const int overflowW = 24;
    overflowButton.setBounds(area.removeFromRight(overflowW));

    // Context controls sit between tabs and overflow.
    const int controlGap = 4;
    const int controlW = 92;
    juce::Rectangle<int> controlsArea;
    if (!controls.empty())
    {
        const int totalControlsW = static_cast<int>(controls.size()) * controlW
                                   + static_cast<int>(controls.size() - 1) * controlGap;
        controlsArea = area.removeFromRight(totalControlsW);
        area.removeFromRight(6);
    }

    // Zone label on the left — fixed 90px so tabs can use remaining space
    const int labelW = 90;
    zoneLabel.setBounds(area.removeFromLeft(labelW));

    area.removeFromLeft(6); // gap between label and tabs

    // Distribute tab buttons evenly in remaining space
    if (!tabs.empty())
    {
        const int n     = static_cast<int>(tabs.size());
        const int total = area.getWidth();
        const int w     = total / n;
        int x = area.getX();

        for (int i = 0; i < n; ++i)
        {
            const int right = (i == n - 1) ? (area.getX() + total) : (x + w);
            tabs[i].button->setBounds(x, area.getY(), right - x, area.getHeight());
            x = right;
        }
    }

    if (!controls.empty())
    {
        int x = controlsArea.getX();
        for (auto& c : controls)
        {
            c.button->setBounds(x, controlsArea.getY(), controlW, controlsArea.getHeight());
            x += controlW + controlGap;
        }
    }
}

void ZoneHeaderComponent::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();

    // Background
    g.fillAll(setle::theme::panelHeaderBackground(theme, setle::theme::ZoneRole::workPanel));

    // Bottom separator
    g.setColour(setle::theme::timelineGridLine(theme, false, false).withAlpha(0.6f));
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    // Active tab accent indicator — 2px underline on the active tab button
    for (const auto& tab : tabs)
    {
        if (tab.tabIndex == activeTabIndex)
        {
            g.setColour(theme.accent);
            const auto tb = tab.button->getBounds();
            g.fillRect(tb.getX(), getHeight() - 2, tb.getWidth(), 2);
            break;
        }
    }
}

void ZoneHeaderComponent::themeChanged()
{
    updateTabAppearance();
    updateControlAppearance();
    repaint();
}

void ZoneHeaderComponent::rebuildTabs()
{
    // Remove existing tab button children
    for (auto& t : tabs)
        removeChildComponent(t.button.get());
    tabs.clear();

    // Update zone label text
    const auto& info = getNavSectionInfo(activeSection);
    zoneLabel.setText(info.label, juce::dontSendNotification);

    // Build new tabs from section definition
    const auto contextTabs = getContextTabs(activeSection);
    for (const auto& ct : contextTabs)
    {
        TabEntry entry;
        entry.tabIndex = ct.tabIndex;
        entry.button   = std::make_unique<juce::TextButton>(ct.label);
        entry.button->setTooltip(ct.tooltip);
        entry.button->setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
        entry.button->setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);

        const int capturedIndex = ct.tabIndex;
        entry.button->onClick = [this, capturedIndex]
        {
            activeTabIndex = capturedIndex;
            updateTabAppearance();
            repaint();
            if (onTabSelected)
                onTabSelected(capturedIndex);
        };
        addAndMakeVisible(*entry.button);
        tabs.push_back(std::move(entry));
    }
}

void ZoneHeaderComponent::rebuildControls()
{
    for (auto& c : controls)
        removeChildComponent(c.button.get());
    controls.clear();

    const auto contextControls = getContextControls(activeSection);
    for (const auto& cc : contextControls)
    {
        ControlEntry entry;
        entry.id = cc.id;
        entry.commandId = cc.commandId;
        entry.availableNow = cc.availableNow;
        entry.plannedNote = cc.plannedNote;
        entry.button = std::make_unique<juce::TextButton>(cc.label);
        juce::String tooltipText(cc.tooltip);
        if (!cc.availableNow && juce::String(cc.plannedNote).isNotEmpty())
            tooltipText << "\nPlanned: " << cc.plannedNote;
        entry.button->setTooltip(tooltipText);
        entry.button->setEnabled(cc.availableNow);
        entry.button->setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        entry.button->setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);

        const auto capturedId = entry.id;
        const auto capturedCommandId = entry.commandId;
        const bool capturedAvailable = entry.availableNow;
        entry.button->onClick = [this, capturedId, capturedCommandId, capturedAvailable]
        {
            if (onContextControlTriggered)
                onContextControlTriggered(capturedId, capturedCommandId, capturedAvailable);
        };

        addAndMakeVisible(*entry.button);
        controls.push_back(std::move(entry));
    }
}

void ZoneHeaderComponent::updateTabAppearance()
{
    const auto& theme = ThemeManager::get().theme();

    zoneLabel.setColour(juce::Label::textColourId, theme.inkLight.withAlpha(0.88f));
    overflowButton.setColour(juce::TextButton::textColourOffId, theme.inkMuted.withAlpha(0.8f));

    for (const auto& tab : tabs)
    {
        const bool isActive = (tab.tabIndex == activeTabIndex);
        tab.button->setColour(juce::TextButton::textColourOffId,
                              isActive ? theme.inkLight
                                       : theme.inkMuted.withAlpha(0.65f));
    }
}

void ZoneHeaderComponent::updateControlAppearance()
{
    const auto& theme = ThemeManager::get().theme();

    for (const auto& c : controls)
    {
        const auto ink = c.availableNow ? theme.inkLight.withAlpha(0.82f)
                                        : theme.inkMuted.withAlpha(0.5f);

        c.button->setColour(juce::TextButton::textColourOffId, ink);
        c.button->setColour(juce::TextButton::buttonColourId,
                            c.availableNow ? theme.surface3.withAlpha(0.30f)
                                           : theme.surface2.withAlpha(0.18f));
    }
}

} // namespace setle::ui
