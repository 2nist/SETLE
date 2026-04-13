#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

namespace setle::gridroll
{

class DrumPatternMidiReader
{
public:
    struct Result
    {
        bool success { false };
        int stepCount { 16 };
        juce::Array<uint8_t> velocities;
        std::vector<std::pair<int, uint8_t>> stepEvents;
    };

    static Result readFile(const juce::File& midiFile, int stepCount = 16);
};

} // namespace setle::gridroll
