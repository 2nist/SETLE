#include "ThemeStyleHelpers.h"

namespace setle::theme
{

namespace
{
float stableNoise(int x, int y)
{
    const auto v = std::sin(static_cast<float>(x * 17 + y * 31)) * 0.5f + 0.5f;
    return juce::jlimit(0.0f, 1.0f, v);
}
}

MaterialToggles materialToggles(const ThemeData& theme)
{
    return {
        theme.isPebbled ? 1.0f : 0.0f,
        juce::jlimit(0.0f, 1.0f, theme.glassDistortion),
        juce::jlimit(0.0f, 1.0f, theme.materialInsetFuzz),
        juce::jlimit(0.0f, 1.0f, theme.glowWarmth)
    };
}

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

void drawChassis(juce::Graphics& g,
                 const juce::Rectangle<float>& bounds,
                 juce::Colour baseColour,
                 const ThemeData& theme,
                 float cornerRadius)
{
    const auto material = materialToggles(theme);
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, cornerRadius);

    const auto textureStrength = 0.012f + 0.060f * material.chassisTexture;
    if (textureStrength > 0.013f)
    {
        for (int y = static_cast<int>(bounds.getY()) + 2; y < static_cast<int>(bounds.getBottom()) - 2; y += 2)
        {
            for (int x = static_cast<int>(bounds.getX()) + 2; x < static_cast<int>(bounds.getRight()) - 2; x += 3)
            {
                const auto n = stableNoise(x, y);
                g.setColour(juce::Colours::black.withAlpha(textureStrength * (0.4f + 0.8f * n)));
                g.fillRect(x, y, 1, 1);
            }
        }
    }

    juce::ColourGradient topSheen(juce::Colours::white.withAlpha(0.06f + 0.08f * material.glassDepth),
                                  bounds.getTopLeft(),
                                  juce::Colours::transparentWhite,
                                  bounds.getBottomLeft(),
                                  false);
    g.setGradientFill(topSheen);
    g.fillRoundedRectangle(bounds.reduced(1.0f), cornerRadius - 1.0f);

    g.setColour(theme.surfaceEdge.withAlpha(0.70f + 0.15f * material.chassisTexture));
    g.drawRoundedRectangle(bounds.reduced(0.6f), cornerRadius, theme.strokeNormal);
}

void drawGlassPanel(juce::Graphics& g,
                    const juce::Rectangle<float>& bounds,
                    juce::Colour baseColour,
                    const ThemeData& theme,
                    bool drawBorder,
                    float cornerRadius)
{
    const auto material = materialToggles(theme);
    const float depth = material.glassDepth;

    for (int i = 0; i < 4; ++i)
    {
        const auto inset = static_cast<float>(i) * 0.8f;
        g.setColour(baseColour.withAlpha((0.05f + i * 0.025f) * (0.5f + 0.7f * depth)));
        g.fillRoundedRectangle(bounds.reduced(inset), cornerRadius - inset * 0.15f);
    }

    juce::ColourGradient gloss(juce::Colours::white.withAlpha(0.03f + 0.10f * depth),
                               bounds.getX(), bounds.getY(),
                               juce::Colours::white.withAlpha(0.0f),
                               bounds.getRight(), bounds.getBottom(),
                               false);
    g.setGradientFill(gloss);
    g.fillRoundedRectangle(bounds.reduced(1.0f), cornerRadius - 0.5f);

    if (depth > 0.05f)
    {
        for (int y = static_cast<int>(bounds.getY()) + 3; y < static_cast<int>(bounds.getBottom()) - 3; y += 3)
        {
            const int x = static_cast<int>(bounds.getX()) + 3 + (y % 9);
            g.setColour(juce::Colours::white.withAlpha(0.008f + 0.020f * depth));
            g.drawHorizontalLine(y, static_cast<float>(x), static_cast<float>(juce::jmin(x + 12, static_cast<int>(bounds.getRight()) - 3)));
        }
    }

    if (drawBorder)
    {
        g.setColour(theme.surfaceEdge.withAlpha(0.35f + 0.25f * depth));
        g.drawRoundedRectangle(bounds.reduced(0.6f), cornerRadius, theme.strokeNormal);
    }
}

void drawFuzzyInsetPanel(juce::Graphics& g,
                         const juce::Rectangle<float>& bounds,
                         juce::Colour baseColour,
                         const ThemeData& theme,
                         bool drawBorder,
                         float cornerRadius)
{
    const auto material = materialToggles(theme);
    const float fuzz = material.insetFuzz;

    g.setColour(baseColour.darker(0.20f + 0.10f * fuzz));
    g.fillRoundedRectangle(bounds, cornerRadius);

    juce::ColourGradient inset(juce::Colours::black.withAlpha(0.18f + 0.22f * fuzz),
                               bounds.getTopLeft(),
                               juce::Colours::transparentBlack,
                               bounds.getCentre(),
                               true);
    g.setGradientFill(inset);
    g.fillRoundedRectangle(bounds.reduced(1.0f), cornerRadius - 0.6f);

    if (drawBorder)
    {
        g.setColour(theme.surfaceEdge.withAlpha(0.25f + 0.20f * fuzz));
        g.drawRoundedRectangle(bounds.reduced(0.5f), cornerRadius, theme.strokeSubtle + 0.4f);
    }
}

void drawIndicatorGlow(juce::Graphics& g,
                       const juce::Point<float>& centre,
                       float radius,
                       juce::Colour accentColour,
                       const ThemeData& theme,
                       float glowFactor,
                       bool active)
{
    const auto material = materialToggles(theme);
    const auto clamped = juce::jlimit(0.0f, 1.0f, glowFactor) * (0.4f + 0.6f * material.glowAmount);

    if (!active)
    {
        g.setColour(accentColour.darker(0.5f).withAlpha(0.35f));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        return;
    }

    const auto bloom = radius + (6.0f + 10.0f * material.glowAmount) * clamped;
    juce::ColourGradient outer(accentColour.withAlpha(0.10f + 0.40f * clamped),
                               centre,
                               accentColour.withAlpha(0.0f),
                               centre.translated(bloom, 0.0f),
                               false);
    g.setGradientFill(outer);
    g.fillEllipse(centre.x - bloom, centre.y - bloom, bloom * 2.0f, bloom * 2.0f);

    g.setColour(accentColour.withAlpha(0.45f + 0.45f * clamped));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.10f + 0.25f * clamped));
    g.fillEllipse(centre.x - radius * 0.45f, centre.y - radius * 0.65f, radius * 0.7f, radius * 0.7f);
}

} // namespace setle::theme
