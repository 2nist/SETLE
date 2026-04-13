#pragma once

#include <juce_core/juce_core.h>

#include <vector>

namespace setle::gridroll
{

struct DrumPattern
{
    juce::String id;
    juce::String name;
    juce::String category;
    juce::String style;
    juce::String feel;
    juce::String timeSignature;
    juce::File midiFile;
};

class DrumPatternLibrary
{
public:
    void loadFromManifest(const juce::File& manifestFile, const juce::File& patternsRootDir);

    const std::vector<DrumPattern>& getPatterns() const noexcept { return patterns; }
    std::vector<DrumPattern> getPatternsForCategory(const juce::String& category) const;

private:
    std::vector<DrumPattern> patterns;
};

} // namespace setle::gridroll
