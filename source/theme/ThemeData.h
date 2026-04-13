#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

struct ThemeData
{
    // SURFACES
    juce::Colour surface0     { 0xFF0f0d09 };
    juce::Colour surface1     { 0xFF1c1610 };
    juce::Colour surface2     { 0xFF26201a };
    juce::Colour surface3     { 0xFF322a22 };
    juce::Colour surface4     { 0xFF3e342a };
    juce::Colour surfaceEdge  { 0xFF4a3e32 };

    // INK
    juce::Colour inkLight  { 0xFFf8f4ec };
    juce::Colour inkMid    { 0xFFd4c9b0 };
    juce::Colour inkMuted  { 0xFFa89880 };
    juce::Colour inkGhost  { 0xFF6a6050 };
    juce::Colour inkDark   { 0xFF2a1f0e };

    // ACCENT
    juce::Colour accent     { 0xFFd67f5c };
    juce::Colour accentWarm { 0xFFee7c4a };
    juce::Colour accentDim  { 0xFF8a5a1e };

    // UI CHROME
    juce::Colour headerBg       { 0xFF1c1610 };
    juce::Colour cardBg         { 0xFF26201a };
    juce::Colour rowBg          { 0xFF26201a };
    juce::Colour rowHover       { 0xFF322a22 };
    juce::Colour rowSelected    { 0xFF3e342a };
    juce::Colour badgeBg        { 0xFF322a22 };
    juce::Colour controlBg      { 0xFF26201a };
    juce::Colour controlOnBg    { 0xFF3e342a };
    juce::Colour controlText    { 0xFFa89880 };
    juce::Colour controlTextOn  { 0xFFf8f4ec };
    juce::Colour focusOutline   { 0xFF4a9eff };
    juce::Colour sliderTrack    { 0xFFd4603a };
    juce::Colour sliderThumb    { 0xFFe07b54 };

    // ZONE ACCENTS
    juce::Colour zoneA    { 0xFF4a9eff };   // IN panel - blue
    juce::Colour zoneB    { 0xFFc4822a };   // WORK panel - amber
    juce::Colour zoneC    { 0xFF4aee8a };   // OUT panel - green
    juce::Colour zoneD    { 0xFFeec44a };   // Timeline - yellow
    juce::Colour zoneMenu { 0xFFd4603a };

    // ZONE BACKGROUNDS
    juce::Colour zoneBgA  { 0xFF26201a };
    juce::Colour zoneBgB  { 0xFF26201a };
    juce::Colour zoneBgC  { 0xFF26201a };
    juce::Colour zoneBgD  { 0xFF1c1610 };

    // SIGNAL
    juce::Colour signalAudio { 0xFF4a9eff };
    juce::Colour signalMidi  { 0xFFee7c4a };
    juce::Colour signalCv    { 0xFF4aee8a };
    juce::Colour signalGate  { 0xFFeec44a };

    // TYPOGRAPHY
    float sizeDisplay   { 14.0f };
    float sizeHeading   { 13.0f };
    float sizeLabel     { 11.0f };
    float sizeBody      { 11.0f };
    float sizeMicro     { 10.0f };
    float sizeValue     { 11.0f };
    float sizeTransport { 13.0f };

    // SPACING
    float spaceXs  { 4.0f };
    float spaceSm  { 8.0f };
    float spaceMd  { 12.0f };
    float spaceLg  { 16.0f };
    float spaceXl  { 20.0f };
    float spaceXxl { 24.0f };

    // Layout
    float menuBarHeight    { 28.0f };
    float zoneAWidth       { 160.0f };
    float zoneCWidth       { 200.0f };
    float zoneDNormHeight  { 180.0f };
    float moduleSlotHeight { 80.0f };
    float stepHeight       { 24.0f };
    float stepWidth        { 24.0f };
    float knobSize         { 32.0f };

    // TAPE
    juce::Colour tapeBase       { 0xFFbfaa82 };
    juce::Colour tapeClipBg     { 0xFFe8dbc0 };
    juce::Colour tapeClipBorder { 0xFF7a6438 };
    juce::Colour tapeSeam       { 0xFF9a8660 };
    juce::Colour tapeBeatTick   { 0xFF41300e };
    juce::Colour playheadColor  { 0xFF694412 };
    juce::Colour housingEdge    { 0xFF080604 };

    // RADIUS
    float radiusXs   { 2.0f };
    float radiusChip { 3.0f };
    float radiusSm   { 4.0f };
    float radiusMd   { 6.0f };
    float radiusLg   { 10.0f };

    // STROKE
    float strokeSubtle { 0.5f };
    float strokeNormal { 1.0f };
    float strokeAccent { 1.5f };
    float strokeThick  { 2.0f };

    // CONTROLS
    float btnCornerRadius      { 4.0f };
    float btnBorderStrength    { 0.45f };
    float btnFillStrength      { 1.0f };
    float btnOnFillStrength    { 0.30f };
    float sliderTrackThickness { 4.0f };
    float sliderCornerRadius   { 4.0f };
    float sliderThumbSize      { 8.0f };
    float knobRingThickness    { 2.5f };
    float knobCapSize          { 0.72f };
    float knobDotSize          { 3.0f };
    float switchWidth          { 28.0f };
    float switchHeight         { 14.0f };
    float switchCornerRadius   { 7.0f };
    float switchThumbInset     { 2.0f };

    juce::String presetName { "Slate Dark" };
};
