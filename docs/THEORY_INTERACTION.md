# Theory Interaction Model

## Primary Interaction

Right-click is the primary modifier and entry point for theory editing.

## Context-Sensitive Menus

- Right-click note: note-level pitch/timing/theory actions
- Right-click chord: chord edit, substitution, inversion, duration/snap
- Right-click progression: progression edit, variant controls
- Right-click section: section-level theory, conflict checks, repeat behavior

## Editing Scope

Theory edits should support explicit scope selection:

- this occurrence only
- this repeat only
- selected repeats
- all repeats (base progression)

Scope selection is required to keep variant creation intentional and predictable.

## Snap and Precision

Snap should be discoverable from context menus (not keyboard-only):

- whole bar
- half bar
- beat
- half beat
- 1/8
- 1/16
- 1/32

Numeric duration entry is the fallback for exact values.

## Visibility Levels

Theory data should be layered so users can tune complexity:

- clip annotations only (always-on, low-noise)
- docked theory strip (opt-in)
- contextual suggestions (opt-in)

## Current Implementation Status

- Right-click remains the primary gesture on timeline theory lanes.
- Scope actions are wired (`occurrence`, `repeat`, `selected repeats`, `all repeats`).
- Snap actions are discoverable in the chord context menu (`bar` through `1/32`).
- Context actions now write into the persisted song model (`song_state.xml`).
- Core context actions now open edit dialogs for section/chord/note/progression objects.
- Theory mutations support undo/redo from the top strip and standard shortcuts.
- Core edit actions are now non-modal and open in the center `WORK` theory editor panel.
- Editor actions are bound to selected theory objects (section/progression/chord/note), not fixed first-item defaults.
