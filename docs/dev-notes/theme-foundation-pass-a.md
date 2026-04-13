# Theme Foundation Pass A

## Theme architecture layers
- `ThemeData` remains the single source of truth for design tokens.
- `ThemeManager` owns runtime state, broadcasts theme changes, and persists themes.
- `SetleLookAndFeel` keeps JUCE-native rendering for stock controls.
- `ThemeEditorPanel` is the developer-facing theme console for live iteration.
- `ThemeStyleHelpers` adds semantic style access for custom SETLE surfaces.

## Persistence changes
- Expanded `ThemeManager` serialization to round-trip the full editable token set:
  - base/semantic/chrome colors
  - zone + signal colors
  - typography, spacing, layout, radii, stroke, control metrics
  - timeline/tape colors
  - `presetName`
- Added backwards compatibility read support for legacy `surfaceRadius`.
- Kept `ThemeData` as the canonical source and preserved existing preset loading behavior.

## Theme editor upgrades
- Reworked `ThemeEditorPanel` into grouped sections:
  - Surfaces / Ink
  - Accent / Zones
  - UI Chrome
  - Typography
  - Spacing
  - Layout
  - Control Metrics
  - Timeline / Tape
- Added live preview area with:
  - normal button
  - toggled button
  - horizontal slider
  - rotary knob
  - combo box
  - card/row preview
  - progression/timeline preview
  - mini IN/WORK/OUT preview
- Added workflow actions:
  - Save As (existing behavior retained)
  - Duplicate current theme
  - Revert unsaved changes
  - Export theme
  - Import theme
  - Reset to default
- Added color token editing UX:
  - visible hex value per token
  - copy/paste hex actions
  - per-token reset
  - per-group reset
  - changed indicators against a baseline snapshot

## Semantic helper adoption
Initial `ThemeStyleHelpers` usage was applied in:
- `WorkspaceShellComponent` chrome/top strip painting
- `WorkspaceShellComponent::TimelineShell::ContextLane` timeline/theory block rendering
- `WorkspaceShellComponent::InDevicePanel::GrabSlotCard` card/row rendering
- `timeline::TrackLane` beat-grid and text role usage
