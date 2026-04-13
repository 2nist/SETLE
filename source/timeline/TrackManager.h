#pragma once

#include <tracktion_engine/tracktion_engine.h>

namespace te = tracktion::engine;

namespace tracktion::engine
{
using MidiTrack = AudioTrack;
}

namespace setle::timeline
{

class TrackManager
{
public:
    explicit TrackManager(te::Edit& edit);

    te::AudioTrack* addAudioTrack(const juce::String& name);
    te::MidiTrack* addMidiTrack(const juce::String& name);
    void removeTrack(te::Track* track);

    juce::Array<te::AudioTrack*> getAudioTracks() const;
    juce::Array<te::MidiTrack*> getMidiTracks() const;
    juce::Array<te::Track*> getAllUserTracks() const;

    void ensureDefaultTracks();

    static bool isSystemTrack(const te::Track& track);

private:
    te::Edit& edit;
};

} // namespace setle::timeline
