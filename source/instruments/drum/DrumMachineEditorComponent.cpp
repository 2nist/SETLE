#include "DrumMachineEditorComponent.h"

#include "../../theme/ThemeManager.h"
#include "../../theme/ThemeStyleHelpers.h"

#include <cmath>

namespace setle::instruments::drum
{

namespace
{
constexpr int kArcCount = 5;
}

DrumMachineEditorComponent::DrumMachineEditorComponent(DrumMachineProcessor& processorRef)
    : processor(processorRef)
{
    setOpaque(false);

    voices[0] = { "Kick",      static_cast<int>(DrumVoiceId::Kick),      0, 210.0f };
    voices[1] = { "Sub",       static_cast<int>(DrumVoiceId::Sub),       1, 330.0f };
    voices[2] = { "Snare",     static_cast<int>(DrumVoiceId::Snare),     2,  34.0f };
    voices[3] = { "Perc",      static_cast<int>(DrumVoiceId::Perc),      3, 152.0f };
    voices[4] = { "HH Closed", static_cast<int>(DrumVoiceId::HatClosed), 4, 250.0f };
    voices[5] = { "HH Open",   static_cast<int>(DrumVoiceId::HatOpen),   4, 292.0f };
    voices[6] = { "Clap",      static_cast<int>(DrumVoiceId::Clap),      3,  96.0f };

    setupKnob(knobs[static_cast<size_t>(DrumMacroId::Thump)], "THUMP", static_cast<int>(DrumMacroId::Thump));
    setupKnob(knobs[static_cast<size_t>(DrumMacroId::Sizzle)], "SIZZLE", static_cast<int>(DrumMacroId::Sizzle));
    setupKnob(knobs[static_cast<size_t>(DrumMacroId::Choke)], "CHOKE", static_cast<int>(DrumMacroId::Choke));
    setupKnob(knobs[static_cast<size_t>(DrumMacroId::Texture)], "TEXTURE", static_cast<int>(DrumMacroId::Texture));
    setupKnob(knobs[static_cast<size_t>(DrumMacroId::Arc)], "ARC", static_cast<int>(DrumMacroId::Arc));
    setupKnob(knobs[static_cast<size_t>(DrumMacroId::Gloss)], "GLOSS", static_cast<int>(DrumMacroId::Gloss));

    syncKnobValuesFromProcessor();
    startTimerHz(30);
}

void DrumMachineEditorComponent::setupKnob(MacroKnob& knob, const juce::String& label, int macroIndex)
{
    knob.label = label;
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.slider.setRange(0.0, 1.0, 0.001);
    knob.slider.onValueChange = [this, macroIndex, &knob]
    {
        processor.setMacro(macroIndex, static_cast<float>(knob.slider.getValue()));
    };
    addAndMakeVisible(knob.slider);
}

void DrumMachineEditorComponent::syncKnobValuesFromProcessor()
{
    for (int i = 0; i < static_cast<int>(DrumMacroId::Count); ++i)
        knobs[static_cast<size_t>(i)].slider.setValue(processor.getMacro(i), juce::dontSendNotification);
}

juce::Rectangle<float> DrumMachineEditorComponent::getRadarBounds() const
{
    auto area = getLocalBounds().toFloat().reduced(6.0f);
    area.removeFromBottom(54.0f);
    return area;
}

juce::Rectangle<float> DrumMachineEditorComponent::getFooterBounds() const
{
    auto area = getLocalBounds().toFloat().reduced(6.0f);
    return area.removeFromBottom(50.0f);
}

juce::Point<float> DrumMachineEditorComponent::radarPointFor(const juce::Rectangle<float>& radarBounds,
                                                             int arcIndex,
                                                             float angleDegrees) const
{
    const auto centre = radarBounds.getCentre();
    const auto maxRadius = juce::jmin(radarBounds.getWidth(), radarBounds.getHeight()) * 0.42f;
    const auto arcNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>(arcIndex) / static_cast<float>(kArcCount - 1));
    const auto radius = maxRadius * (0.33f + arcNorm * 0.64f);
    const auto radians = juce::degreesToRadians(angleDegrees - 90.0f);
    return { centre.x + std::cos(radians) * radius, centre.y + std::sin(radians) * radius };
}

void DrumMachineEditorComponent::timerCallback()
{
    for (auto& voice : voices)
    {
        voice.meter = processor.getVoiceMeter(voice.voiceIndex);
        if (voice.meter > voice.lastMeter + 0.18f && voice.meter > 0.12f)
        {
            voice.bloom = juce::jmax(voice.bloom, voice.meter);
            voice.detonation = 1.0f;
        }

        voice.lastMeter = voice.meter * 0.9f + voice.lastMeter * 0.1f;
        voice.bloom *= 0.88f;
        voice.detonation *= 0.84f;
    }

    const auto arcValue = processor.getMacro(static_cast<int>(DrumMacroId::Arc));
    arcSpecularNudge += (arcValue - arcSpecularNudge) * 0.20f;
    repaint();
}

