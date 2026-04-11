# UI Layout Direction

## High-Level Flow

`IN -> WORK -> OUT`

- Left: `IN` (hardware input map, history buffer source, grab sampler, clip library)
- Center: `WORK` (active editor: piano roll, sequencer, plugin UI, theory editor)
- Right: `OUT` (mixer, FX chains, routing, export)
- Bottom: full-width timeline (sections + theory strip + tracks + history lane)
- Top: customizable control strip + transport

## Panel Rules

- Bottom timeline always keeps full horizontal width.
- Left and right side panels are independently resizable.
- Side panels support focus states:
  - left-expanded / right-collapsed
  - center-focused balanced
  - right-expanded / left-collapsed
- Drag handles should support snap states for fast transitions.

## Timeline Integration

- Section lane aligns with track timeline.
- Theory strip aligns to section/timeline timing and is resizable.
- History buffer lane remains visible and resizable at the bottom zone.

## Design Intent

- Keep primary composition and arrangement actions visible.
- Preserve context while allowing deep edits in center workspace.
- Avoid forcing one workflow: container-first and idea-first both remain valid.
