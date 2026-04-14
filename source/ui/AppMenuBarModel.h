#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "AppCommands.h"
#include "NavSection.h"

namespace setle::ui
{

// -----------------------------------------------------------------------
// AppMenuBarModel — JUCE MenuBarModel used by the compact classic menu bar.
//
// Uses juce::ApplicationCommandManager to populate PopupMenu items so that
// shortcut keys are shown automatically in the menus.
//
// Dispatch happens via the ApplicationCommandManager's registered target
// (WorkspaceShellComponent), not via callbacks here — keeping this class
// framework-side only.
// -----------------------------------------------------------------------
class AppMenuBarModel final : public juce::MenuBarModel
{
public:
    explicit AppMenuBarModel(juce::ApplicationCommandManager& commandManagerRef);
    ~AppMenuBarModel() override;

    // juce::MenuBarModel overrides
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu   getMenuForIndex(int topLevelMenuIndex,
                                      const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

private:
    juce::PopupMenu buildFileMenu();
    juce::PopupMenu buildEditMenu();
    juce::PopupMenu buildViewMenu();
    juce::PopupMenu buildTransportMenu();
    juce::PopupMenu buildInsertMenu();
    juce::PopupMenu buildToolsMenu();
    juce::PopupMenu buildHelpMenu();

    juce::ApplicationCommandManager& manager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppMenuBarModel)
};

} // namespace setle::ui
