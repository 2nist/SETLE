#include "MeterContext.h"

#include "../model/SetleSongModel.h"

namespace setle::theory
{

MeterContext MeterContext::forBeat(double beat, const setle::model::Song& song)
{
    const auto sections = song.getSections();
    const auto progressions = song.getProgressions();

    // If no sections, use default meter
    if (sections.empty())
        return MeterContext { 4, 4 };

    double currentStartBeat = 0.0;

    for (const auto& section : sections)
    {
        // Calculate this section's total duration by summing its progressions
        double sectionDurationBeats = 0.0;

        for (const auto& ref : section.getProgressionRefs())
        {
            for (const auto& prog : progressions)
            {
                if (prog.getId() == ref.getProgressionId())
                {
                    // Sum the durations of all chords in this progression
                    for (const auto& chord : prog.getChords())
                        sectionDurationBeats += chord.getDurationBeats();
                    break;
                }
            }
        }

        // If section has no progressions, give it a default duration (one bar)
        // This allows sections with explicit meter settings to still work
        if (sectionDurationBeats <= 0.0)
            sectionDurationBeats = 4.0; // default to 4/4 one bar

        const double currentEndBeat = currentStartBeat + sectionDurationBeats;

        // Check if the beat falls within this section's range
        if (beat >= currentStartBeat && beat < currentEndBeat)
        {
            return MeterContext {
                section.getTimeSigNumerator(),
                section.getTimeSigDenominator()
            };
        }

        currentStartBeat = currentEndBeat;
    }

    // Beat is beyond all sections; use the last section's meter
    if (!sections.empty())
    {
        const auto& lastSection = sections.back();
        return MeterContext {
            lastSection.getTimeSigNumerator(),
            lastSection.getTimeSigDenominator()
        };
    }

    return MeterContext { 4, 4 };
}

} // namespace setle::theory
