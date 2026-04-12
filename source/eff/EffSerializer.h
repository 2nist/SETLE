#pragma once

#include "EffModel.h"
#include <juce_data_structures/juce_data_structures.h>

namespace setle::eff
{

class EffSerializer
{
public:
    static juce::String serialize(const EffDefinition& def);
    static EffDefinition deserialize(const juce::String& json);
    static bool saveToFile(const EffDefinition& def, const juce::File& file);
    static EffDefinition loadFromFile(const juce::File& file);
};

} // namespace setle::eff
