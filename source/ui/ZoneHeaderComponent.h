#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "NavSection.h"
#include "../theme/ThemeManager.h"

namespace setle::ui
{

// -----------------------------------------------------------------------
// ZoneHeaderComponent — placed at the top of the WORK panel.
//
// Shows:
//   [Zone label]   [Tab 0] [Tab 1] [Tab 2] ...   [⋮ overflow]
//
// The zone label reflects the active NavSection.
// Tabs are dynamic and come from getContextTabs(activeSection).
// The overflow/hamburger button (⋮) opens a PopupMenu of additional
// zone-specific commands — initially shows focus and theme items.
// -----------------------------------------------------------------------
class ZoneHeaderComponent final : public juce::Component,
                                   public ThemeManager::Listener
{
public:
    // Called when a tab button is clicked.  tabIndex matches
    // WorkspaceShellComponent::workPanelTabIndex values (0=Theory, 1=GridRoll, 2=FX).
    std::function<void(int tabIndex)> onTabSelected;

    // Called when the overflow/hamburger button is clicked.
    // Caller is responsible for showing the appropriate context menu.
    std::function<void(juce::Component& anchor)> onOverflowRequested;

    // Called when a context control is clicked.
    // commandId may be 0 for planned (not-yet-implemented) controls.
    std::function<void(const juce::String& controlId, int commandId, bool availableNow)> onContextControlTriggered;

    ZoneHeaderComponent();
    ~ZoneHeaderComponent() override;

    // Update the displayed section + rebuild tabs accordingly.
    void setSection(NavSection section);

    // Highlight the given tab index as active.  Does not fire onTabSelected.
    void setActiveTab(int tabIndex);

    int  getActiveTab() const noexcept { return activeTabIndex; }

    // juce::Component overrides
    void resized() override;
    void paint(juce::Graphics& g) override;

    // ThemeManager::Listener
    void themeChanged() override;

    static constexpr int kPreferredHeight = 36;

private:
    void rebuildTabs();
    void rebuildControls();
    void updateTabAppearance();
    void updateControlAppearance();

    juce::Label     zoneLabel;
    juce::TextButton overflowButton { juce::CharPointer_UTF8("\xe2\x8b\xae") }; // ⋮

    // Each entry maps (UI button order → tab index in the shell)
    struct TabEntry
    {
        int                          tabIndex;
        std::unique_ptr<juce::TextButton> button;
    };
    std::vector<TabEntry> tabs;

    struct ControlEntry
    {
        juce::String                id;
        int                         commandId;
        bool                        availableNow;
        juce::String                plannedNote;
        std::unique_ptr<juce::TextButton> button;
    };
    std::vector<ControlEntry> controls;

    NavSection activeSection  { NavSection::edit };
    int        activeTabIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoneHeaderComponent)
};

} // namespace setle::ui
