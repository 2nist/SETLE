# SETLE Developer Setup

This project is configured for a CMake + VS Code workflow on Windows.

## Prerequisites

- Visual Studio 2022 (Desktop development with C++)
- CMake 3.24+
- Git (required for submodules)
- VS Code

## First-Time Setup

1. Clone with submodules:
   ```bash
   git clone --recurse-submodules https://github.com/2nist/SETLE.git
   cd SETLE
   ```
2. Open the folder in VS Code.
3. Run task: `SETLE: bootstrap dependencies`
4. Run task: `SETLE: configure (Debug)`
5. Run task: `SETLE: build (Debug)`
6. Launch with:
   - Run task `SETLE: run (Debug)`, or
   - Start debugger config `SETLE: Debug active CMake target`

## Presets

`CMakePresets.json` defines:

- `windows-debug`
- `windows-release`

Default preset module flags:

- `SETLE_ENABLE_MELATONIN_INSPECTOR=ON`
- `SETLE_ENABLE_PITCH_DETECTOR=ON`
- `SETLE_ENABLE_FONTAUDIO=ON`
- `SETLE_ENABLE_CELLO=ON`

You can override these at configure time if needed:

```bash
cmake --preset windows-debug -DSETLE_ENABLE_FONTAUDIO=OFF -DSETLE_ENABLE_CELLO=OFF
```

You can also build from terminal:

```bash
cmake --preset windows-debug
cmake --build --preset windows-debug --target SETLE
```

## Troubleshooting

- If configure fails with missing `modules/*`, run:
  ```bash
  git submodule update --init --recursive
  ```
- If `pitch_detector` is enabled and `modules/audio_fft/AudioFFT` is empty, run:
  ```bash
  git -C modules/audio_fft submodule update --init --recursive
  ```
- If you are not in a git clone (no `.git`), populate `modules/` manually.
- If launch target is unresolved in VS Code, run `SETLE: configure (Debug)` once and select target `SETLE` from CMake Tools status bar.
- If enabling optional modules causes build failures, switch them back off and reconfigure to return to the known-good baseline.

## Local Compatibility Notes

- Current workspace baseline includes a JUCE/Tracktion compatibility patch in `modules/tracktion_engine/modules/tracktion_engine/plugins/tracktion_PluginWindowState.cpp` (`userBounds` -> `totalArea`) to match JUCE API changes.
