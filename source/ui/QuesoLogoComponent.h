#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../theme/ThemeManager.h"
#include "../theme/ThemeStyleHelpers.h"

namespace setle::ui
{

class QuesoLogoComponent final : public juce::Component,
                                 private juce::Timer,
                                 public ThemeManager::Listener
{
public:
    QuesoLogoComponent();
    ~QuesoLogoComponent() override;

    void setBpm(double bpm);
    void setToneDetectorState(float dominantFrequencyHz, float normalizedEnergy);
    void setStructureProgress(float normalizedProgress);
    void setMeltFactor(float normalizedMeltFactor);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void themeChanged() override;

private:
    void timerCallback() override;

    void drawChassis(juce::Graphics& g, const juce::Rectangle<float>& area);
    void drawGlossyNameplate(juce::Graphics& g, const juce::Rectangle<float>& area);
    void drawBackronym(juce::Graphics& g, const juce::Rectangle<float>& area);
    juce::Path buildLogoPath(const juce::Rectangle<float>& area) const;

    double bpm { 120.0 };
    float pulsePhase { 0.0f };
    float pulseValue { 0.0f };
    float dominantFrequencyHz { 0.0f };
    float toneEnergy { 0.0f };
    float glowFactor { 0.0f };
    float structureProgress { 0.0f };
    float meltFactor { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuesoLogoComponent)
};

} // namespace setle::ui
