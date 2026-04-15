#include "TimelineStampManager.h"

#include "../theory/TheoryEngine.h"

#include <cmath>
#include <set>
#include <vector>

namespace
{
const juce::Identifier kSectionIdProp { "setleSectionId" };
const juce::Identifier kSectionLocalStartProp { "setleSectionLocalStartBeats" };
const juce::Identifier kPlaybackSpeedProp { "setlePlaybackSpeed" };
const juce::Identifier kProgressionIdProp { "progressionId" };

juce::StringArray buildOrderFromSong(const setle::model::Song& song)
{
    juce::StringArray ids;
    for (const auto& section : song.getSections())
        ids.add(section.getId());
    return ids;
}
} // namespace

namespace setle::timeline
{

TimelineStampManager::TimelineStampManager(te::Edit& editToUse)
    : edit(editToUse)
{
}

void TimelineStampManager::applySectionReorder(const model::Song& song,
                                               const juce::StringArray& previousOrder,
                                               const juce::StringArray& newOrder,
                                               const std::function<double(const model::Section&)>& sectionLengthProvider)
{
    static constexpr double kEpsilon = 1.0e-6;

    const auto effectivePreviousOrder = previousOrder.isEmpty() ? buildOrderFromSong(song) : previousOrder;
    const auto effectiveNewOrder = newOrder.isEmpty() ? buildOrderFromSong(song) : newOrder;
    const auto oldBounds = buildBoundsMap(song,
                                          effectivePreviousOrder,
                                          sectionLengthProvider);
    const auto newBounds = buildBoundsMap(song,
                                          effectiveNewOrder,
                                          sectionLengthProvider);
    if (oldBounds.empty() || newBounds.empty())
        return;

    ghostTrails.clear();
    for (const auto& [sectionId, activeBounds] : newBounds)
    {
        const auto oldIt = oldBounds.find(sectionId);
        const auto originBounds = oldIt != oldBounds.end() ? oldIt->second : activeBounds;
        ghostTrails[sectionId] = GhostTrail { activeBounds, originBounds };
    }

    auto& undoManager = edit.getUndoManager();

    const auto progMap = buildProgressionSectionMap(song);
    struct ClipLink
    {
        te::Clip* clip { nullptr };
        juce::String sectionId;
    };
    std::vector<ClipLink> linkedClips;

    for (auto* track : te::getAudioTracks(edit))
    {
        if (track == nullptr)
            continue;

        for (auto* clip : track->getClips())
        {
            if (clip == nullptr)
                continue;

            auto sectionId = resolveClipSectionId(*clip, progMap, oldBounds);
            if (sectionId.isEmpty())
                continue;

            const auto oldIt = oldBounds.find(sectionId);
            const auto newIt = newBounds.find(sectionId);
            if (oldIt == oldBounds.end() || newIt == newBounds.end())
                continue;

            const auto clipStartBeat = clip->getPosition().getStart().inSeconds() / getSecondsPerBeat();
            const auto localStart = clip->state.hasProperty(kSectionLocalStartProp)
                                        ? static_cast<double>(clip->state.getProperty(kSectionLocalStartProp))
                                        : clipStartBeat - oldIt->second.startBeat;
            const auto targetStartBeat = newIt->second.startBeat + localStart;
            const auto delta = targetStartBeat - clipStartBeat;
            if (std::abs(delta) > kEpsilon)
                shiftClipByBeats(*clip, delta);

            clip->state.setProperty(kSectionIdProp, sectionId, &undoManager);
            clip->state.setProperty(kSectionLocalStartProp, localStart, &undoManager);
            linkedClips.push_back({ clip, sectionId });
        }
    }

    // Ripple pass: keep the spool gapless after movement by shifting each section
    // so it starts where the previous section actually ends.
    std::map<juce::String, double> observedSectionEnd;
    for (const auto& [sectionId, bounds] : newBounds)
        observedSectionEnd[sectionId] = bounds.endBeat();

    for (const auto& link : linkedClips)
    {
        if (link.clip == nullptr || link.sectionId.isEmpty())
            continue;

        const auto startBeat = link.clip->getPosition().getStart().inSeconds() / getSecondsPerBeat();
        const auto endBeat = startBeat + link.clip->getPosition().getLength().inSeconds() / getSecondsPerBeat();
        auto& currentEnd = observedSectionEnd[link.sectionId];
        currentEnd = juce::jmax(currentEnd, endBeat);
    }

    std::map<juce::String, double> sectionShift;
    double runningCursorBeat = 0.0;
    for (const auto& sectionId : effectiveNewOrder)
    {
        const auto boundsIt = newBounds.find(sectionId);
        if (boundsIt == newBounds.end())
            continue;

        const auto observedEnd = observedSectionEnd[sectionId];
        const auto baseStart = boundsIt->second.startBeat;
        const auto shift = runningCursorBeat - baseStart;
        sectionShift[sectionId] = shift;
        runningCursorBeat = observedEnd + shift;
    }

    for (const auto& link : linkedClips)
    {
        if (link.clip == nullptr || link.sectionId.isEmpty())
            continue;

        const auto shiftIt = sectionShift.find(link.sectionId);
        if (shiftIt == sectionShift.end() || std::abs(shiftIt->second) <= kEpsilon)
            continue;

        const auto boundsIt = newBounds.find(link.sectionId);
        if (boundsIt == newBounds.end())
            continue;

        shiftClipByBeats(*link.clip, shiftIt->second);
        const auto shiftedSectionStart = boundsIt->second.startBeat + shiftIt->second;
        const auto shiftedClipStartBeat = link.clip->getPosition().getStart().inSeconds() / getSecondsPerBeat();
        link.clip->state.setProperty(kSectionLocalStartProp,
                                     shiftedClipStartBeat - shiftedSectionStart,
                                     &undoManager);
        link.clip->state.setProperty(kSectionIdProp, link.sectionId, &undoManager);
    }
}

void TimelineStampManager::stampSectionDuplicate(const model::Song& song,
                                                 const juce::String& sourceSectionId,
                                                 const juce::String& stampedSectionId,
                                                 DuplicateStampMode mode,
                                                 double insertionBeat,
                                                 const juce::String& sessionKey,
                                                 const juce::String& sourceMode,
                                                 const juce::String& targetMode,
                                                 const std::function<double(const model::Section&)>& sectionLengthProvider)
{
    static constexpr double kMinClipLenSeconds = 1.0e-3;
    const auto order = buildOrderFromSong(song);
    const auto bounds = buildBoundsMap(song, order, sectionLengthProvider);
    const auto sourceBoundsIt = bounds.find(sourceSectionId);
    const auto stampedBoundsIt = bounds.find(stampedSectionId);
    if (sourceBoundsIt == bounds.end() || stampedBoundsIt == bounds.end())
        return;

    const auto sourceBounds = sourceBoundsIt->second;
    if (insertionBeat < 0.0)
        insertionBeat = sourceBounds.endBeat();

    auto& undoManager = edit.getUndoManager();
    undoManager.beginNewTransaction("Stamp Mosaic Duplicate");

    const auto progMap = buildProgressionSectionMap(song);
    const auto insertionDelta = insertionBeat - sourceBounds.startBeat;
    const auto stampedLengthBeats = juce::jmax(0.001, stampedBoundsIt->second.lengthBeats);

    for (auto* track : te::getAudioTracks(edit))
    {
        if (track == nullptr)
            continue;

        for (auto* clip : track->getClips())
        {
            if (clip == nullptr)
                continue;

            const auto clipStartBeat = clip->getPosition().getStart().inSeconds() / getSecondsPerBeat();
            const auto clipSectionId = resolveClipSectionId(*clip, progMap, bounds);

            // Ripple-edit any material at/after the insertion boundary to keep clips non-overlapping.
            if (clipStartBeat >= insertionBeat - 1.0e-6
                && clipSectionId != sourceSectionId
                && stampedLengthBeats > 1.0e-6)
            {
                shiftClipByBeats(*clip, stampedLengthBeats);
                if (clipSectionId.isNotEmpty())
                {
                    const auto boundsIt = bounds.find(clipSectionId);
                    if (boundsIt != bounds.end())
                    {
                        const auto shiftedBeat = clipStartBeat + stampedLengthBeats;
                        const auto shiftedSectionStart = boundsIt->second.startBeat
                                                        + (boundsIt->second.startBeat >= insertionBeat - 1.0e-6
                                                               ? stampedLengthBeats
                                                               : 0.0);
                        clip->state.setProperty(kSectionLocalStartProp,
                                                shiftedBeat - shiftedSectionStart,
                                                &undoManager);
                    }
                }
            }
        }
    }

    for (auto* track : te::getAudioTracks(edit))
    {
        if (track == nullptr)
            continue;

        juce::Array<te::Clip*> sourceClips;
        for (auto* clip : track->getClips())
        {
            if (clip == nullptr)
                continue;
            if (resolveClipSectionId(*clip, progMap, bounds) == sourceSectionId)
                sourceClips.add(clip);
        }

        for (auto* sourceClip : sourceClips)
        {
            auto clonedState = sourceClip->state.createCopy();
            const auto newClipId = juce::Uuid().toString();
            clonedState.setProperty("id", newClipId, nullptr);
            clonedState.setProperty(kSectionIdProp, stampedSectionId, nullptr);
            clonedState.setProperty(kPlaybackSpeedProp,
                                    mode == DuplicateStampMode::rhythmicDecimation ? 0.5 : 1.0,
                                    nullptr);
            track->state.appendChild(clonedState, &undoManager);

            auto* newClip = findClipByStateId(*track, newClipId);
            if (newClip == nullptr)
                continue;

            const auto sourceStartBeat = sourceClip->getPosition().getStart().inSeconds() / getSecondsPerBeat();
            moveClipToStartBeat(*newClip, sourceStartBeat + insertionDelta);

            const auto localStart = (sourceStartBeat + insertionDelta) - insertionBeat;
            newClip->state.setProperty(kSectionLocalStartProp, localStart, &undoManager);

            if (mode == DuplicateStampMode::modalExtraction)
            {
                if (auto* midiClip = dynamic_cast<te::MidiClip*>(newClip))
                {
                    for (auto* note : midiClip->getSequence().getNotes())
                    {
                        if (note == nullptr)
                            continue;

                        const auto transposed = setle::theory::TheoryEngine::applyModalTransform(note->getNoteNumber(),
                                                                                                   sessionKey,
                                                                                                   sourceMode,
                                                                                                   targetMode);
                        note->setNoteNumber(transposed, &undoManager);
                    }
                }
            }

            if (mode == DuplicateStampMode::rhythmicDecimation)
            {
                if (auto* midiClip = dynamic_cast<te::MidiClip*>(newClip))
                {
                    auto& sequence = midiClip->getSequence();
                    for (auto* note : sequence.getNotes())
                    {
                        if (note == nullptr)
                            continue;

                        note->setStartAndLength(note->getStartBeat() * 2.0,
                                                note->getLengthBeats() * 2.0,
                                                &undoManager);
                    }

                    const auto currentPos = midiClip->getPosition();
                    midiClip->setPosition(te::ClipPosition {
                        { currentPos.getStart(),
                          tracktion::TimeDuration::fromSeconds(juce::jmax(kMinClipLenSeconds,
                                                                          currentPos.getLength().inSeconds() * 2.0)) },
                        currentPos.getOffset()
                    });
                }

                if (auto* audioClip = dynamic_cast<te::AudioClipBase*>(newClip))
                    audioClip->setSpeedRatio(0.5);
            }
        }
    }

    ghostTrails[stampedSectionId] = GhostTrail { stampedBoundsIt->second, { insertionBeat, sourceBounds.lengthBeats } };
}

std::optional<TimelineStampManager::GhostTrail> TimelineStampManager::getGhostTrail(const juce::String& sectionId) const
{
    const auto it = ghostTrails.find(sectionId);
    if (it == ghostTrails.end())
        return std::nullopt;

    return it->second;
}

TimelineStampManager::BoundsMap TimelineStampManager::buildBoundsMap(
    const model::Song& song,
    const juce::StringArray& order,
    const std::function<double(const model::Section&)>& sectionLengthProvider)
{
    BoundsMap bounds;
    double cursor = 0.0;

    for (const auto& sectionId : order)
    {
        const auto section = song.findSectionById(sectionId);
        if (!section.has_value())
            continue;

        const auto length = juce::jmax(0.001, sectionLengthProvider(section.value()));
        bounds[sectionId] = { cursor, length };
        cursor += length;
    }

    return bounds;
}

TimelineStampManager::ProgMap TimelineStampManager::buildProgressionSectionMap(const model::Song& song)
{
    std::map<juce::String, std::set<juce::String>> candidates;
    for (const auto& section : song.getSections())
        for (const auto& ref : section.getProgressionRefs())
            candidates[ref.getProgressionId()].insert(section.getId());

    ProgMap map;
    for (const auto& [progressionId, sectionIds] : candidates)
    {
        if (sectionIds.size() == 1)
            map[progressionId] = *sectionIds.begin();
    }
    return map;
}

juce::String TimelineStampManager::resolveClipSectionId(const te::Clip& clip,
                                                        const ProgMap& progressionSectionMap,
                                                        const BoundsMap& fallbackBounds) const
{
    const auto explicitSectionId = clip.state.getProperty(kSectionIdProp).toString();
    if (explicitSectionId.isNotEmpty())
        return explicitSectionId;

    const auto progressionId = clip.state.getProperty(kProgressionIdProp).toString();
    if (progressionId.isNotEmpty())
    {
        const auto progIt = progressionSectionMap.find(progressionId);
        if (progIt != progressionSectionMap.end())
            return progIt->second;
    }

    const auto clipStartBeat = clip.getPosition().getStart().inSeconds() / getSecondsPerBeat();
    for (const auto& [sectionId, bounds] : fallbackBounds)
    {
        if (clipStartBeat >= bounds.startBeat - 1.0e-6 && clipStartBeat < bounds.endBeat() + 1.0e-6)
            return sectionId;
    }

    return {};
}

double TimelineStampManager::getSecondsPerBeat() const
{
    const auto* tempo = edit.tempoSequence.getTempo(0);
    const auto bpm = tempo != nullptr ? tempo->getBpm() : 120.0;
    return 60.0 / juce::jmax(1.0, bpm);
}

void TimelineStampManager::shiftClipByBeats(te::Clip& clip, double deltaBeats) const
{
    const auto secPerBeat = getSecondsPerBeat();
    auto pos = clip.getPosition();
    const auto newStart = pos.getStart().inSeconds() + deltaBeats * secPerBeat;
    clip.setPosition(te::ClipPosition { { tracktion::TimePosition::fromSeconds(juce::jmax(0.0, newStart)),
                                          pos.getLength() },
                                        pos.getOffset() });
}

void TimelineStampManager::moveClipToStartBeat(te::Clip& clip, double startBeat) const
{
    const auto secPerBeat = getSecondsPerBeat();
    auto pos = clip.getPosition();
    clip.setPosition(te::ClipPosition { { tracktion::TimePosition::fromSeconds(juce::jmax(0.0, startBeat * secPerBeat)),
                                          pos.getLength() },
                                        pos.getOffset() });
}

te::Clip* TimelineStampManager::findClipByStateId(te::AudioTrack& track, const juce::String& clipStateId) const
{
    for (auto* clip : track.getClips())
        if (clip != nullptr && clip->state.getProperty("id").toString() == clipStateId)
            return clip;

    return nullptr;
}

} // namespace setle::timeline
