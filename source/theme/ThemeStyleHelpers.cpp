#include "ThemeStyleHelpers.h"

namespace setle::theme
{

juce::Colour panelBackground(const ThemeData& theme, ZoneRole zone)
{
    switch (zone)
    {
        case ZoneRole::inPanel: return theme.zoneBgA;
        case ZoneRole::workPanel: return theme.zoneBgB;
        case ZoneRole::outPanel: return theme.zoneBgC;
        case ZoneRole::timeline: return theme.zoneBgD;
        case ZoneRole::neutral: break;
    }

    return theme.surface1;
}

juce::Colour panelHeaderBackground(const ThemeData& theme, ZoneRole zone)
{
    switch (zone)
    {
        case ZoneRole::inPanel: return theme.headerBg.interpolatedWith(theme.zoneA, 0.16f);
        case ZoneRole::workPanel: return theme.headerBg.interpolatedWith(theme.zoneB, 0.12f);
        case ZoneRole::outPanel: return theme.headerBg.interpolatedWith(theme.zoneC, 0.14f);
        case ZoneRole::timeline: return theme.headerBg.interpolatedWith(theme.zoneD, 0.15f);
        case ZoneRole::neutral: break;
    }

    return theme.headerBg;
}

juce::Colour textForRole(const ThemeData& theme, TextRole role)
{
    switch (role)
    {
        case TextRole::primary: return theme.inkLight;
        case TextRole::muted: return theme.inkMuted;
        case TextRole::ghost: return theme.inkGhost;
        case TextRole::inverse: return theme.inkDark;
    }

    return theme.inkLight;
}

juce::Colour stateOverlay(const ThemeData& theme, SurfaceState state)
{
    switch (state)
    {
        case SurfaceState::hovered: return theme.inkLight.withAlpha(0.07f);
        case SurfaceState::selected: return theme.accent.withAlpha(0.16f);
        case SurfaceState::muted: return theme.surface0.withAlpha(0.25f);
        case SurfaceState::warning: return theme.accentWarm.withAlpha(0.18f);
        case SurfaceState::success: return theme.zoneC.withAlpha(0.16f);
        case SurfaceState::normal: break;
    }

    return juce::Colours::transparentBlack;
}

SurfaceStyle cardStyle(const ThemeData& theme, SurfaceState state)
{
    SurfaceStyle style;
    style.fill = theme.cardBg;
    style.outline = theme.surfaceEdge.withAlpha(0.65f);
    style.text = theme.inkLight.withAlpha(0.94f);
    style.radius = theme.radiusSm;
    style.stroke = theme.strokeNormal;

    switch (state)
    {
        case SurfaceState::hovered:
            style.fill = theme.rowHover;
            style.outline = theme.surfaceEdge.withAlpha(0.85f);
            break;
        case SurfaceState::selected:
            style.fill = theme.rowSelected;
            style.outline = theme.accent.withAlpha(0.95f);
            style.stroke = theme.strokeAccent;
            break;
        case SurfaceState::muted:
            style.fill = theme.rowBg.withMultipliedBrightness(0.85f);
            style.text = theme.inkMuted.withAlpha(0.75f);
            break;
        case SurfaceState::warning:
            style.fill = theme.rowBg.interpolatedWith(theme.accentWarm, 0.18f);
            style.outline = theme.accentWarm.withAlpha(0.85f);
            break;
        case SurfaceState::success:
            style.fill = theme.rowBg.interpolatedWith(theme.zoneC, 0.18f);
            style.outline = theme.zoneC.withAlpha(0.92f);
            style.stroke = theme.strokeAccent;
            break;
        case SurfaceState::normal:
            break;
    }

    return style;
}

SurfaceStyle rowStyle(const ThemeData& theme, SurfaceState state)
{
    auto style = cardStyle(theme, state);
    style.radius = theme.radiusXs;
    style.fill = theme.rowBg;
    style.outline = theme.surfaceEdge.withAlpha(0.55f);

    switch (state)
    {
        case SurfaceState::hovered:
            style.fill = theme.rowHover;
            break;
        case SurfaceState::selected:
            style.fill = theme.rowSelected;
            style.outline = theme.accent.withAlpha(0.95f);
            break;
        case SurfaceState::muted:
            style.fill = theme.rowBg.withMultipliedBrightness(0.9f);
            style.text = theme.inkGhost;
            break;
        case SurfaceState::warning:
            style.fill = theme.rowBg.interpolatedWith(theme.accentWarm, 0.14f);
            style.outline = theme.accentWarm.withAlpha(0.8f);
            break;
        case SurfaceState::success:
            style.fill = theme.rowBg.interpolatedWith(theme.zoneC, 0.12f);
            style.outline = theme.zoneC.withAlpha(0.9f);
            break;
        case SurfaceState::normal:
            break;
    }

    return style;
}

ChipStyle chipStyle(const ThemeData& theme, SurfaceState state, juce::Colour tint)
{
    ChipStyle style;

    const auto base = tint.isTransparent() ? theme.badgeBg : theme.badgeBg.interpolatedWith(tint, 0.35f);
    style.fill = base;
    style.outline = theme.surfaceEdge.withAlpha(0.75f);
    style.text = theme.inkMid.withAlpha(0.94f);

    switch (state)
    {
        case SurfaceState::selected:
            style.fill = base.interpolatedWith(theme.accent, 0.22f);
            style.outline = theme.accent.withAlpha(0.9f);
            style.text = theme.inkLight;
            break;
        case SurfaceState::muted:
            style.fill = base.withMultipliedBrightness(0.82f);
            style.text = theme.inkMuted.withAlpha(0.72f);
            break;
        case SurfaceState::warning:
            style.fill = base.interpolatedWith(theme.accentWarm, 0.26f);
            style.outline = theme.accentWarm.withAlpha(0.85f);
            break;
        case SurfaceState::success:
            style.fill = base.interpolatedWith(theme.zoneC, 0.22f);
            style.outline = theme.zoneC.withAlpha(0.85f);
            break;
        case SurfaceState::hovered:
            style.fill = base.brighter(0.08f);
            break;
        case SurfaceState::normal:
            break;
    }

    return style;
}

TimelineBlockStyle progressionBlockStyle(const ThemeData& theme,
                                         bool selected,
                                         bool active,
                                         juce::Colour tint)
{
    TimelineBlockStyle style;

    style.fill = tint.isTransparent() ? theme.surface3 : theme.surface3.interpolatedWith(tint, 0.55f);
    style.outline = theme.surfaceEdge.withAlpha(0.8f);
    style.label = theme.inkLight.withAlpha(0.96f);
    style.subtitle = theme.inkMuted.withAlpha(0.9f);
    style.badgeFill = theme.badgeBg.withAlpha(0.9f);
    style.badgeText = theme.inkMid.withAlpha(0.95f);

    if (selected)
    {
        style.fill = style.fill.interpolatedWith(theme.zoneA, 0.38f);
        style.outline = theme.accent.withAlpha(0.9f);
    }

    if (active)
    {
        style.fill = style.fill.brighter(0.2f);
        style.outline = theme.accent.withAlpha(0.98f);
    }

    return style;
}

juce::Colour timelineGridLine(const ThemeData& theme, bool barLine, bool majorMarker)
{
    if (majorMarker)
        return theme.surfaceEdge.withAlpha(0.45f);

    return theme.surfaceEdge.withAlpha(barLine ? 0.34f : 0.18f);
}

float radius(const ThemeData& theme, RadiusRole role)
{
    switch (role)
    {
        case RadiusRole::xs: return theme.radiusXs;
        case RadiusRole::chip: return theme.radiusChip;
        case RadiusRole::sm: return theme.radiusSm;
        case RadiusRole::md: return theme.radiusMd;
        case RadiusRole::lg: return theme.radiusLg;
    }

    return theme.radiusMd;
}

float stroke(const ThemeData& theme, StrokeRole role)
{
    switch (role)
    {
        case StrokeRole::subtle: return theme.strokeSubtle;
        case StrokeRole::normal: return theme.strokeNormal;
        case StrokeRole::accent: return theme.strokeAccent;
        case StrokeRole::thick: return theme.strokeThick;
    }

    return theme.strokeNormal;
}

} // namespace setle::theme
