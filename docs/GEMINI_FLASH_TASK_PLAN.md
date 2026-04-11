# SETLE Delegated Tasks Plan (Gemini 3 Flash)

This plan is designed for a parallel agent working in small, low-conflict batches.

## Working Rules

1. Keep changes narrowly scoped and reviewable.
2. Avoid unrelated refactors.
3. Prefer tests/docs/CI additions before behavior changes.
4. Provide verification notes for each task.

## Task Order

### Task A: Model Test Expansion

Scope:
- Add tests for Song/Section/Progression persistence roundtrips.
- Add tests for repeat scope metadata defaults and migration.
- Add tests for undo/redo snapshot roundtrip utility behavior when possible.

Deliverables:
- Updated test sources under `source/tests`.
- Short summary of scenarios covered.

Success criteria:
- Tests compile and run in debug preset.

### Task B: Scope-Mutation Audit Report

Scope:
- Inspect section/progression reference mutation paths.
- Identify cases where selected progression context can diverge from mutated ref.
- Recommend concrete fixes with file and line references.

Deliverables:
- `docs/SCOPE_MUTATION_AUDIT.md` with severity and execution order.

Success criteria:
- Every finding includes exact location and expected behavior.

### Task C: Persistence Reliability Pass

Scope:
- Review `Song::saveToFile` and `Song::loadFromFile` error handling.
- Add defensive checks for parent directory creation and stream status.
- Keep storage format behavior backward compatible.

Deliverables:
- Small patch in model persistence code.
- Verification notes for XML, binary, and GZIP save/load paths.

Success criteria:
- Failure cases return explicit `juce::Result` errors.

### Task D: CI Bootstrap (Windows)

Scope:
- Add a minimal Windows workflow to bootstrap dependencies, configure, build, and run model tests.

Deliverables:
- `.github/workflows/windows-ci.yml`.
- Runtime estimate and any known caveats.

Success criteria:
- Workflow syntax validates and expected commands are present.

## Handoff Prompt

You are working in the SETLE repository. Execute Task A, then Task B, then Task D. Keep patches small and avoid changing production behavior unless required for correctness or testability. For each task, report changed files, rationale, verification commands, and residual risk. If blocked, stop and report blocker details with exact missing context.