void DrumMachineEditorComponent::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    auto body = getLocalBounds().toFloat().reduced(2.0f);
    setle::theme::drawChassis(g,
                              body,
                              theme.windowBackground.interpolatedWith(theme.surfaceVariant, 0.62f),
                              theme,
                              8.0f);

    auto radarBounds = getRadarBounds();
    auto footerBounds = getFooterBounds();

    const auto brushedPlate = theme.surfaceLow.interpolatedWith(theme.surfaceVariant, 0.65f);
    setle::theme::drawChassis(g, radarBounds, brushedPlate, theme, 8.0f);

    for (int y = static_cast<int>(radarBounds.getY()) + 2; y < static_cast<int>(radarBounds.getBottom()) - 2; y += 2)
    {
        const auto alpha = ((y % 7) == 0 ? 0.06f : 0.03f);
        g.setColour(theme.inkLight.withAlpha(alpha));
        g.drawHorizontalLine(y, radarBounds.getX() + 4.0f, radarBounds.getRight() - 4.0f);
    }

    const auto centre = radarBounds.getCentre();
    const auto maxRadius = juce::jmin(radarBounds.getWidth(), radarBounds.getHeight()) * 0.42f;
    const auto ringColor = theme.secondaryAccent.interpolatedWith(theme.signalAudio, 0.5f);
    for (int arc = 0; arc < kArcCount; ++arc)
    {
        const auto arcNorm = static_cast<float>(arc) / static_cast<float>(kArcCount - 1);
        const auto radius = maxRadius * (0.33f + arcNorm * 0.64f);
        g.setColour(ringColor.withAlpha(0.56f - arcNorm * 0.16f));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.3f);
    }

    for (const auto& voice : voices)
    {
        const auto p = radarPointFor(radarBounds, voice.arcIndex, voice.angleDeg + (voice.arcIndex == 4 ? arcSpecularNudge * 6.0f : 0.0f));
        const auto cobalt = theme.focusOutline.interpolatedWith(theme.signalAudio, 0.35f);
        const auto saffronBloom = theme.primaryAccent.interpolatedWith(theme.signalMidi, 0.5f);

        if (voice.detonation > 0.01f)
        {
            setle::theme::drawIndicatorGlow(g,
                                            p,
                                            5.5f + voice.bloom * 4.0f,
                                            saffronBloom,
                                            theme,
                                            juce::jlimit(0.0f, 1.0f, voice.detonation),
                                            true);
        }

        const auto beadRadius = 2.6f + voice.meter * 2.8f;
        g.setColour(cobalt.withAlpha(0.88f));
        g.fillEllipse(p.x - beadRadius, p.y - beadRadius, beadRadius * 2.0f, beadRadius * 2.0f);
        g.setColour(theme.inkLight.withAlpha(0.62f));
        g.fillEllipse(p.x - beadRadius * 0.34f, p.y - beadRadius * 0.68f, beadRadius * 0.66f, beadRadius * 0.66f);
    }

    setle::theme::drawGlassPanel(g,
                                 radarBounds,
                                 theme.secondaryAccent.interpolatedWith(theme.surfaceLow, 0.78f),
                                 theme,
                                 true,
                                 8.0f);

    const auto specNudge = (arcSpecularNudge - 0.5f) * 14.0f;
    juce::Path specularPath;
    specularPath.startNewSubPath(radarBounds.getX() + radarBounds.getWidth() * 0.15f + specNudge,
                                 radarBounds.getY() + radarBounds.getHeight() * 0.20f);
    specularPath.quadraticTo(radarBounds.getCentreX() + specNudge * 0.6f,
                             radarBounds.getY() + radarBounds.getHeight() * 0.10f,
                             radarBounds.getRight() - radarBounds.getWidth() * 0.20f + specNudge,
                             radarBounds.getY() + radarBounds.getHeight() * 0.30f);
    g.setColour(theme.inkLight.withAlpha(0.22f));
    g.strokePath(specularPath, juce::PathStrokeType(2.0f));

    setle::theme::drawFuzzyInsetPanel(g, footerBounds, theme.surfaceVariant, theme, true, 8.0f);

    g.setColour(theme.inkLight.withAlpha(0.86f));
    g.setFont(juce::FontOptions(9.5f).withStyle("Bold"));
    for (const auto& knob : knobs)
    {
        auto labelBounds = knob.slider.getBounds().translated(0, -1);
        labelBounds.setY(footerBounds.getBottom() - 14.0f);
        labelBounds.setHeight(12.0f);
        g.drawFittedText(knob.label, labelBounds.toNearestInt(), juce::Justification::centred, 1);
    }
}

void DrumMachineEditorComponent::resized()
{
    auto footer = getFooterBounds().reduced(6.0f, 2.0f);
    const int knobCount = static_cast<int>(knobs.size());
    const auto slotW = footer.getWidth() / juce::jmax(1, knobCount);

    for (int i = 0; i < knobCount; ++i)
    {
        auto x = footer.getX() + static_cast<float>(i * slotW);
        auto knobBounds = juce::Rectangle<float>(x + 1.0f, footer.getY(), static_cast<float>(slotW) - 2.0f, footer.getHeight() - 14.0f);
        knobs[static_cast<size_t>(i)].slider.setBounds(knobBounds.toNearestInt());
    }
}

} // namespace setle::instruments::drum
