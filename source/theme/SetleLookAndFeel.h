#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#include "ThemeManager.h"

class SetleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override
    {
        const auto& t = ThemeManager::get().theme();
        const auto size = juce::jlimit(t.sizeMicro, t.sizeTransport, static_cast<float>(buttonHeight) * 0.48f);
        return juce::FontOptions(size);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool highlighted,
                              bool down) override
    {
        const auto& t = ThemeManager::get().theme();
        const bool enabled = button.isEnabled();
        const bool toggled = button.getToggleState();

        auto fill = toggled ? t.controlOnBg : t.controlBg;
        if (backgroundColour.getAlpha() > 0 && backgroundColour != juce::Colours::transparentBlack)
            fill = fill.interpolatedWith(backgroundColour, juce::jlimit(0.0f, 1.0f, 0.30f + t.btnFillStrength * 0.45f));

        if (toggled)
            fill = fill.interpolatedWith(t.accent, juce::jlimit(0.0f, 0.5f, 0.10f + t.btnOnFillStrength * 0.18f));

        if (highlighted)
            fill = fill.brighter(0.06f);
        if (down)
            fill = fill.brighter(0.14f);
        if (!enabled)
            fill = fill.withMultipliedSaturation(0.35f).withAlpha(0.70f);

        const auto corner = juce::jmax(1.0f, 0.5f * (t.btnCornerRadius + t.radiusSm));
        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, corner);

        auto border = t.surfaceEdge.withAlpha(juce::jlimit(0.08f, 1.0f, t.btnBorderStrength));
        if (toggled)
            border = border.interpolatedWith(t.accent, 0.42f);
        if (highlighted)
            border = border.brighter(0.08f);
        if (!enabled)
            border = border.withAlpha(0.35f);

        g.setColour(border);
        g.drawRoundedRectangle(bounds, corner, juce::jmax(0.6f, t.strokeNormal));

        if (button.hasKeyboardFocus(true))
        {
            g.setColour(t.focusOutline.withAlpha(enabled ? 0.95f : 0.45f));
            g.drawRoundedRectangle(bounds.expanded(1.1f), corner + 1.0f, juce::jmax(1.0f, t.strokeAccent));
        }
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool) override
    {
        const auto& t = ThemeManager::get().theme();

        g.setFont(getTextButtonFont(button, button.getHeight()));

        auto textColour = button.getToggleState() ? t.controlTextOn : t.controlText;
        if (shouldDrawButtonAsHighlighted)
            textColour = textColour.brighter(0.08f);
        if (!button.isEnabled())
            textColour = t.inkGhost.withAlpha(0.60f);

        g.setColour(textColour);
        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().reduced(6, 2),
                         juce::Justification::centred,
                         1);
    }

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool highlighted,
                          bool down) override
    {
        const auto& t = ThemeManager::get().theme();
        const auto bounds = button.getLocalBounds();
        const bool enabled = button.isEnabled();
        const bool toggled = button.getToggleState();

        const int preferredSwitchWidth = juce::roundToInt(t.switchWidth);
        const int preferredSwitchHeight = juce::roundToInt(t.switchHeight);
        const int switchWidth = juce::jlimit(12, juce::jmax(12, bounds.getWidth() / 2), preferredSwitchWidth);
        const int switchHeight = juce::jlimit(8, juce::jmax(8, bounds.getHeight() - 4), preferredSwitchHeight);
        const bool canUseSwitch = switchWidth >= 12 && switchHeight >= 8;

        const auto switchBounds = juce::Rectangle<float>(3.0f,
                                                         (bounds.getHeight() - static_cast<float>(switchHeight)) * 0.5f,
                                                         static_cast<float>(switchWidth),
                                                         static_cast<float>(switchHeight));

        if (canUseSwitch)
        {
            auto trackColour = toggled ? t.controlOnBg : t.controlBg;
            if (highlighted)
                trackColour = trackColour.brighter(0.06f);
            if (down)
                trackColour = trackColour.brighter(0.1f);
            if (!enabled)
                trackColour = trackColour.withMultipliedSaturation(0.35f).withAlpha(0.68f);

            const auto corner = juce::jlimit(1.0f,
                                             switchBounds.getHeight() * 0.5f,
                                             t.switchCornerRadius);
            g.setColour(trackColour);
            g.fillRoundedRectangle(switchBounds, corner);

            auto border = t.surfaceEdge.withAlpha(juce::jlimit(0.08f, 1.0f, t.btnBorderStrength));
            if (toggled)
                border = border.interpolatedWith(t.accent, 0.45f);
            if (!enabled)
                border = border.withAlpha(0.35f);
            g.setColour(border);
            g.drawRoundedRectangle(switchBounds, corner, juce::jmax(0.6f, t.strokeNormal));

            const auto inset = juce::jlimit(0.0f, switchBounds.getHeight() * 0.35f, t.switchThumbInset);
            const auto thumbDiameter = juce::jmax(4.0f, switchBounds.getHeight() - inset * 2.0f);
            const auto minX = switchBounds.getX() + inset + thumbDiameter * 0.5f;
            const auto maxX = switchBounds.getRight() - inset - thumbDiameter * 0.5f;
            const auto thumbX = toggled ? maxX : minX;
            const auto thumbCenter = juce::Point<float>(thumbX, switchBounds.getCentreY());

            auto thumb = t.sliderThumb;
            if (highlighted)
                thumb = thumb.brighter(0.07f);
            if (!enabled)
                thumb = thumb.withAlpha(0.52f);

            g.setColour(thumb);
            g.fillEllipse(juce::Rectangle<float>(thumbDiameter, thumbDiameter).withCentre(thumbCenter));
            g.setColour(t.surfaceEdge.withAlpha(enabled ? 0.85f : 0.35f));
            g.drawEllipse(juce::Rectangle<float>(thumbDiameter, thumbDiameter).withCentre(thumbCenter),
                          juce::jmax(0.6f, t.strokeSubtle));
        }
        else
        {
            const int boxSize = juce::jlimit(12, 20, bounds.getHeight() - 6);
            const auto boxBounds = bounds.withSizeKeepingCentre(boxSize, boxSize).withX(3);
            drawTickBox(g,
                        button,
                        static_cast<float>(boxBounds.getX()),
                        static_cast<float>(boxBounds.getY()),
                        static_cast<float>(boxBounds.getWidth()),
                        static_cast<float>(boxBounds.getHeight()),
                        toggled,
                        enabled,
                        highlighted,
                        down);
        }

        g.setFont(juce::FontOptions(t.sizeLabel));
        auto textColour = toggled ? t.controlTextOn : t.controlText;
        if (!enabled)
            textColour = t.inkGhost.withAlpha(0.62f);

        g.setColour(textColour);
        g.drawText(button.getButtonText(),
                   bounds.withTrimmedLeft(static_cast<int>(switchBounds.getRight()) + 6),
                   juce::Justification::centredLeft,
                   true);

        if (button.hasKeyboardFocus(true))
        {
            g.setColour(t.focusOutline.withAlpha(0.92f));
            g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), juce::jmax(2.0f, t.radiusSm), juce::jmax(1.0f, t.strokeAccent));
        }
    }

    void drawTickBox(juce::Graphics& g,
                     juce::Component& component,
                     float x,
                     float y,
                     float w,
                     float h,
                     bool ticked,
                     bool isEnabled,
                     bool highlighted,
                     bool down) override
    {
        const auto& t = ThemeManager::get().theme();
        auto rect = juce::Rectangle<float>(x, y, w, h).reduced(0.5f);

        auto fill = ticked ? t.controlOnBg : t.controlBg;
        if (highlighted)
            fill = fill.brighter(0.07f);
        if (down)
            fill = fill.brighter(0.10f);
        if (!isEnabled)
            fill = fill.withMultipliedSaturation(0.3f).withAlpha(0.7f);

        const auto corner = juce::jmin(juce::jmax(1.0f, t.radiusXs + 1.0f), rect.getWidth() * 0.35f);

        g.setColour(fill);
        g.fillRoundedRectangle(rect, corner);

        auto border = t.surfaceEdge.withAlpha(juce::jlimit(0.08f, 1.0f, t.btnBorderStrength));
        if (ticked)
            border = border.interpolatedWith(t.accent, 0.45f);
        if (!isEnabled)
            border = border.withAlpha(0.35f);

        g.setColour(border);
        g.drawRoundedRectangle(rect, corner, juce::jmax(0.6f, t.strokeNormal));

        if (ticked)
        {
            juce::Path tick;
            tick.startNewSubPath(rect.getX() + rect.getWidth() * 0.22f, rect.getCentreY());
            tick.lineTo(rect.getX() + rect.getWidth() * 0.44f, rect.getBottom() - rect.getHeight() * 0.24f);
            tick.lineTo(rect.getRight() - rect.getWidth() * 0.20f, rect.getY() + rect.getHeight() * 0.24f);

            auto mark = t.controlTextOn;
            if (!isEnabled)
                mark = mark.withAlpha(0.55f);

            g.setColour(mark);
            g.strokePath(tick, juce::PathStrokeType(juce::jmax(1.2f, t.strokeAccent), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (component.hasKeyboardFocus(true))
        {
            g.setColour(t.focusOutline.withAlpha(0.9f));
            g.drawRoundedRectangle(rect.expanded(1.0f), corner + 1.0f, juce::jmax(1.0f, t.strokeAccent));
        }
    }

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox& combo) override
    {
        const auto& t = ThemeManager::get().theme();
        const bool enabled = combo.isEnabled();
        const bool hovered = combo.isMouseOver(true);

        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
        auto fill = t.controlBg;

        if (hovered)
            fill = fill.brighter(0.05f);
        if (isButtonDown)
            fill = fill.brighter(0.09f);
        if (!enabled)
            fill = fill.withMultipliedSaturation(0.35f).withAlpha(0.7f);

        const auto corner = juce::jmax(1.0f, 0.5f * (t.sliderCornerRadius + t.radiusSm));

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, corner);

        auto arrowZone = juce::Rectangle<float>(static_cast<float>(buttonX),
                                                static_cast<float>(buttonY),
                                                static_cast<float>(buttonW),
                                                static_cast<float>(buttonH));
        g.setColour((isButtonDown ? t.controlOnBg : t.surface3).withAlpha(enabled ? 0.92f : 0.5f));
        g.fillRoundedRectangle(arrowZone.reduced(1.0f), juce::jmax(1.0f, t.radiusXs + 1.0f));

        juce::Path arrow;
        const auto cx = arrowZone.getCentreX();
        const auto cy = arrowZone.getCentreY();
        arrow.startNewSubPath(cx - 4.0f, cy - 1.0f);
        arrow.lineTo(cx, cy + 3.0f);
        arrow.lineTo(cx + 4.0f, cy - 1.0f);

        g.setColour((enabled ? t.controlText : t.inkGhost).withAlpha(0.95f));
        g.strokePath(arrow, juce::PathStrokeType(juce::jmax(1.1f, t.strokeNormal), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto border = t.surfaceEdge.withAlpha(juce::jlimit(0.08f, 1.0f, t.btnBorderStrength));
        if (combo.hasKeyboardFocus(true))
            border = border.interpolatedWith(t.focusOutline, 0.6f);

        g.setColour(border);
        g.drawRoundedRectangle(bounds, corner, juce::jmax(0.6f, t.strokeNormal));

        if (combo.hasKeyboardFocus(true))
        {
            g.setColour(t.focusOutline.withAlpha(0.92f));
            g.drawRoundedRectangle(bounds.expanded(1.0f), corner + 1.0f, juce::jmax(1.0f, t.strokeAccent));
        }
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::FontOptions(ThemeManager::get().theme().sizeBody);
    }

    void positionComboBoxText(juce::ComboBox& combo, juce::Label& label) override
    {
        auto bounds = combo.getLocalBounds().reduced(6, 1);
        bounds.removeFromRight(18);
        label.setBounds(bounds);
        label.setFont(getComboBoxFont(combo));
        label.setJustificationType(juce::Justification::centredLeft);
    }

    int getSliderThumbRadius(juce::Slider&) override
    {
        return juce::jlimit(2, 32, juce::roundToInt(ThemeManager::get().theme().sliderThumbSize * 0.5f));
    }

    void drawLinearSlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float,
                          float,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override
    {
        const auto& t = ThemeManager::get().theme();
        const bool enabled = slider.isEnabled();
        const bool hovered = slider.isMouseOverOrDragging();
        const bool focused = slider.hasKeyboardFocus(true);

        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);

        if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
        {
            const bool verticalBar = (style == juce::Slider::LinearBarVertical);
            auto fill = t.controlBg;
            if (!enabled)
                fill = fill.withAlpha(0.6f);

            g.setColour(fill);
            g.fillRoundedRectangle(bounds, juce::jmax(1.0f, t.sliderCornerRadius));

            g.setColour(t.sliderTrack.withAlpha(enabled ? 0.95f : 0.45f));
            if (verticalBar)
                g.fillRoundedRectangle(bounds.withY(sliderPos).withHeight(bounds.getBottom() - sliderPos), juce::jmax(1.0f, t.sliderCornerRadius));
            else
                g.fillRoundedRectangle(bounds.withWidth(sliderPos - bounds.getX()), juce::jmax(1.0f, t.sliderCornerRadius));

            g.setColour(t.surfaceEdge.withAlpha(0.8f));
            g.drawRoundedRectangle(bounds, juce::jmax(1.0f, t.sliderCornerRadius), juce::jmax(0.6f, t.strokeNormal));
            return;
        }

        const bool vertical = (style == juce::Slider::LinearVertical
                               || style == juce::Slider::LinearBarVertical
                               || style == juce::Slider::TwoValueVertical
                               || style == juce::Slider::ThreeValueVertical);
        const float trackThickness = juce::jlimit(1.0f,
                                                  vertical ? bounds.getWidth() - 4.0f : bounds.getHeight() - 4.0f,
                                                  t.sliderTrackThickness);
        const float corner = juce::jmax(1.0f, t.sliderCornerRadius);

        juce::Rectangle<float> track;
        if (vertical)
            track = { bounds.getCentreX() - trackThickness * 0.5f, bounds.getY() + 2.0f, trackThickness, bounds.getHeight() - 4.0f };
        else
            track = { bounds.getX() + 2.0f, bounds.getCentreY() - trackThickness * 0.5f, bounds.getWidth() - 4.0f, trackThickness };

        g.setColour(t.controlBg.withAlpha(enabled ? 0.95f : 0.45f));
        g.fillRoundedRectangle(track, corner);

        g.setColour(t.surfaceEdge.withAlpha(enabled ? 0.65f : 0.3f));
        g.drawRoundedRectangle(track, corner, juce::jmax(0.6f, t.strokeSubtle));

        juce::Rectangle<float> activeTrack = track;
        if (vertical)
            activeTrack = track.withY(sliderPos).withHeight(track.getBottom() - sliderPos);
        else
            activeTrack = track.withWidth(sliderPos - track.getX());

        auto activeColour = t.sliderTrack;
        if (hovered)
            activeColour = activeColour.brighter(0.08f);
        if (!enabled)
            activeColour = activeColour.withAlpha(0.5f);

        g.setColour(activeColour);
        g.fillRoundedRectangle(activeTrack, corner);

        const float thumbDiameter = juce::jlimit(4.0f,
                                                 juce::jmin(bounds.getWidth(), bounds.getHeight()),
                                                 t.sliderThumbSize);
        juce::Point<float> thumbCenter(vertical ? track.getCentreX() : sliderPos,
                                       vertical ? sliderPos : track.getCentreY());

        auto thumbRect = juce::Rectangle<float>(thumbDiameter, thumbDiameter).withCentre(thumbCenter);
        auto thumbFill = t.sliderThumb;
        if (hovered)
            thumbFill = thumbFill.brighter(0.1f);
        if (!enabled)
            thumbFill = thumbFill.withAlpha(0.5f);

        g.setColour(thumbFill);
        g.fillEllipse(thumbRect);
        g.setColour(t.surfaceEdge.withAlpha(enabled ? 0.9f : 0.35f));
        g.drawEllipse(thumbRect, juce::jmax(0.7f, t.strokeNormal));

        if (focused)
        {
            g.setColour(t.focusOutline.withAlpha(0.9f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), corner + 1.0f, juce::jmax(1.0f, t.strokeAccent));
        }
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x,
                          int y,
                          int width,
                          int height,
                          float sliderPos,
                          float startAngle,
                          float endAngle,
                          juce::Slider& slider) override
    {
        const auto& t = ThemeManager::get().theme();
        const bool enabled = slider.isEnabled();
        const bool hovered = slider.isMouseOverOrDragging();

        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
        const auto center = bounds.getCentre();
        const float radius = juce::jmax(2.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f);
        const float ringThickness = juce::jlimit(0.8f, radius * 0.8f, t.knobRingThickness);

        auto body = t.controlBg;
        if (hovered)
            body = body.brighter(0.05f);
        if (!enabled)
            body = body.withAlpha(0.65f);

        g.setColour(body);
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour(t.surfaceEdge.withAlpha(enabled ? 0.9f : 0.45f));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, juce::jmax(0.7f, t.strokeNormal));

        const float ringRadius = juce::jmax(1.0f, radius - ringThickness * 0.85f);
        juce::Path baseArc;
        baseArc.addCentredArc(center.x, center.y, ringRadius, ringRadius, 0.0f, startAngle, endAngle, true);
        g.setColour(t.surfaceEdge.withAlpha(enabled ? 0.45f : 0.25f));
        g.strokePath(baseArc, juce::PathStrokeType(ringThickness));

        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path fillArc;
        fillArc.addCentredArc(center.x, center.y, ringRadius, ringRadius, 0.0f, startAngle, angle, true);
        auto ringColour = t.sliderTrack;
        if (hovered)
            ringColour = ringColour.brighter(0.1f);
        if (!enabled)
            ringColour = ringColour.withAlpha(0.45f);

        g.setColour(ringColour);
        g.strokePath(fillArc, juce::PathStrokeType(ringThickness));

        const float capRadius = juce::jlimit(2.0f, radius * 0.75f, radius * juce::jlimit(0.2f, 1.0f, t.knobCapSize) * 0.5f);
        g.setColour((hovered ? t.controlOnBg.brighter(0.05f) : t.controlOnBg).withAlpha(enabled ? 0.95f : 0.55f));
        g.fillEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

        const float dotRadius = juce::jlimit(1.0f, radius * 0.35f, t.knobDotSize * 0.5f);
        const float dotDistance = juce::jmax(dotRadius + 1.0f, ringRadius - ringThickness * 0.65f);
        const auto dotPoint = juce::Point<float>(center.x + std::cos(angle) * dotDistance,
                                                 center.y + std::sin(angle) * dotDistance);
        g.setColour((enabled ? t.sliderThumb : t.sliderThumb.withAlpha(0.5f)).withAlpha(0.95f));
        g.fillEllipse(dotPoint.x - dotRadius, dotPoint.y - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);

        if (slider.hasKeyboardFocus(true))
        {
            g.setColour(t.focusOutline.withAlpha(0.9f));
            g.drawEllipse(bounds.expanded(1.0f), juce::jmax(1.0f, t.strokeAccent));
        }
    }

    void drawScrollbar(juce::Graphics& g,
                       juce::ScrollBar& scrollbar,
                       int x,
                       int y,
                       int width,
                       int height,
                       bool isScrollbarVertical,
                       int thumbStartPosition,
                       int thumbSize,
                       bool isMouseOver,
                       bool isMouseDown) override
    {
        const auto& t = ThemeManager::get().theme();
        const bool enabled = scrollbar.isEnabled();
        const auto track = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height));

        g.setColour(t.surface1.withAlpha(enabled ? 0.85f : 0.5f));
        g.fillRect(track);

        juce::Rectangle<float> thumb;
        if (isScrollbarVertical)
            thumb = { static_cast<float>(x) + 1.0f, static_cast<float>(thumbStartPosition), static_cast<float>(width) - 2.0f, static_cast<float>(thumbSize) };
        else
            thumb = { static_cast<float>(thumbStartPosition), static_cast<float>(y) + 1.0f, static_cast<float>(thumbSize), static_cast<float>(height) - 2.0f };

        auto thumbColour = t.sliderThumb.withAlpha(enabled ? 0.72f : 0.38f);
        if (isMouseOver)
            thumbColour = thumbColour.brighter(0.08f);
        if (isMouseDown)
            thumbColour = thumbColour.brighter(0.12f);

        const auto corner = juce::jmax(1.0f, t.radiusChip);
        g.setColour(thumbColour);
        g.fillRoundedRectangle(thumb.reduced(0.5f), corner);

        g.setColour(t.surfaceEdge.withAlpha(enabled ? 0.75f : 0.35f));
        g.drawRect(x, y, width, height, static_cast<int>(juce::jmax(1.0f, t.strokeSubtle)));
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        const auto& t = ThemeManager::get().theme();
        auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        g.setColour(t.surface1.withAlpha(0.98f));
        g.fillRoundedRectangle(bounds.reduced(0.5f), juce::jmax(2.0f, t.radiusSm));
        g.setColour(t.surfaceEdge.withAlpha(0.9f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), juce::jmax(2.0f, t.radiusSm), juce::jmax(0.8f, t.strokeNormal));
    }

    void drawPopupMenuItem(juce::Graphics& g,
                           const juce::Rectangle<int>& area,
                           bool isSeparator,
                           bool isActive,
                           bool isHighlighted,
                           bool isTicked,
                           bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override
    {
        const auto& t = ThemeManager::get().theme();

        if (isSeparator)
        {
            auto line = area.reduced(6, 0);
            g.setColour(t.surfaceEdge.withAlpha(0.55f));
            g.fillRect(line.removeFromTop(1).withY(area.getCentreY()));
            return;
        }

        auto row = area.reduced(2, 1);
        if (isHighlighted)
        {
            g.setColour(t.rowHover.withAlpha(0.95f));
            g.fillRoundedRectangle(row.toFloat(), juce::jmax(2.0f, t.radiusXs + 1.0f));
            g.setColour(t.surfaceEdge.withAlpha(0.85f));
            g.drawRoundedRectangle(row.toFloat(), juce::jmax(2.0f, t.radiusXs + 1.0f), juce::jmax(0.8f, t.strokeNormal));
        }

        auto textArea = row.reduced(6, 0);

        if (icon != nullptr)
        {
            icon->drawWithin(g, textArea.removeFromLeft(row.getHeight() - 4).toFloat(), juce::RectanglePlacement::centred, 1.0f);
            textArea.removeFromLeft(2);
        }
        else
        {
            textArea.removeFromLeft(row.getHeight() - 4);
        }

        if (isTicked)
        {
            auto tickRect = row.removeFromLeft(row.getHeight()).toFloat().reduced(6.0f);
            g.setColour(t.accent.withAlpha(isActive ? 0.95f : 0.45f));
            g.fillEllipse(tickRect);
        }

        auto baseTextColour = textColour != nullptr
                                  ? *textColour
                                  : (isActive ? t.inkLight : t.inkGhost.withAlpha(0.82f));

        if (isHighlighted)
            baseTextColour = baseTextColour.brighter(0.08f);

        g.setColour(baseTextColour);
        g.setFont(juce::FontOptions(t.sizeBody));
        g.drawFittedText(text, textArea, juce::Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour((isActive ? t.inkMuted : t.inkGhost).withAlpha(0.9f));
            g.setFont(juce::FontOptions(t.sizeMicro));
            g.drawText(shortcutKeyText, textArea, juce::Justification::centredRight, true);
        }

        if (hasSubMenu)
        {
            juce::Path arrow;
            const auto cx = static_cast<float>(row.getRight() - 9);
            const auto cy = static_cast<float>(row.getCentreY());
            arrow.startNewSubPath(cx - 2.0f, cy - 3.0f);
            arrow.lineTo(cx + 2.0f, cy);
            arrow.lineTo(cx - 2.0f, cy + 3.0f);

            g.setColour((isActive ? t.inkMuted : t.inkGhost).withAlpha(0.9f));
            g.strokePath(arrow, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void getIdealPopupMenuItemSize(const juce::String& text,
                                   bool isSeparator,
                                   int standardMenuItemHeight,
                                   int& idealWidth,
                                   int& idealHeight) override
    {
        const auto& t = ThemeManager::get().theme();

        if (isSeparator)
        {
            idealWidth = 50;
            idealHeight = 7;
            return;
        }

        const juce::Font font(juce::FontOptions(t.sizeBody));
        idealHeight = standardMenuItemHeight > 0 ? standardMenuItemHeight : juce::roundToInt(t.sizeBody + 8.0f);
        idealWidth = font.getStringWidth(text) + 56;
    }

    void drawMenuBarBackground(juce::Graphics& g,
                               int width,
                               int height,
                               bool,
                               juce::MenuBarComponent&) override
    {
        const auto& t = ThemeManager::get().theme();
        g.setColour(t.headerBg.withAlpha(0.94f));
        g.fillRect(0, 0, width, height);
        g.setColour(t.surfaceEdge.withAlpha(0.75f));
        g.fillRect(0, height - 1, width, 1);
    }

    void drawMenuBarItem(juce::Graphics& g,
                         int width,
                         int height,
                         int,
                         const juce::String& itemText,
                         bool isMouseOverItem,
                         bool isMenuOpen,
                         bool,
                         juce::MenuBarComponent&) override
    {
        const auto& t = ThemeManager::get().theme();
        auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(2, 1);

        if (isMouseOverItem || isMenuOpen)
        {
            g.setColour(t.rowHover.withAlpha(0.9f));
            g.fillRoundedRectangle(bounds.toFloat(), juce::jmax(2.0f, t.radiusXs + 1.0f));
        }

        g.setColour((isMouseOverItem || isMenuOpen ? t.inkLight : t.inkMuted).withAlpha(0.95f));
        g.setFont(juce::FontOptions(t.sizeBody));
        g.drawFittedText(itemText, bounds, juce::Justification::centred, 1);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        const auto& t = ThemeManager::get().theme();
        auto bounds = label.getLocalBounds().toFloat();

        const auto bg = label.findColour(juce::Label::backgroundColourId);
        if (!bg.isTransparent())
        {
            g.setColour(bg);
            g.fillRoundedRectangle(bounds.reduced(0.5f), juce::jmax(1.0f, t.radiusSm));
        }

        const auto outline = label.findColour(juce::Label::outlineColourId);
        if (!outline.isTransparent())
        {
            g.setColour(outline);
            g.drawRoundedRectangle(bounds.reduced(0.5f), juce::jmax(1.0f, t.radiusSm), juce::jmax(0.6f, t.strokeSubtle));
        }

        auto textColour = label.findColour(juce::Label::textColourId);
        if (!label.isEnabled())
            textColour = t.inkGhost.withAlpha(0.68f);

        g.setColour(textColour);
        g.setFont(juce::FontOptions(label.getFont().getHeight() > 0.1f ? label.getFont().getHeight() : t.sizeBody));
        g.drawFittedText(label.getText(),
                         label.getBorderSize().subtractedFrom(label.getLocalBounds()),
                         label.getJustificationType(),
                         juce::jmax(1, static_cast<int>(bounds.getHeight() / 12.0f)));
    }

    void drawTabButton(juce::TabBarButton& button,
                       juce::Graphics& g,
                       bool isMouseOver,
                       bool isMouseDown) override
    {
        const auto& t = ThemeManager::get().theme();
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);

        auto fill = button.isFrontTab() ? t.controlOnBg : t.controlBg;
        if (isMouseOver)
            fill = fill.brighter(0.05f);
        if (isMouseDown)
            fill = fill.brighter(0.1f);

        g.setColour(fill);
        g.fillRoundedRectangle(bounds, juce::jmax(2.0f, t.radiusSm));

        auto border = t.surfaceEdge.withAlpha(0.82f);
        if (button.isFrontTab())
            border = border.interpolatedWith(t.accent, 0.55f);

        g.setColour(border);
        g.drawRoundedRectangle(bounds, juce::jmax(2.0f, t.radiusSm), juce::jmax(0.8f, t.strokeNormal));

        drawTabButtonText(button, g, isMouseOver, isMouseDown);
    }

    void drawTabButtonText(juce::TabBarButton& button,
                           juce::Graphics& g,
                           bool,
                           bool) override
    {
        const auto& t = ThemeManager::get().theme();
        g.setFont(juce::FontOptions(t.sizeLabel));
        g.setColour((button.isFrontTab() ? t.controlTextOn : t.controlText).withAlpha(0.95f));
        g.drawFittedText(button.getButtonText(), button.getTextArea().reduced(4, 0), juce::Justification::centred, 1);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::FontOptions(ThemeManager::get().theme().sizeBody);
    }
};
