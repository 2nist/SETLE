#include "DrumMachineEditorComponent.h"
#include "DrumVoiceParams.h"
#include "FmDrumVoice.h"

#include <cmath>

namespace setle::instruments::drum
{

DrumMachineEditorComponent::DrumMachineEditorComponent(DrumMachineProcessor& processorRef)
    : processor(processorRef)
{
    setOpaque(true);

    setupRow(rows[0], "Kick", 0);
    setupRow(rows[1], "Snare", 1);
    setupRow(rows[2], "HH C", 2);
    setupRow(rows[3], "HH O", 3);
    setupRow(rows[4], "Clap", 4);

    masterLabel.setText("Master", juce::dontSendNotification);
    masterLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
    addAndMakeVisible(masterLabel);

    master.setSliderStyle(juce::Slider::LinearHorizontal);
    master.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 16);
    master.setRange(0.0, 1.2, 0.01);
    master.setValue(processor.getMasterVolume(), juce::dontSendNotification);
    master.onValueChange = [this]
    {
        processor.setMasterVolume(static_cast<float>(master.getValue()));
    };
    addAndMakeVisible(master);

    startTimerHz(20);
}

void DrumMachineEditorComponent::setupRow(VoiceRow& row, const juce::String& name, int voiceIndex)
{
    row.name.setText(name, juce::dontSendNotification);
    row.name.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    row.name.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(row.name);

    for (auto* slider : { &row.tune, &row.decay, &row.level })
    {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 16);
        addAndMakeVisible(*slider);
    }

    row.tune.setRange(30.0, 2000.0, 1.0);
    row.decay.setRange(0.02, 1.0, 0.01);
    row.level.setRange(0.0, 1.0, 0.01);

    row.tune.setValue(processor.getVoiceTune(voiceIndex), juce::dontSendNotification);
    row.decay.setValue(processor.getVoiceDecay(voiceIndex), juce::dontSendNotification);
    row.level.setValue(processor.getVoiceLevel(voiceIndex), juce::dontSendNotification);

    row.tune.onValueChange = [this, voiceIndex, &row]
    {
        processor.setVoiceTune(voiceIndex, static_cast<float>(row.tune.getValue()));
    };
    row.decay.onValueChange = [this, voiceIndex, &row]
    {
        processor.setVoiceDecay(voiceIndex, static_cast<float>(row.decay.getValue()));
    };
    row.level.onValueChange = [this, voiceIndex, &row]
    {
        processor.setVoiceLevel(voiceIndex, static_cast<float>(row.level.getValue()));
    };
}

void DrumMachineEditorComponent::timerCallback()
{
    for (int i = 0; i < DrumMachineProcessor::kVoiceCount; ++i)
    {
        rows[static_cast<size_t>(i)].meter = processor.getVoiceMeter(i);
        refreshWavePreview(i);
    }
    repaint();
}

void DrumMachineEditorComponent::refreshWavePreview(int voiceIndex)
{
    if (voiceIndex < 0 || voiceIndex >= DrumMachineProcessor::kVoiceCount)
        return;

    auto& row = rows[static_cast<size_t>(voiceIndex)];

    DrumVoiceParams params;
    params.tuneHz = processor.getVoiceTune(voiceIndex);
    params.decaySeconds = processor.getVoiceDecay(voiceIndex);
    params.level = processor.getVoiceLevel(voiceIndex);

    FmDrumVoice previewVoice;
    previewVoice.setSampleRate(44100.0);
    previewVoice.trigger(params, 1.0f, false);

    float peak = 0.0001f;
    for (auto& v : row.preview)
    {
        v = previewVoice.renderSample();
        peak = juce::jmax(peak, std::abs(v));
    }

    for (auto& v : row.preview)
        v = v / peak;
}

void DrumMachineEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121417));

    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(4);
    for (int i = 0; i < DrumMachineProcessor::kVoiceCount; ++i)
    {
        auto row = area.removeFromTop(20);
        auto meter = juce::Rectangle<int>(row.getRight() - 42, row.getY() + 4, 36, 10);
        auto wave = juce::Rectangle<int>(meter.getX() - 78, row.getY() + 2, 72, 14);

        g.setColour(juce::Colour(0xff1f2b33));
        g.fillRect(wave);
        g.setColour(juce::Colour(0xff3e5867));
        g.drawRect(wave);

        juce::Path previewPath;
        const auto& preview = rows[static_cast<size_t>(i)].preview;
        const float midY = static_cast<float>(wave.getCentreY());
        const float halfH = static_cast<float>(wave.getHeight()) * 0.42f;
        for (size_t s = 0; s < preview.size(); ++s)
        {
            const float x = static_cast<float>(wave.getX()) + (static_cast<float>(s) / static_cast<float>(preview.size() - 1))
                                                       * static_cast<float>(wave.getWidth());
            const float y = midY - preview[s] * halfH;
            if (s == 0)
                previewPath.startNewSubPath(x, y);
            else
                previewPath.lineTo(x, y);
        }
        g.setColour(juce::Colour(0xff79b4e2));
        g.strokePath(previewPath, juce::PathStrokeType(1.0f));

        g.setColour(juce::Colour(0xff22313a));
        g.fillRect(meter);
        g.setColour(juce::Colour(0xff58a6d6));
        g.fillRect(meter.withWidth(static_cast<int>(meter.getWidth() * juce::jlimit(0.0f, 1.0f, rows[static_cast<size_t>(i)].meter))));
    }
}

void DrumMachineEditorComponent::resized()
{
    auto area = getLocalBounds().reduced(8);

    for (auto& row : rows)
    {
        auto r = area.removeFromTop(20);
        row.name.setBounds(r.removeFromLeft(48));
        row.tune.setBounds(r.removeFromLeft(96));
        row.decay.setBounds(r.removeFromLeft(86));
        row.level.setBounds(r.removeFromLeft(78));
    }

    area.removeFromTop(4);
    auto m = area.removeFromTop(20);
    masterLabel.setBounds(m.removeFromLeft(56));
    master.setBounds(m.removeFromLeft(240));
}

} // namespace setle::instruments::drum
