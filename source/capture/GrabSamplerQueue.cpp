#include "GrabSamplerQueue.h"

#include "../instruments/polysynth/TracktionPolySynthPlugin.h"
#include "../instruments/reel/TracktionReelPlugin.h"

namespace setle::capture
{
namespace
{
constexpr const char* kSamplerTrackTag = "__sampler_queue";

void resetSlot(GrabSlot& slot)
{
    slot.slotId = juce::Uuid().toString();
    slot.progressionId.clear();
    slot.displayName.clear();
    slot.state = GrabSlot::State::Empty;
    slot.confidence = 0.0f;
    slot.looping = false;
    slot.repeatCount = 1;
    slot.coupledAudio.reset();
    slot.coupledSampleRate = 0.0;
}
} // namespace

GrabSamplerQueue::GrabSamplerQueue()
{
    for (auto& slot : slots)
        resetSlot(slot);
}

int GrabSamplerQueue::loadProgression(const juce::String& progressionId,
                                      const juce::String& displayName,
                                      float confidence)
{
    for (int i = 0; i < kSlotCount; ++i)
    {
        if (slots[static_cast<size_t>(i)].state == GrabSlot::State::Empty)
        {
            loadProgression(i, progressionId, displayName, confidence);
            return i;
        }
    }

    loadProgression(0, progressionId, displayName, confidence);
    return 0;
}

bool GrabSamplerQueue::loadProgression(int slotIndex,
                                       const juce::String& progressionId,
                                       const juce::String& displayName,
                                       float confidence)
{
    if (!isValidSlot(slotIndex) || progressionId.isEmpty())
        return false;

    auto& slot = slots[static_cast<size_t>(slotIndex)];
    slot.progressionId = progressionId;
    slot.displayName = displayName;
    slot.confidence = juce::jlimit(0.0f, 1.0f, confidence);
    slot.state = GrabSlot::State::Ready;
    slot.looping = false;
    slot.repeatCount = 1;
    // Loading a new progression into a reused slot should not retain stale
    // coupled audio from the previous assignment.
    slot.coupledAudio.reset();
    slot.coupledSampleRate = 0.0;
    notifyQueueChanged();
    return true;
}

bool GrabSamplerQueue::setSlotAudioBuffer(int slotIndex,
                                          const juce::AudioBuffer<float>& audio,
                                          double sampleRate)
{
    if (!isValidSlot(slotIndex) || audio.getNumSamples() <= 0 || sampleRate <= 0.0)
        return false;

    auto& slot = slots[static_cast<size_t>(slotIndex)];
    slot.coupledAudio = std::make_shared<juce::AudioBuffer<float>>(audio);
    slot.coupledSampleRate = sampleRate;
    notifyQueueChanged();
    return true;
}

void GrabSamplerQueue::clearSlot(int slotIndex)
{
    if (!isValidSlot(slotIndex))
        return;

    if (activeSlot == slotIndex)
        activeSlot = -1;

    resetSlot(slots[static_cast<size_t>(slotIndex)]);
    notifyQueueChanged();
}

const GrabSlot& GrabSamplerQueue::getSlot(int slotIndex) const
{
    static GrabSlot invalid {};
    if (!isValidSlot(slotIndex))
        return invalid;

    return slots[static_cast<size_t>(slotIndex)];
}

GrabSlot& GrabSamplerQueue::getSlotMutable(int slotIndex)
{
    jassert(isValidSlot(slotIndex));
    return slots[static_cast<size_t>(slotIndex)];
}

void GrabSamplerQueue::playSlot(int slotIndex, te::Edit& edit)
{
    if (!isValidSlot(slotIndex))
        return;

    auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (slot.state == GrabSlot::State::Empty || slot.progressionId.isEmpty())
        return;

    if (!slot.hasCoupledAudio() && songRef == nullptr)
        return;

    if (activeSlot >= 0 && activeSlot != slotIndex)
        stopPlayback(edit);

    auto* track = findOrCreateSamplerTrack(edit);
    if (track == nullptr)
        return;

    prePlayheadSeconds = edit.getTransport().getPosition().inSeconds();

    if (slot.hasCoupledAudio())
    {
        static bool reelRegistered = false;
        if (!reelRegistered)
        {
            edit.engine.getPluginManager().createBuiltInType<setle::instruments::reel::TracktionReelPlugin>();
            reelRegistered = true;
        }

        setle::instruments::reel::TracktionReelPlugin* reel = nullptr;
        for (auto* p : track->pluginList.getPlugins())
        {
            if (auto* casted = dynamic_cast<setle::instruments::reel::TracktionReelPlugin*>(p))
            {
                reel = casted;
                break;
            }
        }

        if (reel == nullptr)
        {
            auto plugin = edit.getPluginCache().createNewPlugin(setle::instruments::reel::TracktionReelPlugin::xmlTypeName, {});
            if (plugin != nullptr)
            {
                track->pluginList.insertPlugin(plugin, 0, nullptr);
                reel = dynamic_cast<setle::instruments::reel::TracktionReelPlugin*>(plugin.get());
            }
        }

        if (reel != nullptr && slot.coupledAudio != nullptr)
        {
            reel->setEnabled(true);
            reel->getProcessor().setMode(ReelMode::sample);
            reel->getProcessor().setParam("sample.oneshot", 1.0f);
            reel->getProcessor().loadFromBuffer(*slot.coupledAudio, slot.coupledSampleRate);
        }

        // Explicitly disable polysynth on coupled-audio playback so the queue
        // routes through Reel only.
        for (auto* p : track->pluginList.getPlugins())
            if (auto* poly = dynamic_cast<setle::instruments::polysynth::TracktionPolySynthPlugin*>(p))
                poly->setEnabled(false);

        const auto durationSeconds = static_cast<double>(slot.coupledAudio->getNumSamples()) / slot.coupledSampleRate;
        rebuildSamplerClipForAudioSlot(*track, juce::jmax(0.2, durationSeconds));
    }
    else
    {
        static bool polyRegistered = false;
        if (!polyRegistered)
        {
            edit.engine.getPluginManager().createBuiltInType<setle::instruments::polysynth::TracktionPolySynthPlugin>();
            polyRegistered = true;
        }

        setle::instruments::polysynth::TracktionPolySynthPlugin* poly = nullptr;
        for (auto* p : track->pluginList.getPlugins())
        {
            if (auto* casted = dynamic_cast<setle::instruments::polysynth::TracktionPolySynthPlugin*>(p))
            {
                poly = casted;
                break;
            }

            if (auto* reel = dynamic_cast<setle::instruments::reel::TracktionReelPlugin*>(p))
            {
                reel->setEnabled(false);
                reel->getProcessor().clearBuffer();
            }
        }

        if (poly == nullptr)
        {
            auto plugin = edit.getPluginCache().createNewPlugin(setle::instruments::polysynth::TracktionPolySynthPlugin::xmlTypeName, {});
            if (plugin != nullptr)
            {
                track->pluginList.insertPlugin(plugin, 0, nullptr);
                poly = dynamic_cast<setle::instruments::polysynth::TracktionPolySynthPlugin*>(plugin.get());
            }
        }

        if (poly != nullptr)
            poly->setEnabled(true);
        rebuildSamplerClipForSlot(edit, slotIndex, *track);
    }

    std::optional<model::Progression> progressionOpt;
    if (songRef != nullptr)
        progressionOpt = songRef->findProgressionById(slot.progressionId);

    double totalSeconds = 0.0;
    if (slot.hasCoupledAudio())
    {
        totalSeconds = static_cast<double>(slot.coupledAudio->getNumSamples()) / slot.coupledSampleRate;
    }
    else
    {
        if (!progressionOpt.has_value())
            return;

        double totalBeats = 0.0;
        for (const auto& chord : progressionOpt->getChords())
        {
            const auto d = chord.getDurationBeats();
            totalBeats += (d > 0.0 ? d : 1.0);
        }
        const auto bpm = juce::jmax(1.0, songRef->getBpm());
        totalSeconds = (60.0 / bpm) * totalBeats;
    }

    edit.getTransport().setLoopRange({ tracktion::TimePosition::fromSeconds(0.0), tracktion::TimeDuration::fromSeconds(totalSeconds) });
    edit.getTransport().looping = slot.looping;
    edit.getTransport().setPosition(tracktion::TimePosition::fromSeconds(0.0));
    edit.getTransport().play(false);

    slot.state = GrabSlot::State::Playing;
    activeSlot = slotIndex;
    notifyQueueChanged();
}

void GrabSamplerQueue::stopPlayback(te::Edit& edit)
{
    edit.getTransport().stop(false, false);
    edit.getTransport().setPosition(tracktion::TimePosition::fromSeconds(prePlayheadSeconds));

    auto tracks = te::getAudioTracks(edit);
    for (auto* track : tracks)
    {
        if (track == nullptr)
            continue;

        if (track->state.getProperty("setleSystemTag").toString() != kSamplerTrackTag)
            continue;

        juce::Array<te::Clip*> oldClips;
        for (auto* clip : track->getClips())
            oldClips.add(clip);
        for (auto* clip : oldClips)
            clip->removeFromParent();
    }

    for (auto& slot : slots)
    {
        if (slot.state == GrabSlot::State::Playing)
            slot.state = GrabSlot::State::Ready;
    }

    activeSlot = -1;
    notifyQueueChanged();
}

int GrabSamplerQueue::getActiveSlotIndex() const noexcept
{
    return activeSlot;
}

juce::String GrabSamplerQueue::promoteToLibrary(int slotIndex, model::Song& song)
{
    if (!isValidSlot(slotIndex))
        return {};

    const auto& slot = slots[static_cast<size_t>(slotIndex)];
    if (slot.progressionId.isEmpty())
        return {};

    if (!song.findProgressionById(slot.progressionId).has_value())
        return {};

    return slot.progressionId;
}

void GrabSamplerQueue::setSong(model::Song* songState)
{
    songRef = songState;
}

void GrabSamplerQueue::setOnQueueChanged(std::function<void()> callback)
{
    onQueueChanged = std::move(callback);
}

bool GrabSamplerQueue::isValidSlot(int slotIndex) const
{
    return slotIndex >= 0 && slotIndex < kSlotCount;
}

te::AudioTrack* GrabSamplerQueue::findOrCreateSamplerTrack(te::Edit& edit)
{
    auto tracks = te::getAudioTracks(edit);
    for (auto* track : tracks)
    {
        if (track != nullptr && track->state.getProperty("setleSystemTag").toString() == kSamplerTrackTag)
            return track;
    }

    auto created = edit.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(edit), nullptr, false);
    if (created == nullptr)
        return nullptr;

