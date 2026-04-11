# SETLE Main Development Plan

This is the primary execution plan for the main coding agent.

## Goal

Ship a stable alpha baseline with reliable model editing, timeline/theory integration, and repeatable quality checks.

## Milestones

### M0: Baseline Verification (Day 1)

1. Verify dependency bootstrap, configure, and build on Windows debug preset.
2. Run `setle_model_tests` and capture current pass/fail baseline.
3. Record any known warnings and runtime blockers.

Done criteria:
- Debug build completes locally.
- Model tests run and outcomes are documented.

### M1: Phase 0 Stabilization (Days 1-3)

1. Harden theory editor numeric parsing and invalid-input behavior.
2. Improve save reliability checks, including non-XML formats.
3. Align scope mutation with selected progression context.
4. Validate undo/redo snapshot stability after edits.

Done criteria:
- Invalid numeric input preserves previous values.
- Save/load returns explicit failures on invalid write paths.
- Scope actions target selected mapping and never silently mutate unrelated refs.

### M2: Model Correctness (Days 3-6)

1. Formalize scope semantics: occurrence, repeat, selected repeats, all repeats.
2. Persist repeat selection metadata deterministically.
3. Add schema migration notes for future `schemaVersion` changes.

Done criteria:
- Scope behavior is deterministic and test-covered.
- Schema migration behavior is documented.

### M3: Timeline/Theory Integration (Days 6-10)

1. Replace timeline lane stubs with timeline-backed objects.
2. Bind chord boundary edits to snap values (`bar` to `1/32`).
3. Persist and restore theory strip size independently.

Done criteria:
- Timeline changes and theory edits stay synchronized.
- UI state restores correctly across relaunches.

### M4: Capture + Queue (Days 10-14)

1. Add always-on capture source selection.
2. Store coupled MIDI+audio capture metadata and confidence.
3. Route capture to sampler queue and progression annotation path.

Done criteria:
- End-to-end capture -> queue -> annotation flow works.

### M5: Quality Gate (Days 14-16)

1. Expand model and persistence tests.
2. Add integration checks for undo/redo snapshots.
3. Add Windows CI build+test workflow.

Done criteria:
- CI is green for build and model tests.
- Smoke checklist passes before merge.

## Immediate Next Actions

1. Complete M0 baseline command run and log outcomes.
2. Land remaining M1 fixes.
3. Add/extend tests for each M1 behavior before moving to M2.
