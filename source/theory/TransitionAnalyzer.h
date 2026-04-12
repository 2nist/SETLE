#pragma once

#include <juce_core/juce_core.h>

#include "../model/SetleSongModel.h"

namespace setle::theory
{

struct BoundaryScore
{
    enum class Level
    {
        green,
        yellow,
        red
    };

    Level level = Level::yellow;
    juce::uint32 colourArgb = 0xffd9b84f;
    juce::String label;
};

class TransitionAnalyzer
{
public:
    explicit TransitionAnalyzer(const model::Song& songIn) : song(songIn) {}

    BoundaryScore getBoundaryScore(const juce::String& sectionIdA,
                                   const juce::String& sectionIdB) const;

private:
    const model::Song& song;
};

} // namespace setle::theory
