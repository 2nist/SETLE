# UI Shell Implementation (Phase 2 - Steps 2-6)

Initial JUCE shell implementation is now in place with these behaviors:

## Layout

- Top control strip (title + focus-state controls)
- Main body split as `IN -> WORK -> OUT`
- Full-width bottom timeline region

## Timeline Region

The bottom timeline region is always full-width and contains:

- Section lane
- Theory strip
- Main track area
- History buffer lane

## Resizing

- Vertical drag bars between `IN|WORK` and `WORK|OUT`
- Horizontal drag bar between upper workspace and timeline region
- Width/height limits enforce minimum center work area and timeline visibility

## Focus States

Three built-in panel states:

- `Focus IN`
- `Focus Balanced`
- `Focus OUT`

Each state updates side panel proportions for quick context switching.

## Theory Context Menus (Step 3 Scaffold)

Right-click menus are now stubbed on timeline lanes:

- Section lane: section theory actions
- Theory strip: chord/progression actions
- Main track area: note/progression derivation actions
- History lane: progression capture/tagging actions

Selected actions initially updated the shell status line as stubs.

## Theory Command Wiring (Step 4)

Theory context actions are now routed into real command handlers that mutate the
`ValueTree` song model (`setle::model::Song`).

- Action callback now carries:
  - target lane (`section`, `chord`, `note`, `progression`)
  - stable action ID
  - display label
- Chord context menu now includes:
  - full scope options (`occurrence`, `repeat`, `selected repeats`, `all repeats`)
  - discoverable theory snap options (`bar`, `half bar`, `beat`, `half beat`, `1/8`, `1/16`, `1/32`)

Implemented handlers:

- Section actions:
  - edit section theory
  - repeat pattern updates
  - transition anchor creation
  - section conflict checks (missing progression refs)
- Chord actions:
  - chord edits/substitutions
  - harmonic function updates
  - scope-driven progression variant generation and assignment
  - theory snap state updates
- Note actions:
  - note edit
  - scale quantize
  - note-to-chord conversion
  - progression derivation from note context
- Progression actions:
  - queue/candidate progression creation
  - key/mode annotation cycling
  - transition material tagging

The status line now reports command results plus model counts (`P/S/T`).

## Persisted UI State

Saved via `juce::ApplicationProperties` (`SETLE.settings`):

- left panel width
- right panel width
- timeline height
- focus mode

## Persisted Theory Model State

Song/domain state now persists as XML:

- `%APPDATA%/SETLE/song_state.xml`

The shell loads existing state on startup, seeds defaults when empty, and saves
after each theory action.

## Theory Editors and Undo (Step 5)

Step 5 adds real edit surfaces and reversible actions:

- Top strip now includes `Undo Theory` and `Redo Theory`.
- Keyboard shortcuts:
  - `Ctrl/Cmd + Z` undo
  - `Ctrl/Cmd + Shift + Z` redo
  - `Ctrl + Y` redo
- Undo/redo is snapshot-based against persisted song XML, with capped history depth.

Interactive editors are now wired for core actions:

- Section:
  - `Edit Section Theory...` opens editable name/repeat dialog.
  - `Set Repeat Pattern...` opens repeat-count dialog.
  - `Add Transition Anchor...` opens transition name/strategy dialog.
- Chord:
  - `Edit Chord...` opens symbol/quality/function/root/duration dialog.
  - `Chord Substitution...` supports profile selection (`qualityCycle`, `tritone`, `relative`).
  - `Set Harmonic Function...` opens direct function entry.
- Note:
  - `Edit Note Theory...` opens pitch/velocity/start/duration dialog.
- Progression:
  - `Create Progression Candidate` and `Grab to Sampler Queue` open naming + key/mode dialog.
  - `Annotate Key / Mode` opens direct key/mode editor.

## Center Theory Panel (Step 6)

Step 6 moves edit actions into the `WORK` panel as non-modal editors.

- Right-click edit actions now open a dedicated center theory editor instead of modal popups.
- The center editor includes:
  - target object selector (section/chord/note/progression)
  - context-specific editable fields
  - `Apply Edit` and `Reload` controls
- Action routing now binds to selected timeline-theory objects:
  - selected section ID
  - selected progression ID
  - selected chord ID
  - selected note ID

Selection-aware behavior:

- Scope/snap and derived actions now run against current selected objects where applicable.
- Undo/redo refreshes the center editor state after restoring snapshots.

Implementation files:

- `source/ui/WorkspaceShellComponent.h`
- `source/ui/WorkspaceShellComponent.cpp`
