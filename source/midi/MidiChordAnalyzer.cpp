#include "MidiChordAnalyzer.h"

#include "../theory/DiatonicHarmony.h"

#include <algorithm>
#include <array>
#include <random>
#include <set>

namespace setle::midi
{

MidiChordAnalyzer::AnalysisResult MidiChordAnalyzer::analyzeClip(const te::MidiClip& clip,
                                                                 double bpm,
                                                                 const juce::String& hintKey,
                                                                 const juce::String& hintMode)
{
    AnalysisResult result;
    result.detectedBpm = bpm;

    const auto& nonConstClip = const_cast<te::MidiClip&>(clip);
    const auto notes = nonConstClip.getSequence().getNotes();

    std::vector<std::pair<double, int>> noteOns;
    noteOns.reserve(static_cast<size_t>(notes.size()));

    for (auto* note : notes)
    {
        if (note == nullptr)
            continue;

        noteOns.push_back({ note->getStartBeat().inBeats(), note->getNoteNumber() });
        result.totalBeats = juce::jmax(result.totalBeats,
                                       note->getStartBeat().inBeats() + note->getLengthBeats().inBeats());
    }

    if (noteOns.empty())
        return result;

    std::sort(noteOns.begin(), noteOns.end(), [](const auto& a, const auto& b)
    {
        if (a.first != b.first)
            return a.first < b.first;
        return a.second < b.second;
    });

    const auto clusters = clusterNotes(noteOns, 0.15);
    if (clusters.empty())
        return result;

    for (size_t i = 0; i < clusters.size(); ++i)
    {
        const auto startBeat = clusters[i].first;
        const auto nextBeat = (i + 1 < clusters.size())
                                  ? clusters[i + 1].first
                                  : juce::jmax(result.totalBeats, startBeat + 1.0);
        const auto duration = juce::jmax(0.25, nextBeat - startBeat);

        auto chord = identifyChord(startBeat, duration, clusters[i].second);
        if (chord.confidence > 0.3f)
            result.chords.push_back(std::move(chord));
    }

    if (hintKey.isNotEmpty())
    {
        result.detectedKey = hintKey;
        result.detectedMode = hintMode.isNotEmpty() ? hintMode : "ionian";
        result.keyConfidence = 1.0f;
    }
    else
    {
        std::tie(result.detectedKey, result.detectedMode) = detectKeyMode(result.chords, result.keyConfidence);
    }

    return result;
}

MidiChordAnalyzer::AnalysisResult MidiChordAnalyzer::analyzeTrack(const te::AudioTrack& track,
                                                                  double bpm)
{
    AnalysisResult result;
    result.detectedBpm = bpm;

    const double secPerBeat = 60.0 / juce::jmax(1.0, bpm);

    for (auto* clip : track.getClips())
    {
        auto* midiClip = dynamic_cast<te::MidiClip*>(clip);
        if (midiClip == nullptr)
            continue;

        auto clipAnalysis = analyzeClip(*midiClip, bpm);
        const auto clipStartBeat = clip->getPosition().getStart().inSeconds() / secPerBeat;

        for (auto chord : clipAnalysis.chords)
        {
            chord.startBeat += clipStartBeat;
            result.chords.push_back(std::move(chord));
        }

        result.totalBeats = juce::jmax(result.totalBeats, clipStartBeat + clipAnalysis.totalBeats);
    }

    std::sort(result.chords.begin(), result.chords.end(), [](const auto& a, const auto& b)
    {
        return a.startBeat < b.startBeat;
    });

    std::tie(result.detectedKey, result.detectedMode) = detectKeyMode(result.chords, result.keyConfidence);
    return result;
}

std::vector<std::pair<double, std::vector<int>>> MidiChordAnalyzer::clusterNotes(const std::vector<std::pair<double, int>>& noteOns,
                                                                                  double toleranceBeats)
{
    std::vector<std::pair<double, std::vector<int>>> clusters;
    if (noteOns.empty())
        return clusters;

    double clusterStart = noteOns.front().first;
    std::vector<int> current { noteOns.front().second };

    for (size_t i = 1; i < noteOns.size(); ++i)
    {
        if (noteOns[i].first - clusterStart <= toleranceBeats)
        {
            current.push_back(noteOns[i].second);
        }
        else
        {
            clusters.push_back({ clusterStart, current });
            clusterStart = noteOns[i].first;
            current = { noteOns[i].second };
        }
    }

    clusters.push_back({ clusterStart, current });
    return clusters;
}

MidiChordAnalyzer::ChordEvent MidiChordAnalyzer::identifyChord(double startBeat,
                                                               double durationBeats,
                                                               const std::vector<int>& notes)
{
    ChordEvent event;
    event.startBeat = startBeat;
    event.durationBeats = durationBeats;
    event.midiNotes = notes;

    if (notes.empty())
        return event;

    std::set<int> pcSet;
    for (int note : notes)
        pcSet.insert(((note % 12) + 12) % 12);

    event.pitchClasses.assign(pcSet.begin(), pcSet.end());

    static const std::vector<std::pair<std::vector<int>, juce::String>> chordTypes {
        {{ 0, 4, 7, 11 }, "maj7"},
        {{ 0, 3, 7, 10 }, "m7"},
        {{ 0, 4, 7, 10 }, "7"},
        {{ 0, 3, 6, 10 }, "m7b5"},
        {{ 0, 3, 6, 9 }, "dim7"},
        {{ 0, 4, 7, 9 }, "maj6"},
        {{ 0, 3, 7, 9 }, "m6"},
        {{ 0, 4, 7 }, "maj"},
        {{ 0, 3, 7 }, "min"},
        {{ 0, 3, 6 }, "dim"},
        {{ 0, 4, 8 }, "aug"},
        {{ 0, 2, 7 }, "sus2"},
        {{ 0, 5, 7 }, "sus4"}
    };

    static constexpr std::array<const char*, 12> noteNames { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };

    int bestRoot = 0;
    juce::String bestQuality = "maj";
    float bestScore = 0.0f;

    for (int root = 0; root < 12; ++root)
    {
        std::set<int> intervals;
        for (int pc : event.pitchClasses)
            intervals.insert((pc - root + 12) % 12);

        for (const auto& [typeIntervals, typeName] : chordTypes)
        {
            std::set<int> typeSet(typeIntervals.begin(), typeIntervals.end());

            int matches = 0;
            for (int interval : typeSet)
                if (intervals.count(interval) > 0)
                    ++matches;

            if (typeSet.empty() || intervals.empty())
                continue;

            const float coverage = static_cast<float>(matches) / static_cast<float>(typeSet.size());
            const float precision = static_cast<float>(matches) / static_cast<float>(intervals.size());
            const float score = coverage * 0.7f + precision * 0.3f;

            if (score > bestScore && coverage >= 0.66f)
            {
                bestScore = score;
                bestRoot = root;
                bestQuality = typeName;
            }
        }
    }

    if (bestScore <= 0.0f)
    {
        bestRoot = *std::min_element(event.pitchClasses.begin(), event.pitchClasses.end());
        bestQuality = "cluster";
        bestScore = 0.25f;
    }

    event.rootMidi = 60 + bestRoot;
    event.confidence = bestScore;
    event.quality = bestQuality;

    const auto rootName = juce::String(noteNames[static_cast<size_t>(bestRoot % 12)]);
    if (bestQuality == "maj")
        event.symbol = rootName;
    else if (bestQuality == "min")
        event.symbol = rootName + "m";
    else if (bestQuality == "cluster")
        event.symbol = rootName + "(cluster)";
    else
        event.symbol = rootName + bestQuality;

    return event;
}

std::pair<juce::String, juce::String> MidiChordAnalyzer::detectKeyMode(const std::vector<ChordEvent>& chords,
                                                                        float& outConfidence)
{
    if (chords.empty())
    {
        outConfidence = 0.0f;
        return { "C", "ionian" };
    }

    static constexpr std::array<const char*, 12> keys { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static constexpr std::array<const char*, 7> modes { "ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian", "locrian" };

    float bestScore = 0.0f;
    juce::String bestKey = "C";
    juce::String bestMode = "ionian";

    for (const auto* key : keys)
    {
        for (const auto* mode : modes)
        {
            const int rootPc = setle::theory::DiatonicHarmony::pitchClassForRoot(key);
            const auto intervals = setle::theory::DiatonicHarmony::modeIntervals(mode);

            std::set<int> diatonic;
            for (int interval : intervals)
                diatonic.insert((rootPc + interval) % 12);

            float score = 0.0f;
            float totalDuration = 0.0f;

            for (const auto& chord : chords)
            {
                int inKey = 0;
                for (int pc : chord.pitchClasses)
                    if (diatonic.count(pc) > 0)
                        ++inKey;

                const float fit = chord.pitchClasses.empty()
                                      ? 0.0f
                                      : static_cast<float>(inKey) / static_cast<float>(chord.pitchClasses.size());

                score += fit * static_cast<float>(chord.durationBeats);
                totalDuration += static_cast<float>(chord.durationBeats);
            }

            if (totalDuration > 0.0f)
                score /= totalDuration;

            if (score > bestScore)
            {
                bestScore = score;
                bestKey = key;
                bestMode = mode;
            }
        }
    }

    outConfidence = bestScore;
    return { bestKey, bestMode };
}

} // namespace setle::midi
