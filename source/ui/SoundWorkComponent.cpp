#include "SoundWorkComponent.h"

#include "../eff/EffBuilderComponent.h"
#include "../theme/ThemeManager.h"
#include "../theme/ThemeStyleHelpers.h"

#include <cmath>

namespace setle::ui
{

namespace
{
const auto kTurquoise = juce::Colour(0xff2ec7b0);
const auto kSaffron = juce::Colour(0xffF4C430);
const auto kOrangeRed = juce::Colour(0xffFF4500);
}

SoundWorkComponent::OrganicBulbButton::OrganicBulbButton(juce::String text)
    : juce::Button(std::move(text))
{
}

void SoundWorkComponent::OrganicBulbButton::setBaseColour(juce::Colour colour)
{
    if (baseColour == colour)
        return;
    baseColour = colour;
    repaint();
}

void SoundWorkComponent::OrganicBulbButton::setGlowFactor(float amount)
{
    const auto clamped = juce::jlimit(0.0f, 1.0f, amount);
    if (std::abs(glowFactor - clamped) < 0.001f)
        return;
    glowFactor = clamped;
    repaint();
}

void SoundWorkComponent::OrganicBulbButton::paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto& theme = ThemeManager::get().theme();
    auto area = getLocalBounds().toFloat().reduced(4.0f, 2.0f);
    const auto pressedMix = isButtonDown ? 0.18f : 0.0f;
    const auto hoverMix = isMouseOver ? 0.14f : 0.0f;
    auto fill = baseColour.interpolatedWith(juce::Colours::white, hoverMix).darker(pressedMix);

    setle::theme::drawIndicatorGlow(g,
                                    area.getCentre(),
                                    juce::jmin(area.getWidth(), area.getHeight()) * 0.30f,
                                    fill,
                                    theme,
                                    isEnabled() ? (0.35f + glowFactor * 0.65f) : 0.12f,
                                    isEnabled());

    juce::Path capsule;
    capsule.addRoundedRectangle(area, area.getHeight() * 0.48f);
    g.setColour(fill.withAlpha(isEnabled() ? 0.92f : 0.35f));
    g.fillPath(capsule);

    juce::ColourGradient gloss(juce::Colours::white.withAlpha(isEnabled() ? 0.22f : 0.08f),
                               area.getX(), area.getY(),
                               juce::Colours::transparentWhite,
                               area.getX(), area.getBottom(),
                               false);
    g.setGradientFill(gloss);
    g.fillRoundedRectangle(area.reduced(1.3f), area.getHeight() * 0.42f);

    g.setColour(theme.surfaceEdge.withAlpha(isEnabled() ? 0.50f : 0.22f));
    g.strokePath(capsule, juce::PathStrokeType(1.0f));

    g.setColour(juce::Colours::white.withAlpha(isEnabled() ? 0.94f : 0.55f));
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawFittedText(getName().isNotEmpty() ? getName() : getButtonText(), getLocalBounds().reduced(10, 4), juce::Justification::centred, 1);
}

void SoundWorkComponent::RosterButton::setFocused(bool isFocused)
{
    if (focused == isFocused)
        return;
    focused = isFocused;
    repaint();
}

void SoundWorkComponent::RosterButton::paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto area = getLocalBounds().toFloat();
    const auto& theme = ThemeManager::get().theme();
    const auto base = theme.surface1.interpolatedWith(theme.surface0, 0.45f);
    const auto hover = base.brighter(0.08f);
    auto fill = base;
    if (isMouseOver)
        fill = fill.interpolatedWith(hover, 0.45f);
    if (isButtonDown)
        fill = fill.brighter(0.08f);

    setle::theme::drawFuzzyInsetPanel(g, area, fill, theme, true, 6.0f);

    g.setColour(focused ? kSaffron.withAlpha(0.95f) : theme.surfaceEdge.withAlpha(0.72f));
    g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, focused ? 1.7f : 1.0f);

    if (focused)
    {
        setle::theme::drawIndicatorGlow(g,
                                        { area.getX() + 10.0f, area.getCentreY() },
                                        4.0f,
                                        kSaffron,
                                        theme,
                                        0.8f,
                                        true);
    }

    g.setColour(juce::Colours::white.withAlpha(focused ? 0.95f : 0.82f));
    g.setFont(juce::FontOptions(12.5f).withStyle("Bold"));
    g.drawText(getButtonText(), getLocalBounds().reduced(10, 6), juce::Justification::centredLeft, true);
}

