#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PolySynthProcessor.h"

namespace setle::instruments::polysynth
{

class PolySynthEditorComponent final : public juce::Component
{
public:
    explicit PolySynthEditorComponent(PolySynthProcessor& processorRef);
    ~PolySynthEditorComponent() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void syncFromProcessor();

    PolySynthProcessor& processor;

    juce::ToggleButton monoToggle { "Mono" };
    juce::Label patchLabel;
    juce::TextEditor patchName;

    juce::Slider attack;
    juce::Slider decay;
    juce::Slider sustain;
    juce::Slider release;
    juce::Slider cutoff;
    juce::Slider res;
    juce::Slider volume;

    juce::Label attackLabel;
    juce::Label decayLabel;
    juce::Label sustainLabel;
    juce::Label releaseLabel;
    juce::Label cutoffLabel;
    juce::Label resLabel;
    juce::Label volumeLabel;
};

} // namespace setle::instruments::polysynth
