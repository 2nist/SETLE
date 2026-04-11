# Domain Model Schema (Phase 2 - Step 1)

This schema defines the initial `ValueTree`-backed model used by SETLE.

## Root Tree

- Type: `setleSong`
- Required properties:
  - `schemaVersion` (int, current `1`)
  - `title` (string)
  - `bpm` (double)
- Required child containers:
  - `progressions`
  - `sections`
  - `transitions`

## Atomic and Structural Types

### `note`
- `id` (string UUID)
- `pitch` (int MIDI note)
- `velocity` (float 0..1)
- `startBeats` (double)
- `durationBeats` (double)
- `channel` (int)

### `chord`
- `id` (string UUID)
- `name` (string)
- `symbol` (string, ex: `Dm7`)
- `quality` (string)
- `function` (string, ex: `T`, `PD`, `D`)
- `rootMidi` (int)
- `startBeats` (double)
- `durationBeats` (double)
- `tension` (int)
- Children:
  - `note` nodes

### `progression`
- `id` (string UUID)
- `name` (string)
- `key` (string)
- `mode` (string)
- `lengthBeats` (double)
- `variantOf` (string progression ID or empty)
- Children:
  - `chord` nodes

### `sectionProgressionRef`
- `progressionId` (string)
- `orderIndex` (int)
- `variantName` (string)

### `section`
- `id` (string UUID)
- `name` (string)
- `repeatCount` (int)
- Children:
  - `sectionProgressionRef` nodes

### `transition`
- `id` (string UUID)
- `name` (string)
- `fromSectionId` (string)
- `toSectionId` (string)
- `strategy` (string)
- `tension` (int target)

## Persistence

`Song` supports:
- XML (`StorageFormat::xml`)
- JUCE binary (`StorageFormat::binary`)
- GZIP-compressed binary (`StorageFormat::gzipBinary`)

Implementation lives in:
- `source/model/SetleSongModel.h`
- `source/model/SetleSongModel.cpp`
