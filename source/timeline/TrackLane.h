#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <functional>
#include <vector>

namespace te = tracktion::engine;

namespace setle::timeline
{

class TrackLane : public juce::Component
{
public:
    struct Config
    {
        juce::String trackName;
        juce::Colour trackColour;
        bool isMidi { false };
        bool isMuted { false };
        bool isSoloed { false };
    };

    explicit TrackLane(te::Track& track);

    void setTimeRange(double startBeat, double endBeat);
    void setPlayheadFraction(double fraction);
    void refreshClips();

    std::function<void(te::Track&)> onMuteToggle;
    std::function<void(te::Track&)> onSoloToggle;
    std::function<void(te::Track&)> onRename;
    std::function<void(te::Track&)> onRemove;
    std::function<void(te::Track&)> onAddMidiTrackBelow;
    std::function<void(te::Track&)> onAddAudioTrackBelow;
    std::function<void(te::Clip&)> onClipClicked;
    std::function<void(te::Track&)> onDuplicateTrack;
    std::function<void(te::Track&)> onMoveTrackUp;
    std::function<void(te::Track&)> onMoveTrackDown;
    std::function<void(te::Track&)> onOpenFx;
    std::function<void(te::Track&)> onArmToggle;
    std::function<void(te::Track&)> onBounceToAudio;

    void paint(juce::Graphics& g) override;
    void resized() override;
void mouseDown(const juce::MouseEvent& e) override;
void mouseUp(const juce::MouseEvent& e) override;

private:
    struct PaintedClip
    {
        te::Clip* clip = nullptr;
        juce::Rectangle<int> bounds;
    };

    void rebuildPaintedClips(const juce::Rectangle<int>& clipArea);
    const PaintedClip* findPaintedClipAt(juce::Point<int> point) const;

    te::Track& trackRef;
    double visibleStartBeat { 0.0 };
    double visibleEndBeat { 32.0 };
    double playheadFraction { 0.0 };

    juce::TextButton muteButton { "M" };
    juce::TextButton soloButton { "S" };
    juce::Label nameLabel;

    std::vector<PaintedClip> paintedClips;

    static constexpr int kHeaderWidth = 120;
};

} // namespace setle::timeline
