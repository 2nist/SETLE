#pragma once

#include <juce_core/juce_core.h>
#include <tracktion_engine/tracktion_engine.h>

#include <utility>
#include <vector>

namespace te = tracktion::engine;

namespace setle::midi
{

class MidiChordAnalyzer
{
public:
    struct ChordEvent
    {
        double startBeat { 0.0 };
        double durationBeats { 0.0 };
        juce::String symbol;
        juce::String quality;
        int rootMidi { 60 };
        float confidence { 0.0f };
        std::vector<int> pitchClasses;
        std::vector<int> midiNotes;
    };

    struct AnalysisResult
    {
        std::vector<ChordEvent> chords;
        double totalBeats { 0.0 };
        double detectedBpm { 120.0 };
        juce::String detectedKey;
        juce::String detectedMode;
        float keyConfidence { 0.0f };
    };

    static AnalysisResult analyzeClip(const te::MidiClip& clip,
                                      double bpm,
                                      const juce::String& hintKey = {},
                                      const juce::String& hintMode = {});

    static AnalysisResult analyzeTrack(const te::AudioTrack& track,
                                       double bpm);

private:
    static std::vector<std::pair<double, std::vector<int>>> clusterNotes(const std::vector<std::pair<double, int>>& noteOns,
                                                                          double toleranceBeats);

    static ChordEvent identifyChord(double startBeat,
                                    double durationBeats,
                                    const std::vector<int>& notes);

    static std::pair<juce::String, juce::String> detectKeyMode(const std::vector<ChordEvent>& chords,
                                                                float& outConfidence);
};

} // namespace setle::midi
