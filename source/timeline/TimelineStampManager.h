#pragma once

#include <tracktion_engine/tracktion_engine.h>

#include <functional>
#include <map>
#include <optional>

#include "../model/SetleSongModel.h"

namespace te = tracktion::engine;

namespace setle::timeline
{

class TimelineStampManager
{
public:
    struct SectionBounds
    {
        double startBeat { 0.0 };
        double lengthBeats { 0.0 };

        double endBeat() const { return startBeat + lengthBeats; }
    };

    struct GhostTrail
    {
        SectionBounds active;
        SectionBounds origin;
    };

    enum class DuplicateStampMode
    {
        exactObject = 0,
        modalExtraction,
        rhythmicDecimation
    };

    explicit TimelineStampManager(te::Edit& edit);

    void applySectionReorder(const model::Song& song,
                             const juce::StringArray& previousOrder,
                             const juce::StringArray& newOrder,
                             const std::function<double(const model::Section&)>& sectionLengthProvider);

    void stampSectionDuplicate(const model::Song& song,
                               const juce::String& sourceSectionId,
                               const juce::String& stampedSectionId,
                               DuplicateStampMode mode,
                               double insertionBeat,
                               const juce::String& sessionKey,
                               const juce::String& sourceMode,
                               const juce::String& targetMode,
                               const std::function<double(const model::Section&)>& sectionLengthProvider);

    std::optional<GhostTrail> getGhostTrail(const juce::String& sectionId) const;

private:
    using BoundsMap = std::map<juce::String, SectionBounds>;
    using ProgMap = std::map<juce::String, juce::String>;

    static BoundsMap buildBoundsMap(const model::Song& song,
                                    const juce::StringArray& order,
                                    const std::function<double(const model::Section&)>& sectionLengthProvider);
    static ProgMap buildProgressionSectionMap(const model::Song& song);

    juce::String resolveClipSectionId(const te::Clip& clip,
                                      const ProgMap& progressionSectionMap,
                                      const BoundsMap& fallbackBounds) const;
    double getSecondsPerBeat() const;
    void shiftClipByBeats(te::Clip& clip, double deltaBeats) const;
    void moveClipToStartBeat(te::Clip& clip, double startBeat) const;
    te::Clip* findClipByStateId(te::AudioTrack& track, const juce::String& clipStateId) const;

    te::Edit& edit;
    std::map<juce::String, GhostTrail> ghostTrails;
};

} // namespace setle::timeline
