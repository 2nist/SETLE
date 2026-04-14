#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppCommands.h"

namespace setle::ui
{

// -----------------------------------------------------------------------
// NavSection — primary workflow areas shown in the left nav strip
// -----------------------------------------------------------------------
enum class NavSection
{
    create  = 0,  // Capture, history grab, queue
    edit    = 1,  // Theory editor, GridRoll, chord/note tools
    arrange = 2,  // Timeline, sections, progressions
    sound   = 3,  // Instruments, FX chains
    mix     = 4,  // Channel strips, sends, master
    library = 5,  // Progression templates, drum patterns
    session = 6,  // Key, mode, BPM, global preferences
    count_  = 7
};

static constexpr int kNavSectionCount = static_cast<int>(NavSection::count_);

struct NavSectionInfo
{
    NavSection  section;
    const char* id;          // stable string key for persistence
    const char* label;       // full display label
    const char* shortLabel;  // abbreviated for narrow strip widths
    const char* tooltip;
};

inline const NavSectionInfo& getNavSectionInfo(NavSection s)
{
    static constexpr NavSectionInfo table[] = {
        { NavSection::create,  "create",  "Create",  "CAP",  "Capture: MIDI history, grab queue, input" },
        { NavSection::edit,    "edit",    "Edit",    "EDT",  "Edit: Theory editor, GridRoll, chord and note tools" },
        { NavSection::arrange, "arrange", "Arrange", "ARR",  "Arrange: Timeline, sections, progressions" },
        { NavSection::sound,   "sound",   "Sound",   "SND",  "Sound: Instruments, FX chains per track" },
        { NavSection::mix,     "mix",     "Mix",     "MIX",  "Mix: Channel strips, sends, master output" },
        { NavSection::library, "library", "Library", "LIB",  "Library: Progression templates, drum patterns" },
        { NavSection::session, "session", "Session", "SET",  "Session: Key, mode, BPM, global preferences" },
    };

    const auto index = static_cast<int>(s);
    jassert(index >= 0 && index < kNavSectionCount);
    return table[index];
}

inline NavSection navSectionFromString(const juce::String& id)
{
    for (int i = 0; i < kNavSectionCount; ++i)
    {
        const auto s = static_cast<NavSection>(i);
        if (juce::String(getNavSectionInfo(s).id) == id)
            return s;
    }
    return NavSection::edit; // safe default
}

// -----------------------------------------------------------------------
// ContextTab — per-section tab definitions
// -----------------------------------------------------------------------
struct ContextTabInfo
{
    int         tabIndex;   // maps to WorkspaceShellComponent::workPanelTabIndex
    const char* label;
    const char* tooltip;
    NavSection  ownerSection;
};

// -----------------------------------------------------------------------
// ContextControl — section-aware quick actions shown in ZoneHeader.
//
// "availableNow" controls are clickable and should map to an existing
// command ID when possible. "planned" controls are visible placeholders
// that provide product context for future phases.
// -----------------------------------------------------------------------
struct ContextControlInfo
{
    const char* id;
    const char* label;
    const char* tooltip;
    bool        availableNow;
    int         commandId;      // AppCommandIDs value, 0 when not command-backed yet
    const char* plannedNote;    // optional short roadmap note
    NavSection  ownerSection;
};

// Returns the set of context tabs for a given section.
// Tab indices 0/1/2 are the existing Theory/GridRoll/FX tabs in the WORK panel.
inline juce::Array<ContextTabInfo> getContextTabs(NavSection section)
{
    juce::Array<ContextTabInfo> result;
    switch (section)
    {
        case NavSection::edit:
            result.add({ 0, "Theory",   "Theory editor and chord palette",        NavSection::edit });
            result.add({ 1, "GridRoll", "Piano roll and drum sequencer",           NavSection::edit });
            break;
        case NavSection::sound:
            result.add({ 2, "FX",       "FX chain builder per track",              NavSection::sound });
            break;
        case NavSection::library:
            result.add({ 0, "Theory",   "View and apply progression templates",    NavSection::library });
            break;
        default:
            break;  // create, arrange, mix, session: no sub-tabs in first pass
    }
    return result;
}

inline juce::Array<ContextControlInfo> getContextControls(NavSection section)
{
    namespace ID = AppCommandIDs;

    juce::Array<ContextControlInfo> result;
    switch (section)
    {
        case NavSection::create:
            result.add({ "capture-now", "Capture Now", "Open capture workflow and grab recent input", true, ID::navCreate,
                         "Future: one-click source-aware capture scenes", NavSection::create });
            result.add({ "auto-grab", "Auto-Grab", "Toggle automatic post-stop grab behavior", false, 0,
                         "Planned: routed to dedicated capture command", NavSection::create });
            break;

        case NavSection::edit:
            result.add({ "undo", "Undo", "Undo last theory edit", true, ID::editUndo,
                         "", NavSection::edit });
            result.add({ "redo", "Redo", "Redo last undone theory edit", true, ID::editRedo,
                         "", NavSection::edit });
            result.add({ "humanize", "Humanize", "Apply timing/velocity humanization to selected material", false, 0,
                         "Planned: expressive timing macro", NavSection::edit });
            break;

        case NavSection::arrange:
            result.add({ "play", "Play", "Start transport playback", true, ID::transportPlay,
                         "", NavSection::arrange });
            result.add({ "stop", "Stop", "Stop transport playback", true, ID::transportStop,
                         "", NavSection::arrange });
            result.add({ "smart-duplicate", "Smart Duplicate", "Duplicate section while preserving harmonic constraints", false, 0,
                         "Planned: arrangement-aware duplicate", NavSection::arrange });
            break;

        case NavSection::sound:
            result.add({ "fx-tab", "Open FX", "Open FX chain builder for active instrument track", true, ID::workTabFx,
                         "", NavSection::sound });
            result.add({ "add-midi", "Add MIDI", "Add a new MIDI track", true, ID::insertMidiTrack,
                         "", NavSection::sound });
            result.add({ "smart-sidechain", "Smart Sidechain", "Auto-route ducking/sidechain suggestions", false, 0,
                         "Planned: quick mix glue tools", NavSection::sound });
            break;

        case NavSection::mix:
            result.add({ "focus-out", "Focus OUT", "Emphasize instrument and output side of the workspace", true, ID::viewFocusOut,
                         "", NavSection::mix });
            result.add({ "add-audio", "Add Audio", "Add a new audio track", true, ID::insertAudioTrack,
                         "", NavSection::mix });
            result.add({ "auto-gain", "Auto Gain", "Analyze and normalize perceived loudness per track", false, 0,
                         "Planned: gain staging assistant", NavSection::mix });
            break;

        case NavSection::library:
            result.add({ "open-library", "Open Library", "Jump to progression and pattern library workflows", true, ID::navLibrary,
                         "", NavSection::library });
            result.add({ "theme", "Theme", "Open theme editor", true, ID::viewThemeEditor,
                         "", NavSection::library });
            result.add({ "recommend", "Recommend", "Suggest compatible progressions/patterns for current section", false, 0,
                         "Planned: context-aware recommendation engine", NavSection::library });
            break;

        case NavSection::session:
            result.add({ "focus-balanced", "Balanced", "Return workspace to balanced layout", true, ID::viewFocusBalanced,
                         "", NavSection::session });
            result.add({ "theme-editor", "Theme", "Open theme editor", true, ID::viewThemeEditor,
                         "", NavSection::session });
            result.add({ "session-template", "Session Preset", "Export and recall complete session setups", false, 0,
                         "Planned: session template save/load", NavSection::session });
            break;

        default:
            break;
    }

    return result;
}

} // namespace setle::ui