SoundWorkComponent::SoundWorkComponent()
{
    ThemeManager::get().addListener(this);

    addAndMakeVisible(rosterPanel);
    addAndMakeVisible(centerPanel);
    addAndMakeVisible(footerPanel);

    footerPanel.addAndMakeVisible(smartSidechainButton);
    footerPanel.addAndMakeVisible(autoGainButton);
    footerPanel.addAndMakeVisible(addMidiButton);
    footerPanel.addAndMakeVisible(monoModeToggle);
    footerPanel.addAndMakeVisible(footerStatusLabel);

    smartSidechainButton.onClick = [this] { triggerSmartSidechain(); };
    autoGainButton.onClick = [this] { triggerAutoGain(); };
    addMidiButton.onClick = [this] { triggerAddMidi(); };
    monoModeToggle.onClick = [this]
    {
        refreshFooterButtonStyles();
        if (onMonoModeChanged)
            onMonoModeChanged(monoModeToggle.getToggleState());
        updateStatus(monoModeToggle.getToggleState() ? "Mono mode enabled" : "Mono mode disabled");
    };

    footerStatusLabel.setJustificationType(juce::Justification::centredLeft);
    footerStatusLabel.setFont(juce::FontOptions(12.0f));
    footerStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.78f));
    footerStatusLabel.setText("Sound station ready", juce::dontSendNotification);

    syncUiState();
    startTimerHz(30);
}

SoundWorkComponent::~SoundWorkComponent()
{
    stopTimer();
    ThemeManager::get().removeListener(this);
}

void SoundWorkComponent::setEffBuilder(setle::eff::EffBuilderComponent* builder)
{
    effBuilder = builder;
    if (effBuilder != nullptr)
        addAndMakeVisible(*effBuilder);
    syncUiState();
    resized();
}

void SoundWorkComponent::setInstrumentRoster(std::vector<InstrumentRosterEntry> entries)
{
    rosterEntries = std::move(entries);

    const auto focusedExists = std::any_of(rosterEntries.begin(), rosterEntries.end(),
                                           [this](const auto& entry) { return entry.trackId == focusedTrackId; });

    if (!focusedExists)
        focusedTrackId = rosterEntries.empty() ? juce::String{} : rosterEntries.front().trackId;

    rebuildRosterButtons();
    if (rosterEntries.empty())
        updateStatus("Add a MIDI vessel to start sculpting sound");
}

void SoundWorkComponent::setFocusedTrack(const juce::String& trackId)
{
    focusedTrackId = trackId;
    for (size_t i = 0; i < rosterButtons.size(); ++i)
    {
        const bool focused = i < rosterEntries.size() && rosterEntries[i].trackId == focusedTrackId;
        rosterButtons[i]->setFocused(focused);
    }
    syncUiState();
    repaint();
}

juce::String SoundWorkComponent::getFocusedTrack() const
{
    return focusedTrackId;
}

void SoundWorkComponent::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    auto area = getLocalBounds().toFloat();
    auto chassis = theme.windowBackground.interpolatedWith(juce::Colour(0xff2f2016), 0.5f);
    setle::theme::drawChassis(g, area, chassis, theme, 10.0f);

    auto rosterBounds = rosterPanel.getBounds().toFloat();
    setle::theme::drawFuzzyInsetPanel(g, rosterBounds, theme.surface1, theme, true, 9.0f);

    auto centerBounds = centerPanel.getBounds().toFloat();
    setle::theme::drawGlassPanel(g, centerBounds, theme.secondaryAccent.interpolatedWith(theme.surfaceLow, 0.7f), theme, true, 10.0f);

    auto footerBounds = footerPanel.getBounds().toFloat();
    setle::theme::drawFuzzyInsetPanel(g, footerBounds, theme.surface1.interpolatedWith(theme.surface0, 0.5f), theme, true, 9.0f);

    g.setColour(juce::Colours::white.withAlpha(0.72f));
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText("Instrument Roster", rosterBounds.toNearestInt().removeFromTop(24).reduced(10, 4), juce::Justification::centredLeft, false);
    g.drawText("FX Logic Desk", centerBounds.toNearestInt().removeFromTop(24).reduced(10, 4), juce::Justification::centredLeft, false);
    g.drawText("Smart Tools", footerBounds.toNearestInt().removeFromTop(22).reduced(10, 2), juce::Justification::centredLeft, false);

    setle::theme::drawIndicatorGlow(g, rosterBounds.getTopLeft().translated(14.0f, 14.0f), 4.0f, theme.primaryAccent, theme, 0.65f, true);
    setle::theme::drawIndicatorGlow(g, centerBounds.getTopLeft().translated(14.0f, 14.0f), 4.0f, theme.secondaryAccent, theme, focusedTrackId.isNotEmpty() ? 0.80f : 0.25f, true);
    setle::theme::drawIndicatorGlow(g, footerBounds.getTopLeft().translated(14.0f, 14.0f), 4.0f, theme.alertColor, theme, monoModeToggle.getToggleState() ? 0.85f : 0.30f, true);

    if (rosterEntries.empty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::FontOptions(13.0f));
        g.drawFittedText("No active vessels\nUse Add MIDI to seed the roster",
                         rosterBounds.toNearestInt().reduced(16, 36),
                         juce::Justification::centred, 2);
    }

    if (focusedTrackId.isEmpty())
    {
        g.setColour(juce::Colours::white.withAlpha(0.48f));
        g.setFont(juce::FontOptions(14.0f));
        g.drawFittedText("Select a vessel to inspect and shape its FX chain",
                         centerBounds.toNearestInt().reduced(20, 40),
                         juce::Justification::centred, 2);
    }

}