    created->state.setProperty("setleSystemTag", kSamplerTrackTag, nullptr);
    created->setName("Sampler Queue");
    return created.get();
}

void GrabSamplerQueue::rebuildSamplerClipForSlot(te::Edit& edit, int slotIndex, te::AudioTrack& track)
{
    if (!isValidSlot(slotIndex) || songRef == nullptr)
        return;

    auto progressionOpt = songRef->findProgressionById(slots[static_cast<size_t>(slotIndex)].progressionId);
    if (!progressionOpt.has_value())
        return;

    juce::Array<te::Clip*> oldClips;
    for (auto* clip : track.getClips())
        oldClips.add(clip);
    for (auto* clip : oldClips)
        clip->removeFromParent();

    const auto chords = progressionOpt->getChords();
    if (chords.empty())
        return;

    const auto bpm = juce::jmax(1.0, songRef->getBpm());
    const auto secPerBeat = 60.0 / bpm;

    double totalBeats = 0.0;
    for (const auto& chord : chords)
    {
        const auto d = chord.getDurationBeats();
        totalBeats += (d > 0.0 ? d : 1.0);
    }

    const auto clipRange = tracktion::TimeRange(
        tracktion::TimePosition::fromSeconds(0.0),
        tracktion::TimePosition::fromSeconds(totalBeats * secPerBeat));

    auto clip = track.insertMIDIClip(clipRange, nullptr);
    if (clip == nullptr)
        return;

    auto& seq = clip->getSequence();
    double beatPos = 0.0;
    for (const auto& chord : chords)
    {
        auto d = chord.getDurationBeats();
        if (d <= 0.0)
            d = 1.0;

        const auto startBeat = tracktion::BeatPosition::fromBeats(beatPos);
        const auto beatLen = tracktion::BeatDuration::fromBeats(d * 0.95);

        const auto notes = chord.getNotes();
        if (notes.empty())
        {
            seq.addNote(chord.getRootMidi(), startBeat, beatLen, 90, 0, nullptr);
        }
        else
        {
            for (const auto& note : notes)
            {
                seq.addNote(note.getPitch(),
                            startBeat,
                            beatLen,
                            static_cast<int>(note.getVelocity() * 127.0f),
                            0,
                            nullptr);
            }
        }

        beatPos += d;
    }
}

void GrabSamplerQueue::rebuildSamplerClipForAudioSlot(te::AudioTrack& track, double durationSeconds)
{
    juce::Array<te::Clip*> oldClips;
    for (auto* clip : track.getClips())
        oldClips.add(clip);
    for (auto* clip : oldClips)
        clip->removeFromParent();

    const auto clipRange = tracktion::TimeRange(
        tracktion::TimePosition::fromSeconds(0.0),
        tracktion::TimePosition::fromSeconds(durationSeconds));

    auto clip = track.insertMIDIClip(clipRange, nullptr);
    if (clip == nullptr)
        return;

    auto& seq = clip->getSequence();
    seq.addNote(60,
                tracktion::BeatPosition::fromBeats(0.0),
                tracktion::BeatDuration::fromBeats(0.25),
                100,
                0,
                nullptr);
}

void GrabSamplerQueue::notifyQueueChanged()
{
    if (onQueueChanged)
        onQueueChanged();
}

} // namespace setle::capture
