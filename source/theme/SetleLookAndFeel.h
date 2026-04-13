#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ThemeManager.h"

class SetleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float,
                          float,
                          const juce::Slider::SliderStyle style,
                          juce::Slider&) override
    {
        const auto& t = ThemeManager::get().theme();
        const bool vertical = (style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical);
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(1.0f);

        g.setColour(t.controlBg);
        g.fillRoundedRectangle(bounds, t.sliderCornerRadius);

        g.setColour(t.sliderTrack);
        if (vertical)
        {
            auto fill = bounds.withY(sliderPos).withHeight(bounds.getBottom() - sliderPos);
            g.fillRoundedRectangle(fill, t.sliderCornerRadius);
        }
        else
        {
            auto fill = bounds.withWidth(sliderPos - bounds.getX());
            g.fillRoundedRectangle(fill, t.sliderCornerRadius);
        }

        g.setColour(t.surfaceEdge);
        g.drawRoundedRectangle(bounds, t.sliderCornerRadius, t.strokeNormal);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float startAngle,
                          float endAngle,
                          juce::Slider&) override
    {
        const auto& t = ThemeManager::get().theme();
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height).reduced(1.0f);
        const auto center = bounds.getCentre();
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

        g.setColour(t.controlBg);
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        juce::Path arc;
        arc.addCentredArc(center.x, center.y, radius - 3.0f, radius - 3.0f, 0.0f, startAngle, endAngle, true);
        g.setColour(t.surfaceEdge);
        g.strokePath(arc, juce::PathStrokeType(t.knobRingThickness));

        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        juce::Path fillArc;
        fillArc.addCentredArc(center.x, center.y, radius - 3.0f, radius - 3.0f, 0.0f, startAngle, angle, true);
        g.setColour(t.sliderTrack);
        g.strokePath(fillArc, juce::PathStrokeType(t.knobRingThickness));
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour&,
                              bool highlighted,
                              bool down) override
    {
        const auto& t = ThemeManager::get().theme();
        auto fill = button.getToggleState() ? t.controlOnBg : t.controlBg;
        if (highlighted)
            fill = fill.brighter(0.08f);
        if (down)
            fill = fill.brighter(0.12f);

        const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, t.btnCornerRadius);
        g.setColour(t.surfaceEdge.withAlpha(t.btnBorderStrength));
        g.drawRoundedRectangle(bounds, t.btnCornerRadius, t.strokeNormal);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool,
                        bool) override
    {
        const auto& t = ThemeManager::get().theme();
        g.setFont(juce::FontOptions(t.sizeLabel));
        g.setColour(button.getToggleState() ? t.controlTextOn : t.controlText);
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 2), juce::Justification::centred, 1);
    }

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool,
                      int,
                      int,
                      int,
                      int,
                      juce::ComboBox&) override
    {
        const auto& t = ThemeManager::get().theme();
        const auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(0.5f);
        g.setColour(t.controlBg);
        g.fillRoundedRectangle(bounds, t.btnCornerRadius);
        g.setColour(t.surfaceEdge);
        g.drawRoundedRectangle(bounds, t.btnCornerRadius, t.strokeNormal);
    }

    void drawScrollbar(juce::Graphics& g,
                       juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical,
                       int thumbStartPosition,
                       int thumbSize,
                       bool isMouseOver,
                       bool isMouseDown) override
    {
        const auto& t = ThemeManager::get().theme();
        const auto track = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
        g.setColour(t.surface2.withAlpha(0.85f));
        g.fillRect(track);

        juce::Rectangle<float> thumb;
        if (isScrollbarVertical)
            thumb = { (float)x + 1.0f, (float)thumbStartPosition, (float)width - 2.0f, (float)thumbSize };
        else
            thumb = { (float)thumbStartPosition, (float)y + 1.0f, (float)thumbSize, (float)height - 2.0f };

        auto thumbColour = t.sliderThumb.withAlpha(0.70f);
        if (isMouseOver)
            thumbColour = thumbColour.brighter(0.08f);
        if (isMouseDown)
            thumbColour = thumbColour.brighter(0.12f);
        g.setColour(thumbColour);
        g.fillRoundedRectangle(thumb, 3.0f);

        g.setColour(t.surfaceEdge.withAlpha(0.8f));
        g.drawRect(x, y, width, height);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::FontOptions(ThemeManager::get().theme().sizeBody);
    }
};