void SoundWorkComponent::resized()
{
    auto area = getLocalBounds().reduced(10);
    auto footer = area.removeFromBottom(64);
    footerPanel.setBounds(footer);

    const int rosterW = juce::jlimit(200, 280, static_cast<int>(area.getWidth() * 0.28f));
    rosterPanel.setBounds(area.removeFromLeft(rosterW));
    area.removeFromLeft(10);
    centerPanel.setBounds(area);

    auto rosterInner = rosterPanel.getLocalBounds().reduced(8);
    rosterInner.removeFromTop(24);
    const int rowH = 34;
    for (auto& btn : rosterButtons)
    {
        btn->setBounds(rosterInner.removeFromTop(rowH));
        rosterInner.removeFromTop(6);
    }

    auto centerInner = centerPanel.getBounds().reduced(10);
    centerInner.removeFromTop(24);
    if (effBuilder != nullptr)
        effBuilder->setBounds(centerInner);

    auto footerInner = footerPanel.getLocalBounds().reduced(8, 6);
    footerInner.removeFromTop(18);
    auto row = footerInner.removeFromTop(34);
    smartSidechainButton.setBounds(row.removeFromLeft(164));
    row.removeFromLeft(8);
    autoGainButton.setBounds(row.removeFromLeft(128));
    row.removeFromLeft(8);
    addMidiButton.setBounds(row.removeFromLeft(112));
    row.removeFromLeft(8);
    monoModeToggle.setBounds(row.removeFromLeft(136));

    footerInner.removeFromTop(2);
    footerStatusLabel.setBounds(footerInner.removeFromTop(20));
}

void SoundWorkComponent::rebuildRosterButtons()
{
    for (auto& btn : rosterButtons)
        rosterPanel.removeChildComponent(btn.get());
    rosterButtons.clear();

    for (const auto& entry : rosterEntries)
    {
        auto button = std::make_unique<RosterButton>();
        button->setButtonText(entry.trackName + "  [" + slotTypeLabel(entry.slotType) + "]");
        button->setFocused(entry.trackId == focusedTrackId);
        const auto trackId = entry.trackId;
        button->onClick = [this, trackId]
        {
            setFocusedTrack(trackId);
            if (onFocusTrackRequested)
                onFocusTrackRequested(trackId);
        };
        rosterPanel.addAndMakeVisible(*button);
        rosterButtons.push_back(std::move(button));
    }

    resized();
    repaint();
}

