#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace setle::ui
{

// -----------------------------------------------------------------------
// AppCommandIDs — canonical command IDs for the entire application.
//
// Ranges:
//   0x8001 – 0x800F   File commands
//   0x8010 – 0x801F   Edit commands
//   0x8020 – 0x802F   Transport commands
//   0x8030 – 0x803F   Edit-tool commands
//   0x8040 – 0x804F   View / focus commands
//   0x8050 – 0x805F   Nav section commands
//   0x8060 – 0x806F   Work-tab commands
//   0x8070 – 0x807F   Insert commands
//   0x8090 – 0x809F   Help commands
//
// Command IDs below 0x8000 are reserved by JUCE for standard commands.
// Theory action IDs (100-513) live in the anonymous TheoryActionId enum
// inside WorkspaceShellComponent.cpp and are not migrated here yet.
// -----------------------------------------------------------------------
namespace AppCommandIDs
{
    enum
    {
        // File
        fileNew          = 0x8001,
        fileSave         = 0x8003,
        fileExportMidi   = 0x8005,
        fileExit         = 0x8009,

        // Edit
        editUndo         = 0x8010,
        editRedo         = 0x8011,

        // Transport
        transportPlay    = 0x8020,
        transportStop    = 0x8021,
        transportRecord  = 0x8022,

        // Edit tools
        toolSelect       = 0x8030,
        toolDraw         = 0x8031,
        toolErase        = 0x8032,
        toolSplit        = 0x8033,
        toolStretch      = 0x8034,
        toolListen       = 0x8035,
        toolMarquee      = 0x8036,
        toolScaleSnap    = 0x8037,

        // View / focus
        viewFocusIn      = 0x8040,
        viewFocusBalanced= 0x8041,
        viewFocusOut     = 0x8042,
        viewThemeEditor  = 0x8043,

        // Nav sections
        navCreate        = 0x8050,
        navEdit          = 0x8051,
        navArrange       = 0x8052,
        navSound         = 0x8053,
        navMix           = 0x8054,
        navLibrary       = 0x8055,
        navSession       = 0x8056,

        // Work tabs
        workTabTheory    = 0x8060,
        workTabGridRoll  = 0x8061,
        workTabFx        = 0x8062,

        // Insert
        insertMidiTrack  = 0x8070,
        insertAudioTrack = 0x8071,

        // Help
        helpAbout        = 0x8090,
        helpThemeEditor  = 0x8091,
    };
} // namespace AppCommandIDs

// -----------------------------------------------------------------------
// AppCommandManager — singleton wrapper around juce::ApplicationCommandManager.
//
// Components that need to dispatch commands call:
//     AppCommandManager::get().invoke(AppCommandIDs::transportPlay);
//
// WorkspaceShellComponent registers as the ApplicationCommandTarget and
// implements getAllCommands / getCommandInfo / perform.
// -----------------------------------------------------------------------
class AppCommandManager
{
public:
    static AppCommandManager& get()
    {
        static AppCommandManager instance;
        return instance;
    }

    juce::ApplicationCommandManager& manager() noexcept { return commandManager; }

    void invoke(int commandId)
    {
        commandManager.invokeDirectly(commandId, false);
    }

private:
    juce::ApplicationCommandManager commandManager;
};

} // namespace setle::ui
