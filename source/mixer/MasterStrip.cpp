#include "MasterStrip.h"

namespace setle::mixer
{
namespace
{
float dbToLinear(float db)
{
    return juce::Decibels::decibelsToGain(db, -100.0f);
}

juce::Colour meterColourForLevel(float linear)
{
    const auto db = juce::Decibels::gainToDecibels(juce::jmax(0.000001f, linear), -100.0f);
    if (db >= -3.0f)
        return juce::Colour(0xffcc3333);
    if (db >= -6.0f)
        return juce::Colour(0xffd4aa00);
    return juce::Colour(0xff44aa44);
}
} // namespace

MasterStrip::MasterStrip(te::Edit& e)
    : edit(e)
{
    addAndMakeVisible(nameLabel);
    addAndMakeVisible(volumeFader);
    addAndMakeVisible(panKnob);

    nameLabel.setText("MASTER", juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);

    volumeFader.setSliderStyle(juce::Slider::LinearVertical);
    volumeFader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeFader.setRange(-100.0, 6.0, 0.1);
    if (auto vp = edit.getMasterVolumePlugin())
        volumeFader.setValue(vp->getVolumeDb(), juce::dontSendNotification);
    volumeFader.onValueChange = [this]
    {
        if (auto vp = edit.getMasterVolumePlugin())
            vp->setVolumeDb(static_cast<float>(volumeFader.getValue()));
        volumeFader.setTooltip(juce::String(volumeFader.getValue(), 1) + " dB");
    };

    panKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    panKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panKnob.setRange(-1.0, 1.0, 0.01);
    if (auto vp = edit.getMasterVolumePlugin())
        panKnob.setValue(vp->getPan(), juce::dontSendNotification);
    panKnob.onValueChange = [this]
    {
        if (auto vp = edit.getMasterVolumePlugin())
            vp->setPan(static_cast<float>(panKnob.getValue()));
    };

    for (auto* p : edit.getMasterPluginList().getPlugins())
    {
        if (auto* lm = dynamic_cast<te::LevelMeterPlugin*>(p))
        {
            meterPlugin = lm;
            meterPlugin->measurer.addClient(meterClient);
            break;
        }
    }

    startTimerHz(30);
}

MasterStrip::~MasterStrip()
{
    if (meterPlugin != nullptr)
        meterPlugin->measurer.removeClient(meterClient);
}

void MasterStrip::resized()
{
    auto b = getLocalBounds().reduced(6);
    b.removeFromTop(8);
    nameLabel.setBounds(b.removeFromTop(20));
    b.removeFromTop(8);
    b.removeFromTop(80);
    volumeFader.setBounds(b.removeFromTop(120).reduced(14, 0));
    panKnob.setBounds(b.removeFromTop(40).reduced(20, 2));
}

void MasterStrip::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff14181d));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(juce::Colour(0xff4b5763));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);

    auto clipLed = juce::Rectangle<int>(8, 6, 10, 10);
    g.setColour(clipLatched ? juce::Colour(0xffd02929) : juce::Colour(0xff3f2323));
    g.fillRect(clipLed);

    auto meterBounds = getLocalBounds().reduced(10).withTrimmedTop(36).removeFromTop(80);
    juce::Rectangle<int> leftBar(meterBounds.getCentreX() - 5, meterBounds.getY(), 4, meterBounds.getHeight());
    juce::Rectangle<int> rightBar(meterBounds.getCentreX() + 1, meterBounds.getY(), 4, meterBounds.getHeight());

    auto drawMeter = [&g](juce::Rectangle<int> area, float level, float hold)
    {
        g.setColour(juce::Colour(0xff232c32));
        g.fillRect(area);

        const int fill = juce::roundToInt(static_cast<float>(area.getHeight()) * juce::jlimit(0.0f, 1.0f, level));
        if (fill > 0)
        {
            auto active = area.withY(area.getBottom() - fill).withHeight(fill);
            g.setColour(meterColourForLevel(level));
            g.fillRect(active);
        }

        const int holdY = area.getBottom() - juce::roundToInt(static_cast<float>(area.getHeight()) * juce::jlimit(0.0f, 1.0f, hold));
        g.setColour(juce::Colours::white);
        g.fillRect(area.getX(), juce::jlimit(area.getY(), area.getBottom() - 1, holdY), area.getWidth(), 1);
    };

    drawMeter(leftBar, peakLeft, peakHoldLeft);
    drawMeter(rightBar, peakRight, peakHoldRight);
}

void MasterStrip::mouseUp(const juce::MouseEvent& event)
{
    const auto led = juce::Rectangle<int>(8, 6, 10, 10);
    if (led.contains(event.getPosition()))
        clipLatched = false;
}

void MasterStrip::timerCallback()
{
    if (meterPlugin != nullptr)
    {
        const auto l = meterClient.getAndClearAudioLevel(0);
        const auto r = meterClient.getAndClearAudioLevel(1);
        peakLeft = juce::jlimit(0.0f, 1.0f, dbToLinear(l.dB));
        peakRight = juce::jlimit(0.0f, 1.0f, dbToLinear(r.dB));
    }
    else
    {
        peakLeft = 0.0f;
        peakRight = 0.0f;
    }

    if (peakLeft >= 0.999f || peakRight >= 0.999f)
        clipLatched = true;

    const auto peakNow = juce::jmax(peakLeft, peakRight);
    if (peakNow >= juce::jmax(peakHoldLeft, peakHoldRight))
    {
        peakHoldLeft = peakLeft;
        peakHoldRight = peakRight;
        peakHoldFrames = kPeakHoldFrameCount;
    }
    else if (--peakHoldFrames <= 0)
    {
        peakHoldLeft *= 0.92f;
        peakHoldRight *= 0.92f;
    }

    repaint();
}

} // namespace setle::mixer
