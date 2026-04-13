#include "MixerStrip.h"
#include "../theme/ThemeManager.h"

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
    const auto& theme = ThemeManager::get().theme();
    const auto db = juce::Decibels::gainToDecibels(juce::jmax(0.000001f, linear), -100.0f);
    if (db >= -3.0f)
        return theme.accentWarm;
    if (db >= -6.0f)
        return theme.zoneD;
    return theme.zoneC;
}
} // namespace

MixerStrip::MixerStrip(te::AudioTrack& t,
                       te::Edit& e,
                       const juce::String& instrumentType,
                       BusManager& manager)
    : track(t),
      edit(e),
      busManager(manager)
{
    addAndMakeVisible(nameLabel);
    addAndMakeVisible(muteButton);
    addAndMakeVisible(soloButton);
    addAndMakeVisible(slotTypeLabel);
    addAndMakeVisible(volumeFader);
    addAndMakeVisible(panKnob);

    nameLabel.setText(track.getName(), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centred);
    nameLabel.setEditable(true, false, false);
    nameLabel.onTextChange = [this]
    {
        const auto n = nameLabel.getText().trim();
        if (n.isNotEmpty())
            track.setName(n);
    };

    slotTypeLabel.setText(instrumentType.isNotEmpty() ? instrumentType : "Empty", juce::dontSendNotification);
    slotTypeLabel.setJustificationType(juce::Justification::centred);
    slotTypeLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));

    muteButton.setClickingTogglesState(true);
    muteButton.setToggleState(track.isMuted(false), juce::dontSendNotification);
    muteButton.onClick = [this]
    {
        track.setMute(muteButton.getToggleState());
        if (onMuteChanged)
            onMuteChanged(track);
    };

    soloButton.setClickingTogglesState(true);
    soloButton.setToggleState(track.isSolo(false), juce::dontSendNotification);
    soloButton.onClick = [this]
    {
        track.setSolo(soloButton.getToggleState());
        if (onSoloChanged)
            onSoloChanged(track);
    };

    volumeFader.setSliderStyle(juce::Slider::LinearVertical);
    volumeFader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeFader.setRange(-100.0, 6.0, 0.1);
    if (auto* vp = track.getVolumePlugin())
        volumeFader.setValue(vp->getVolumeDb(), juce::dontSendNotification);
    volumeFader.onValueChange = [this]
    {
        if (auto* vp = track.getVolumePlugin())
            vp->setVolumeDb(static_cast<float>(volumeFader.getValue()));
        volumeFader.setTooltip(juce::String(volumeFader.getValue(), 1) + " dB");
    };

    panKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    panKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panKnob.setRange(-1.0, 1.0, 0.01);
    if (auto* vp = track.getVolumePlugin())
        panKnob.setValue(vp->getPan(), juce::dontSendNotification);
    panKnob.onValueChange = [this]
    {
        if (auto* vp = track.getVolumePlugin())
            vp->setPan(static_cast<float>(panKnob.getValue()));
    };

    for (int i = 0; i < BusManager::kMaxBuses; ++i)
    {
        auto& knob = sendKnobs[i];
        addAndMakeVisible(knob);
        knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange(0.0, 1.0, 0.01);
        knob.setValue(busManager.getSendLevel(track, i), juce::dontSendNotification);
        knob.onValueChange = [this, i]
        {
            busManager.setSendLevel(track, i, static_cast<float>(sendKnobs[i].getValue()));
        };
        knob.setVisible(busManager.getBus(i).isActive);
        knob.setTooltip(busManager.getBus(i).name);
    }

    if (auto* m = track.getLevelMeterPlugin())
    {
        meterPlugin = m;
        meterPlugin->measurer.addClient(meterClient);
    }

    startTimerHz(30);

    // Register for tool changes so we can update cursor
    setle::ui::EditToolManager::get().addListener(this);
}

MixerStrip::~MixerStrip()
{
    if (meterPlugin != nullptr)
        meterPlugin->measurer.removeClient(meterClient);

    setle::ui::EditToolManager::get().removeListener(this);
}

void MixerStrip::mouseEnter(const juce::MouseEvent&)
{
    setMouseCursor(setle::ui::EditToolManager::get().getCursorFor(setle::ui::EditToolManager::Surface::MixerStrip));
}

void MixerStrip::mouseExit(const juce::MouseEvent&)
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void MixerStrip::activeToolChanged(setle::ui::EditTool newTool)
{
    // update cursor when active tool changes while mouse is over
    if (isMouseOver(true))
        setMouseCursor(setle::ui::EditToolManager::get().getCursorFor(setle::ui::EditToolManager::Surface::MixerStrip));
}

void MixerStrip::resized()
{
    auto b = getLocalBounds().reduced(4);
    nameLabel.setBounds(b.removeFromTop(20));

    auto buttons = b.removeFromTop(24);
    soloButton.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2).reduced(1));
    muteButton.setBounds(buttons.reduced(1));

    slotTypeLabel.setBounds(b.removeFromTop(16));

    const auto meterArea = b.removeFromTop(80).reduced(8, 2);
    (void) meterArea;

    volumeFader.setBounds(b.removeFromTop(kFaderHeight).reduced(10, 0));
    panKnob.setBounds(b.removeFromTop(40).reduced(14, 2));

    for (int i = 0; i < BusManager::kMaxBuses; ++i)
    {
        if (!sendKnobs[i].isVisible())
            continue;

        sendKnobs[i].setBounds(b.removeFromTop(24).reduced(18, 0));
    }
}

void MixerStrip::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto& theme = ThemeManager::get().theme();
    g.setColour(theme.surface1);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(theme.surfaceEdge);
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);

    auto meterBounds = getLocalBounds().reduced(8).withTrimmedTop(20 + 24 + 16).removeFromTop(80);
    const int barGap = 3;
    juce::Rectangle<int> leftBar(meterBounds.getCentreX() - (kMeterWidth + barGap / 2), meterBounds.getY(), kMeterWidth, meterBounds.getHeight());
    juce::Rectangle<int> rightBar(meterBounds.getCentreX() + (barGap / 2), meterBounds.getY(), kMeterWidth, meterBounds.getHeight());

    auto drawMeter = [&g, &theme](juce::Rectangle<int> area, float level, float hold)
    {
        g.setColour(theme.surface2);
        g.fillRect(area);

        const int fill = juce::roundToInt(static_cast<float>(area.getHeight()) * juce::jlimit(0.0f, 1.0f, level));
        if (fill > 0)
        {
            auto active = area.withY(area.getBottom() - fill).withHeight(fill);
            g.setColour(meterColourForLevel(level));
            g.fillRect(active);
        }

        const int holdY = area.getBottom() - juce::roundToInt(static_cast<float>(area.getHeight()) * juce::jlimit(0.0f, 1.0f, hold));
        g.setColour(theme.inkLight);
        g.fillRect(area.getX(), juce::jlimit(area.getY(), area.getBottom() - 1, holdY), area.getWidth(), 1);
    };

    drawMeter(leftBar, peakLeft, peakHoldLeft);
    drawMeter(rightBar, peakRight, peakHoldRight);
}

void MixerStrip::timerCallback()
{
    peakLeft = getPeakLevel(0);
    peakRight = getPeakLevel(1);

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

float MixerStrip::getPeakLevel(int channel)
{
    if (meterPlugin == nullptr)
        return 0.0f;

    const auto pair = meterClient.getAndClearAudioLevel(juce::jlimit(0, 1, channel));
    return juce::jlimit(0.0f, 1.0f, dbToLinear(pair.dB));
}

} // namespace setle::mixer
