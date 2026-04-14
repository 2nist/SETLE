# Phase 2 Context Control Matrix

This document defines the section-level control ownership model used by the WORK zone header.

## Why this exists

Phase 1 introduced structure (menu bar, left nav, context tabs).
Phase 2 adds contextual quick controls that show both:

- actions that are live now and command-backed
- actions planned for future development so users can see product direction in-place

## Current behavior

- Zone header reads controls from `getContextControls(NavSection)` in `source/ui/NavSection.h`.
- Live controls trigger command dispatch through `ApplicationCommandManager`.
- Planned controls are visible but disabled.
- Overflow menu includes a "Planned" section for roadmap visibility.

## Ownership matrix

| Section | Control | Status | Command / Owner | Future context |
| --- | --- | --- | --- | --- |
| Create | Capture Now | Live | `AppCommandIDs::navCreate` -> WorkspaceShell nav routing | Future one-click source-aware capture scenes |
| Create | Auto-Grab | Planned | Not yet command-backed | Route to dedicated capture command |
| Edit | Undo | Live | `AppCommandIDs::editUndo` -> theory snapshot undo | Keep scoped undo history by object selection |
| Edit | Redo | Live | `AppCommandIDs::editRedo` -> theory snapshot redo | Add selective redo branches |
| Edit | Humanize | Planned | Not yet command-backed | Expressive timing and velocity macro |
| Arrange | Play | Live | `AppCommandIDs::transportPlay` -> transport | Add pre-roll and loop region playback |
| Arrange | Stop | Live | `AppCommandIDs::transportStop` -> transport | Add stop-to-start marker mode |
| Arrange | Smart Duplicate | Planned | Not yet command-backed | Duplicate with harmonic and form constraints |
| Sound | Open FX | Live | `AppCommandIDs::workTabFx` -> WORK tab switch | Auto-select active slot on open |
| Sound | Add MIDI | Live | `AppCommandIDs::insertMidiTrack` -> TrackManager | Prompt for instrument template |
| Sound | Smart Sidechain | Planned | Not yet command-backed | Suggested ducking/bus routes |
| Mix | Focus OUT | Live | `AppCommandIDs::viewFocusOut` -> focus mode | Add monitor perspectives |
| Mix | Add Audio | Live | `AppCommandIDs::insertAudioTrack` -> TrackManager | Optional source routing wizard |
| Mix | Auto Gain | Planned | Not yet command-backed | Loudness-aware gain staging assistant |
| Library | Open Library | Live | `AppCommandIDs::navLibrary` -> section navigation | Auto-open relevant browser pane |
| Library | Theme | Live | `AppCommandIDs::viewThemeEditor` -> theme editor | Section-specific visual presets |
| Library | Recommend | Planned | Not yet command-backed | Context-aware progression/pattern recommendations |
| Session | Balanced | Live | `AppCommandIDs::viewFocusBalanced` -> focus mode | Remember per-song focus presets |
| Session | Theme | Live | `AppCommandIDs::viewThemeEditor` -> theme editor | Session-level style packs |
| Session | Session Preset | Planned | Not yet command-backed | Full session template save/load |

## Next implementation targets (Phase 3)

1. Promote planned controls to command IDs in `source/ui/AppCommands.h`.
2. Add command registration and dispatch in `WorkspaceShellComponent`.
3. Implement feature handlers in the relevant subsystem:
- capture: auto-grab command
- edit: humanize command
- arrange: smart duplicate command
- sound/mix: sidechain and auto-gain assistants
- library: recommendation command
- session: session preset command
4. Add command enablement rules per context selection and transport state.
