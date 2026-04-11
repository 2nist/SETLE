# SETLE Project Status

## What SETLE is

SETLE is a theory-aware, arrangement-focused DAW prototype built with JUCE and Tracktion Engine. It is designed around a harmonic backbone where musical objects are first-class model elements and theory is integrated as infrastructure rather than an afterthought.

Key concepts:
- Note → Chord → Progression → Section → Transition → Song
- Sections contain progression references, and transitions connect sections
- Theory-aware editor actions support scoped chord modifications: occurrence, repeat, selected repeats, all repeats
- The app is intended to capture music ideas with coupled audio/MIDI history, theory metadata, and timeline editing semantics

## Core technology stack

- JUCE 8 for UI, audio, and application framework
- Tracktion Engine for audio engine, plugin hosting, and audio device management
- C++20
- CMake presets for Windows debug/release builds
- Additional modules:
  - `melatonin_inspector` for UI inspection
  - `pitch_detector` for pitch analysis
  - `fontaudio` for audio icon font rendering
  - `cello` for internal ValueTree helpers

## Current repo status

### Implemented foundation

- `source/Main.cpp` creates a JUCE app and single `tracktion::engine::Engine` instance
- `WorkspaceShellComponent` is the main app shell component and loads inside the main window
- `source/model/SetleSongModel.h` defines the domain model and schema:
  - `Note`, `Chord`, `Progression`, `SectionProgressionRef`, `Section`, `Transition`, `Song`
  - `StorageFormat` for XML, binary, and gzip formats
  - Schema identifiers for all model node types and properties
- Model persistence supports XML and binary/gzip roundtrips
- `source/ui/WorkspaceShellComponent.h` and `.cpp` implement the UI shell, theory action menu, undo/redo, song state persistence, and editor panels
- `docs/MAIN_IMPLEMENTATION_PLAN.md` and `docs/GEMINI_FLASH_TASK_PLAN.md` exist as structured execution plans for the project and delegation

### Recent implementation work

- Fixed chord scope mutation so UI actions resolve against the selected progression and create a new section mapping when necessary
- Hardened persistence in `SetleSongModel`:
  - fail-fast parent directory creation handling when saving
  - reject empty binary/gzip loads prior to `ValueTree` parsing
- Added strict UI validation for numeric fields:
  - section repeat count
  - chord root midi and duration
  - note pitch, velocity, start, duration
- Moved transient theory state out of the persisted song model to avoid leaking runtime state into saved files
- Refactored schema migration to use explicit version-gated upgrade logic for future schema bumps
- Resolved build blockers in third-party modules:
  - `tracktion_PluginWindowState.cpp` API fix for display bounds
  - removed Projucer-only `AppConfig.h` include from `fontaudio.cpp`
  - removed `JuceHeader.h` from cello implementation files, then added explicit `cello_path.h` and `cello_query.h` includes to `cello_object.cpp`

### Build and test status

- `build/windows-debug/SETLE_artefacts/Debug/SETLE.exe` now builds successfully
- `build/windows-debug/Debug/setle_model_tests.exe` builds and runs successfully
- Model test suite currently includes:
  - schema migration coverage for repeat-scope metadata
  - XML roundtrip preservation of repeat scope metadata
  - binary/gzip persistence roundtrips
  - invalid non-XML data rejection checks

## What has been implemented

### Domain model and schema

- `Schema::version` = 1
- Song schema includes:
  - `repeatScope`, `repeatSelection`, `repeatIndices` on `SectionProgressionRef`
  - `progressions`, `sections`, `transitions` containers
- Schema migration logic now handles missing repeat-scope fields for legacy files and stamps the current schema version

### Theory editor / UI shell

- Main `WorkspaceShellComponent` provides:
  - panel layout with top strip, in/work/out panels, timeline shell, and editor panel
  - theory action menu IDs for sections, chords, notes, and progressions
  - scoped chord actions: occurrence, repeat, selected repeats, all repeats
  - editor field validation and status feedback
  - undo/redo snapshot management
  - persistent window layout and song state file support

### Persistence and format support

- `Song::saveToFile()` handles:
  - XML
  - raw binary ValueTree
  - GZIP-compressed binary
- `Song::loadFromFile()` supports the same three formats and includes early rejection for invalid or empty payloads
- The model is designed to preserve both musical structure and theory scope metadata across save/load cycles

### Project coordination docs

- `docs/MAIN_IMPLEMENTATION_PLAN.md` describes milestones M0–M5 from baseline verification through CI
- `docs/GEMINI_FLASH_TASK_PLAN.md` documents work suitable for a parallel delegated agent
- `docs/IMPLEMENTATION_LOG.md` tracks implementation actions, fixes, and blocker resolutions

## Remaining work and open tasks

### Immediate tasks

1. Verify VS Code task integration
   - ensure `cmake` is available on the integrated terminal PATH for `SETLE: configure` and build tasks
2. Manual editor smoke pass
   - validate invalid numeric input rejection in the actual UI
   - verify scoped chord action behavior in the running app
3. Confirm persistence behavior
   - ensure saved files do not contain transient runtime theory state
   - validate roundtrip for XML, binary, and gzip with actual app save/load flows

### Phase 1 / model correctness

1. Finalize scope semantics documentation and UI invariants
   - occurrence: single chord occurrence in the current repeat
   - repeat: the current repeat only
   - selected repeats: explicit repeat index selection
   - all repeats: entire progression base mapping
2. Expand test coverage for all scope modes
3. Add deterministic metadata preservation for repeat selection choices

### Phase 2 / timeline and theory integration

1. Replace timeline lane stubs with real timeline-backed objects
2. Bind chord boundary edits to the current snap setting
3. Persist and restore timeline/theory strip UI state across launches
4. Add edit synchronization between timeline, chord model, and theory metadata

### Phase 3 / capture and queue

1. Implement always-on capture source selection
2. Capture coupled MIDI+audio metadata and confidence values
3. Route capture into a sampler queue and theory annotation flow

### Phase 4 / quality gate and CI

1. Add broader model and persistence tests
2. Add integration coverage for undo/redo consistency
3. Add a Windows CI workflow for build + model test pass

## Known risks / blockers

- The app is still early proof-of-concept and only a partial UI exists; many core features are not yet present
- Timeline integration is incomplete and not currently fully synchronized with theory edits
- Capture/queue flow is not implemented
- `cmake` not reliably available in the VS Code integrated task shell until PATH fix is verified

## Recommended next step

1. Use the existing `docs/MAIN_IMPLEMENTATION_PLAN.md` milestones as the project roadmap
2. Complete the current Phase 0 / M1 stabilization checklist first
3. Add a small CI/quality gate around the model test binary and schema migration behavior

## Document location

- `docs/PROJECT_STATUS.md`

This document is intended as the current single-point status summary for SETLE, combining implementation state, app architecture, recent fixes, build results, and pending work.