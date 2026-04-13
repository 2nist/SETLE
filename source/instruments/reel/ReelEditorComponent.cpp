#include "ReelEditorComponent.h"

namespace setle::instruments::reel
{
namespace
{
void configureSmallSlider(juce::Slider& s, double min, double max, double step)
{
    s.setSliderStyle(juce::Slider::LinearHorizontal);
    s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 14);
    s.setRange(min, max, step);
}

void configureSmallLabel(juce::Label& l, const juce::String& text)
{
    l.setText(text, juce::dontSendNotification);
    l.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
    l.setJustificationType(juce::Justification::centredLeft);
}
} // namespace

ReelEditorComponent::ReelEditorComponent(::ReelProcessor& processorRef)
    : processor(processorRef)
{
    setOpaque(true);

    configureSmallLabel(modeLabel, "Mode");
    configureSmallLabel(pitchLabel, "Pitch");
    configureSmallLabel(volumeLabel, "Vol");

    modeSelector.addItem("Sampler", 1);
    modeSelector.addItem("Loop", 2);
    modeSelector.addItem("Granular", 3);
    modeSelector.onChange = [this]
    {
        switch (modeSelector.getSelectedId())
        {
            case 1: processor.setMode(ReelMode::sample); break;
            case 2: processor.setMode(ReelMode::loop); break;
            case 3: processor.setMode(ReelMode::grain); break;
            default: break;
        }
    };

    loopToggle.onClick = [this]
    {
        processor.setParam("sample.oneshot", loopToggle.getToggleState() ? 0.0f : 1.0f);
    };

    configureSmallSlider(pitchSlider, -24.0, 24.0, 1.0);
    configureSmallSlider(volumeSlider, 0.0, 1.0, 0.01);
    pitchSlider.onValueChange = [this] { processor.setParam("loop.pitch", static_cast<float>(pitchSlider.getValue())); };
    volumeSlider.onValueChange = [this] { processor.setParam("out.level", static_cast<float>(volumeSlider.getValue())); };

    addAndMakeVisible(modeLabel);
    addAndMakeVisible(modeSelector);
    addAndMakeVisible(loopToggle);
    addAndMakeVisible(pitchLabel);
    addAndMakeVisible(pitchSlider);
    addAndMakeVisible(volumeLabel);
    addAndMakeVisible(volumeSlider);

    syncFromProcessor();
    startTimerHz(12);
}

void ReelEditorComponent::timerCallback()
{
    syncFromProcessor();
    repaint();
}

juce::Rectangle<int> ReelEditorComponent::getWaveArea() const
{
    auto area = getLocalBounds().reduced(6);
    return area.removeFromTop(32);
}

void ReelEditorComponent::syncFromProcessor()
{
    const auto& params = processor.getParams();

    switch (params.mode)
    {
        case ReelMode::sample: modeSelector.setSelectedId(1, juce::dontSendNotification); break;
        case ReelMode::loop: modeSelector.setSelectedId(2, juce::dontSendNotification); break;
        case ReelMode::grain: modeSelector.setSelectedId(3, juce::dontSendNotification); break;
        case ReelMode::slice: modeSelector.setSelectedId(1, juce::dontSendNotification); break;
    }

    loopToggle.setToggleState(!params.sample.oneshot, juce::dontSendNotification);
    pitchSlider.setValue(params.loop.pitch, juce::dontSendNotification);
    volumeSlider.setValue(params.out.level, juce::dontSendNotification);
}

void ReelEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121417));

    auto wave = getWaveArea();
    g.setColour(juce::Colour(0xff1c2530));
    g.fillRoundedRectangle(wave.toFloat(), 3.0f);
    g.setColour(juce::Colour(0xff33424e));
    g.drawRoundedRectangle(wave.toFloat().reduced(0.5f), 3.0f, 1.0f);

    const auto& buffer = processor.getBuffer().getBuffer();
    if (buffer.getNumSamples() > 1 && buffer.getNumChannels() > 0)
    {
        const auto* data = buffer.getReadPointer(0);
        const int n = buffer.getNumSamples();

        juce::Path path;
        const float midY = static_cast<float>(wave.getCentreY());
        const float h = static_cast<float>(wave.getHeight()) * 0.42f;
        for (int x = 0; x < wave.getWidth(); ++x)
        {
            const int idx = juce::jlimit(0, n - 1, (x * n) / juce::jmax(1, wave.getWidth() - 1));
            const float y = midY - data[idx] * h;
            const float px = static_cast<float>(wave.getX() + x);
            if (x == 0)
                path.startNewSubPath(px, y);
            else
                path.lineTo(px, y);
        }

        g.setColour(juce::Colour(0xff7bb0d8));
        g.strokePath(path, juce::PathStrokeType(1.0f));
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("No sample loaded", wave, juce::Justification::centred);
    }

    const auto& params = processor.getParams();
    const int startX = wave.getX() + juce::roundToInt(params.play.start * static_cast<float>(wave.getWidth()));
    const int endX = wave.getX() + juce::roundToInt(params.play.end * static_cast<float>(wave.getWidth()));

    g.setColour(juce::Colour(0x66eea67a));
    g.fillRect(juce::Rectangle<int>(startX, wave.getY(), juce::jmax(1, endX - startX), wave.getHeight()));
    g.setColour(juce::Colour(0xffeea67a));
    g.drawVerticalLine(startX, static_cast<float>(wave.getY()), static_cast<float>(wave.getBottom()));
    g.drawVerticalLine(endX, static_cast<float>(wave.getY()), static_cast<float>(wave.getBottom()));
}

void ReelEditorComponent::resized()
{
    auto area = getLocalBounds().reduced(6);
    area.removeFromTop(34);
    area.removeFromTop(4);

    auto row1 = area.removeFromTop(18);
    modeLabel.setBounds(row1.removeFromLeft(36));
    modeSelector.setBounds(row1.removeFromLeft(92));
    loopToggle.setBounds(row1.removeFromLeft(60));

    area.removeFromTop(2);
    auto row2 = area.removeFromTop(18);
    pitchLabel.setBounds(row2.removeFromLeft(36));
    pitchSlider.setBounds(row2.removeFromLeft(146));
    volumeLabel.setBounds(row2.removeFromLeft(28));
    volumeSlider.setBounds(row2.removeFromLeft(120));
}

void ReelEditorComponent::updateStartFromX(int x)
{
    const auto wave = getWaveArea();
    const auto norm = juce::jlimit(0.0f, 1.0f, static_cast<float>(x - wave.getX()) / static_cast<float>(juce::jmax(1, wave.getWidth())));
    const auto end = processor.getParam("play.end");
    processor.setParam("play.start", juce::jmin(norm, end - 0.01f));
}

void ReelEditorComponent::updateEndFromX(int x)
{
    const auto wave = getWaveArea();
    const auto norm = juce::jlimit(0.0f, 1.0f, static_cast<float>(x - wave.getX()) / static_cast<float>(juce::jmax(1, wave.getWidth())));
    const auto start = processor.getParam("play.start");
    processor.setParam("play.end", juce::jmax(norm, start + 0.01f));
}

void ReelEditorComponent::mouseDown(const juce::MouseEvent& event)
{
    const auto wave = getWaveArea();
    if (!wave.contains(event.position.toInt()))
        return;

    const auto start = processor.getParam("play.start");
    const auto end = processor.getParam("play.end");
    const auto startX = wave.getX() + juce::roundToInt(start * static_cast<float>(wave.getWidth()));
    const auto endX = wave.getX() + juce::roundToInt(end * static_cast<float>(wave.getWidth()));

    if (std::abs(event.x - startX) < 8)
        dragHandle = DragHandle::start;
    else if (std::abs(event.x - endX) < 8)
        dragHandle = DragHandle::end;
    else
        dragHandle = (event.x < (startX + endX) / 2) ? DragHandle::start : DragHandle::end;

    if (dragHandle == DragHandle::start)
        updateStartFromX(event.x);
    else if (dragHandle == DragHandle::end)
        updateEndFromX(event.x);
}

void ReelEditorComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (dragHandle == DragHandle::start)
        updateStartFromX(event.x);
    else if (dragHandle == DragHandle::end)
        updateEndFromX(event.x);
}

void ReelEditorComponent::mouseUp(const juce::MouseEvent&)
{
    dragHandle = DragHandle::none;
    syncFromProcessor();
}

} // namespace setle::instruments::reel
