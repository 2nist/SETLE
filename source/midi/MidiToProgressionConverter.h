#pragma once

#include "MidiChordAnalyzer.h"
#include "../model/SetleSongModel.h"

#include <vector>

namespace setle::midi
{

class MidiToProgressionConverter
{
public:
    struct ConversionResult
    {
        std::vector<model::Progression> progressions;
        std::vector<model::Section> sections;

        int numProgressions { 0 };
        int numSections { 0 };
        int numChords { 0 };
        juce::String detectedKey;
        juce::String detectedMode;
        float keyConfidence { 0.0f };
    };

    static ConversionResult convert(const MidiChordAnalyzer::AnalysisResult& analysis,
                                    const juce::String& songTitle);

private:
    static std::vector<std::vector<juce::String>> findRepeatingPatterns(const std::vector<juce::String>& chordNames,
                                                                         int windowSize,
                                                                         int minRepetitions);

    static std::vector<std::vector<size_t>> segmentByPattern(const std::vector<MidiChordAnalyzer::ChordEvent>& chords,
                                                             const std::vector<std::vector<juce::String>>& patterns);

    static model::Progression makeProgression(const std::vector<MidiChordAnalyzer::ChordEvent>& chords,
                                              const std::vector<size_t>& indices,
                                              const juce::String& key,
                                              const juce::String& mode,
                                              const juce::String& name);
};

} // namespace setle::midi
