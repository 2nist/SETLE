#include "MixerStrip.h"
#include "../theme/ThemeManager.h"
#include <array>

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
        repaint();
    };
    volumeFader.onDragStart = [this]
    {
        faderDragging = true;
        repaint();
    };
    volumeFader.onDragEnd = [this]
    {
        faderDragging = false;
        repaint();
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
    const auto& theme = ThemeManager::get().theme();
    auto r = getLocalBounds().toFloat();
    auto headerTint = theme.surface2;
    auto colorText = track.state.getProperty("setleTrackColor").toString().trim();
    if (colorText.startsWith("#"))
        colorText = colorText.substring(1);
    if (colorText.length() == 6)
        colorText = "ff" + colorText;
    if (colorText.length() == 8)
        headerTint = juce::Colour::fromString(colorText).withMultipliedSaturation(0.7f);

    g.setColour(theme.surface1);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(headerTint.withAlpha(0.35f));
    g.fillRoundedRectangle(r.removeFromTop(40.0f), 4.0f);
    g.setColour(theme.surfaceEdge);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);

    auto meterBounds = getLocalBounds().reduced(8).withTrimmedTop(20 + 24 + 16).removeFromTop(80);
    const int barGap = 3;
    juce::Rectangle<int> leftBar(meterBounds.getCentreX() - (kMeterWidth + barGap / 2), meterBounds.getY(), kMeterWidth, meterBounds.getHeight());
    juce::Rectangle<int> rightBar(meterBounds.getCentreX() + (barGap / 2), meterBounds.getY(), kMeterWidth, meterBounds.getHeight());

    auto drawMeter = [&g, &theme](juce::Rectangle<int> area, float level, float hold)
    {
        // Meter background zones (green/yellow/red).
        g.setColour(theme.zoneC.withAlpha(0.25f));
        g.fillRect(area.withY(area.getY() + static_cast<int>(area.getHeight() * 0.36f))
                       .withHeight(static_cast<int>(area.getHeight() * 0.64f)));
        g.setColour(theme.zoneD.withAlpha(0.30f));
        g.fillRect(area.withY(area.getY() + static_cast<int>(area.getHeight() * 0.20f))
                       .withHeight(static_cast<int>(area.getHeight() * 0.16f)));
        g.setColour(theme.accentWarm.withAlpha(0.30f));
        g.fillRect(area.withHeight(static_cast<int>(area.getHeight() * 0.20f)));
        g.setColour(theme.surfaceEdge.withAlpha(0.6f));
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

    // Meter scale labels.
    g.setColour(theme.inkMuted.withAlpha(0.85f));
    g.setFont(juce::FontOptions(8.5f));
    const std::array<std::pair<const char*, float>, 5> meterMarks { {
        { "0", 1.0f }, { "-3", 0.82f }, { "-6", 0.70f }, { "-12", 0.50f }, { "-inf", 0.0f }
    } };
    for (const auto& mark : meterMarks)
    {
        const int y = meterBounds.getBottom() - static_cast<int>(meterBounds.getHeight() * mark.second);
        g.drawText(mark.first, meterBounds.getRight() + 2, y - 5, 22, 10, juce::Justification::centredLeft, false);
    }

    // Volume fader dB ticks.
    const auto faderBounds = volumeFader.getBounds();
    const std::array<double, 6> dbTicks { 0.0, -6.0, -12.0, -18.0, -24.0, -100.0 };
    for (double db : dbTicks)
    {
        const double minDb = volumeFader.getMinimum();
        const double maxDb = volumeFader.getMaximum();
        const double frac = juce::jlimit(0.0, 1.0, (db - minDb) / (maxDb - minDb));
        const int y = faderBounds.getBottom() - static_cast<int>(frac * faderBounds.getHeight());
        g.setColour(theme.surfaceEdge.withAlpha(0.75f));
        g.drawLine(static_cast<float>(faderBounds.getX() - 6), static_cast<float>(y),
                   static_cast<float>(faderBounds.getX() - 1), static_cast<float>(y), 1.0f);
        g.setColour(theme.inkMuted.withAlpha(0.82f));
        g.setFont(juce::FontOptions(8.0f));
        const auto label = (db <= -99.0) ? "-inf" : juce::String(juce::roundToInt(db));
        g.drawText(label, faderBounds.getX() - 30, y - 5, 22, 10, juce::Justification::centredRight, false);
    }

    if (faderDragging)
    {
        const auto dbText = juce::String(volumeFader.getValue(), 1) + " dB";
        auto bubble = faderBounds.withY(faderBounds.getY() - 18).withHeight(14).expanded(6, 0);
        g.setColour(theme.surface3.withAlpha(0.9f));
        g.fillRoundedRectangle(bubble.toFloat(), 3.0f);
        g.setColour(theme.inkLight.withAlpha(0.95f));
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawText(dbText, bubble, juce::Justification::centred, false);
    }

    // Pan labels and center detent.
    const auto panBounds = panKnob.getBounds();
    g.setColour(theme.inkMuted.withAlpha(0.85f));
    g.setFont(juce::FontOptions(8.5f));
    g.drawText("L", panBounds.getX() - 2, panBounds.getCentreY() - 5, 8, 10, juce::Justification::centred, false);
    g.drawText("R", panBounds.getRight() - 6, panBounds.getCentreY() - 5, 8, 10, juce::Justification::centred, false);
    g.setColour(theme.surfaceEdge.withAlpha(0.9f));
    g.drawLine(static_cast<float>(panBounds.getCentreX()), static_cast<float>(panBounds.getBottom() - 3),
               static_cast<float>(panBounds.getCentreX()), static_cast<float>(panBounds.getBottom() - 9), 1.0f);

    // Instrument indicator dot + icon near slot label.
    const auto slotBounds = slotTypeLabel.getBounds();
    const auto slotText = slotTypeLabel.getText().toLowerCase();
    juce::Colour typeDot = theme.signalMidi;
    if (slotText.contains("reel") || slotText.contains("sampler"))
        typeDot = theme.signalAudio;
    else if (slotText.contains("vst"))
        typeDot = theme.zoneA;

    g.setColour(typeDot.withAlpha(0.95f));
    g.fillEllipse(static_cast<float>(slotBounds.getX() + 2), static_cast<float>(slotBounds.getCentreY() - 2), 4.0f, 4.0f);

    if (slotText.contains("reel"))
    {
        juce::Path wave;
        wave.startNewSubPath(static_cast<float>(slotBounds.getX() + 10), static_cast<float>(slotBounds.getCentreY()));
        wave.lineTo(static_cast<float>(slotBounds.getX() + 13), static_cast<float>(slotBounds.getCentreY() - 2));
        wave.lineTo(static_cast<float>(slotBounds.getX() + 16), static_cast<float>(slotBounds.getCentreY() + 2));
        wave.lineTo(static_cast<float>(slotBounds.getX() + 19), static_cast<float>(slotBounds.getCentreY() - 1));
        g.setColour(theme.inkMid.withAlpha(0.9f));
        g.strokePath(wave, juce::PathStrokeType(1.0f));
    }
    else if (slotText.contains("vst"))
    {
        auto plug = juce::Rectangle<int>(slotBounds.getX() + 10, slotBounds.getY() + 4, 8, 8);
        g.setColour(theme.inkMid.withAlpha(0.9f));
        g.drawRect(plug);
        g.drawLine(static_cast<float>(plug.getX() + 2), static_cast<float>(plug.getY() - 2),
                   static_cast<float>(plug.getX() + 2), static_cast<float>(plug.getY()), 1.0f);
        g.drawLine(static_cast<float>(plug.getX() + 6), static_cast<float>(plug.getY() - 2),
                   static_cast<float>(plug.getX() + 6), static_cast<float>(plug.getY()), 1.0f);
    }
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
