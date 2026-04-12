#include "GrabSamplerQueue.h"

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
    if (slot.state == GrabSlot::State::Empty || slot.progressionId.isEmpty() || songRef == nullptr)
        return;

    if (activeSlot >= 0 && activeSlot != slotIndex)
        stopPlayback(edit);

    auto* track = findOrCreateSamplerTrack(edit);
    if (track == nullptr)
        return;

    prePlayheadSeconds = edit.getTransport().getPosition().inSeconds();
    rebuildSamplerClipForSlot(edit, slotIndex, *track);

    auto progressionOpt = songRef->findProgressionById(slot.progressionId);
    if (!progressionOpt.has_value())
        return;

    double totalBeats = 0.0;
    for (const auto& chord : progressionOpt->getChords())
    {
        const auto d = chord.getDurationBeats();
        totalBeats += (d > 0.0 ? d : 1.0);
    }

    const auto bpm = juce::jmax(1.0, songRef->getBpm());
    const auto totalSeconds = (60.0 / bpm) * totalBeats;

    edit.getTransport().setLoopRange({ te::TimePosition::fromSeconds(0.0), te::TimeDuration::fromSeconds(totalSeconds) });
    edit.getTransport().looping = slot.looping;
    edit.getTransport().setPosition(te::TimePosition::fromSeconds(0.0));
    edit.getTransport().play(false);

    slot.state = GrabSlot::State::Playing;
    activeSlot = slotIndex;
    notifyQueueChanged();
}

void GrabSamplerQueue::stopPlayback(te::Edit& edit)
{
    edit.getTransport().stop(false, false);
    edit.getTransport().setPosition(te::TimePosition::fromSeconds(prePlayheadSeconds));

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

void GrabSamplerQueue::notifyQueueChanged()
{
    if (onQueueChanged)
        onQueueChanged();
}

} // namespace setle::capture
