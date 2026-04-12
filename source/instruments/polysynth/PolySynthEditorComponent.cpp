#include "PolySynthEditorComponent.h"

namespace setle::instruments::polysynth
{

namespace
{
void configureSlider(juce::Slider& s, double min, double max, double step)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    s.setRange(min, max, step);
}

void configureLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
}
} // namespace

PolySynthEditorComponent::PolySynthEditorComponent(PolySynthProcessor& processorRef)
    : processor(processorRef)
{
    setOpaque(true);

    addAndMakeVisible(monoToggle);
    monoToggle.onClick = [this]
    {
        processor.setMonoMode(monoToggle.getToggleState());
    };

    patchLabel.setText("Patch", juce::dontSendNotification);
    patchLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
    addAndMakeVisible(patchLabel);

    patchName.onReturnKey = [this]
    {
        processor.setPatchName(patchName.getText().trim());
    };
    patchName.onFocusLost = [this]
    {
        processor.setPatchName(patchName.getText().trim());
    };
    addAndMakeVisible(patchName);

    configureSlider(attack, 1.0, 2000.0, 1.0);
    configureSlider(decay, 1.0, 2000.0, 1.0);
    configureSlider(sustain, 0.0, 1.0, 0.01);
    configureSlider(release, 1.0, 4000.0, 1.0);
    configureSlider(cutoff, 40.0, 18000.0, 1.0);
    configureSlider(res, 0.0, 0.98, 0.01);
    configureSlider(volume, 0.0, 1.2, 0.01);

    configureLabel(attackLabel, "A");
    configureLabel(decayLabel, "D");
    configureLabel(sustainLabel, "S");
    configureLabel(releaseLabel, "R");
    configureLabel(cutoffLabel, "Cut");
    configureLabel(resLabel, "Res");
    configureLabel(volumeLabel, "Vol");

    for (auto* c : { (juce::Component*) &attack, &decay, &sustain, &release, &cutoff, &res, &volume,
                     &attackLabel, &decayLabel, &sustainLabel, &releaseLabel, &cutoffLabel, &resLabel, &volumeLabel })
        addAndMakeVisible(*c);

    attack.onValueChange = [this] { processor.setAttackMs(static_cast<float>(attack.getValue())); };
    decay.onValueChange = [this] { processor.setDecayMs(static_cast<float>(decay.getValue())); };
    sustain.onValueChange = [this] { processor.setSustain(static_cast<float>(sustain.getValue())); };
    release.onValueChange = [this] { processor.setReleaseMs(static_cast<float>(release.getValue())); };
    cutoff.onValueChange = [this] { processor.setCutoffHz(static_cast<float>(cutoff.getValue())); };
    res.onValueChange = [this] { processor.setResonance(static_cast<float>(res.getValue())); };
    volume.onValueChange = [this] { processor.setOutputGain(static_cast<float>(volume.getValue())); };

    syncFromProcessor();
}

void PolySynthEditorComponent::syncFromProcessor()
{
    monoToggle.setToggleState(processor.isMonoMode(), juce::dontSendNotification);
    patchName.setText(processor.getPatchName(), juce::dontSendNotification);
    attack.setValue(processor.getAttackMs(), juce::dontSendNotification);
    decay.setValue(processor.getDecayMs(), juce::dontSendNotification);
    sustain.setValue(processor.getSustain(), juce::dontSendNotification);
    release.setValue(processor.getReleaseMs(), juce::dontSendNotification);
    cutoff.setValue(processor.getCutoffHz(), juce::dontSendNotification);
    res.setValue(processor.getResonance(), juce::dontSendNotification);
    volume.setValue(processor.getOutputGain(), juce::dontSendNotification);
}

void PolySynthEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121417));
    g.setColour(juce::Colour(0xff2a333c));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
}

void PolySynthEditorComponent::resized()
{
    auto b = getLocalBounds().reduced(8);

    auto top = b.removeFromTop(26);
    monoToggle.setBounds(top.removeFromLeft(80));
    patchLabel.setBounds(top.removeFromLeft(50));
    patchName.setBounds(top.removeFromLeft(200));

    b.removeFromTop(4);
    auto row = b.removeFromTop(110);

    auto placeKnob = [&row](juce::Label& label, juce::Slider& slider)
    {
        auto col = row.removeFromLeft(70);
        label.setBounds(col.removeFromTop(14));
        slider.setBounds(col);
    };

    placeKnob(attackLabel, attack);
    placeKnob(decayLabel, decay);
    placeKnob(sustainLabel, sustain);
    placeKnob(releaseLabel, release);
    placeKnob(cutoffLabel, cutoff);
    placeKnob(resLabel, res);
    placeKnob(volumeLabel, volume);
}

} // namespace setle::instruments::polysynth
