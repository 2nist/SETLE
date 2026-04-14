# JUCE Control Family Refinement

## Control families improved
- `TextButton` / `ToggleButton` / `TickBox`: unified fill-border-focus treatment with clearer hover/pressed/on/disabled states.
- `Slider` (linear + bar + rotary): unified track/body/thumb language and stronger active-value readability.
- `ComboBox`: clearer field/arrow split, improved focus treatment, and better disabled readability.
- `PopupMenu` + menu bar items: themed background, item hover row treatment, tick/submenu affordances, and improved text contrast.
- `Scrollbar`: aligned thumb/track styling and state readability with the same token family.
- `Label` + tab buttons: aligned text and container styling to keep control surfaces cohesive.

## Tokens with stronger visual effect
- Slider/knob metrics: `sliderTrackThickness`, `sliderThumbSize`, `sliderCornerRadius`, `knobRingThickness`, `knobCapSize`, `knobDotSize`.
- Button metrics: `btnBorderStrength`, `btnFillStrength`, `btnOnFillStrength`, `btnCornerRadius`.
- Shared shape/stroke tokens: `radiusXs/radiusSm`, `strokeSubtle/strokeNormal/strokeAccent`.
- Color-state tokens: `controlBg`, `controlOnBg`, `controlText`, `controlTextOn`, `surfaceEdge`, `focusOutline`, `sliderTrack`, `sliderThumb`, plus row/surface tokens for menu/tab/readability states.

## Notable tradeoffs
- Prioritized dense-UI readability over dramatic visual effects: stronger active/focus/on states, restrained motion/ornamentation.
- Kept rendering purely JUCE-native (`SetleLookAndFeel`) to avoid introducing any parallel styling system.
- Popup-menu text sizing remains conservative for legibility in compact DAW contexts.
