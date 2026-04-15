#include "MidiToProgressionConverter.h"

#include "../theory/BachTheory.h"
#include "../theory/DiatonicHarmony.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>

namespace setle::midi
{
namespace
{
juce::String sectionLabelForOrdinal(int index)
{
    static constexpr std::array<const char*, 8> sectionNames { "Intro", "Verse", "Chorus", "Bridge", "Verse 2", "Chorus 2", "Outro", "Section" };

    if (index >= 0 && index < static_cast<int>(sectionNames.size()))
        return sectionNames[static_cast<size_t>(index)];

    return "Section " + juce::String(index - static_cast<int>(sectionNames.size()) + 2);
}

juce::String sectionNameForProgression(const model::Progression& progression)
{
    const auto name = progression.getName();
    const auto separator = name.indexOf(juce::String::fromUTF8(" — "));
    if (separator >= 0)
        return name.substring(separator + 3).trim();

    return name;
}
} // namespace

MidiToProgressionConverter::ConversionResult MidiToProgressionConverter::convert(const MidiChordAnalyzer::AnalysisResult& analysis,
                                                                                 const juce::String& songTitle)
{
    ConversionResult result;
    result.detectedKey = analysis.detectedKey;
    result.detectedMode = analysis.detectedMode;
    result.keyConfidence = analysis.keyConfidence;

    if (analysis.chords.empty())
        return result;

    std::vector<juce::String> chordNames;
    chordNames.reserve(analysis.chords.size());
    for (const auto& chord : analysis.chords)
        chordNames.push_back(chord.symbol);

    struct PatternCandidate
    {
        std::vector<juce::String> chords;
        int repetitions { 0 };
        int length { 0 };
        float score { 0.0f };
    };

    std::vector<PatternCandidate> candidates;

    for (int len = 2; len <= 6; ++len)
    {
        auto patterns = findRepeatingPatterns(chordNames, len, 2);
        for (const auto& pattern : patterns)
        {
            int repetitions = 0;
            for (size_t i = 0; i + static_cast<size_t>(len) <= chordNames.size(); ++i)
            {
                bool match = true;
                for (int j = 0; j < len; ++j)
                {
                    if (chordNames[i + static_cast<size_t>(j)] != pattern[static_cast<size_t>(j)])
                    {
                        match = false;
                        break;
                    }
                }

                if (match)
                    ++repetitions;
            }

            if (repetitions >= 2)
                candidates.push_back({ pattern, repetitions, len, static_cast<float>(repetitions * len) });
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b)
    {
        return a.score > b.score;
    });

    std::vector<int> chordToProgression(analysis.chords.size(), -1);
    std::map<std::vector<juce::String>, int> patternToProgressionIndex;

    int sectionNameIndex = 0;

    for (const auto& candidate : candidates)
    {
        if (result.progressions.size() >= 8)
            break;

        if (patternToProgressionIndex.count(candidate.chords) > 0)
            continue;

        const auto len = candidate.length;
        std::vector<size_t> firstIndices;

        for (size_t i = 0; i + static_cast<size_t>(len) <= chordNames.size(); ++i)
        {
            bool match = true;
            for (int j = 0; j < len; ++j)
            {
                if (chordNames[i + static_cast<size_t>(j)] != candidate.chords[static_cast<size_t>(j)])
                {
                    match = false;
                    break;
                }
            }

            if (!match)
                continue;

            bool overlapsAssigned = false;
            for (int j = 0; j < len; ++j)
            {
                if (chordToProgression[i + static_cast<size_t>(j)] != -1)
                {
                    overlapsAssigned = true;
                    break;
                }
            }

            if (!overlapsAssigned)
            {
                for (int j = 0; j < len; ++j)
                    firstIndices.push_back(i + static_cast<size_t>(j));
                break;
            }
        }

        if (firstIndices.empty())
            continue;

        const auto sectionName = sectionLabelForOrdinal(sectionNameIndex++);
        auto progression = makeProgression(analysis.chords,
                                           firstIndices,
                                           analysis.detectedKey,
                                           analysis.detectedMode,
                                           songTitle + " — " + sectionName);

        const int progressionIndex = static_cast<int>(result.progressions.size());
        patternToProgressionIndex[candidate.chords] = progressionIndex;
        result.progressions.push_back(std::move(progression));

        for (size_t i = 0; i + static_cast<size_t>(len) <= chordNames.size(); ++i)
        {
            bool match = true;
            for (int j = 0; j < len; ++j)
            {
                if (chordNames[i + static_cast<size_t>(j)] != candidate.chords[static_cast<size_t>(j)])
                {
                    match = false;
                    break;
                }
            }

            if (!match)
                continue;

            for (int j = 0; j < len; ++j)
            {
                auto& slot = chordToProgression[i + static_cast<size_t>(j)];
                if (slot == -1)
                    slot = progressionIndex;
            }
        }
    }

    std::vector<size_t> unassigned;
    for (size_t i = 0; i < chordToProgression.size(); ++i)
    {
        if (chordToProgression[i] == -1)
            unassigned.push_back(i);
    }

    if (!unassigned.empty())
    {
        auto progression = makeProgression(analysis.chords,
                                           unassigned,
                                           analysis.detectedKey,
                                           analysis.detectedMode,
                                           songTitle + " — Other");

        const int otherIndex = static_cast<int>(result.progressions.size());
        result.progressions.push_back(std::move(progression));

        for (auto idx : unassigned)
            chordToProgression[idx] = otherIndex;
    }

    if (!result.progressions.empty() && !chordToProgression.empty())
    {
        int previousProgressionIndex = chordToProgression.front();

        for (size_t i = 1; i < chordToProgression.size(); ++i)
        {
            const int currentProgressionIndex = chordToProgression[i];
            if (currentProgressionIndex == previousProgressionIndex)
                continue;

            if (previousProgressionIndex >= 0
                && previousProgressionIndex < static_cast<int>(result.progressions.size()))
            {
                const auto& progression = result.progressions[static_cast<size_t>(previousProgressionIndex)];
                auto section = model::Section::create(sectionNameForProgression(progression));
                section.setTimeSigNumerator(4);
                section.setTimeSigDenominator(4);
                section.addProgressionRef(model::SectionProgressionRef::create(progression.getId(), 0, {}));
                result.sections.push_back(std::move(section));
            }

            previousProgressionIndex = currentProgressionIndex;
        }

        if (previousProgressionIndex >= 0
            && previousProgressionIndex < static_cast<int>(result.progressions.size()))
        {
            const auto& progression = result.progressions[static_cast<size_t>(previousProgressionIndex)];
            auto section = model::Section::create(sectionNameForProgression(progression));
            section.setTimeSigNumerator(4);
            section.setTimeSigDenominator(4);
            section.addProgressionRef(model::SectionProgressionRef::create(progression.getId(), 0, {}));
            result.sections.push_back(std::move(section));
        }
    }

    result.numProgressions = static_cast<int>(result.progressions.size());
    result.numSections = static_cast<int>(result.sections.size());
    result.numChords = static_cast<int>(analysis.chords.size());

    return result;
}

std::vector<std::vector<juce::String>> MidiToProgressionConverter::findRepeatingPatterns(const std::vector<juce::String>& chordNames,
                                                                                          int windowSize,
                                                                                          int minRepetitions)
{
    std::map<std::vector<juce::String>, int> counts;

    if (windowSize <= 0 || chordNames.size() < static_cast<size_t>(windowSize))
        return {};

    for (size_t i = 0; i + static_cast<size_t>(windowSize) <= chordNames.size(); ++i)
    {
        std::vector<juce::String> window(chordNames.begin() + static_cast<std::ptrdiff_t>(i),
                                         chordNames.begin() + static_cast<std::ptrdiff_t>(i + static_cast<size_t>(windowSize)));
        counts[window]++;
    }

    std::vector<std::vector<juce::String>> result;
    for (const auto& [pattern, count] : counts)
    {
        if (count >= minRepetitions)
            result.push_back(pattern);
    }

    return result;
}

std::vector<std::vector<size_t>> MidiToProgressionConverter::segmentByPattern(const std::vector<MidiChordAnalyzer::ChordEvent>& chords,
                                                                               const std::vector<std::vector<juce::String>>& patterns)
{
    std::vector<std::vector<size_t>> segments;

    if (chords.empty() || patterns.empty())
        return segments;

    std::vector<juce::String> chordNames;
    chordNames.reserve(chords.size());
    for (const auto& chord : chords)
        chordNames.push_back(chord.symbol);

    for (const auto& pattern : patterns)
    {
        const auto len = pattern.size();
        if (len == 0)
            continue;

        for (size_t i = 0; i + len <= chordNames.size(); ++i)
        {
            bool match = true;
            for (size_t j = 0; j < len; ++j)
            {
                if (chordNames[i + j] != pattern[j])
                {
                    match = false;
                    break;
                }
            }

            if (!match)
                continue;

            std::vector<size_t> indices;
            indices.reserve(len);
            for (size_t j = 0; j < len; ++j)
                indices.push_back(i + j);
            segments.push_back(std::move(indices));
        }
    }

    return segments;
}

model::Progression MidiToProgressionConverter::makeProgression(const std::vector<MidiChordAnalyzer::ChordEvent>& allChords,
                                                               const std::vector<size_t>& indices,
                                                               const juce::String& key,
                                                               const juce::String& mode,
                                                               const juce::String& name)
{
    auto progression = model::Progression::create(name,
                                                  key.isNotEmpty() ? key : "C",
                                                  mode.isNotEmpty() ? mode : "ionian");

    if (indices.empty())
        return progression;

    std::vector<size_t> sorted = indices;
    std::sort(sorted.begin(), sorted.end());

    double beatCursor = 0.0;

    for (auto index : sorted)
    {
        if (index >= allChords.size())
            continue;

        const auto& chordEvent = allChords[index];

        auto chord = model::Chord::create(chordEvent.symbol,
                                          chordEvent.quality,
                                          chordEvent.rootMidi);

        chord.setFunction(setle::theory::DiatonicHarmony::functionForChord(chordEvent.symbol,
                                                                            key,
                                                                            mode));
        chord.setSource("midi");
        chord.setConfidence(chordEvent.confidence);
        chord.setStartBeats(beatCursor);

        const auto chordDuration = juce::jmax(0.25, chordEvent.durationBeats);
        chord.setDurationBeats(chordDuration);

        auto pitchClasses = setle::theory::BachTheory::getChordPitchClasses(chordEvent.symbol,
                                                                             key,
                                                                             mode);
        if (pitchClasses.empty())
            pitchClasses = chordEvent.pitchClasses;

        const int rootPc = ((chordEvent.rootMidi % 12) + 12) % 12;
        for (int pitchClass : pitchClasses)
        {
            int midiNote = 60 + ((pitchClass - rootPc + 12) % 12);
            if (midiNote < 48)
                midiNote += 12;
            if (midiNote > 84)
                midiNote -= 12;

            chord.addNote(model::Note::create(midiNote,
                                              0.8f,
                                              beatCursor,
                                              chordDuration * 0.95,
                                              1));
        }

        progression.addChord(chord);
        beatCursor += chordDuration;
    }

    progression.setLengthBeats(beatCursor);
    return progression;
}

} // namespace setle::midi
