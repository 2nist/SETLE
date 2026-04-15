#pragma once

#include <juce_graphics/juce_graphics.h>

#include "ThemeData.h"

namespace setle::theme
{

enum class ZoneRole
{
    neutral,
    inPanel,
    workPanel,
    outPanel,
    timeline
};

enum class TextRole
{
    primary,
    muted,
    ghost,
    inverse
};

enum class SurfaceState
{
    normal,
    hovered,
    selected,
    muted,
    warning,
    success
};

enum class RadiusRole
{
    xs,
    chip,
    sm,
    md,
    lg
};

enum class StrokeRole
{
    subtle,
    normal,
    accent,
    thick
};

struct SurfaceStyle
{
    juce::Colour fill;
    juce::Colour outline;
    juce::Colour text;
    float radius { 0.0f };
    float stroke { 1.0f };
};

struct ChipStyle
{
    juce::Colour fill;
    juce::Colour outline;
    juce::Colour text;
};

struct TimelineBlockStyle
{
    juce::Colour fill;
    juce::Colour outline;
    juce::Colour label;
    juce::Colour subtitle;
    juce::Colour badgeFill;
    juce::Colour badgeText;
};

struct MaterialToggles
{
    float chassisTexture { 1.0f };
    float glassDepth { 1.0f };
    float insetFuzz { 1.0f };
    float glowAmount { 1.0f };
};

juce::Colour panelBackground(const ThemeData& theme, ZoneRole zone);
juce::Colour panelHeaderBackground(const ThemeData& theme, ZoneRole zone);
juce::Colour textForRole(const ThemeData& theme, TextRole role);
juce::Colour stateOverlay(const ThemeData& theme, SurfaceState state);

MaterialToggles materialToggles(const ThemeData& theme);

SurfaceStyle cardStyle(const ThemeData& theme, SurfaceState state);
SurfaceStyle rowStyle(const ThemeData& theme, SurfaceState state);
ChipStyle chipStyle(const ThemeData& theme,
                    SurfaceState state,
                    juce::Colour tint = juce::Colours::transparentBlack);

TimelineBlockStyle progressionBlockStyle(const ThemeData& theme,
                                         bool selected,
                                         bool active,
                                         juce::Colour tint = juce::Colours::transparentBlack);

juce::Colour timelineGridLine(const ThemeData& theme, bool barLine, bool majorMarker = false);
float radius(const ThemeData& theme, RadiusRole role);
float stroke(const ThemeData& theme, StrokeRole role);

void drawChassis(juce::Graphics& g,
                 const juce::Rectangle<float>& bounds,
                 juce::Colour baseColour,
                 const ThemeData& theme,
                 float cornerRadius = 10.0f);

void drawGlassPanel(juce::Graphics& g,
                    const juce::Rectangle<float>& bounds,
                    juce::Colour baseColour,
                    const ThemeData& theme,
                    bool drawBorder = true,
                    float cornerRadius = 10.0f);

void drawFuzzyInsetPanel(juce::Graphics& g,
                         const juce::Rectangle<float>& bounds,
                         juce::Colour baseColour,
                         const ThemeData& theme,
                         bool drawBorder = true,
                         float cornerRadius = 8.0f);

void drawIndicatorGlow(juce::Graphics& g,
                       const juce::Point<float>& centre,
                       float radius,
                       juce::Colour accentColour,
                       const ThemeData& theme,
                       float glowFactor,
                       bool active = true);

} // namespace setle::theme
