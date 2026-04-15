#include "QuesoLogoComponent.h"

#include <cmath>

namespace setle::ui
{

QuesoLogoComponent::QuesoLogoComponent()
{
    setOpaque(true);
    ThemeManager::get().addListener(this);
    startTimerHz(60);
}

QuesoLogoComponent::~QuesoLogoComponent()
{
    stopTimer();
    ThemeManager::get().removeListener(this);
}

void QuesoLogoComponent::setBpm(double newBpm)
{
    bpm = juce::jlimit(20.0, 300.0, newBpm);
}

void QuesoLogoComponent::setToneDetectorState(float frequencyHz, float normalizedEnergy)
{
    dominantFrequencyHz = juce::jmax(0.0f, frequencyHz);
    toneEnergy = juce::jlimit(0.0f, 1.0f, normalizedEnergy);
    glowFactor = juce::jlimit(0.0f, 1.0f, toneEnergy * 0.7f + (dominantFrequencyHz > 20.0f ? 0.3f : 0.0f));
}

void QuesoLogoComponent::setStructureProgress(float normalizedProgress)
{
    structureProgress = juce::jlimit(0.0f, 1.0f, normalizedProgress);
}

void QuesoLogoComponent::setMeltFactor(float normalizedMeltFactor)
{
    meltFactor = juce::jlimit(0.0f, 1.0f, normalizedMeltFactor);
}

void QuesoLogoComponent::paint(juce::Graphics& g)
{
    auto& themeManager = ThemeManager::get();
    auto area = getLocalBounds().toFloat();
    g.fillAll(themeManager.getColor(ThemeManager::ThemeKey::windowBackground));
    drawChassis(g, area);
    drawGlossyNameplate(g, area.reduced(6.0f, 3.0f));
    drawBackronym(g, area.reduced(8.0f, 4.0f));
}

void QuesoLogoComponent::resized()
{
}

void QuesoLogoComponent::themeChanged()
{
    repaint();
}

void QuesoLogoComponent::timerCallback()
{
    const auto pulsesPerSecond = static_cast<float>(juce::jmax(0.5, bpm / 60.0));
    pulsePhase += pulsesPerSecond * 0.09f;
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    pulseValue = 0.5f + 0.5f * std::sin(pulsePhase);
    glowFactor = juce::jlimit(0.0f, 1.0f, glowFactor * 0.82f + toneEnergy * 0.18f);
    repaint();
}

void QuesoLogoComponent::drawChassis(juce::Graphics& g, const juce::Rectangle<float>& area)
{
    auto& themeManager = ThemeManager::get();
    const auto& theme = themeManager.theme();
    const auto windowBackground = themeManager.getColor(ThemeManager::ThemeKey::windowBackground);
    auto chassis = windowBackground.interpolatedWith(themeManager.getColor(ThemeManager::ThemeKey::surfaceLow), 0.22f)
                                   .interpolatedWith(themeManager.getColor(ThemeManager::ThemeKey::surfaceVariant), 0.30f);
    setle::theme::drawChassis(g, area, chassis, theme, 7.0f);
}

void QuesoLogoComponent::drawGlossyNameplate(juce::Graphics& g, const juce::Rectangle<float>& area)
{
    auto& themeManager = ThemeManager::get();
    const auto& theme = themeManager.theme();
    const auto meltColors = themeManager.getMeltColors();
    auto logoPath = buildLogoPath(area);
    const auto b = logoPath.getBounds();

    juce::DropShadow shadow(themeManager.getColor(ThemeManager::ThemeKey::inkDark).withAlpha(0.58f), 10, { 0, 4 });
    shadow.drawForPath(g, logoPath);

    juce::ColourGradient melt(meltColors.top, b.getX(), b.getY(),
                              meltColors.bottom, b.getX(), b.getBottom(), false);
    melt.addColour(0.42, meltColors.top.brighter(0.3f));
    melt.addColour(0.74, meltColors.top.brighter(0.6f));
    g.setGradientFill(melt);
    g.fillPath(logoPath);

    g.setColour(meltColors.top.darker(0.4f).withAlpha(0.65f));
    g.strokePath(logoPath, juce::PathStrokeType(1.6f));

    // Thin rim on top edge.
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.55f));
    g.saveState();
    g.reduceClipRegion(b.withHeight(b.getHeight() * 0.33f).toNearestInt());
    g.strokePath(logoPath, juce::PathStrokeType(1.0f));
    g.restoreState();

    // Wet look specular sweep.
    juce::Path gloss;
    gloss.startNewSubPath(b.getX() + b.getWidth() * 0.05f, b.getY() + b.getHeight() * 0.20f);
    gloss.quadraticTo(b.getCentreX(), b.getY() - 3.0f, b.getRight() - b.getWidth() * 0.03f, b.getY() + b.getHeight() * 0.25f);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.16f));
    g.strokePath(gloss, juce::PathStrokeType(2.0f));

    // Focused q drip: primary elongated melt point with tone-reactive bloom.
    const auto meltExtension = 12.0f * meltFactor;
    const auto dripStem = juce::Rectangle<float>(b.getX() + b.getWidth() * 0.155f,
                                                 b.getBottom() - 6.0f,
                                                 14.0f,
                                                 area.getBottom() - b.getBottom() - 13.0f + meltExtension);
    juce::Path drip;
    drip.startNewSubPath(dripStem.getX() + dripStem.getWidth() * 0.45f, dripStem.getY());
    drip.cubicTo(dripStem.getX() + dripStem.getWidth() * 0.95f, dripStem.getY() + dripStem.getHeight() * 0.18f,
                 dripStem.getX() + dripStem.getWidth() * 0.65f, dripStem.getBottom() - 12.0f,
                 dripStem.getCentreX(), dripStem.getBottom());
    drip.cubicTo(dripStem.getX() + dripStem.getWidth() * 0.35f, dripStem.getBottom() - 12.0f,
                 dripStem.getX() + dripStem.getWidth() * 0.08f, dripStem.getY() + dripStem.getHeight() * 0.24f,
                 dripStem.getX() + dripStem.getWidth() * 0.24f, dripStem.getY());
    drip.closeSubPath();
    g.setGradientFill(melt);
    g.fillPath(drip);
    g.setColour(meltColors.top.darker(0.4f).withAlpha(0.65f));
    g.strokePath(drip, juce::PathStrokeType(1.2f));

    auto dropletBounds = juce::Rectangle<float>(dripStem.getCentreX() - 13.0f,
                                                dripStem.getBottom() - 2.0f,
                                                26.0f,
                                                30.0f);
    juce::Path droplet;
    droplet.addEllipse(dropletBounds);
    g.setGradientFill(melt);
    g.fillPath(droplet);
    setle::theme::drawIndicatorGlow(g,
                                    dropletBounds.getCentre(),
                                    9.0f + 2.0f * pulseValue + 2.0f * meltFactor,
                                    themeManager.getColor(ThemeManager::ThemeKey::secondaryAccent),
                                    theme,
                                    juce::jlimit(0.0f, 1.0f, glowFactor * 0.65f + 0.25f * pulseValue + 0.35f * meltFactor),
                                    dominantFrequencyHz > 20.0f);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.24f));
    g.fillEllipse(dropletBounds.getX() + 6.0f, dropletBounds.getY() + 4.0f, 7.0f, 10.0f);

    auto progressRing = juce::Rectangle<float>(b.getX() + b.getWidth() * 0.72f,
                                               b.getY() + b.getHeight() * 0.20f,
                                               b.getHeight() * 0.58f,
                                               b.getHeight() * 0.58f);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::surfaceVariant).withAlpha(0.55f));
    g.fillEllipse(progressRing);

    juce::Path progressFill;
    progressFill.addPieSegment(progressRing, -juce::MathConstants<float>::halfPi,
                               -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::twoPi * structureProgress,
                               0.42);
    juce::ColourGradient fillGradient(meltColors.top.withAlpha(0.85f), progressRing.getCentreX(), progressRing.getY(),
                                      meltColors.top.withAlpha(0.75f), progressRing.getCentreX(), progressRing.getBottom(), false);
    g.setGradientFill(fillGradient);
    g.fillPath(progressFill);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.12f));
    g.fillEllipse(progressRing.reduced(progressRing.getWidth() * 0.24f));
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.22f));
    g.drawEllipse(progressRing, 1.2f);
}