void SoundWorkComponent::refreshFooterButtonStyles()
{
    const auto& theme = ThemeManager::get().theme();
    const bool hasFocus = focusedTrackId.isNotEmpty();

    smartSidechainButton.setBaseColour(theme.primaryAccent.darker(0.45f));
    smartSidechainButton.setGlowFactor(hasFocus ? 0.68f : 0.18f);

    auto autoGainBase = theme.secondaryAccent.darker(0.52f);
    if (autoGainMeasuring)
    {
        const auto phase = static_cast<float>(std::sin(juce::Time::getMillisecondCounterHiRes() * 0.012));
        const auto mix = juce::jmap(phase, -1.0f, 1.0f, 0.25f, 0.85f);
        autoGainBase = autoGainBase.interpolatedWith(kTurquoise, mix);
    }
    else if (autoGainApplied)
    {
        autoGainBase = autoGainBase.interpolatedWith(kTurquoise, 0.62f);
    }
    autoGainButton.setBaseColour(autoGainBase);
    autoGainButton.setGlowFactor(autoGainMeasuring ? 1.0f : (autoGainApplied ? 0.82f : 0.26f));

    addMidiButton.setBaseColour(theme.surfaceVariant.interpolatedWith(theme.alertColor, 0.18f).withAlpha(0.92f));
    addMidiButton.setGlowFactor(0.32f);

    monoModeToggle.setColour(juce::ToggleButton::tickColourId,
                             monoModeToggle.getToggleState() ? theme.alertColor : theme.surfaceEdge);
    monoModeToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.88f));

    smartSidechainButton.setEnabled(hasFocus && !autoGainMeasuring);
    autoGainButton.setEnabled(hasFocus && !autoGainMeasuring);
}

void SoundWorkComponent::syncUiState()
{
    if (effBuilder != nullptr)
        effBuilder->setVisible(focusedTrackId.isNotEmpty());

    refreshFooterButtonStyles();
}

void SoundWorkComponent::themeChanged()
{
    refreshFooterButtonStyles();
    repaint();
}

void SoundWorkComponent::triggerSmartSidechain()
{
    if (focusedTrackId.isEmpty() || onSmartSidechainRequested == nullptr)
    {
        updateStatus("Select a target instrument first");
        return;
    }

    const auto suggestion = onSmartSidechainRequested(focusedTrackId);
    if (!suggestion.isValid())
    {
        updateStatus("No Kick/Drums -> Bass/Pad sidechain candidate found");
        return;
    }

    juce::PopupMenu menu;
    menu.addSectionHeader("Suggested: Route " + suggestion.sourceTrackName + " -> " + suggestion.targetTrackName + " Compressor?");
    menu.addItem(1, "Accept");
    menu.addItem(2, "Decline");

    const auto selected = menu.showMenu(juce::PopupMenu::Options().withTargetComponent(&smartSidechainButton));
    if (selected == 1)
    {
        if (onSmartSidechainAccepted)
            onSmartSidechainAccepted(suggestion);
        updateStatus("Smart Sidechain applied");
    }
    else
    {
        updateStatus("Smart Sidechain suggestion declined");
    }
}

void SoundWorkComponent::triggerAutoGain()
{
    if (focusedTrackId.isEmpty())
    {
        updateStatus("Select a focused track for Auto Gain");
        return;
    }

    autoGainMeasuring = true;
    autoGainApplied = false;
    autoGainStartedMs = juce::Time::getMillisecondCounter();
    updateStatus("Auto Gain measuring 2048-sample RMS...");
    syncUiState();
}

void SoundWorkComponent::triggerAddMidi()
{
    if (onAddMidiRequested)
        onAddMidiRequested();
}

void SoundWorkComponent::updateStatus(const juce::String& message)
{
    footerStatusLabel.setText(message, juce::dontSendNotification);
}

void SoundWorkComponent::timerCallback()
{
    if (!autoGainMeasuring)
        return;

    syncUiState();

    const auto elapsed = juce::Time::getMillisecondCounter() - autoGainStartedMs;
    if (elapsed < 650)
        return;

    autoGainMeasuring = false;
    autoGainApplied = false;
    if (onAutoGainRequested)
    {
        if (const auto appliedDb = onAutoGainRequested(focusedTrackId))
        {
            autoGainApplied = true;
            updateStatus("Auto Gain baked: " + juce::String(*appliedDb, 1) + " dB offset");
        }
        else
        {
            updateStatus("Auto Gain skipped: no usable recent output signal");
        }
    }
    else
    {
        updateStatus("Auto Gain unavailable");
    }

    syncUiState();
}

juce::String SoundWorkComponent::slotTypeLabel(setle::instruments::InstrumentSlot::SlotType type) const
{
    using SlotType = setle::instruments::InstrumentSlot::SlotType;
    switch (type)
    {
        case SlotType::PolySynth: return "PolySynth";
        case SlotType::DrumMachine: return "Drums";
        case SlotType::ReelSampler: return "Reel";
        case SlotType::VST3: return "VST3";
        case SlotType::MidiOut: return "MIDI Out";
        case SlotType::Empty: break;
    }
    return "Empty";
}

} // namespace setle::ui
