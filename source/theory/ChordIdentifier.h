#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace setle::theory
{

struct ChordIdentification
{
    juce::String root;
    juce::String quality;
    juce::String symbol;
    float confidence = 0.0f;
};

ChordIdentification identify(const std::vector<int>& pitchClasses);

} // namespace setle::theory