void QuesoLogoComponent::drawBackronym(juce::Graphics& g, const juce::Rectangle<float>& area)
{
    auto& themeManager = ThemeManager::get();
    auto laneArea = area;
    auto lane = laneArea.removeFromBottom(11.0f);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.58f));
    g.setFont(juce::FontOptions(9.0f));
    g.drawFittedText("Quick Utility for Extraction of Sound Objects", lane.toNearestInt(), juce::Justification::centredLeft, 1);
}

juce::Path QuesoLogoComponent::buildLogoPath(const juce::Rectangle<float>& area) const
{
    juce::GlyphArrangement glyphs;
    auto options = juce::FontOptions("Segoe Script", area.getHeight() * 0.78f, juce::Font::bold)
                       .withFallbacks({ "Brush Script MT", "Segoe UI", juce::Font::getDefaultSansSerifFontName() })
                       .withHorizontalScale(1.08f);
    juce::Font titleFont(options);
    glyphs.addLineOfText(titleFont, "queso", area.getX() + 12.0f, area.getY() + area.getHeight() * 0.72f);

    juce::Path path;
    glyphs.createPath(path);

    auto bounds = path.getBounds();
    const auto targetWidth = area.getWidth() * 0.90f;
    const auto targetHeight = area.getHeight() * 0.72f;
    const auto scale = juce::jmin(targetWidth / bounds.getWidth(), targetHeight / bounds.getHeight());
    juce::AffineTransform transform = juce::AffineTransform::scale(scale, scale)
        .sheared(-0.08f, 0.0f)
        .translated(area.getX() + 8.0f - bounds.getX() * scale,
                    area.getY() + 8.0f - bounds.getY() * scale);
    auto logoPath = path.createPathWithRoundedCorners(7.0f);
    logoPath.applyTransform(transform);
    return logoPath;
}

} // namespace setle::ui
