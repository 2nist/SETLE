#include "TimelineTracksComponent.h"

namespace setle::timeline
{

TimelineTracksComponent::TimelineTracksComponent(te::Edit& e, TrackManager& manager)
    : edit(e),
      trackManager(manager)
{
    addMidiTrackButton.onClick = [this]
    {
        const auto next = static_cast<int>(trackManager.getMidiTracks().size()) + 1;
        trackManager.addMidiTrack("MIDI " + juce::String(next));
        refreshTracks();
    };

    addAudioTrackButton.onClick = [this]
    {
        const auto next = static_cast<int>(trackManager.getAudioTracks().size()) + 1;
        trackManager.addAudioTrack("Audio " + juce::String(next));
        refreshTracks();
    };

    addAndMakeVisible(addMidiTrackButton);
    addAndMakeVisible(addAudioTrackButton);

    refreshTracks();
    startTimerHz(30);
}

void TimelineTracksComponent::refreshTracks()
{
    rebuildLanes();
}

void TimelineTracksComponent::setPlayheadFraction(double fraction)
{
    playheadFraction = juce::jlimit(0.0, 1.0, fraction);
    for (auto& lane : lanes)
        lane->setPlayheadFraction(playheadFraction);
}

void TimelineTracksComponent::setVisibleBeatRange(double startBeat, double endBeat)
{
    visibleStart = startBeat;
    visibleEnd = juce::jmax(startBeat + 1.0, endBeat);

    for (auto& lane : lanes)
        lane->setTimeRange(visibleStart, visibleEnd);
}

void TimelineTracksComponent::resized()
{
    auto area = getLocalBounds().reduced(2);
    const auto laneCount = static_cast<int>(lanes.size());
    const auto lanesHeight = laneCount * kTrackHeight;

    auto lanesArea = area.removeFromTop(lanesHeight);
    for (auto& lane : lanes)
        lane->setBounds(lanesArea.removeFromTop(kTrackHeight));

    area.removeFromTop(4);
    auto buttonRow = area.removeFromTop(kAddTrackButtonHeight);
    addMidiTrackButton.setBounds(buttonRow.removeFromLeft(88));
    buttonRow.removeFromLeft(8);
    addAudioTrackButton.setBounds(buttonRow.removeFromLeft(88));
}

void TimelineTracksComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141a23));

    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRect(getLocalBounds());
}

void TimelineTracksComponent::timerCallback()
{
    const auto* tempo = edit.tempoSequence.getTempo(0);
    const auto bpm = tempo != nullptr ? tempo->getBpm() : 120.0;
    const auto secPerBeat = 60.0 / juce::jmax(1.0, bpm);

    const auto playheadSeconds = edit.getTransport().getPosition().inSeconds();
    const auto playheadBeat = playheadSeconds / secPerBeat;
    const auto beatSpan = juce::jmax(1.0, visibleEnd - visibleStart);
    const auto fraction = juce::jlimit(0.0, 1.0, (playheadBeat - visibleStart) / beatSpan);

    setPlayheadFraction(fraction);

    for (auto& lane : lanes)
        lane->refreshClips();
}

void TimelineTracksComponent::rebuildLanes()
{
    lanes.clear();

    auto tracks = trackManager.getAllUserTracks();
    for (auto* track : tracks)
    {
        if (track == nullptr)
            continue;

        auto lane = std::make_unique<TrackLane>(*track);
        lane->setTimeRange(visibleStart, visibleEnd);
        lane->setPlayheadFraction(playheadFraction);

        lane->onMuteToggle = [this](te::Track& t)
        {
            t.setMute(!t.isMuted(false));
            refreshTracks();
        };

        lane->onSoloToggle = [this](te::Track& t)
        {
            t.setSolo(!t.isSolo(false));
            refreshTracks();
        };

        lane->onRename = [this](te::Track& t)
        {
            juce::AlertWindow renameWindow("Rename Track", "Enter track name", juce::AlertWindow::NoIcon);
            renameWindow.addTextEditor("name", t.getName(), "Name");
            renameWindow.addButton("Rename", 1);
            renameWindow.addButton("Cancel", 0);

            if (renameWindow.runModalLoop() == 1)
            {
                const auto newName = renameWindow.getTextEditorContents("name").trim();
                if (newName.isNotEmpty())
                    t.setName(newName);
            }

            refreshTracks();
        };

        lane->onRemove = [this](te::Track& t)
        {
            trackManager.removeTrack(&t);
            trackManager.ensureDefaultTracks();
            refreshTracks();
        };

        lane->onAddMidiTrackBelow = [this](te::Track&)
        {
            const auto next = static_cast<int>(trackManager.getMidiTracks().size()) + 1;
            trackManager.addMidiTrack("MIDI " + juce::String(next));
            refreshTracks();
        };

        lane->onAddAudioTrackBelow = [this](te::Track&)
        {
            const auto next = static_cast<int>(trackManager.getAudioTracks().size()) + 1;
            trackManager.addAudioTrack("Audio " + juce::String(next));
            refreshTracks();
        };

        lane->onDuplicateTrack = [this](te::Track& t)
        {
            const auto next = static_cast<int>(trackManager.getAudioTracks().size()) + 1;
            const auto name = t.getName().isNotEmpty() ? (t.getName() + " Copy") : ("Track " + juce::String(next));
            trackManager.addAudioTrack(name);
            refreshTracks();
            if (onStatusMessage)
                onStatusMessage("Duplicated track: " + t.getName());
        };

        lane->onMoveTrackUp = [this](te::Track& t)
        {
            if (onStatusMessage)
                onStatusMessage("Move Up requested for " + t.getName() + " (ordering hook pending)");
        };

        lane->onMoveTrackDown = [this](te::Track& t)
        {
            if (onStatusMessage)
                onStatusMessage("Move Down requested for " + t.getName() + " (ordering hook pending)");
        };

        lane->onOpenFx = [this](te::Track& t)
        {
            if (onStatusMessage)
                onStatusMessage("Open FX requested for " + t.getName());
        };

        lane->onArmToggle = [this](te::Track& t)
        {
            const bool armed = static_cast<bool>(t.state.getProperty("setleArmed", false));
            t.state.setProperty("setleArmed", !armed, nullptr);
            if (onStatusMessage)
                onStatusMessage((!armed ? "Armed: " : "Disarmed: ") + t.getName());
            refreshTracks();
        };

        lane->onBounceToAudio = [this](te::Track& t)
        {
            if (onStatusMessage)
                onStatusMessage("Bounce to audio requested for " + t.getName() + " (bounce hook pending)");
        };

        lane->onClipClicked = [this](te::Clip& clip)
        {
            if (onClipClicked)
                onClipClicked(clip);
            else if (onStatusMessage)
                onStatusMessage("GridRoll: " + clip.getName());
        };

        addAndMakeVisible(*lane);
        lanes.push_back(std::move(lane));
    }

    resized();
    repaint();
}

} // namespace setle::timeline
