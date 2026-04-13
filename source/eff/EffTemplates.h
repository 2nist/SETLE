#pragma once

#include "EffModel.h"
#include <vector>

namespace setle::eff
{

class EffTemplates
{
public:
    // Returns all 12 built-in template definitions
    static std::vector<EffDefinition> getBuiltInTemplates();

    // Find by name (returns empty EffDefinition with effId=="" if not found)
    static EffDefinition findByName(const juce::String& name);
};

} // namespace setle::eff
