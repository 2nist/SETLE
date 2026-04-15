#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

#include "DrumMachineProcessor.h"

namespace setle::instruments::drum
{

class DrumMachineEditorComponent final : public juce::Component,
                                         private juce::Timer
{
public:
    explicit DrumMachineEditorComponent(DrumMachineProcessor& processorRef);
    ~DrumMachineEditorComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    struct VoiceVisual
    {
        juce::String name;
        int voiceIndex { 0 };
        int arcIndex { 0 };
        float angleDeg { 0.0f };
        float meter { 0.0f };
        float lastMeter { 0.0f };
        float bloom { 0.0f };
        float detonation { 0.0f };
    };

    struct MacroKnob
    {
        juce::String label;
        juce::Slider slider;
    };

    void setupKnob(MacroKnob& knob, const juce::String& label, int macroIndex);
    void syncKnobValuesFromProcessor();
    juce::Rectangle<float> getRadarBounds() const;
    juce::Rectangle<float> getFooterBounds() const;
    juce::Point<float> radarPointFor(const juce::Rectangle<float>& radarBounds,
                                     int arcIndex,
                                     float angleDegrees) const;

    DrumMachineProcessor& processor;
    std::array<VoiceVisual, DrumMachineProcessor::kVoiceCount> voices;
    std::array<MacroKnob, static_cast<size_t>(DrumMacroId::Count)> knobs;
    float arcSpecularNudge { 0.0f };
};

} // namespace setle::instruments::drum
