# SETLE

A theory-aware arrangement DAW built on JUCE and Tracktion Engine.

**Status:** Early development — proof of concept stage.

## Stack

- JUCE 8
- Tracktion Engine (GPL)
- C++20
- CMake 3.24+

## Modules

- `tracktion_engine` — audio graph, clip model, MIDI, plugin host
- `melatonin_inspector` — component inspector for UI development (enabled by default)
- `pitch_detector` — YIN-based pitch detection for audio analysis (enabled by default)
- `fontaudio` — audio-specific icon font (enabled by default)
- `cello` — ValueTree helpers (enabled by default)

## Building

```bash
git clone --recurse-submodules https://github.com/2nist/SETLE.git
cd SETLE
cmake --preset windows-debug
cmake --build --preset windows-debug --target SETLE
```

For VS Code workflow and troubleshooting, see [docs/DEV_SETUP.md](docs/DEV_SETUP.md).

Note: all listed modules above are enabled by default in `windows-debug` and `windows-release` presets.

## Concept

SETLE is built around a harmonic backbone — theory awareness is infrastructure,
not an afterthought. The hierarchy is:

Note → Chord → Progression → Section → Transition → Song

Transitions connect sections. A coupled MIDI+audio history buffer captures
ideas as they happen. The theory engine annotates everything automatically.

## Design Docs

- [Architecture](docs/ARCHITECTURE.md)
- [Domain Model Schema](docs/DOMAIN_MODEL_SCHEMA.md)
- [UI Layout](docs/UI_LAYOUT.md)
- [UI Shell Implementation](docs/UI_SHELL_IMPLEMENTATION.md)
- [Theory Interaction](docs/THEORY_INTERACTION.md)

## License

GPL-3.0 — matching Tracktion Engine's open source licence.
