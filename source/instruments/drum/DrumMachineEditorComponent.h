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

    struct VoiceRow
    {
        juce::Label name;
        juce::Slider tune;
        juce::Slider decay;
        juce::Slider level;
        float meter { 0.0f };
        std::array<float, 96> preview {};
    };

    void setupRow(VoiceRow& row, const juce::String& name, int voiceIndex);
    void refreshWavePreview(int voiceIndex);

    DrumMachineProcessor& processor;
    std::array<VoiceRow, DrumMachineProcessor::kVoiceCount> rows;
    juce::Slider master;
    juce::Label masterLabel;
};

} // namespace setle::instruments::drum
