#pragma once

#include <juce_core/juce_core.h>

namespace setle::theory
{

class TheoryEngine
{
public:
    static int applyModalTransform(int midiPitch,
                                   const juce::String& sessionKey,
                                   const juce::String& sourceMode,
                                   const juce::String& targetMode);

    static int transposeToMode(int midiPitch,
                               const juce::String& sessionKey,
                               const juce::String& sourceMode,
                               const juce::String& targetMode);
};

} // namespace setle::theory
