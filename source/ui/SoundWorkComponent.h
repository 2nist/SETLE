#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "../instruments/InstrumentSlot.h"
#include "../theme/ThemeManager.h"

namespace setle::eff
{
class EffBuilderComponent;
}

namespace setle::ui
{

class SoundWorkComponent final : public juce::Component,
                                 private juce::Timer,
                                 private ThemeManager::Listener
{
public:
    struct InstrumentRosterEntry
    {
        juce::String trackId;
        juce::String trackName;
        setle::instruments::InstrumentSlot::SlotType slotType { setle::instruments::InstrumentSlot::SlotType::Empty };
    };

    struct SidechainSuggestion
    {
        juce::String sourceTrackId;
        juce::String sourceTrackName;
        juce::String targetTrackId;
        juce::String targetTrackName;

        bool isValid() const noexcept
        {
            return sourceTrackId.isNotEmpty() && targetTrackId.isNotEmpty();
        }
    };

    SoundWorkComponent();
    ~SoundWorkComponent() override;

    void setEffBuilder(setle::eff::EffBuilderComponent* builder);
    void setInstrumentRoster(std::vector<InstrumentRosterEntry> entries);
    void setFocusedTrack(const juce::String& trackId);
    juce::String getFocusedTrack() const;

    std::function<void(const juce::String&)> onFocusTrackRequested;
    std::function<void()> onAddMidiRequested;
    std::function<SidechainSuggestion(const juce::String& focusedTrackId)> onSmartSidechainRequested;
    std::function<void(const SidechainSuggestion&)> onSmartSidechainAccepted;
    std::function<std::optional<float>(const juce::String& focusedTrackId)> onAutoGainRequested;
    std::function<void(bool enabled)> onMonoModeChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    class RosterButton final : public juce::TextButton
    {
    public:
        void setFocused(bool isFocused);
        void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override;

    private:
        bool focused { false };
    };

    class OrganicBulbButton final : public juce::Button
    {
    public:
        explicit OrganicBulbButton(juce::String text);

        void setBaseColour(juce::Colour colour);
        void setGlowFactor(float amount);
        void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override;

    private:
        juce::Colour baseColour { juce::Colours::grey };
        float glowFactor { 0.0f };
    };

    void rebuildRosterButtons();
    void refreshFooterButtonStyles();
    void triggerSmartSidechain();
    void triggerAutoGain();
    void triggerAddMidi();
    void syncUiState();
    void updateStatus(const juce::String& message);
    void timerCallback() override;
    void themeChanged() override;
    juce::String slotTypeLabel(setle::instruments::InstrumentSlot::SlotType type) const;

    std::vector<InstrumentRosterEntry> rosterEntries;
    std::vector<std::unique_ptr<RosterButton>> rosterButtons;

    juce::String focusedTrackId;

    juce::Component rosterPanel;
    juce::Component centerPanel;
    juce::Component footerPanel;
    juce::Label footerStatusLabel;

    OrganicBulbButton smartSidechainButton { "Smart Sidechain" };
    OrganicBulbButton autoGainButton { "Auto Gain" };
    OrganicBulbButton addMidiButton { "Add MIDI" };
    juce::ToggleButton monoModeToggle { "Mono Mode" };

    setle::eff::EffBuilderComponent* effBuilder { nullptr };

    bool autoGainMeasuring { false };
    bool autoGainApplied { false };
    juce::uint32 autoGainStartedMs { 0 };
};

} // namespace setle::ui
