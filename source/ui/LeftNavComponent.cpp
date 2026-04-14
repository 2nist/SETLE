#include "LeftNavComponent.h"

#include "../theme/ThemeStyleHelpers.h"

namespace setle::ui
{

LeftNavComponent::LeftNavComponent()
{
    for (int i = 0; i < kNavSectionCount; ++i)
    {
        const auto section = static_cast<NavSection>(i);
        const auto& info   = getNavSectionInfo(section);

        auto* btn = sectionButtons.add(new juce::TextButton(info.label));
        btn->setTooltip(info.tooltip);
        btn->onClick = [this, section]
        {
            if (activeSection == section)
                return;

            activeSection = section;
            updateButtonAppearance();

            if (onSectionChanged)
                onSectionChanged(section);
        };
        btn->setColour(juce::TextButton::buttonColourId,   juce::Colours::transparentBlack);
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(*btn);
    }

    ThemeManager::get().addListener(this);
    updateButtonAppearance();
}

LeftNavComponent::~LeftNavComponent()
{
    ThemeManager::get().removeListener(this);
}

void LeftNavComponent::setActiveSection(NavSection section)
{
    if (activeSection == section)
        return;

    activeSection = section;
    updateButtonAppearance();
    repaint();
}

void LeftNavComponent::resized()
{
    if (sectionButtons.isEmpty())
        return;

    const int total = getWidth();
    const int n     = sectionButtons.size();
    const int w     = total / n;
    int x = 0;

    for (int i = 0; i < n; ++i)
    {
        const int right = (i == n - 1) ? total : (x + w);
        sectionButtons[i]->setBounds(x, 0, right - x, getHeight());
        x = right;
    }
}

void LeftNavComponent::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();

    // Background — slightly distinct from the top strip
    g.fillAll(setle::theme::panelHeaderBackground(theme, setle::theme::ZoneRole::workPanel)
                  .darker(0.08f));

    // Bottom separator line
    g.setColour(setle::theme::timelineGridLine(theme, false, false).withAlpha(0.5f));
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    // Active section accent indicator — 2px line at the bottom of the button
    const int activeIdx = static_cast<int>(activeSection);
    if (activeIdx >= 0 && activeIdx < sectionButtons.size())
    {
        const auto btnBounds = sectionButtons[activeIdx]->getBounds();
        g.setColour(theme.accent);
        g.fillRect(btnBounds.getX(), getHeight() - 2, btnBounds.getWidth(), 2);
    }
}

void LeftNavComponent::themeChanged()
{
    updateButtonAppearance();
    repaint();
}

void LeftNavComponent::updateButtonAppearance()
{
    const auto& theme = ThemeManager::get().theme();

    for (int i = 0; i < sectionButtons.size(); ++i)
    {
        const bool isActive = (static_cast<NavSection>(i) == activeSection);
        auto* btn = sectionButtons[i];

        btn->setColour(juce::TextButton::textColourOffId,
                       isActive ? theme.inkLight
                                : theme.inkMuted.withAlpha(0.72f));
        btn->setColour(juce::TextButton::textColourOnId,  theme.inkLight);
        btn->setColour(juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        btn->setColour(juce::TextButton::buttonOnColourId,juce::Colours::transparentBlack);
    }
}

} // namespace setle::ui
