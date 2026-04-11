# SETLE Architecture Snapshot

This is the current architecture direction captured from product discussions.

## Core Principle

Theory is infrastructure, not a detached plugin. Analysis runs continuously and appears contextually when useful.

## Object Hierarchy

`Note -> Chord -> Progression -> Section -> Transition -> Song`

- `Note`: atomic timing + pitch event
- `Chord`: named set of notes with explicit duration and subdivision
- `Progression`: ordered chord events with per-chord durations
- `Section`: repeats/variants of one or more progressions plus clips
- `Transition`: bridge object connecting sections with harmonic intent
- `Song`: ordered section graph

## Capture Model

- Standard timeline remains the primary arrangement surface.
- History buffer always records selected source(s).
- Preferred capture is coupled MIDI + audio to maximize theory accuracy.
- Audio-only capture remains supported with confidence-scored analysis.

## Theory Engine Behavior

- Annotates clips/progressions/sections in the background.
- Uses MIDI as primary source when available.
- Supports user overrides as authoritative data.
- Exposes compatibility and tension metadata for arrangement decisions.
