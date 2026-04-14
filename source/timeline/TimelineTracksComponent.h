#pragma once

#include "TrackLane.h"
#include "TrackManager.h"

namespace setle::timeline
{

class TimelineTracksComponent : public juce::Component,
                                private juce::Timer
{
public:
    explicit TimelineTracksComponent(te::Edit& edit, TrackManager& trackManager);

    void refreshTracks();
    void setPlayheadFraction(double fraction);
    void setVisibleBeatRange(double startBeat, double endBeat);

    void resized() override;
    void paint(juce::Graphics& g) override;

    std::function<void(const juce::String&)> onStatusMessage;

    /** Fires when the user clicks a clip in any lane. Set from WorkspaceShellComponent. */
    std::function<void(te::Clip&)> onClipClicked;
    std::function<void(te::Clip&)> onAnalyzeImportedClip;

private:
    void timerCallback() override;
    void rebuildLanes();

    te::Edit& edit;
    TrackManager& trackManager;
    std::vector<std::unique_ptr<TrackLane>> lanes;
    double playheadFraction { 0.0 };
    double visibleStart { 0.0 };
    double visibleEnd { 32.0 };

    static constexpr int kTrackHeight = 56;
    static constexpr int kAddTrackButtonHeight = 28;

    juce::TextButton addMidiTrackButton { "+ MIDI" };
    juce::TextButton addAudioTrackButton { "+ Audio" };
};

} // namespace setle::timeline
