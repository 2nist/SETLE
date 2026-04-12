# SETLE Development Plan (Working Draft)

This plan reflects the current proof-of-concept baseline and the next implementation priorities.

## Phase 0: Stabilization (Current)

- Harden theory edit input parsing to avoid accidental zeroing on invalid numeric input.
- Improve save reliability checks for non-XML persistence formats.
- Align scope mutation behavior with selected progression context instead of always mutating the first section reference.
- Keep shipping with static review + manual runtime checks until CI/test harness exists.

## Phase 1: Model Correctness

- Extend section/progression mapping so scope actions are explicit and auditable:
  - single occurrence
  - repeat
  - selected repeats
  - all repeats
- Define repeat-selection representation in the model (not only UI state).
- Add schema migration notes for future `schemaVersion` bumps.

## Phase 2: Timeline/Theory Integration

- Replace lane stubs with real timeline-backed objects and selection.
- Make theory strip height independently resizable and persist the state.
- Bind chord boundary edits to timeline timing with snap values (`bar` to `1/32`).
- Surface scope + snap state visibly in the top strip and context menus.

## Phase 3: Capture + History Buffer

- Implement always-on history capture source selector.
- Add coupled MIDI+audio capture metadata and confidence flags.
- Support "grab to sampler queue" with queue persistence.
- Route grabbed material into progression/section annotation pipeline.

## Phase 4: Quality and Tooling

- Add model unit tests (`Song`, `Section`, `Progression`, persistence roundtrips).
- Add integration tests for undo/redo snapshot behavior.
- Add build/test CI on Windows (primary), then macOS.
- Add a developer "smoke test" checklist for manual validation of theory workflows.

## Execution Order

1. Complete Phase 0 stabilization issues and verify via manual test run.
2. Implement Phase 1 schema and scope semantics before deeper UI expansion.
3. Build Phase 2 timeline integration against the stabilized model.
4. Land Phase 3 capture pipeline and sampler queue.
5. Backfill Phase 4 test/CI before beta.

