#include "TrackManager.h"

namespace setle::timeline
{
namespace
{
constexpr const char* kTrackKindProperty = "setleTrackKind";
constexpr const char* kTrackKindMidi = "midi";
constexpr const char* kTrackKindAudio = "audio";
}

TrackManager::TrackManager(te::Edit& e)
    : edit(e)
{
}

te::AudioTrack* TrackManager::addAudioTrack(const juce::String& name)
{
    auto created = edit.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(edit), nullptr, false);
    if (created == nullptr)
        return nullptr;

    created->setName(name.isNotEmpty() ? name : "Audio");
    created->state.setProperty(kTrackKindProperty, kTrackKindAudio, nullptr);
    return created.get();
}

te::MidiTrack* TrackManager::addMidiTrack(const juce::String& name)
{
    auto created = edit.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(edit), nullptr, false);
    if (created == nullptr)
        return nullptr;

    created->setName(name.isNotEmpty() ? name : "MIDI");
    created->state.setProperty(kTrackKindProperty, kTrackKindMidi, nullptr);
    return created.get();
}

void TrackManager::removeTrack(te::Track* track)
{
    if (track == nullptr || isSystemTrack(*track))
        return;

    edit.deleteTrack(track);
}

juce::Array<te::AudioTrack*> TrackManager::getAudioTracks() const
{
    juce::Array<te::AudioTrack*> tracks;
    for (auto* track : te::getAudioTracks(edit))
    {
        if (track == nullptr || isSystemTrack(*track))
            continue;

        const auto kind = track->state.getProperty(kTrackKindProperty).toString();
        if (kind == kTrackKindMidi)
            continue;

        tracks.add(track);
    }
    return tracks;
}

juce::Array<te::MidiTrack*> TrackManager::getMidiTracks() const
{
    juce::Array<te::MidiTrack*> tracks;
    for (auto* track : te::getAudioTracks(edit))
    {
        if (track == nullptr || isSystemTrack(*track))
            continue;

        const auto kind = track->state.getProperty(kTrackKindProperty).toString();
        if (kind == kTrackKindMidi)
            tracks.add(track);
    }
    return tracks;
}

juce::Array<te::Track*> TrackManager::getAllUserTracks() const
{
    juce::Array<te::Track*> tracks;
    for (auto* track : te::getAudioTracks(edit))
    {
        if (track != nullptr && !isSystemTrack(*track))
            tracks.add(track);
    }
    return tracks;
}

void TrackManager::ensureDefaultTracks()
{
    if (!getAllUserTracks().isEmpty())
        return;

    addMidiTrack("MIDI 1");
}

bool TrackManager::isSystemTrack(const te::Track& track)
{
    const auto tag = track.state.getProperty("setleSystemTag").toString();
    const auto name = track.getName();

    return tag == "__history_buffer"
        || tag == "__sampler_queue"
        || name == "__history_buffer"
        || name == "__sampler_queue";
}

} // namespace setle::timeline
