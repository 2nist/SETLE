#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "NavSection.h"
#include "../theme/ThemeManager.h"

namespace setle::ui
{

// -----------------------------------------------------------------------
// LeftNavComponent — compact horizontal nav strip placed directly below
// the classic menu bar and above the transport/top strip.
//
// Renders one button per NavSection.  The active section gets an accent
// underline; inactive sections use a quieter label colour.
// Width is not fixed — buttons share available width equally.
// -----------------------------------------------------------------------
class LeftNavComponent final : public juce::Component,
                               public ThemeManager::Listener
{
public:
    // Called when the user clicks a section button.
    std::function<void(NavSection)> onSectionChanged;

    LeftNavComponent();
    ~LeftNavComponent() override;

    // Call this from WorkspaceShellComponent when the section is changed
    // programmatically (e.g., from the menu bar) so the nav strip reflects it.
    void setActiveSection(NavSection section);

    NavSection getActiveSection() const noexcept { return activeSection; }

    // juce::Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;

    // ThemeManager::Listener
    void themeChanged() override;

    static constexpr int kPreferredHeight = 30;

private:
    void updateButtonAppearance();

    juce::OwnedArray<juce::TextButton> sectionButtons;
    NavSection activeSection { NavSection::edit };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LeftNavComponent)
};

} // namespace setle::ui
