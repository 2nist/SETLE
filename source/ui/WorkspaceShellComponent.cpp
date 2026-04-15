#include "WorkspaceShellComponent.h"

#include "ToolPaletteComponent.h"
#include "EditToolManager.h"
#include "AppCommands.h"
#include "AppMenuBarModel.h"
#include "LeftNavComponent.h"
#include "ZoneHeaderComponent.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <numeric>
#include <set>

#include "../theme/ThemePresets.h"
#include "../theme/ThemeStyleHelpers.h"
#include "../theory/BachTheory.h"
#include "../theory/DiatonicHarmony.h"
#include "../theory/TheoryEngine.h"

namespace te = tracktion::engine;

namespace setle::ui
{

namespace
{
void shellTrace(const juce::String& message)
{
    const auto stamp = juce::Time::getCurrentTime().toString(true, true);
    juce::Logger::writeToLog("[SETLE][Shell][" + stamp + "] " + message);
}

enum TheoryActionId
{
    sectionEditTheory = 101,
    sectionSetRepeatPattern = 102,
    sectionAddTransitionAnchor = 103,
    sectionConflictCheck = 104,

    chordEdit = 201,
    chordSubstitution = 202,
    chordSetFunction = 203,
    chordScopeOccurrence = 204,
    chordScopeRepeat = 205,
    chordScopeSelectedRepeats = 206,
    chordScopeAllRepeats = 207,
    chordSnapWholeBar = 208,
    chordSnapHalfBar = 209,
    chordSnapBeat = 210,
    chordSnapHalfBeat = 211,
    chordSnapEighth = 212,
    chordSnapSixteenth = 213,
    chordSnapThirtySecond = 214,

    noteEditTheory = 301,
    noteQuantizeScale = 302,
    noteConvertToChord = 303,
    noteDeriveProgression = 304,

    progressionGrabSampler = 401,
    progressionCreateCandidate = 402,
    progressionAnnotateKeyMode = 403,
    progressionTagTransition = 404,
    progressionEditIdentity = 405,
    progressionForkVariant = 406,
    progressionAddChord = 407,
    progressionClearChords = 408,
    progressionAssignToSection = 409,
    progressionSubAllDominants = 410,
    progressionTransposeToKey = 411,
    progressionSaveAsTemplate = 412,
    progressionBrowseLibrary = 420,

    historyGrab4 = 501,
    historyGrab8 = 502,
    historyGrab16 = 503,
    historyGrabCustom = 504,
    historySetLength = 505,
    historyChangeSource = 506,
    historyClear = 507,
    historyAutoGrabToggle = 508,
    historyPreviewMode = 509,
    historySetBpm = 510,
    historySyncToTransport = 511,
    historyExportMidi = 512,
    historyCaptureSingle = 513,

    sectionRename = 105,
    sectionDelete = 106,
    sectionDuplicate = 107,
    sectionSetColor = 108,
    sectionSetTimeSig = 109,
    sectionExportMidi = 110,
    sectionJumpTo = 111,

    chordDelete = 215,
    chordDuplicate = 216,
    chordInsertBefore = 217,
    chordInsertAfter = 218,
    chordNudgeLeft = 219,
    chordNudgeRight = 220,
    chordSetDuration = 221,
    chordInvertVoicing = 222,
    chordSendToGridRoll = 223
};
static const juce::Identifier kInstrumentSlotsContainerId { "instrumentSlots" };
static const juce::Identifier kInstrumentSlotEntryId { "instrumentSlot" };
static const juce::Identifier kSlotTrackIdProp { "trackId" };
static const juce::Identifier kSlotTypeProp { "slotType" };
static const juce::Identifier kSlotPersistentIdProp { "persistentId" };
static const juce::Identifier kSlotPersistentNameProp { "persistentName" };

te::AudioTrack* findAudioTrackById(setle::timeline::TrackManager* trackManager, const juce::String& trackId)
{
    if (trackManager == nullptr)
        return nullptr;

    for (auto* track : trackManager->getAllUserTracks())
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*>(track))
            if (audioTrack->itemID.toString() == trackId)
                return audioTrack;

    return nullptr;
}

juce::String findSidechainSourceChoice(te::Plugin& plugin, const juce::String& trackName)
{
    for (const auto& choice : plugin.getSidechainSourceNames(false))
        if (choice.endsWithIgnoreCase(trackName))
            return choice;

    return {};
}

te::CompressorPlugin* findExistingTracktionCompressor(te::AudioTrack& track)
{
    for (auto* plugin : track.pluginList.getPlugins())
        if (auto* compressor = dynamic_cast<te::CompressorPlugin*>(plugin))
            return compressor;

    return nullptr;
}

te::CompressorPlugin* ensureTracktionCompressor(te::AudioTrack& track, te::Edit& edit)
{
    if (auto* existing = findExistingTracktionCompressor(track))
        return existing;

    auto plugin = edit.getPluginCache().createNewPlugin(te::CompressorPlugin::xmlTypeName, {});
    auto* compressor = dynamic_cast<te::CompressorPlugin*>(plugin.get());
    if (compressor == nullptr)
        return nullptr;

    const auto existingPlugins = track.pluginList.getPlugins();
    const int insertIndex = juce::jmax(0, existingPlugins.size() - 2);
    track.pluginList.insertPlugin(plugin, insertIndex, nullptr);
    return compressor;
}

std::optional<float> measureIntegratedLoudnessLufs(const juce::AudioBuffer<float>& sourceBuffer, double sampleRate)
{
    if (sourceBuffer.getNumChannels() <= 0 || sourceBuffer.getNumSamples() <= 0 || sampleRate <= 0.0)
        return std::nullopt;

    juce::AudioBuffer<float> weighted(sourceBuffer.getNumChannels(), sourceBuffer.getNumSamples());
    weighted.makeCopyOf(sourceBuffer, true);

    auto highPass = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 38.0f);
    auto highShelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 1500.0f, 0.7071f,
                                                                        juce::Decibels::decibelsToGain(4.0f));

    for (int ch = 0; ch < weighted.getNumChannels(); ++ch)
    {
        juce::dsp::IIR::Filter<float> hpFilter;
        juce::dsp::IIR::Filter<float> shelfFilter;
        hpFilter.coefficients = highPass;
        shelfFilter.coefficients = highShelf;
        hpFilter.reset();
        shelfFilter.reset();

        auto* data = weighted.getWritePointer(ch);
        for (int sampleIndex = 0; sampleIndex < weighted.getNumSamples(); ++sampleIndex)
            data[sampleIndex] = shelfFilter.processSample(hpFilter.processSample(data[sampleIndex]));
    }

    const int blockSize = juce::jmax(1, static_cast<int>(std::round(sampleRate * 0.4)));
    const int hopSize = juce::jmax(1, static_cast<int>(std::round(sampleRate * 0.1)));

    std::vector<double> gatedBlockEnergies;
    for (int start = 0; start + blockSize <= weighted.getNumSamples(); start += hopSize)
    {
        double energy = 0.0;
        for (int ch = 0; ch < weighted.getNumChannels(); ++ch)
        {
            const auto* data = weighted.getReadPointer(ch, start);
            for (int i = 0; i < blockSize; ++i)
            {
                const double sample = static_cast<double>(data[i]);
                energy += sample * sample;
            }
        }

        energy /= static_cast<double>(blockSize * weighted.getNumChannels());
        if (energy <= 0.0)
            continue;

        const auto blockLufs = static_cast<float>(-0.691 + 10.0 * std::log10(energy));
        if (blockLufs >= -70.0f)
            gatedBlockEnergies.push_back(energy);
    }

    if (gatedBlockEnergies.empty())
        return std::nullopt;

    const auto absoluteMeanEnergy = std::accumulate(gatedBlockEnergies.begin(), gatedBlockEnergies.end(), 0.0)
                                    / static_cast<double>(gatedBlockEnergies.size());
    const auto ungatedLufs = -0.691 + 10.0 * std::log10(absoluteMeanEnergy);
    const auto relativeGate = ungatedLufs - 10.0;

    std::vector<double> integratedEnergies;
    integratedEnergies.reserve(gatedBlockEnergies.size());
    for (const auto energy : gatedBlockEnergies)
    {
        const auto blockLufs = -0.691 + 10.0 * std::log10(energy);
        if (blockLufs >= relativeGate)
            integratedEnergies.push_back(energy);
    }

    if (integratedEnergies.empty())
        integratedEnergies = gatedBlockEnergies;

    const auto integratedMeanEnergy = std::accumulate(integratedEnergies.begin(), integratedEnergies.end(), 0.0)
                                      / static_cast<double>(integratedEnergies.size());
    if (integratedMeanEnergy <= 0.0)
        return std::nullopt;

    return static_cast<float>(-0.691 + 10.0 * std::log10(integratedMeanEnergy));
}

int scoreSidechainSourceCandidate(const juce::String& trackName,
                                  setle::instruments::InstrumentSlot::SlotType slotType)
{
    auto lower = trackName.toLowerCase();
    int score = 0;

    if (slotType == setle::instruments::InstrumentSlot::SlotType::DrumMachine)
        score += 50;
    if (lower.contains("kick"))
        score += 120;
    if (lower.contains("drum"))
        score += 70;
    if (lower.contains("beat") || lower.contains("perc"))
        score += 25;

    return score;
}

int scoreSidechainTargetCandidate(const juce::String& trackName,
                                  setle::instruments::InstrumentSlot::SlotType slotType,
                                  bool isFocusedTrack)
{
    auto lower = trackName.toLowerCase();
    int score = 0;

    if (slotType == setle::instruments::InstrumentSlot::SlotType::DrumMachine
        || slotType == setle::instruments::InstrumentSlot::SlotType::MidiOut
        || slotType == setle::instruments::InstrumentSlot::SlotType::Empty)
        return -1;

    if (isFocusedTrack)
        score += 55;
    if (lower.contains("bass") || lower.contains("sub"))
        score += 120;
    if (lower.contains("pad"))
        score += 95;
    if (lower.contains("synth") || lower.contains("poly"))
        score += 25;
    if (slotType == setle::instruments::InstrumentSlot::SlotType::PolySynth)
        score += 20;
    if (slotType == setle::instruments::InstrumentSlot::SlotType::ReelSampler
        || slotType == setle::instruments::InstrumentSlot::SlotType::VST3)
        score += 10;

    return score;
}

class OutputCaptureTap final : public juce::AudioIODeviceCallback
{
public:
    explicit OutputCaptureTap(setle::capture::CircularAudioBuffer& bufferToWrite)
        : buffer(bufferToWrite)
    {
    }

    void setOnAudioBlockCaptured(std::function<void(float, int)> callback)
    {
        onAudioBlockCaptured = std::move(callback);
    }

    void audioDeviceIOCallbackWithContext(const float* const*,
                                          int,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override
    {
        buffer.pushAudioBlock(outputChannelData, numOutputChannels, numSamples);

        if (onAudioBlockCaptured == nullptr || outputChannelData == nullptr || numSamples <= 0 || numOutputChannels <= 0)
            return;

        float peak = 0.0f;
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            const auto* channel = outputChannelData[ch];
            if (channel == nullptr)
                continue;

            for (int s = 0; s < numSamples; ++s)
                peak = juce::jmax(peak, std::abs(channel[s]));
        }

        onAudioBlockCaptured(peak, numSamples);
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        if (device != nullptr)
            buffer.prepare(device->getCurrentSampleRate(),
                           juce::jmax(1, device->getActiveOutputChannels().countNumberOfSetBits()));
    }

    void audioDeviceStopped() override {}

private:
    setle::capture::CircularAudioBuffer& buffer;
    std::function<void(float, int)> onAudioBlockCaptured;
};

juce::String slotTypeToString(setle::instruments::InstrumentSlot::SlotType type)
{
    using SlotType = setle::instruments::InstrumentSlot::SlotType;

    switch (type)
    {
        case SlotType::PolySynth:   return "PolySynth";
        case SlotType::DrumMachine: return "DrumMachine";
        case SlotType::ReelSampler: return "ReelSampler";
        case SlotType::VST3:        return "VST3";
        case SlotType::MidiOut:     return "MidiOut";
        case SlotType::Empty:       break;
    }

    return "Empty";
}

setle::instruments::InstrumentSlot::SlotType slotTypeFromString(const juce::String& value)
{
    using SlotType = setle::instruments::InstrumentSlot::SlotType;
    if (value == "PolySynth") return SlotType::PolySynth;
    if (value == "DrumMachine") return SlotType::DrumMachine;
    if (value == "ReelSampler") return SlotType::ReelSampler;
    if (value == "VST3") return SlotType::VST3;
    if (value == "MidiOut") return SlotType::MidiOut;
    return SlotType::Empty;
}

juce::String chordRootNameForMidi(int midiNote)
{
    static constexpr std::array<const char*, 12> names { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    const auto pitchClass = ((midiNote % 12) + 12) % 12;
    return names[static_cast<size_t>(pitchClass)];
}

int pitchClassForKeyName(const juce::String& keyName)
{
    const auto normalized = keyName.trim();
    if (normalized.equalsIgnoreCase("C")) return 0;
    if (normalized.equalsIgnoreCase("C#") || normalized.equalsIgnoreCase("Db")) return 1;
    if (normalized.equalsIgnoreCase("D")) return 2;
    if (normalized.equalsIgnoreCase("D#") || normalized.equalsIgnoreCase("Eb")) return 3;
    if (normalized.equalsIgnoreCase("E") || normalized.equalsIgnoreCase("Fb")) return 4;
    if (normalized.equalsIgnoreCase("F") || normalized.equalsIgnoreCase("E#")) return 5;
    if (normalized.equalsIgnoreCase("F#") || normalized.equalsIgnoreCase("Gb")) return 6;
    if (normalized.equalsIgnoreCase("G")) return 7;
    if (normalized.equalsIgnoreCase("G#") || normalized.equalsIgnoreCase("Ab")) return 8;
    if (normalized.equalsIgnoreCase("A")) return 9;
    if (normalized.equalsIgnoreCase("A#") || normalized.equalsIgnoreCase("Bb")) return 10;
    if (normalized.equalsIgnoreCase("B") || normalized.equalsIgnoreCase("Cb")) return 11;
    return 0;
}

juce::String transposeChordSymbolRoot(const juce::String& symbol, int semitones)
{
    if (symbol.isEmpty())
        return symbol;

    auto rootLength = 1;
    if (symbol.length() > 1)
    {
        const auto accidental = symbol.substring(1, 2);
        if (accidental == "#" || accidental == "b")
            rootLength = 2;
    }

    const auto oldRoot = symbol.substring(0, rootLength);
    const auto suffix = symbol.substring(rootLength);
    const auto oldPc = pitchClassForKeyName(oldRoot);
    const auto newRoot = chordRootNameForMidi(oldPc + semitones);
    return newRoot + suffix;
}

int quantizeToCMajor(int midiNote)
{
    static constexpr std::array<int, 7> scale { 0, 2, 4, 5, 7, 9, 11 };

    const auto octave = midiNote / 12;
    const auto pitchClass = ((midiNote % 12) + 12) % 12;

    auto nearest = scale[0];
    auto bestDistance = std::abs(pitchClass - nearest);

    for (const auto note : scale)
    {
        const auto deltaFromScale = std::abs(pitchClass - note);
        if (deltaFromScale < bestDistance)
        {
            bestDistance = deltaFromScale;
            nearest = note;
        }
    }

    return juce::jlimit(0, 127, octave * 12 + nearest);
}

juce::String nextSectionName(const juce::String& current)
{
    static constexpr std::array<const char*, 5> names { "Verse", "Pre-Chorus", "Chorus", "Bridge", "Outro" };

    for (size_t i = 0; i < names.size(); ++i)
        if (current.equalsIgnoreCase(names[i]))
            return names[(i + 1) % names.size()];

    return names[0];
}

juce::String relativeModeFor(const juce::String& mode)
{
    const auto lower = mode.trim().toLowerCase();
    if (lower == "ionian" || lower == "major") return "dorian";
    if (lower == "dorian") return "aeolian";
    if (lower == "phrygian") return "aeolian";
    if (lower == "lydian") return "mixolydian";
    if (lower == "mixolydian") return "dorian";
    if (lower == "aeolian" || lower == "minor") return "dorian";
    if (lower == "locrian") return "phrygian";
    return "dorian";
}

juce::String qualitySuffixFor(const setle::model::Chord& chord)
{
    const auto quality = chord.getQuality().toLowerCase();
    if (quality.contains("major7")) return "maj7";
    if (quality.contains("dominant")) return "7";
    if (quality.contains("minor7")) return "m7";
    if (quality.contains("minor")) return "m";
    if (quality.contains("dim")) return "dim";
    if (quality.contains("aug")) return "aug";
    return {};
}

int nearestDegreeIndex(int pitchClass, const std::vector<int>& intervals)
{
    int bestIndex = 0;
    int bestDistance = 12;
    for (int i = 0; i < static_cast<int>(intervals.size()); ++i)
    {
        const auto distance = std::abs(intervals[static_cast<size_t>(i)] - pitchClass);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

juce::String nextFunction(const juce::String& current)
{
    if (current.equalsIgnoreCase("T")) return "PD";
    if (current.equalsIgnoreCase("PD")) return "D";
    if (current.equalsIgnoreCase("D")) return "T";
    return "T";
}

juce::String cycleChordSymbol(const juce::String& current)
{
    static constexpr std::array<const char*, 6> symbols { "Cmaj7", "Am7", "Dm7", "G7", "Fmaj7", "Em7b5" };

    for (size_t i = 0; i < symbols.size(); ++i)
        if (current.equalsIgnoreCase(symbols[i]))
            return symbols[(i + 1) % symbols.size()];

    return symbols[0];
}

int parseIntOr(const juce::String& value, int fallback)
{
    const auto trimmed = value.trim();
    if (trimmed.isEmpty())
        return fallback;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtol(utf8, &end, 10);

    if (utf8 == end || *end != '\0' || errno == ERANGE)
        return fallback;

    return static_cast<int>(parsed);
}

double parseDoubleOr(const juce::String& value, double fallback)
{
    const auto trimmed = value.trim();
    if (trimmed.isEmpty())
        return fallback;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtod(utf8, &end);

    if (utf8 == end || *end != '\0' || errno == ERANGE)
        return fallback;

    return parsed;
}

float parseFloatOr(const juce::String& value, float fallback)
{
    return static_cast<float>(parseDoubleOr(value, static_cast<double>(fallback)));
}

bool isStrictInt(const juce::String& value)
{
    const auto trimmed = value.trim();
    if (trimmed.isEmpty())
        return false;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    std::strtol(utf8, &end, 10);
    return (utf8 != end && *end == '\0' && errno != ERANGE);
}

bool isStrictDouble(const juce::String& value)
{
    const auto trimmed = value.trim();
    if (trimmed.isEmpty())
        return false;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    std::strtod(utf8, &end);
    return (utf8 != end && *end == '\0' && errno != ERANGE);
}

std::set<int> parseRepeatIndexSet(const juce::String& serialized)
{
    std::set<int> result;
    const auto tokens = juce::StringArray::fromTokens(serialized, ",", "");

    for (auto token : tokens)
    {
        token = token.trim();
        if (token.isEmpty())
            continue;

        const auto dashIndex = token.indexOfChar('-');
        if (dashIndex <= 0 || dashIndex >= token.length() - 1)
        {
            const auto value = token.getIntValue();
            if (value > 0)
                result.insert(value);
            continue;
        }

        const auto left = token.substring(0, dashIndex).trim().getIntValue();
        const auto right = token.substring(dashIndex + 1).trim().getIntValue();
        if (left <= 0 || right <= 0)
            continue;

        const auto start = juce::jmin(left, right);
        const auto end = juce::jmax(left, right);
        for (int value = start; value <= end; ++value)
            result.insert(value);
    }

    return result;
}

juce::String serializeRepeatIndexSet(const std::set<int>& indices)
{
    if (indices.empty())
        return {};

    juce::StringArray parts;

    auto it = indices.begin();
    while (it != indices.end())
    {
        const auto rangeStart = *it;
        auto rangeEnd = rangeStart;
        ++it;

        while (it != indices.end() && *it == rangeEnd + 1)
        {
            rangeEnd = *it;
            ++it;
        }

        if (rangeStart == rangeEnd)
            parts.add(juce::String(rangeStart));
        else
            parts.add(juce::String(rangeStart) + "-" + juce::String(rangeEnd));
    }

    return parts.joinIntoString(",");
}

juce::String normalizeRepeatIndices(const juce::String& serialized, const juce::String& fallback)
{
    auto parsed = parseRepeatIndexSet(serialized);
    if (parsed.empty())
        parsed = parseRepeatIndexSet(fallback);

    if (parsed.empty())
        parsed.insert(1);

    return serializeRepeatIndexSet(parsed);
}
} // namespace

//==============================================================================
class WorkspaceShellComponent::InDevicePanel final : public juce::Component,
                                                     private juce::Timer,
                                                     private juce::MidiInputCallback
{
public:
    using QueueProvider = std::function<setle::capture::GrabSamplerQueue*()>;
    using SongProvider = std::function<const model::Song*()>;

    struct QueueActions
    {
        std::function<void(int)> playSlot;
        std::function<void()> stopPlayback;
        std::function<void(int)> editSlot;
        std::function<void(int)> promoteSlot;
        std::function<void(int)> dropToTimeline;
        std::function<void(int)> clearSlot;
        std::function<void(int)> toggleLoop;
        std::function<void(int)> renameSlot;
        std::function<void(juce::Component&, const juce::String&)> showProgressionContextMenu;
    };

private:
    class GrabSlotCard final : public juce::Component
    {
    public:
        GrabSlotCard(int indexInQueue,
                     QueueProvider queueProviderIn,
                     SongProvider songProviderIn,
                     QueueActions& queueActionsIn)
            : slotIndex(indexInQueue),
              queueProvider(std::move(queueProviderIn)),
              songProvider(std::move(songProviderIn)),
              queueActions(queueActionsIn)
        {
            playButton.onClick = [this]
            {
                const auto* slot = getSlot();
                if (slot == nullptr)
                    return;

                if (slot->state == capture::GrabSlot::State::Playing)
                {
                    if (queueActions.stopPlayback)
                        queueActions.stopPlayback();
                    return;
                }

                if (queueActions.playSlot)
                    queueActions.playSlot(slotIndex);
            };

            editButton.setButtonText("Edit");
            editButton.onClick = [this]
            {
                if (queueActions.editSlot)
                    queueActions.editSlot(slotIndex);
            };

            promoteButton.setButtonText("-> Library");
            promoteButton.onClick = [this]
            {
                if (queueActions.promoteSlot)
                    queueActions.promoteSlot(slotIndex);
            };

            addAndMakeVisible(playButton);
            addAndMakeVisible(editButton);
            addAndMakeVisible(promoteButton);
            refresh();
        }

        void refresh()
        {
            const auto* slot = getSlot();
            const auto isReady = slot != nullptr && slot->state != capture::GrabSlot::State::Empty;

            playButton.setVisible(isReady);
            editButton.setVisible(isReady);
            promoteButton.setVisible(isReady);

            if (slot != nullptr && slot->state == capture::GrabSlot::State::Playing)
                playButton.setButtonText("Stop");
            else if (slot != nullptr && slot->hasCoupledAudio())
                playButton.setButtonText("Play Reel");
            else
                playButton.setButtonText("Play MIDI");

            repaint();
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(8, 6);
            auto buttonRow = area.removeFromBottom(24);
            const int buttonWidth = juce::jmax(54, (buttonRow.getWidth() - 12) / 3);
            playButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
            buttonRow.removeFromLeft(6);
            editButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
            buttonRow.removeFromLeft(6);
            promoteButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
        }

        void mouseUp(const juce::MouseEvent& event) override
        {
            if (!event.mods.isRightButtonDown())
                return;

            const auto* slot = getSlot();
            if (slot == nullptr || slot->state == capture::GrabSlot::State::Empty)
                return;

            juce::PopupMenu menu;
            if (slot->state == capture::GrabSlot::State::Playing)
                menu.addItem(1, "Stop");
            else
                menu.addItem(1, "Play");

            menu.addItem(2, "Loop", true, slot->looping);
            menu.addSeparator();
            menu.addItem(3, "Rename...");
            menu.addItem(4, "Edit in Theory Panel");
            menu.addItem(8, "Progression Actions...");
            menu.addSeparator();
            menu.addItem(5, "Promote to Library");
            menu.addItem(6, "Drop to Timeline at Playhead");
            menu.addSeparator();
            menu.addItem(7, "Clear Slot");

            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                               [this](int selected)
                               {
                                   if (selected == 1)
                                   {
                                       const auto* slotState = getSlot();
                                       if (slotState == nullptr)
                                           return;

                                       if (slotState->state == capture::GrabSlot::State::Playing)
                                       {
                                           if (queueActions.stopPlayback)
                                               queueActions.stopPlayback();
                                       }
                                       else if (queueActions.playSlot)
                                       {
                                           queueActions.playSlot(slotIndex);
                                       }
                                   }
                                   else if (selected == 2)
                                   {
                                       if (queueActions.toggleLoop)
                                           queueActions.toggleLoop(slotIndex);
                                   }
                                   else if (selected == 3)
                                   {
                                       if (queueActions.renameSlot)
                                           queueActions.renameSlot(slotIndex);
                                   }
                                   else if (selected == 4)
                                   {
                                       if (queueActions.editSlot)
                                           queueActions.editSlot(slotIndex);
                                   }
                                   else if (selected == 8)
                                   {
                                       const auto* slotState = getSlot();
                                       if (slotState != nullptr && queueActions.showProgressionContextMenu)
                                           queueActions.showProgressionContextMenu(*this, slotState->progressionId);
                                   }
                                   else if (selected == 5)
                                   {
                                       if (queueActions.promoteSlot)
                                           queueActions.promoteSlot(slotIndex);
                                   }
                                   else if (selected == 6)
                                   {
                                       if (queueActions.dropToTimeline)
                                           queueActions.dropToTimeline(slotIndex);
                                   }
                                   else if (selected == 7)
                                   {
                                       if (queueActions.clearSlot)
                                           queueActions.clearSlot(slotIndex);
                                   }
                               });
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().reduced(2);
            const auto pulse = 0.5f + 0.5f * std::sin(static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.01));
            const auto& themeData = ThemeManager::get().theme();

            const auto* slot = getSlot();
            if (slot == nullptr || slot->state == capture::GrabSlot::State::Empty)
            {
                const auto style = setle::theme::cardStyle(themeData, setle::theme::SurfaceState::muted);
                g.setColour(style.fill);
                g.fillRoundedRectangle(bounds.toFloat(), style.radius);
                g.setColour(style.outline);
                g.drawRoundedRectangle(bounds.toFloat(), style.radius, style.stroke);
                // subtle dashed frame for empty slots
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.36f));
                for (int x = bounds.getX() + 6; x < bounds.getRight() - 6; x += 6)
                    g.fillRect(x, bounds.getY() + 5, 3, 1);
                g.setFont(juce::FontOptions(12.0f));
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::muted));
                g.drawText("[ Empty - drag or grab ]", bounds, juce::Justification::centred, true);
                return;
            }

            const bool playing = slot->state == capture::GrabSlot::State::Playing;
            const auto state = playing ? setle::theme::SurfaceState::success : setle::theme::SurfaceState::normal;
            auto style = setle::theme::cardStyle(themeData, state);
            g.setColour(style.fill);
            g.fillRoundedRectangle(bounds.toFloat(), style.radius);
            g.setColour(playing ? style.outline.withAlpha(0.65f + 0.35f * pulse) : style.outline);
            g.drawRoundedRectangle(bounds.toFloat(), style.radius, playing ? 2.2f : style.stroke);

            auto textArea = bounds.reduced(8, 4);
            auto reservedButtons = getLocalBounds().reduced(8, 6).removeFromBottom(24);
            textArea = textArea.withBottom(juce::jmax(textArea.getY() + 12, reservedButtons.getY() - 2));
            auto row1 = textArea.removeFromTop(14);
            auto row2 = textArea.removeFromTop(12);
            auto row3 = textArea.removeFromTop(11);

            const auto marker = playing ? juce::String("▶") : juce::String(" ");
            g.setFont(juce::FontOptions(11.8f));
            g.setColour(style.text);

            auto firstThree = juce::String();
            auto keyMode = juce::String();
            auto beatCount = 0.0;
            int chordCount = 0;

            if (const auto* song = songProvider())
            {
                if (auto progression = song->findProgressionById(slot->progressionId))
                {
                    int count = 0;
                    for (const auto& chord : progression->getChords())
                    {
                        if (count > 0)
                            firstThree << " ";
                        firstThree << chord.getSymbol();
                        ++count;
                        if (count >= 3)
                            break;
                    }

                    keyMode = progression->getKey() + " " + progression->getMode();
                    beatCount = progression->getLengthBeats();
                    chordCount = static_cast<int>(progression->getChords().size());
                }
            }

            g.drawText(marker + "  " + slot->displayName + "  •  " + firstThree,
                       row1,
                       juce::Justification::centredLeft,
                       true);

            g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::muted).withAlpha(0.95f));
            g.setFont(juce::FontOptions(10.5f));
            const auto confidenceText = juce::String(juce::roundToInt(slot->confidence * 100.0f)) + "%";
            g.drawText("    " + keyMode + "  •  " + juce::String(chordCount) + " chords  •  "
                           + juce::String(beatCount, 1) + " beats  •  " + confidenceText,
                       row2,
                       juce::Justification::centredLeft,
                       true);

            if (slot->hasCoupledAudio())
            {
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.95f));
                g.setFont(juce::FontOptions(9.5f));
                g.drawText("    " + formatAudioInfo(*slot),
                           row3,
                           juce::Justification::centredLeft,
                           true);

                auto waveArea = textArea.removeFromTop(10);
                if (waveArea.getWidth() > 10 && waveArea.getHeight() > 2 && slot->coupledAudio != nullptr)
                {
                    g.setColour(themeData.surface0.withAlpha(0.55f));
                    g.fillRoundedRectangle(waveArea.toFloat(), 2.0f);
                    g.setColour(themeData.surfaceEdge.withAlpha(0.60f));
                    g.drawRoundedRectangle(waveArea.toFloat(), 2.0f, 0.8f);

                    const auto* audio = slot->coupledAudio.get();
                    const int samples = audio->getNumSamples();
                    const int width = waveArea.getWidth();
                    if (samples > 1 && width > 1)
                    {
                        g.setColour(themeData.signalAudio.withAlpha(0.88f));
                        const float midY = static_cast<float>(waveArea.getCentreY());
                        const float halfH = static_cast<float>(waveArea.getHeight()) * 0.45f;

                        for (int x = 0; x < width; ++x)
                        {
                            const int s0 = (x * samples) / width;
                            const int s1 = juce::jmax(s0 + 1, ((x + 1) * samples) / width);
                            float peak = 0.0f;

                            for (int s = s0; s < s1; ++s)
                            {
                                peak = juce::jmax(peak, std::abs(audio->getSample(0, s)));
                                if (audio->getNumChannels() > 1)
                                    peak = juce::jmax(peak, std::abs(audio->getSample(1, s)));
                            }

                            const float amp = juce::jlimit(0.0f, 1.0f, peak) * halfH;
                            const float px = static_cast<float>(waveArea.getX() + x);
                            g.drawLine(px, midY - amp, px, midY + amp, 1.0f);
                        }
                    }
                }
            }
            else
            {
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.9f));
                g.setFont(juce::FontOptions(9.5f));
                g.drawText("    MIDI pattern playback", row3, juce::Justification::centredLeft, true);
            }

            auto modeBadge = bounds.removeFromRight(72).removeFromTop(14).reduced(4, 2);
            const auto badgeColor = slot->hasCoupledAudio() ? themeData.signalAudio : themeData.signalMidi;
            g.setColour(badgeColor.withAlpha(0.24f));
            g.fillRoundedRectangle(modeBadge.toFloat(), 2.0f);
            g.setColour(badgeColor.withAlpha(0.92f));
            g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
            g.drawText(slot->hasCoupledAudio() ? "AUDIO" : "MIDI", modeBadge, juce::Justification::centred, false);
        }

    private:
        const capture::GrabSlot* getSlot() const
        {
            if (auto* queue = queueProvider())
                return &queue->getSlot(slotIndex);
            return nullptr;
        }

        juce::String formatAudioInfo(const capture::GrabSlot& slot) const
        {
            if (!slot.hasCoupledAudio() || slot.coupledAudio == nullptr)
                return "audio unavailable";

            const auto samples = slot.coupledAudio->getNumSamples();
            if (samples <= 0 || slot.coupledSampleRate <= 0.0)
                return "audio unavailable";

            const auto seconds = static_cast<double>(samples) / slot.coupledSampleRate;
            const auto kHz = slot.coupledSampleRate / 1000.0;
            return juce::String(seconds, 2) + "s  @  " + juce::String(kHz, 1) + "kHz";
        }

        int slotIndex = 0;
        QueueProvider queueProvider;
        SongProvider songProvider;
        QueueActions& queueActions;
        juce::TextButton playButton;
        juce::TextButton editButton;
        juce::TextButton promoteButton;
    };

public:
    InDevicePanel(te::Engine& e,
                  QueueProvider queueProviderIn,
                  SongProvider songProviderIn,
                  QueueActions queueActionsIn)
        : engine(e),
          queueProvider(std::move(queueProviderIn)),
          songProvider(std::move(songProviderIn)),
          queueActions(std::move(queueActionsIn))
    {
        openMidiInputs();

        for (int i = 0; i < 4; ++i)
        {
            slotCards[static_cast<size_t>(i)] = std::make_unique<GrabSlotCard>(
                i,
                [this]() { return queueProvider != nullptr ? queueProvider() : nullptr; },
                [this]() { return songProvider != nullptr ? songProvider() : nullptr; },
                queueActions);
            addAndMakeVisible(*slotCards[static_cast<size_t>(i)]);
        }

        startTimerHz(20);
    }

    ~InDevicePanel() override
    {
        stopTimer();
        for (auto& m : midiInputs)
            m->stop();
    }

    void paint(juce::Graphics& g) override
    {
        const auto& themeData = ThemeManager::get().theme();
        const auto bounds = getLocalBounds();
        const auto panelStyle = setle::theme::cardStyle(themeData, setle::theme::SurfaceState::normal);
        g.setColour(setle::theme::panelBackground(themeData, setle::theme::ZoneRole::inPanel));
        g.fillRoundedRectangle(bounds.toFloat().reduced(1.0f), panelStyle.radius);
        g.setColour(panelStyle.outline);
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), panelStyle.radius, panelStyle.stroke);

        auto area = bounds.reduced(10, 8);

        // Title
        g.setFont(juce::FontOptions(16.0f));
        g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::primary).withAlpha(0.92f));
        g.drawText("IN", area.removeFromTop(22), juce::Justification::centredLeft, false);

        area.removeFromTop(4);

        // ── Audio Output ──────────────────────────────────────────────────────
        drawSectionLabel(g, area, "Audio Output");
        {
            auto& dm = engine.getDeviceManager().deviceManager;
            auto* dev = dm.getCurrentAudioDevice();
            if (dev != nullptr)
            {
                const auto sr = static_cast<int>(dev->getCurrentSampleRate());
                const auto bs = dev->getCurrentBufferSizeSamples();
                const auto devName = dev->getName();
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::primary).withAlpha(0.92f));
                g.setFont(juce::FontOptions(12.0f));
                g.drawText(devName, area.removeFromTop(16), juce::Justification::centredLeft, true);
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::muted).withAlpha(0.90f));
                g.setFont(juce::FontOptions(11.0f));
                g.drawText(juce::String(sr) + " Hz  |  " + juce::String(bs) + " samples",
                           area.removeFromTop(15), juce::Justification::centredLeft, true);

                const double latencyMs = sr > 0 ? (1000.0 * static_cast<double>(bs) / static_cast<double>(sr)) : 0.0;
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.95f));
                g.drawText("Latency: " + juce::String(latencyMs, 1) + " ms",
                           area.removeFromTop(14), juce::Justification::centredLeft, true);

                auto meterRow = area.removeFromTop(14).withTrimmedLeft(2);
                auto meterL = meterRow.removeFromLeft(40);
                meterRow.removeFromLeft(4);
                auto meterR = meterRow.removeFromLeft(40);

                auto drawMiniMeter = [&g, &themeData](juce::Rectangle<int> r, float level, juce::Colour colour)
                {
                    g.setColour(setle::theme::panelBackground(themeData, setle::theme::ZoneRole::neutral));
                    g.fillRect(r);
                    const int fill = static_cast<int>(juce::jlimit(0.0f, 1.0f, level) * r.getHeight());
                    g.setColour(colour.withAlpha(0.9f));
                    g.fillRect(r.withY(r.getBottom() - fill).withHeight(fill));
                    g.setColour(setle::theme::timelineGridLine(themeData, true).withAlpha(0.6f));
                    g.drawRect(r);
                };

                drawMiniMeter(meterL, outputMeterLeft, juce::Colour(0xff54d36e));
                drawMiniMeter(meterR, outputMeterRight, juce::Colour(0xff54d36e));
            }
            else
            {
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.95f));
                g.setFont(juce::FontOptions(12.0f));
                g.drawText("(no audio device open)", area.removeFromTop(16), juce::Justification::centredLeft, true);
            }
        }
        area.removeFromTop(8);

        // ── MIDI Inputs ───────────────────────────────────────────────────────
        drawSectionLabel(g, area, "MIDI Inputs");
        {
            const auto midiDevices = juce::MidiInput::getAvailableDevices();
            if (midiDevices.isEmpty())
            {
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.95f));
                g.setFont(juce::FontOptions(12.0f));
                g.drawText("(no MIDI inputs found)", area.removeFromTop(15), juce::Justification::centredLeft, true);
            }
            else
            {
                for (const auto& info : midiDevices)
                {
                    if (area.getHeight() < 14)
                        break;
                    const bool isOpen = isDeviceOpen(info.identifier);
                    const auto nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
                    const auto it = midiActivitySeconds.find(info.identifier);
                    const bool active = it != midiActivitySeconds.end() && (nowSec - it->second) < 0.18;
                    const float pulse = 0.55f + 0.45f * std::sin(static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.02));

                    juce::Colour dotColour = themeData.zoneC.withAlpha(active ? pulse : 0.75f);
                    juce::Colour textColour = isOpen
                                                  ? setle::theme::textForRole(themeData, setle::theme::TextRole::primary).withAlpha(active ? 0.95f : 0.78f)
                                                  : setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.78f);
                    g.setColour(isOpen ? dotColour : setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.45f));
                    g.setFont(juce::FontOptions(12.0f));
                    const auto dot = isOpen ? juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f "))
                                            : juce::String(juce::CharPointer_UTF8("\xe2\x97\x8b "));
                    auto row = area.removeFromTop(15);
                    g.drawText(dot, row.removeFromLeft(14), juce::Justification::centredLeft, false);
                    g.setColour(textColour);
                    g.drawText(info.name, row, juce::Justification::centredLeft, true);
                }
            }
        }
        area.removeFromTop(8);

        // ── MIDI Monitor ──────────────────────────────────────────────────────
        auto queueArea = area.removeFromBottom(getQueueAreaHeight());
        if (area.getHeight() < 36)
            return;

        drawSectionLabel(g, area, "MIDI Monitor");
        {
            juce::Array<juce::MidiMessage> snapshot;
            {
                const juce::ScopedLock sl(logLock);
                for (const auto& m : midiLog)
                    snapshot.add(m);
            }

            g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
            // Draw newest at top
            for (int i = snapshot.size() - 1; i >= 0 && area.getHeight() >= 13; --i)
            {
                const auto& msg = snapshot.getReference(i);
                juce::String text;
                if (msg.isNoteOn())
                    text = "NoteOn  ch" + juce::String(msg.getChannel()) + "  n" + juce::String(msg.getNoteNumber()) + "  v" + juce::String(msg.getVelocity());
                else if (msg.isNoteOff())
                    text = "NoteOff ch" + juce::String(msg.getChannel()) + "  n" + juce::String(msg.getNoteNumber());
                else if (msg.isController())
                    text = "CC  ch" + juce::String(msg.getChannel()) + "  #" + juce::String(msg.getControllerNumber()) + "=" + juce::String(msg.getControllerValue());
                else if (msg.isProgramChange())
                    text = "PC  ch" + juce::String(msg.getChannel()) + "  p" + juce::String(msg.getProgramChangeNumber());
                else if (msg.isPitchWheel())
                    text = "PW  ch" + juce::String(msg.getChannel()) + "  " + juce::String(msg.getPitchWheelValue());
                else
                    text = "0x" + juce::String::toHexString(msg.getRawData(), juce::jmin(3, msg.getRawDataSize()));

                const float alpha = 0.35f + 0.65f * (static_cast<float>(i) / static_cast<float>(juce::jmax(1, snapshot.size() - 1)));
                g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::primary).withAlpha(alpha));
                g.drawText(text, area.removeFromTop(13), juce::Justification::centredLeft, true);
            }
        }

        queueArea.removeFromTop(6);
        drawSectionLabel(g, queueArea, "Grab Queue");

        g.setColour(setle::theme::timelineGridLine(themeData, true).withAlpha(0.40f));
        g.drawRoundedRectangle(queueArea.toFloat(),
                               setle::theme::radius(themeData, setle::theme::RadiusRole::sm),
                               setle::theme::stroke(themeData, setle::theme::StrokeRole::subtle));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10, 8);
        area.removeFromTop(22); // IN title
        area.removeFromTop(4);

        auto queueArea = area.removeFromBottom(getQueueAreaHeight());
        queueArea.removeFromTop(6);
        queueArea.removeFromTop(14); // section label
        queueArea.removeFromTop(4);

        const int gap = 4;
        const int cellHeight = (queueArea.getHeight() - gap * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            if (auto* card = slotCards[static_cast<size_t>(i)].get())
            {
                card->setBounds(queueArea.removeFromTop(cellHeight));
                queueArea.removeFromTop(gap);
            }
        }
    }

private:
    int getQueueAreaHeight() const
    {
        return juce::jlimit(220, 360, static_cast<int>(std::round(static_cast<double>(getHeight()) * 0.36)));
    }

    static void drawSectionLabel(juce::Graphics& g, juce::Rectangle<int>& area, const juce::String& label)
    {
        const auto& themeData = ThemeManager::get().theme();
        g.setColour(setle::theme::panelHeaderBackground(themeData, setle::theme::ZoneRole::inPanel).brighter(0.22f));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(label.toUpperCase(), area.removeFromTop(14), juce::Justification::centredLeft, false);
    }

    void openMidiInputs()
    {
        for (const auto& info : juce::MidiInput::getAvailableDevices())
        {
            if (auto dev = juce::MidiInput::openDevice(info.identifier, this))
            {
                dev->start();
                midiInputs.push_back(std::move(dev));
            }
        }
    }

    bool isDeviceOpen(const juce::String& identifier) const
    {
        for (const auto& m : midiInputs)
            if (m->getIdentifier() == identifier)
                return true;
        return false;
    }

    void timerCallback() override
    {
        for (auto& card : slotCards)
            if (card != nullptr)
                card->refresh();

        outputMeterLeft *= 0.86f;
        outputMeterRight *= 0.86f;
        outputMeterLeft = juce::jlimit(0.0f, 1.0f, outputMeterLeft);
        outputMeterRight = juce::jlimit(0.0f, 1.0f, outputMeterRight);

        repaint();
    }

    void handleIncomingMidiMessage(juce::MidiInput* input, const juce::MidiMessage& msg) override
    {
        if (msg.isController() && setle::state::AppPreferences::get().getMidiLearnActive())
        {
            setle::state::AppPreferences::get().setMidiLearnCC(msg.getControllerNumber());
            setle::state::AppPreferences::get().setMidiLearnChannel(msg.getChannel());
            setle::state::AppPreferences::get().setMidiLearnActive(false);
        }

        if (input != nullptr)
            midiActivitySeconds[input->getIdentifier()] = juce::Time::getMillisecondCounterHiRes() * 0.001;

        if (msg.isNoteOn())
        {
            const float v = juce::jlimit(0.0f, 1.0f, msg.getFloatVelocity());
            outputMeterLeft = juce::jmax(outputMeterLeft, v);
            outputMeterRight = juce::jmax(outputMeterRight, v * 0.92f);
        }
        else if (msg.isController())
        {
            const float v = juce::jlimit(0.0f, 1.0f, static_cast<float>(msg.getControllerValue()) / 127.0f);
            outputMeterLeft = juce::jmax(outputMeterLeft, v * 0.55f);
            outputMeterRight = juce::jmax(outputMeterRight, v * 0.50f);
        }

        const juce::ScopedLock sl(logLock);
        midiLog.add(msg);
        if (midiLog.size() > 12)
            midiLog.remove(0);
    }

    te::Engine& engine;
    QueueProvider queueProvider;
    SongProvider songProvider;
    QueueActions queueActions;
    std::array<std::unique_ptr<GrabSlotCard>, 4> slotCards;
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
    std::map<juce::String, double> midiActivitySeconds;
    float outputMeterLeft { 0.0f };
    float outputMeterRight { 0.0f };
    juce::CriticalSection logLock;
    juce::Array<juce::MidiMessage> midiLog;
};

//==============================================================================
class WorkspaceShellComponent::LabelPanel : public juce::Component
{
public:
    LabelPanel(juce::String panelName, juce::Colour panelColour, juce::String subtitleText)
        : title(std::move(panelName)),
          subtitle(std::move(subtitleText)),
          colour(panelColour)
    {
    }

    void paint(juce::Graphics& g) override
    {
        const auto& theme = ThemeManager::get().theme();
        const auto panelStyle = setle::theme::cardStyle(theme, setle::theme::SurfaceState::normal);
        auto bounds = getLocalBounds();
        g.setColour(setle::theme::panelBackground(theme, setle::theme::ZoneRole::workPanel).interpolatedWith(colour, 0.35f));
        g.fillRoundedRectangle(bounds.toFloat().reduced(1.0f), panelStyle.radius);
        g.setColour(panelStyle.outline.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), panelStyle.radius, panelStyle.stroke);

        g.setColour(setle::theme::textForRole(theme, setle::theme::TextRole::primary).withAlpha(0.96f));
        g.setFont(juce::FontOptions(16.0f));
        g.drawText(title, bounds.removeFromTop(32).reduced(10, 4), juce::Justification::centredLeft, false);

        g.setColour(setle::theme::textForRole(theme, setle::theme::TextRole::muted).withAlpha(0.95f));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(subtitle, bounds.reduced(10, 4), juce::Justification::topLeft, true);
    }

private:
    juce::String title;
    juce::String subtitle;
    juce::Colour colour;
};

class WorkspaceShellComponent::OutPanelHost final : public juce::Component
{
public:
    using SlotType = setle::instruments::InstrumentSlot::SlotType;
    using TypeChangedCallback = std::function<void(const juce::String&, SlotType)>;
    using VstScanCallback = std::function<void()>;
    using VstLoadCallback = std::function<void(const juce::String&)>;
    using MidiOutChangedCallback = std::function<void(const juce::String&, const juce::String&)>;

    class InstrumentStrip final : public juce::Component
    {
    public:
        InstrumentStrip()
        {
            addAndMakeVisible(trackNameLabel);
            addAndMakeVisible(typeSelector);
            addAndMakeVisible(scanVstButton);
            addAndMakeVisible(loadVstButton);
            addAndMakeVisible(midiOutSelector);
            addAndMakeVisible(muteButton);
            addAndMakeVisible(volumeSlider);
            addAndMakeVisible(panSlider);

            const auto& themeData = ThemeManager::get().theme();
            trackNameLabel.setColour(juce::Label::textColourId,
                                     setle::theme::textForRole(themeData, setle::theme::TextRole::primary).withAlpha(0.96f));
            trackNameLabel.setFont(juce::FontOptions(14.0f));
            trackNameLabel.setJustificationType(juce::Justification::centredLeft);

            muteButton.setButtonText("M");
            muteButton.setClickingTogglesState(true);
            const auto muteChip = setle::theme::chipStyle(themeData, setle::theme::SurfaceState::normal, themeData.accentWarm);
            muteButton.setColour(juce::TextButton::buttonColourId, muteChip.fill);
            muteButton.setColour(juce::TextButton::textColourOffId, muteChip.text.withAlpha(0.95f));

            volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            volumeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
            volumeSlider.setRange(0.0, 1.0, 0.01);
            volumeSlider.setValue(0.75, juce::dontSendNotification);
            volumeSlider.onValueChange = [this]
            {
                if (volumePlugin != nullptr)
                    volumePlugin->setSliderPos(static_cast<float>(volumeSlider.getValue()));
            };

            panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
            panSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
            panSlider.setRange(-1.0, 1.0, 0.01);
            panSlider.setValue(0.0, juce::dontSendNotification);
            panSlider.onValueChange = [this]
            {
                if (volumePlugin != nullptr)
                    volumePlugin->setPan(static_cast<float>(panSlider.getValue()));
            };

            muteButton.onClick = [this]
            {
                if (volumePlugin != nullptr)
                    volumePlugin->muteOrUnmute();
            };

            typeSelector.addItem("PolySynth", 1);
            typeSelector.addItem("DrumMachine", 2);
            typeSelector.addItem("ReelSampler", 3);
            typeSelector.addItem("VST3", 4);
            typeSelector.addItem("MIDI Out", 5);
            typeSelector.onChange = [this]
            {
                if (onTypeChanged == nullptr)
                    return;

                switch (typeSelector.getSelectedId())
                {
                    case 1: onTypeChanged(trackId, SlotType::PolySynth); break;
                    case 2: onTypeChanged(trackId, SlotType::DrumMachine); break;
                    case 3: onTypeChanged(trackId, SlotType::ReelSampler); break;
                    case 4: onTypeChanged(trackId, SlotType::VST3); break;
                    case 5: onTypeChanged(trackId, SlotType::MidiOut); break;
                    default: break;
                }

                updateTypeSpecificControls();
            };

            loadVstButton.setButtonText("Load VST...");
            loadVstButton.onClick = [this]
            {
                if (onVstLoadRequested != nullptr)
                    onVstLoadRequested(trackId);
            };

            scanVstButton.setButtonText("Scan VST3...");
            scanVstButton.onClick = [this]
            {
                if (onVstScanRequested != nullptr)
                    onVstScanRequested();
            };

            midiOutSelector.onChange = [this]
            {
                if (onMidiOutChanged == nullptr)
                    return;

                const auto idx = midiOutSelector.getSelectedItemIndex();
                if (idx >= 0 && idx < static_cast<int>(midiOutputIdentifiers.size()))
                    onMidiOutChanged(trackId, midiOutputIdentifiers[static_cast<size_t>(idx)]);
            };
        }

        void configure(te::AudioTrack& audioTrack,
                       const juce::String& name,
                       const juce::String& id,
                       setle::instruments::InstrumentSlot* slot,
                       TypeChangedCallback callback,
                       VstScanCallback scanCallback,
                       VstLoadCallback vstCallback,
                       MidiOutChangedCallback midiOutCallback)
        {
            trackId = id;
            onTypeChanged = std::move(callback);
            onVstScanRequested = std::move(scanCallback);
            onVstLoadRequested = std::move(vstCallback);
            onMidiOutChanged = std::move(midiOutCallback);
            trackNameLabel.setText(name, juce::dontSendNotification);
            volumePlugin = audioTrack.getVolumePlugin();

            midiOutSelector.clear(juce::dontSendNotification);
            midiOutputIdentifiers.clear();
            midiOutSelector.addItem("Default MIDI Output", 1);
            midiOutputIdentifiers.push_back("default");
            int itemId = 2;
            for (const auto& device : juce::MidiOutput::getAvailableDevices())
            {
                midiOutSelector.addItem(device.name, itemId++);
                midiOutputIdentifiers.push_back(device.identifier);
            }
            midiOutSelector.setSelectedItemIndex(0, juce::dontSendNotification);

            if (volumePlugin != nullptr)
            {
                volumeSlider.setValue(volumePlugin->getSliderPos(), juce::dontSendNotification);
                panSlider.setValue(volumePlugin->getPan(), juce::dontSendNotification);
            }

            auto selected = 1;
            auto midiSelectionIndex = 0;
            if (slot != nullptr)
            {
                const auto info = slot->getInfo();
                switch (info.type)
                {
                    case SlotType::PolySynth: selected = 1; break;
                    case SlotType::DrumMachine: selected = 2; break;
                    case SlotType::ReelSampler: selected = 3; break;
                    case SlotType::VST3: selected = 4; break;
                    case SlotType::MidiOut: selected = 5; break;
                    case SlotType::Empty: selected = 1; break;
                }

                if (info.type == SlotType::MidiOut)
                {
                    for (int i = 0; i < static_cast<int>(midiOutputIdentifiers.size()); ++i)
                    {
                        if (midiOutputIdentifiers[static_cast<size_t>(i)] == info.persistentIdentifier)
                        {
                            midiSelectionIndex = i;
                            break;
                        }
                    }
                }

                if (auto* editor = slot->getEditorComponent())
                {
                    if (editor->getParentComponent() != this)
                        addAndMakeVisible(*editor);
                    editorComponent = editor;
                }
            }

            typeSelector.setSelectedId(selected, juce::dontSendNotification);
            midiOutSelector.setSelectedItemIndex(midiSelectionIndex, juce::dontSendNotification);
            updateTypeSpecificControls();
            resized();
        }

        void paint(juce::Graphics& g) override
        {
            const auto& theme = ThemeManager::get().theme();
            const auto stripStyle = setle::theme::cardStyle(theme, setle::theme::SurfaceState::normal);
            auto r = getLocalBounds().toFloat();
            g.setColour(stripStyle.fill);
            g.fillRoundedRectangle(r, setle::theme::radius(theme, setle::theme::RadiusRole::sm));
            g.setColour(stripStyle.outline.withAlpha(0.85f));
            g.drawRoundedRectangle(r.reduced(0.5f),
                                   setle::theme::radius(theme, setle::theme::RadiusRole::sm),
                                   setle::theme::stroke(theme, setle::theme::StrokeRole::normal));
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced(8);
            auto row = b.removeFromTop(28);
            trackNameLabel.setBounds(row.removeFromLeft(160));
            muteButton.setBounds(row.removeFromRight(32));
            row.removeFromRight(6);
            scanVstButton.setBounds(row.removeFromRight(112));
            row.removeFromRight(4);
            loadVstButton.setBounds(row.removeFromRight(104));
            row.removeFromRight(4);
            midiOutSelector.setBounds(row.removeFromRight(170));
            row.removeFromRight(4);
            typeSelector.setBounds(row.removeFromRight(120));

            b.removeFromTop(6);
            if (editorComponent != nullptr)
                editorComponent->setBounds(b.removeFromTop(124));
            else
                b.removeFromTop(124);

            b.removeFromTop(6);
            auto mix = b.removeFromTop(24);
            volumeSlider.setBounds(mix.removeFromLeft(220));
            mix.removeFromLeft(8);
            panSlider.setBounds(mix.removeFromLeft(220));
        }

    private:
        void updateTypeSpecificControls()
        {
            const auto id = typeSelector.getSelectedId();
            scanVstButton.setVisible(id == 4);
            loadVstButton.setVisible(id == 4);
            midiOutSelector.setVisible(id == 5);
        }

        juce::String trackId;
        TypeChangedCallback onTypeChanged;
        VstScanCallback onVstScanRequested;
        VstLoadCallback onVstLoadRequested;
        MidiOutChangedCallback onMidiOutChanged;
        juce::Label trackNameLabel;
        juce::ComboBox typeSelector;
        juce::TextButton scanVstButton;
        juce::TextButton loadVstButton;
        juce::ComboBox midiOutSelector;
        std::vector<juce::String> midiOutputIdentifiers;
        juce::TextButton muteButton;
        juce::Slider volumeSlider;
        juce::Slider panSlider;
        te::VolumeAndPanPlugin* volumePlugin { nullptr };
        juce::Component::SafePointer<juce::Component> editorComponent;
    };

    OutPanelHost()
    {
        addAndMakeVisible(viewport);
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
    }

    void rebuild(const juce::Array<te::Track*>& tracks,
                 std::map<juce::String, std::unique_ptr<setle::instruments::InstrumentSlot>>& slots,
                 TypeChangedCallback callback,
                 VstScanCallback scanCallback,
                 VstLoadCallback vstCallback,
                 MidiOutChangedCallback midiOutCallback)
    {
        onTypeChanged = std::move(callback);
        onVstScanRequested = std::move(scanCallback);
        onVstLoadRequested = std::move(vstCallback);
        onMidiOutChanged = std::move(midiOutCallback);
        strips.clear();

        for (auto* track : tracks)
        {
            if (track == nullptr)
                continue;

            if (setle::timeline::TrackManager::isSystemTrack(*track))
                continue;

            auto* audioTrack = dynamic_cast<te::AudioTrack*>(track);
            if (audioTrack == nullptr)
                continue;

            const auto trackId = audioTrack->itemID.toString();
            auto it = slots.find(trackId);
            if (it == slots.end())
                continue;

            auto strip = std::make_unique<InstrumentStrip>();
            strip->configure(*audioTrack,
                             audioTrack->getName(),
                             trackId,
                             it->second.get(),
                             onTypeChanged,
                             onVstScanRequested,
                             onVstLoadRequested,
                             onMidiOutChanged);
            content.addAndMakeVisible(*strip);
            strips.push_back(std::move(strip));
        }

        resized();
        repaint();
    }

    void resized() override
    {
        viewport.setBounds(getLocalBounds());

        auto width = juce::jmax(260, getWidth() - 14);
        int y = 6;
        for (auto& strip : strips)
        {
            strip->setBounds(6, y, width - 12, 228);
            y += 236;
        }

        content.setSize(width, juce::jmax(y + 6, getHeight()));
    }

private:
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<InstrumentStrip>> strips;
    TypeChangedCallback onTypeChanged;
    VstScanCallback onVstScanRequested;
    VstLoadCallback onVstLoadRequested;
    MidiOutChangedCallback onMidiOutChanged;
};

class WorkspaceShellComponent::TimelineShell final : public juce::Component
{
public:
    using ActionCallback = std::function<void(TheoryMenuTarget, int, const juce::String&)>;

    class ContextLane final : public LabelPanel
    {
    public:
        struct BlockData
        {
            juce::String id;
            juce::String label;
            float widthFraction { 1.0f };
            juce::String subtitle;
            juce::String functionTag;
            float confidence { -1.0f };
            bool confidenceFromMidi { false };
            juce::String timeSigLabel;
            juce::Colour tint;
            juce::Colour boundaryScoreColor;

            // Per-repeat variant map (section lane only).
            // One entry per repeat of the section's progression; isVariant is true
            // when that repeat resolves to a progression with a non-empty variantOf.
            struct RepeatCell { bool isVariant { false }; };
            std::vector<RepeatCell> repeatCells;
        };

        using SelectionCallback = std::function<void(const juce::String&)>;

        ContextLane(juce::String panelName,
                    juce::Colour panelColour,
                    juce::String subtitleText,
                    TheoryMenuTarget targetType,
                    ActionCallback callback)
            : LabelPanel(panelName, panelColour, subtitleText),
              menuTarget(targetType),
              onAction(std::move(callback))
        {
        }

        void setBlocks(std::vector<BlockData> newBlocks,
                       juce::String currentSelectedId,
                       SelectionCallback cb)
        {
            blocks = std::move(newBlocks);
            selectedId = std::move(currentSelectedId);
            onSelect = std::move(cb);
            repaint();
        }

        void setPlayheadFraction(double fraction)
        {
            playheadFraction = juce::jlimit(0.0, 1.0, fraction);
            repaint();
        }

        void setBeatGrid(double total, double unit, double bar)
        {
            totalBeats = total;
            beatUnit = unit;
            beatsPerBar = bar;
            repaint();
        }

        void setHistorySourceLabel(juce::String source)
        {
            historySourceLabel = std::move(source);
            repaint();
        }

        void paint(juce::Graphics& g) override
        {
            const auto& theme = ThemeManager::get().theme();
            auto bounds = getLocalBounds();

            if (menuTarget == TheoryMenuTarget::historyBuffer)
            {
                g.fillAll(theme.surface1);

                auto header = bounds.removeFromTop(24);
                auto tab = header.removeFromLeft(44).reduced(2, 2);
                g.setColour(theme.surface3);
                g.fillRoundedRectangle(tab.toFloat(), 3.0f);
                g.setColour(theme.inkLight);
                g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
                g.drawText("HIST", tab, juce::Justification::centred, false);

                auto drawGrabButton = [&](juce::Rectangle<int> button, const juce::String& label)
                {
                    g.setColour(theme.surface3);
                    g.fillRoundedRectangle(button.toFloat(), 3.0f);
                    g.setColour(theme.surfaceEdge.withAlpha(0.8f));
                    g.drawRoundedRectangle(button.toFloat(), 3.0f, 1.0f);
                    g.setColour(theme.inkMid.withAlpha(0.92f));
                    g.setFont(juce::FontOptions(10.0f));
                    g.drawText(label, button, juce::Justification::centred, false);
                };

                auto buttons = header.removeFromLeft(130);
                const auto b4 = buttons.removeFromLeft(40).reduced(2, 3);
                const auto b8 = buttons.removeFromLeft(40).reduced(2, 3);
                const auto b16 = buttons.removeFromLeft(50).reduced(2, 3);
                const auto leftTriangle = juce::String::fromUTF8("\xE2\x97\x80");
                drawGrabButton(b4, leftTriangle + "4");
                drawGrabButton(b8, leftTriangle + "8");
                drawGrabButton(b16, leftTriangle + "16");

                g.setColour(theme.inkMuted.withAlpha(0.88f));
                g.setFont(juce::FontOptions(10.5f));
                g.drawText("Source: " + (historySourceLabel.isNotEmpty() ? historySourceLabel : "Off"),
                           header.reduced(4, 2),
                           juce::Justification::centredRight,
                           true);

                auto waveform = bounds.reduced(6, 4);
                g.setColour(theme.surface0);
                g.fillRoundedRectangle(waveform.toFloat(), 3.0f);
                g.setColour(theme.surfaceEdge.withAlpha(0.6f));
                g.drawRoundedRectangle(waveform.toFloat(), 3.0f, 1.0f);

                const int midY = waveform.getCentreY();
                g.setColour(theme.inkGhost.withAlpha(0.7f));
                g.drawLine(static_cast<float>(waveform.getX()), static_cast<float>(midY),
                           static_cast<float>(waveform.getRight()), static_cast<float>(midY), 1.2f);
                return;
            }

            if (blocks.empty())
            {
                LabelPanel::paint(g);
                return;
            }

            g.fillAll(setle::theme::panelBackground(theme, setle::theme::ZoneRole::timeline));

            if (totalBeats > 0.0 && beatUnit > 0.0)
            {
                for (double beat = 0.0; beat <= totalBeats + 0.0001; beat += beatUnit)
                {
                    const int x = bounds.getX() + static_cast<int>((beat / totalBeats) * bounds.getWidth());
                    g.setColour(setle::theme::timelineGridLine(theme, false));
                    g.fillRect(x, bounds.getY(), 1, bounds.getHeight());
                }
            }

            if (totalBeats > 0.0 && beatsPerBar > 0.0)
            {
                for (double beat = 0.0; beat <= totalBeats + 0.0001; beat += beatsPerBar)
                {
                    const int x = bounds.getX() + static_cast<int>((beat / totalBeats) * bounds.getWidth());
                    g.setColour(setle::theme::timelineGridLine(theme, true));
                    g.fillRect(x, bounds.getY(), 1, bounds.getHeight());
                }
            }

            const float totalWidth = static_cast<float>(bounds.getWidth());
            const float h = static_cast<float>(bounds.getHeight());
            const float playheadX = static_cast<float>(bounds.getX()) + static_cast<float>(playheadFraction) * totalWidth;
            float x = 0.0f;

            for (const auto& block : blocks)
            {
                const float w = totalWidth * block.widthFraction;
                const juce::Rectangle<float> blockRect(bounds.getX() + x + 1.0f, bounds.getY() + 2.0f, w - 2.0f, h - 4.0f);
                const bool isSelected = (block.id == selectedId);
                const bool isActive = playheadX >= blockRect.getX() && playheadX < blockRect.getRight();

                const auto blockStyle = setle::theme::progressionBlockStyle(theme, isSelected, isActive, block.tint);
                g.setColour(blockStyle.fill);
                g.fillRoundedRectangle(blockRect, 3.0f);

                g.setColour(blockStyle.outline);
                g.drawRoundedRectangle(blockRect, 3.0f, isActive ? 2.0f : 1.0f);

                auto textBounds = blockRect.toNearestInt().reduced(5, 3);
                if (block.subtitle.isNotEmpty())
                {
                    g.setColour(blockStyle.subtitle);
                    g.setFont(juce::FontOptions(10.0f));
                    g.drawText(block.subtitle, textBounds.removeFromTop(10), juce::Justification::centredLeft, true);
                }

                g.setColour(blockStyle.label);
                g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
                g.drawText(block.label, textBounds, juce::Justification::centred, true);

                if (block.functionTag.isNotEmpty())
                {
                    auto badge = blockRect.toNearestInt().removeFromBottom(14).removeFromLeft(34).translated(4, -2);
                    const auto chip = setle::theme::chipStyle(theme,
                                                              isSelected ? setle::theme::SurfaceState::selected : setle::theme::SurfaceState::normal,
                                                              block.tint);
                    g.setColour(chip.fill);
                    g.fillRoundedRectangle(badge.toFloat(), 2.0f);
                    g.setColour(chip.text);
                    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
                    g.drawText("[" + block.functionTag + "]", badge, juce::Justification::centred, false);
                }

                if (block.timeSigLabel.isNotEmpty())
                {
                    auto sigBounds = blockRect.toNearestInt().removeFromBottom(12).reduced(4, 0);
                    g.setColour(theme.inkMuted.withAlpha(0.90f));
                    g.setFont(juce::FontOptions(9.0f));
                    g.drawText(block.timeSigLabel, sigBounds, juce::Justification::centredLeft, true);
                }

                if (block.confidenceFromMidi && block.confidence >= 0.0f)
                {
                    juce::Colour dot = block.confidence >= 0.9f ? theme.zoneC : theme.zoneD;
                    auto dotRect = juce::Rectangle<float>(blockRect.getRight() - 7.0f, blockRect.getY() + 4.0f, 4.0f, 4.0f);
                    g.setColour(dot.withAlpha(0.95f));
                    g.fillEllipse(dotRect);
                }

                if (!block.boundaryScoreColor.isTransparent())
                {
                    auto edge = blockRect.toNearestInt().removeFromRight(3).reduced(0, 2);
                    g.setColour(block.boundaryScoreColor);
                    g.fillRect(edge);
                }

                // Repeat-variant pip strip (section lane only).
                // Each pip = one repeat; accentWarm = variant, inkGhost = base.
                if (menuTarget == TheoryMenuTarget::section && !block.repeatCells.empty())
                {
                    const int numCells = static_cast<int>(block.repeatCells.size());
                    const float stripH = 4.0f;
                    // Leave 3px margin from left/right edges, 1px from bottom
                    const float availW = blockRect.getWidth() - 6.0f;
                    const float gap = 1.0f;
                    const float cellW = juce::jmax(2.0f, (availW - gap * static_cast<float>(numCells - 1)) / static_cast<float>(numCells));
                    const float stripY = blockRect.getBottom() - stripH - 0.5f;

                    if (cellW >= 2.0f && numCells <= 32)
                    {
                        float cx = blockRect.getX() + 3.0f;
                        for (const auto& cell : block.repeatCells)
                        {
                            const juce::Colour pipColour = cell.isVariant
                                ? theme.accentWarm.withAlpha(0.88f)
                                : theme.inkGhost.withAlpha(isSelected ? 0.45f : 0.28f);
                            g.setColour(pipColour);
                            g.fillRoundedRectangle(cx, stripY, cellW, stripH, 1.0f);
                            cx += cellW + gap;
                        }
                    }
                    else
                    {
                        // Too dense to show per-repeat — show a variant-present dot
                        bool anyVariant = false;
                        for (const auto& c : block.repeatCells)
                            if (c.isVariant) { anyVariant = true; break; }
                        if (anyVariant)
                        {
                            const auto dot = juce::Rectangle<float>(
                                blockRect.getX() + 4.0f, blockRect.getBottom() - 7.0f, 4.0f, 4.0f);
                            g.setColour(theme.accentWarm.withAlpha(0.92f));
                            g.fillEllipse(dot);
                        }
                    }
                }

                x += w;
            }
        }

void mouseUp(const juce::MouseEvent& event) override
{
    if (menuTarget == TheoryMenuTarget::historyBuffer && !event.mods.isRightButtonDown())
    {
        auto header = getLocalBounds().removeFromTop(24);
        header.removeFromLeft(44);
        auto buttons = header.removeFromLeft(130);
        const auto b4 = buttons.removeFromLeft(40).reduced(2, 3);
        const auto b8 = buttons.removeFromLeft(40).reduced(2, 3);
        const auto b16 = buttons.removeFromLeft(50).reduced(2, 3);

        int actionId = 0;
        if (b4.contains(event.getPosition())) actionId = historyGrab4;
        else if (b8.contains(event.getPosition())) actionId = historyGrab8;
        else if (b16.contains(event.getPosition())) actionId = historyGrab16;

        if (actionId > 0 && onAction != nullptr)
            onAction(menuTarget, actionId, actionNameFor(actionId));

        return;
    }

    if (!event.mods.isRightButtonDown())
    {
        if (!blocks.empty() && onSelect != nullptr)
        {
            const float totalWidth = static_cast<float>(getWidth());
            float x = 0.0f;
            const float clickX = static_cast<float>(event.x);
            const float clickY = static_cast<float>(event.y);
            const float h = static_cast<float>(getHeight());

            for (const auto& block : blocks)
            {
                const float w = totalWidth * block.widthFraction;
                if (clickX >= x && clickX < x + w)
                {
                    selectedId = block.id;
                    repaint();
                    onSelect(block.id);
                    return;
                }
                x += w;
            }
        }
        return;
    }

    juce::PopupMenu menu;
    populateMenu(menu);

    auto options = juce::PopupMenu::Options().withTargetComponent(this);
    menu.showMenuAsync(
        options,
        [safeThis = juce::Component::SafePointer<ContextLane>(this)](int selectedId)
        {
            if (safeThis == nullptr || selectedId <= 0 || safeThis->onAction == nullptr)
                return;

            safeThis->onAction(safeThis->menuTarget, selectedId, safeThis->actionNameFor(selectedId));
        });
}

    private:
        void populateMenu(juce::PopupMenu& menu) const
        {
            switch (menuTarget)
            {
                case TheoryMenuTarget::section:
                {
                    menu.addItem(sectionEditTheory, "Edit Section Theory...");
                    menu.addItem(sectionSetRepeatPattern, "Set Repeat Pattern...");
                    menu.addItem(sectionAddTransitionAnchor, "Add Transition Anchor...");
                    menu.addSeparator();
                    menu.addItem(sectionRename, "Rename Section...");
                    menu.addItem(sectionDuplicate, "Duplicate Section");
                    menu.addItem(sectionDelete, "Delete Section");
                    menu.addSeparator();
                    menu.addItem(sectionSetColor, "Set Color...");
                    menu.addItem(sectionSetTimeSig, "Set Time Signature...");
                    menu.addSeparator();
                    menu.addItem(sectionJumpTo, "Jump to Section");
                    menu.addItem(sectionExportMidi, "Export Section as MIDI...");
                    menu.addSeparator();
                    menu.addItem(sectionConflictCheck, "Run Section Conflict Check");
                    break;
                }

                case TheoryMenuTarget::chord:
                {
                    menu.addItem(chordEdit, "Edit Chord...");
                    menu.addItem(chordSubstitution, "Chord Substitution...");
                    menu.addItem(chordSetFunction, "Set Harmonic Function...");
                    menu.addSeparator();
                    menu.addItem(chordDuplicate, "Duplicate Chord");
                    menu.addItem(chordInsertBefore, "Insert Chord Before");
                    menu.addItem(chordInsertAfter, "Insert Chord After");
                    menu.addItem(chordDelete, "Delete Chord");
                    menu.addSeparator();
                    menu.addItem(chordNudgeLeft, "Nudge Left");
                    menu.addItem(chordNudgeRight, "Nudge Right");
                    menu.addItem(chordSetDuration, "Set Duration...");
                    menu.addItem(chordInvertVoicing, "Invert Voicing");
                    menu.addItem(chordSendToGridRoll, "Open in Grid Roll");
                    menu.addSeparator();
                    menu.addItem(chordScopeOccurrence, "Apply Scope: This Occurrence");
                    menu.addItem(chordScopeRepeat, "Apply Scope: This Repeat");
                    menu.addItem(chordScopeSelectedRepeats, "Apply Scope: Selected Repeats");
                    menu.addItem(chordScopeAllRepeats, "Apply Scope: All Repeats");
                    menu.addSeparator();
                    menu.addItem(chordSnapWholeBar, "Theory Snap: Whole Bar");
                    menu.addItem(chordSnapHalfBar, "Theory Snap: Half Bar");
                    menu.addItem(chordSnapBeat, "Theory Snap: Beat");
                    menu.addItem(chordSnapHalfBeat, "Theory Snap: Half Beat");
                    menu.addItem(chordSnapEighth, "Theory Snap: 1/8");
                    menu.addItem(chordSnapSixteenth, "Theory Snap: 1/16");
                    menu.addItem(chordSnapThirtySecond, "Theory Snap: 1/32");
                    break;
                }

                case TheoryMenuTarget::note:
                {
                    menu.addItem(noteEditTheory, "Edit Note Theory...");
                    menu.addItem(noteQuantizeScale, "Quantize Note to Scale");
                    menu.addItem(noteConvertToChord, "Convert Notes to Chord");
                    menu.addSeparator();
                    menu.addItem(noteDeriveProgression, "Derive Progression from Selection");
                    break;
                }

                case TheoryMenuTarget::progression:
                {
                    menu.addItem(progressionGrabSampler, "Grab to Sampler Queue");
                    menu.addItem(progressionCreateCandidate, "Create Progression Candidate");
                    menu.addItem(progressionAnnotateKeyMode, "Annotate Key / Mode");
                    menu.addSeparator();
                    menu.addItem(progressionTagTransition, "Tag as Transition Material");
                    break;
                }

                case TheoryMenuTarget::historyBuffer:
                {
                    menu.addItem(historyGrab4, "Grab Last 4 Beats");
                    menu.addItem(historyGrab8, "Grab Last 8 Beats");
                    menu.addItem(historyGrab16, "Grab Last 16 Beats");
                    menu.addItem(historyGrabCustom, "Grab Custom...");
                    menu.addItem(historyCaptureSingle, "Capture Single Take");
                    menu.addSeparator();
                    menu.addItem(historySetLength, "Set Buffer Length...");
                    menu.addItem(historySetBpm, "Override BPM...");
                    menu.addItem(historyChangeSource, "Change Source");
                    menu.addSeparator();
                    menu.addItem(historyAutoGrabToggle, "Auto-Grab After Stop",
                                 true, setle::state::AppPreferences::get().getAutoGrabEnabled());
                    menu.addItem(historySyncToTransport, "Sync to Transport",
                                 true, false);
                    menu.addItem(historyPreviewMode, "Preview Mode",
                                 true, false);
                    menu.addSeparator();
                    menu.addItem(historyExportMidi, "Export Buffer as MIDI...");
                    menu.addItem(historyClear, "Clear Buffer");
                    break;
                }
            }
        }

        juce::String actionNameFor(int id) const
        {
            switch (menuTarget)
            {
                case TheoryMenuTarget::section:
                {
                    if (id == sectionEditTheory) return "Edit Section Theory";
                    if (id == sectionSetRepeatPattern) return "Set Repeat Pattern";
                    if (id == sectionAddTransitionAnchor) return "Add Transition Anchor";
                    if (id == sectionConflictCheck) return "Run Section Conflict Check";
                    if (id == sectionRename) return "Rename Section";
                    if (id == sectionDelete) return "Delete Section";
                    if (id == sectionDuplicate) return "Duplicate Section";
                    if (id == sectionSetColor) return "Set Section Color";
                    if (id == sectionSetTimeSig) return "Set Time Signature";
                    if (id == sectionExportMidi) return "Export Section as MIDI";
                    if (id == sectionJumpTo) return "Jump to Section";
                    break;
                }

                case TheoryMenuTarget::chord:
                {
                    if (id == chordEdit) return "Edit Chord";
                    if (id == chordSubstitution) return "Chord Substitution";
                    if (id == chordSetFunction) return "Set Harmonic Function";
                    if (id == chordScopeOccurrence) return "Apply Scope This Occurrence";
                    if (id == chordScopeRepeat) return "Apply Scope This Repeat";
                    if (id == chordScopeSelectedRepeats) return "Apply Scope Selected Repeats";
                    if (id == chordScopeAllRepeats) return "Apply Scope All Repeats";
                    if (id == chordSnapWholeBar) return "Set Theory Snap Whole Bar";
                    if (id == chordSnapHalfBar) return "Set Theory Snap Half Bar";
                    if (id == chordSnapBeat) return "Set Theory Snap Beat";
                    if (id == chordSnapHalfBeat) return "Set Theory Snap Half Beat";
                    if (id == chordSnapEighth) return "Set Theory Snap 1/8";
                    if (id == chordSnapSixteenth) return "Set Theory Snap 1/16";
                    if (id == chordSnapThirtySecond) return "Set Theory Snap 1/32";
                    if (id == chordDelete) return "Delete Chord";
                    if (id == chordDuplicate) return "Duplicate Chord";
                    if (id == chordInsertBefore) return "Insert Chord Before";
                    if (id == chordInsertAfter) return "Insert Chord After";
                    if (id == chordNudgeLeft) return "Nudge Chord Left";
                    if (id == chordNudgeRight) return "Nudge Chord Right";
                    if (id == chordSetDuration) return "Set Chord Duration";
                    if (id == chordInvertVoicing) return "Invert Chord Voicing";
                    if (id == chordSendToGridRoll) return "Open Chord in Grid Roll";
                    break;
                }

                case TheoryMenuTarget::note:
                {
                    if (id == noteEditTheory) return "Edit Note Theory";
                    if (id == noteQuantizeScale) return "Quantize Note to Scale";
                    if (id == noteConvertToChord) return "Convert Notes to Chord";
                    if (id == noteDeriveProgression) return "Derive Progression from Selection";
                    break;
                }

                case TheoryMenuTarget::progression:
                {
                    if (id == progressionGrabSampler) return "Grab to Sampler Queue";
                    if (id == progressionCreateCandidate) return "Create Progression Candidate";
                    if (id == progressionAnnotateKeyMode) return "Annotate Key / Mode";
                    if (id == progressionTagTransition) return "Tag as Transition Material";
                    break;
                }

                case TheoryMenuTarget::historyBuffer:
                {
                    if (id == historyGrab4) return "Grab Last 4 Beats";
                    if (id == historyGrab8) return "Grab Last 8 Beats";
                    if (id == historyGrab16) return "Grab Last 16 Beats";
                    if (id == historyGrabCustom) return "Grab Custom";
                    if (id == historySetLength) return "Set Buffer Length";
                    if (id == historyChangeSource) return "Change Source";
                    if (id == historyClear) return "Clear Buffer";
                    if (id == historyAutoGrabToggle) return "Toggle Auto-Grab";
                    if (id == historyPreviewMode) return "Toggle Preview Mode";
                    if (id == historySetBpm) return "Override Buffer BPM";
                    if (id == historySyncToTransport) return "Sync to Transport";
                    if (id == historyExportMidi) return "Export Buffer as MIDI";
                    if (id == historyCaptureSingle) return "Capture Single Take";
                    break;
                }
            }

            return "Unknown Action";
        }

        TheoryMenuTarget menuTarget;
        ActionCallback onAction;
        std::vector<BlockData> blocks;
        juce::String selectedId;
        SelectionCallback onSelect;
        double playheadFraction { 0.0 };
        double totalBeats { 0.0 };
        double beatUnit { 1.0 };
        double beatsPerBar { 4.0 };
        juce::String historySourceLabel { "Off" };
    };

    void updateSections(const std::vector<model::Section>& sections,
                        const std::vector<model::Progression>& progressions,
                        const juce::String& currentSectionId,
                        ContextLane::SelectionCallback cb)
    {
        if (sections.empty())
        {
            sectionLane.setBlocks({}, {}, nullptr);
            return;
        }

        // Helper: find a progression by id within the song's progression list.
        auto findProg = [&](const juce::String& progId) -> const model::Progression*
        {
            for (const auto& p : progressions)
                if (p.getId() == progId) return &p;
            return nullptr;
        };

        // Compute beat length per section (repeatCount × progression length) and
        // per-repeat variant map so the section lane can visualise variant repeats.
        struct SectionEntry
        {
            juce::String id;
            juce::String name;
            double beats;
            juce::String timeSig;
            juce::Colour tint;
            juce::Colour boundaryColor;
            juce::String progressionName;
            std::vector<ContextLane::BlockData::RepeatCell> repeatCells;
        };
        std::vector<SectionEntry> entries;
        double totalBeats = 0.0;

        for (const auto& section : sections)
        {
            // Sort refs by orderIndex so the "default" ref comes first.
            auto refs = section.getProgressionRefs();
            std::sort(refs.begin(), refs.end(), [](const auto& a, const auto& b)
                { return a.getOrderIndex() < b.getOrderIndex(); });

            double sectionBeats = 0.0;
            for (const auto& ref : refs)
            {
                if (const auto* prog = findProg(ref.getProgressionId()))
                {
                    double len = prog->getLengthBeats();
                    if (len <= 0.0) len = 8.0;
                    sectionBeats += len;
                }
            }
            if (sectionBeats <= 0.0) sectionBeats = 8.0;
            const int repeatCount = juce::jmax(1, section.getRepeatCount());
            const double withRepeats = sectionBeats * repeatCount;
            totalBeats += withRepeats;

            juce::Colour tint;
            auto colorText = section.getColor().trim();
            if (colorText.startsWith("#"))
                colorText = colorText.substring(1);
            if (colorText.length() == 6)
                colorText = "ff" + colorText;
            if (colorText.length() == 8)
                tint = juce::Colour::fromString(colorText);

            juce::Colour boundary = juce::Colour(0x00000000);
            if (repeatCount <= 2) boundary = ThemeManager::get().theme().zoneC;
            else if (repeatCount <= 4) boundary = ThemeManager::get().theme().zoneD;
            else boundary = ThemeManager::get().theme().accentWarm;

            const auto timeSig = juce::String(section.getTimeSigNumerator()) + "/" + juce::String(section.getTimeSigDenominator());

            // Build a per-repeat progressionId map (0-based).
            // First, seed every slot with the first "all"-scope ref's progressionId.
            std::vector<juce::String> repeatProgIds(static_cast<size_t>(repeatCount), juce::String{});
            for (const auto& ref : refs)
            {
                const auto indices = ref.getRepeatIndices().trim();
                if (ref.getRepeatScope() == "all" || indices.isEmpty())
                {
                    for (auto& slot : repeatProgIds)
                        slot = ref.getProgressionId();
                    break;
                }
            }
            // Then overlay refs that target specific repeat indices.
            for (const auto& ref : refs)
            {
                const auto indices = ref.getRepeatIndices().trim();
                if (indices.isEmpty() || ref.getRepeatScope() == "all")
                    continue;
                auto tokens = juce::StringArray::fromTokens(indices, ",", "");
                for (const auto& t : tokens)
                {
                    const int idx = t.trim().getIntValue() - 1; // 0-based
                    if (idx >= 0 && idx < repeatCount)
                        repeatProgIds[static_cast<size_t>(idx)] = ref.getProgressionId();
                }
            }

            // Convert the id map to RepeatCell values.
            std::vector<ContextLane::BlockData::RepeatCell> cells;
            cells.reserve(static_cast<size_t>(repeatCount));
            for (const auto& progId : repeatProgIds)
            {
                bool isVar = false;
                if (const auto* prog = findProg(progId))
                    isVar = prog->getVariantOf().isNotEmpty();
                cells.push_back({ isVar });
            }

            // Progression name for the subtitle line.
            juce::String progName;
            if (!refs.empty())
            {
                if (const auto* firstProg = findProg(refs[0].getProgressionId()))
                    progName = firstProg->getName();
                bool multi = false;
                for (size_t i = 1; i < refs.size(); ++i)
                    if (refs[i].getProgressionId() != refs[0].getProgressionId())
                        { multi = true; break; }
                if (multi) progName = "Multi";
            }

            entries.push_back({ section.getId(), section.getName(), withRepeats,
                                 timeSig, tint, boundary, progName, std::move(cells) });
        }

        std::vector<ContextLane::BlockData> blockData;
        for (auto& e : entries)
        {
            ContextLane::BlockData block;
            block.id = e.id;
            block.label = e.name;
            block.widthFraction = static_cast<float>(e.beats / totalBeats);
            block.timeSigLabel = e.timeSig != "4/4" ? e.timeSig : juce::String();
            block.tint = e.tint;
            block.boundaryScoreColor = e.boundaryColor;
            block.subtitle = e.progressionName;
            block.repeatCells = std::move(e.repeatCells);
            blockData.push_back(std::move(block));
        }

        sectionLane.setBlocks(std::move(blockData), currentSectionId, std::move(cb));
        sectionLane.setBeatGrid(totalBeats, 1.0, 4.0);
    }

    void updateChords(const std::vector<model::Chord>& chords,
                      const juce::String& currentChordId,
                      double beatUnitValue,
                      double beatsPerBarValue,
                      ContextLane::SelectionCallback cb)
    {
        if (chords.empty())
        {
            theoryLane.setBlocks({}, {}, nullptr);
            return;
        }

        double totalDuration = 0.0;
        for (const auto& chord : chords)
        {
            double d = chord.getDurationBeats();
            totalDuration += (d > 0.0 ? d : 1.0);
        }

        std::vector<ContextLane::BlockData> blockData;
        for (const auto& chord : chords)
        {
            double d = chord.getDurationBeats();
            if (d <= 0.0) d = 1.0;

            ContextLane::BlockData block;
            block.id = chord.getId();
            block.label = chord.getSymbol();
            block.subtitle = chord.getName();
            block.functionTag = chord.getFunction();
            block.widthFraction = static_cast<float>(d / totalDuration);
            block.confidence = chord.getConfidence();
            block.confidenceFromMidi = chord.getSource().equalsIgnoreCase("midi");
            blockData.push_back(std::move(block));
        }

        theoryLane.setBlocks(std::move(blockData), currentChordId, std::move(cb));
        theoryLane.setBeatGrid(totalDuration, beatUnitValue, beatsPerBarValue);
    }

    explicit TimelineShell(ActionCallback callback)
        : sectionLane(
              "Section Lane",
              juce::Colour(0xff28323f),
              "Container and section timing alignment (right-click for section theory tools)",
              TheoryMenuTarget::section,
              callback),
          theoryLane(
              "Theory Strip",
              juce::Colour(0xff203043),
              "Chord/function overlays aligned to timeline (right-click for chord tools)",
              TheoryMenuTarget::chord,
              callback),
          historyLane(
              "History Buffer Lane",
              juce::Colour(0xff232338),
              "Always-on capture lane (right-click for capture/grab controls)",
              TheoryMenuTarget::historyBuffer,
              callback)
    {
        addAndMakeVisible(sectionLane);
        addAndMakeVisible(theoryLane);
        addAndMakeVisible(historyLane);
    }

    void setPlayheadFraction(double fraction)
    {
        playheadFraction = juce::jlimit(0.0, 1.0, fraction);
        sectionLane.setPlayheadFraction(playheadFraction);
        theoryLane.setPlayheadFraction(playheadFraction);
        historyLane.setPlayheadFraction(playheadFraction);
        repaint();
    }

    void setHistorySourceLabel(const juce::String& sourceLabel)
    {
        historyLane.setHistorySourceLabel(sourceLabel);
    }

    juce::Rectangle<int> getTrackAreaBoundsInParent() const
    {
        auto area = getLocalBounds().reduced(0, 1);
        area.removeFromTop(sectionHeight);
        area.removeFromTop(theoryHeight);
        area.removeFromBottom(historyHeight);
        return area.translated(getX(), getY());
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(0, 1);

        sectionLane.setBounds(bounds.removeFromTop(sectionHeight));
        theoryLane.setBounds(bounds.removeFromTop(theoryHeight));
        historyLane.setBounds(bounds.removeFromBottom(historyHeight));
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        auto trackArea = getLocalBounds().reduced(0, 1);
        trackArea.removeFromTop(sectionHeight);
        trackArea.removeFromTop(theoryHeight);
        trackArea.removeFromBottom(historyHeight);

        const auto x = trackArea.getX() + static_cast<int>(std::round(playheadFraction * trackArea.getWidth()));
        g.setColour(juce::Colours::white.withAlpha(0.28f));
        g.drawVerticalLine(x, static_cast<float>(trackArea.getY()), static_cast<float>(trackArea.getBottom()));
    }

private:
    static constexpr int sectionHeight = 38;
    static constexpr int theoryHeight = 38;
    static constexpr int historyHeight = 60;

    ContextLane sectionLane;
    ContextLane theoryLane;
    ContextLane historyLane;
    double playheadFraction { 0.0 };
};

class WorkspaceShellComponent::DragBar final : public juce::Component
{
public:
    enum class Orientation
    {
        vertical,
        horizontal
    };

    DragBar(Orientation barOrientation,
            std::function<void(int)> dragDeltaHandler,
            std::function<void()> dragEndHandler)
        : orientation(barOrientation),
          onDragDelta(std::move(dragDeltaHandler)),
          onDragEnd(std::move(dragEndHandler))
    {
        setMouseCursor(orientation == Orientation::vertical
                           ? juce::MouseCursor::LeftRightResizeCursor
                           : juce::MouseCursor::UpDownResizeCursor);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& theme = ThemeManager::get().theme();
        g.fillAll(setle::theme::panelHeaderBackground(theme, setle::theme::ZoneRole::neutral));
        g.setColour(setle::theme::timelineGridLine(theme, true).withAlpha(0.55f));

        if (orientation == Orientation::vertical)
            g.drawVerticalLine(getWidth() / 2, 2.0f, static_cast<float>(getHeight() - 2));
        else
            g.drawHorizontalLine(getHeight() / 2, 2.0f, static_cast<float>(getWidth() - 2));
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        lastMousePosition = orientation == Orientation::vertical ? event.getScreenX() : event.getScreenY();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        const auto currentPosition = orientation == Orientation::vertical ? event.getScreenX() : event.getScreenY();
        const auto delta = currentPosition - lastMousePosition;
        lastMousePosition = currentPosition;

        if (delta != 0 && onDragDelta != nullptr)
            onDragDelta(delta);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (onDragEnd != nullptr)
            onDragEnd();
    }

private:
    Orientation orientation;
    std::function<void(int)> onDragDelta;
    std::function<void()> onDragEnd;
    int lastMousePosition = 0;
};

WorkspaceShellComponent::WorkspaceShellComponent(te::Engine& engine)
    : engineRef(engine)
{
    shellTrace("constructor: begin");
    grabSamplerQueue = std::make_unique<setle::capture::GrabSamplerQueue>();

    setWantsKeyboardFocus(true);

    auto topStripLabelFont = juce::FontOptions(15.0f);
    topTitle.setText("IN > WORK > OUT", juce::dontSendNotification);
    topTitle.setFont(topStripLabelFont);
    topTitle.setJustificationType(juce::Justification::centredLeft);
    topTitle.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.88f));

    interactionStatus.setText("Right-click timeline lanes to access theory actions", juce::dontSendNotification);
    interactionStatus.setFont(juce::FontOptions(13.0f));
    interactionStatus.setJustificationType(juce::Justification::centredLeft);
    interactionStatus.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.72f));

    quesoLogoComponent = std::make_unique<QuesoLogoComponent>();
    topStrip.addAndMakeVisible(*quesoLogoComponent);
    topStrip.addAndMakeVisible(topTitle);
    topStrip.addAndMakeVisible(interactionStatus);
    topStrip.addAndMakeVisible(playButton);
    topStrip.addAndMakeVisible(stopButton);
    topStrip.addAndMakeVisible(recordButton);
    topStrip.addAndMakeVisible(bpmLabel);
    topStrip.addAndMakeVisible(bpmEditor);
    topStrip.addAndMakeVisible(focusInButton);
    topStrip.addAndMakeVisible(focusBalancedButton);
    topStrip.addAndMakeVisible(focusOutButton);
    topStrip.addAndMakeVisible(undoTheoryButton);
    topStrip.addAndMakeVisible(redoTheoryButton);
    topStrip.addAndMakeVisible(themeButton);
    themeButton.onClick = [this] { showThemeEditor(!themeDismissOverlay.isVisible()); };

    themeDismissOverlay.setAlwaysOnTop(true);
    themeDismissOverlay.setInterceptsMouseClicks(false, false);
    addChildComponent(themeDismissOverlay);

    themeEditorPanel = std::make_unique<ThemeEditorPanel>();
    addChildComponent(*themeEditorPanel);

    // Session Key/Mode Selectors
    sessionKeyLabel.setText("Key:", juce::dontSendNotification);
    sessionKeyLabel.setFont(juce::FontOptions(13.0f));
    sessionKeyLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.88f));
    topStrip.addAndMakeVisible(sessionKeyLabel);

    sessionKeySelector.addItem("C", 1);
    sessionKeySelector.addItem("C#", 2);
    sessionKeySelector.addItem("D", 3);
    sessionKeySelector.addItem("Eb", 4);
    sessionKeySelector.addItem("E", 5);
    sessionKeySelector.addItem("F", 6);
    sessionKeySelector.addItem("F#", 7);
    sessionKeySelector.addItem("G", 8);
    sessionKeySelector.addItem("Ab", 9);
    sessionKeySelector.addItem("A", 10);
    sessionKeySelector.addItem("Bb", 11);
    sessionKeySelector.addItem("B", 12);
    sessionKeySelector.setSelectedId(1, juce::dontSendNotification);
    sessionKeySelector.setColour(juce::ComboBox::textColourId, juce::Colours::white.withAlpha(0.95f));
    sessionKeySelector.setColour(juce::ComboBox::backgroundColourId, ThemeManager::get().theme().surface3.withAlpha(0.78f));
    sessionKeySelector.onChange = [this]()
    {
        auto items = juce::StringArray { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        int selectedId = sessionKeySelector.getSelectedId();
        if (selectedId >= 1 && selectedId <= 12)
        {
            songState.setSessionKey(items[selectedId - 1]);
            saveSongState();
            refreshTimelineData();
        }
    };
    topStrip.addAndMakeVisible(sessionKeySelector);

    sessionModeLabel.setText("Mode:", juce::dontSendNotification);
    sessionModeLabel.setFont(juce::FontOptions(13.0f));
    sessionModeLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.88f));
    topStrip.addAndMakeVisible(sessionModeLabel);

    sessionModeSelector.addItem("ionian", 1);
    sessionModeSelector.addItem("dorian", 2);
    sessionModeSelector.addItem("phrygian", 3);
    sessionModeSelector.addItem("lydian", 4);
    sessionModeSelector.addItem("mixolydian", 5);
    sessionModeSelector.addItem("aeolian", 6);
    sessionModeSelector.addItem("locrian", 7);
    sessionModeSelector.setSelectedId(1, juce::dontSendNotification);
    sessionModeSelector.setColour(juce::ComboBox::textColourId, juce::Colours::white.withAlpha(0.95f));
    sessionModeSelector.setColour(juce::ComboBox::backgroundColourId, ThemeManager::get().theme().surface3.withAlpha(0.78f));
    sessionModeSelector.onChange = [this]()
    {
        auto modes = juce::StringArray { "ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian", "locrian" };
        int selectedId = sessionModeSelector.getSelectedId();
        if (selectedId >= 1 && selectedId <= 7)
        {
            songState.setSessionMode(modes[selectedId - 1]);
            saveSongState();
            refreshTimelineData();
        }
    };
    topStrip.addAndMakeVisible(sessionModeSelector);

    toolPalette = std::make_unique<ToolPaletteComponent>();
    topStrip.addAndMakeVisible(toolPalette.get());
    topStrip.addAndMakeVisible(captureSourceLabel);
    topStrip.addAndMakeVisible(captureSourceSelector);

    addAndMakeVisible(topStrip);

    // ---- Classic menu bar ----
    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);
    appMenuBarModel  = std::make_unique<AppMenuBarModel>(commandManager);
    menuBarComponent = std::make_unique<juce::MenuBarComponent>(appMenuBarModel.get());
    addAndMakeVisible(*menuBarComponent);

    // ---- Left nav section strip ----
    activeNavSection = navSectionFromString(setle::state::AppPreferences::get().getActiveNavSection("edit"));
    leftNavComponent = std::make_unique<LeftNavComponent>();
    leftNavComponent->setActiveSection(activeNavSection);
    shellTrace(juce::String("constructor: left nav created with section=") + getNavSectionInfo(activeNavSection).id);
    leftNavComponent->onSectionChanged = [this](NavSection section)
    {
        switchNavSection(section);
    };
    addAndMakeVisible(*leftNavComponent);

    inPanel = new InDevicePanel(
        engineRef,
        [this]() { return grabSamplerQueue.get(); },
        [this]() -> const model::Song* { return &songState; },
        {
            [this](int slotIndex) { playQueueSlot(slotIndex); },
            [this]() { stopQueuePlayback(); },
            [this](int slotIndex) { editQueueSlotInTheoryPanel(slotIndex); },
            [this](int slotIndex) { promoteQueueSlotToLibrary(slotIndex); },
            [this](int slotIndex) { dropQueueSlotToTimeline(slotIndex); },
            [this](int slotIndex) { clearQueueSlot(slotIndex); },
            [this](int slotIndex)
            {
                if (grabSamplerQueue == nullptr)
                    return;

                auto& slot = grabSamplerQueue->getSlotMutable(slotIndex);
                slot.looping = !slot.looping;
                updateInPanelQueueView();
            },
            [this](int slotIndex) { renameQueueSlot(slotIndex); },
            [this](juce::Component& target, const juce::String& progressionId)
            {
                auto menu = buildProgressionContextMenu(progressionId);
                menu.showMenuAsync(
                    juce::PopupMenu::Options().withTargetComponent(&target),
                    [safeThis = juce::Component::SafePointer<WorkspaceShellComponent>(this), progressionId](int selectedAction)
                    {
                        if (safeThis == nullptr || selectedAction <= 0)
                            return;

                        safeThis->handleProgressionAction(progressionId, selectedAction);
                    });
            }
        });
    addAndMakeVisible(inPanel);

    workPanel = new LabelPanel(
        "WORK",
        juce::Colour(0xff242835),
        "Active editor host:\npiano roll, sequencer, notation,\nor focused plugin UI");
    addAndMakeVisible(workPanel);
    configureTheoryEditorPanel();

    timelineShell = new TimelineShell(
        [this](TheoryMenuTarget target, int actionId, const juce::String& actionName)
        {
            handleTimelineTheoryAction(target, actionId, actionName);
        });
    addAndMakeVisible(timelineShell);

    leftResizeBar = new DragBar(
        DragBar::Orientation::vertical,
        [this](int delta)
        {
            leftPanelWidth += delta;
            focusMode = FocusMode::balanced;
            resized();
        },
        [this] { saveLayoutState(); });
    addAndMakeVisible(leftResizeBar);

    rightResizeBar = new DragBar(
        DragBar::Orientation::vertical,
        [this](int delta)
        {
            rightPanelWidth -= delta;
            focusMode = FocusMode::balanced;
            resized();
        },
        [this] { saveLayoutState(); });
    addAndMakeVisible(rightResizeBar);

    timelineResizeBar = new DragBar(
        DragBar::Orientation::horizontal,
        [this](int delta)
        {
            timelineHeight -= delta;
            resized();
        },
        [this] { saveLayoutState(); });
    addAndMakeVisible(timelineResizeBar);

    // Defer initial section switch until core shell components exist.
    switchNavSection(activeNavSection);
    shellTrace("constructor: initial section switch applied");

    focusInButton.onClick = [this] { applyFocusMode(FocusMode::inFocused); };
    focusBalancedButton.onClick = [this] { applyFocusMode(FocusMode::balanced); };
    focusOutButton.onClick = [this] { applyFocusMode(FocusMode::outFocused); };
    focusInButton.setTooltip("Emphasize the IN panel (capture and queue)");
    focusBalancedButton.setTooltip("Balanced layout across IN/WORK/OUT");
    focusOutButton.setTooltip("Emphasize the OUT panel (instruments and mix)");
    undoTheoryButton.onClick = [this] { performUndo(); };
    redoTheoryButton.onClick = [this] { performRedo(); };

    playButton.onClick = [this]
    {
        if (edit != nullptr)
        {
            loadProgressionToEdit();
            edit->getTransport().play(false);
        }
    };

    stopButton.onClick = [this]
    {
        if (edit != nullptr)
            edit->getTransport().stop(false, false);
    };

    recordButton.onClick = [this]
    {
        if (edit != nullptr)
        {
            loadProgressionToEdit();
            edit->getTransport().record(false);
        }
    };

    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.72f));
    bpmLabel.setFont(juce::FontOptions(13.0f));
    captureSourceLabel.setText("Capture", juce::dontSendNotification);
    captureSourceLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.72f));
    captureSourceLabel.setFont(juce::FontOptions(13.0f));
    captureSourceSelector.setColour(juce::ComboBox::textColourId, juce::Colours::white.withAlpha(0.95f));
    captureSourceSelector.setColour(juce::ComboBox::backgroundColourId, ThemeManager::get().theme().surface3.withAlpha(0.78f));

    captureSourceSelector.onChange = [this]
    {
        applyCaptureSourceSelection();
    };

    bpmEditor.setInputRestrictions(6, "0123456789.");
    bpmEditor.setJustification(juce::Justification::centred);
    bpmEditor.setColour(juce::TextEditor::backgroundColourId, ThemeManager::get().theme().surface3.withAlpha(0.75f));
    bpmEditor.setColour(juce::TextEditor::outlineColourId, ThemeManager::get().theme().surfaceEdge.withAlpha(0.8f));
    captureSourceSelector.setTooltip("Capture source (pulses when incoming MIDI is detected)");
    bpmEditor.onReturnKey = [this]
    {
        const double bpm = juce::jlimit(20.0, 300.0, bpmEditor.getText().getDoubleValue());
        songState.setBpm(bpm);
        if (edit != nullptr)
            if (auto* tempo = edit->tempoSequence.getTempo(0))
                tempo->setBpm(bpm);
        bpmEditor.setText(juce::String(bpm, 1), juce::dontSendNotification);
    };

    juce::LookAndFeel::setDefaultLookAndFeel(&setleLookAndFeel);
    ThemeManager::get().addListener(this);
    ThemeManager::get().applyTheme(ThemePresets::presetByName(setle::state::AppPreferences::get().getActiveThemeName()));

    refreshCaptureSourceSelector();
    midiDeviceSignature = buildMidiDeviceSignature();

    loadLayoutState();
    initialiseSongState();

    // --- Phase 4: create a single-track Tracktion Edit and sync BPM ---
    edit = te::Edit::createSingleTrackEdit(engineRef);
    if (edit == nullptr)
    {
        shellTrace("constructor: fallback active, edit creation returned null");
        interactionStatus.setText("Startup fallback: edit engine unavailable. UI running in limited mode.",
                                  juce::dontSendNotification);
    }
    historyBuffer = std::make_unique<setle::capture::HistoryBuffer>(
        setle::state::AppPreferences::get().getHistoryBufferMaxBeats(64));
    const auto historyBeats = juce::jmax(16, setle::state::AppPreferences::get().getHistoryBufferMaxBeats(64));
    const auto captureSeconds = juce::jlimit(30.0,
                                             180.0,
                                             (60.0 / juce::jmax(20.0, songState.getBpm())) * static_cast<double>(historyBeats) + 2.0);
    circularAudioBuffer = std::make_unique<setle::capture::CircularAudioBuffer>(captureSeconds);
    outputCaptureTap = std::make_unique<OutputCaptureTap>(*circularAudioBuffer);
    engineRef.getDeviceManager().deviceManager.addAudioCallback(outputCaptureTap.get());
    sessionDashboardComponent = std::make_unique<SessionDashboardComponent>(engineRef,
                                                                            songState,
                                                                            *circularAudioBuffer,
                                                                            edit.get());
    if (auto* captureTap = dynamic_cast<OutputCaptureTap*>(outputCaptureTap.get()))
    {
        captureTap->setOnAudioBlockCaptured(
            [safeDashboard = juce::Component::SafePointer<SessionDashboardComponent>(sessionDashboardComponent.get())]
            (float peakLevel, int numSamples)
            {
                if (safeDashboard != nullptr)
                    safeDashboard->pushAudioCallbackMetrics(peakLevel, numSamples);
            });
    }
    workPanel->addAndMakeVisible(*sessionDashboardComponent);
    sessionDashboardComponent->setVisible(false);
    refreshCaptureSourceSelector(setle::state::AppPreferences::get().getCaptureSource());
    if (grabSamplerQueue != nullptr)
        grabSamplerQueue->setSong(&songState);
    if (edit != nullptr)
    {
        if (auto* tempo = edit->tempoSequence.getTempo(0))
            tempo->setBpm(songState.getBpm());

        trackManager = std::make_unique<setle::timeline::TrackManager>(*edit);
        timelineStampManager = std::make_unique<setle::timeline::TimelineStampManager>(*edit);
        trackManager->ensureDefaultTracks();
        ensureInstrumentSlots();
        applyPersistedInstrumentSlotAssignments();

        outPanelHost = std::make_unique<OutPanelHost>();
        addAndMakeVisible(*outPanelHost);

        if (auto midiTracks = trackManager->getMidiTracks(); !midiTracks.isEmpty())
        {
            if (auto* firstMidi = midiTracks.getFirst())
            {
                const auto trackId = firstMidi->itemID.toString();
                auto it = instrumentSlots.find(trackId);
                if (it != instrumentSlots.end() && it->second->getInfo().type == setle::instruments::InstrumentSlot::SlotType::Empty)
                    it->second->loadPolySynth();
            }
        }

        rebuildOutPanelStrips();

        timelineTracks = new setle::timeline::TimelineTracksComponent(*edit, *trackManager);
        timelineTracks->setVisibleBeatRange(0.0, 32.0);
        timelineTracks->onStatusMessage = [this](const juce::String& message)
        {
            interactionStatus.setText(message, juce::dontSendNotification);
        };
        timelineTracks->onClipClicked = [this](te::Clip& clip)
        {
            // Find progression linked to this clip by name
            juce::String targetProgId;
            const auto progs = songState.getProgressions();
            for (const auto& prog : progs)
            {
                if (prog.getName() == clip.getName())
                {
                    targetProgId = prog.getId();
                    break;
                }
            }
            if (targetProgId.isEmpty() && !progs.empty())
                targetProgId = progs[0].getId();

            if (gridRollComponent != nullptr && !targetProgId.isEmpty())
            {
                gridRollComponent->setTargetProgression(targetProgId);
                gridRollComponent->setTheorySnap(theorySnap);
            }
            switchWorkTab(true);
            interactionStatus.setText("GridRoll: " + clip.getName(), juce::dontSendNotification);
        };
        addAndMakeVisible(timelineTracks);
        timelineTracks->refreshTracks();

        // ---- GridRollComponent (needs edit to be valid) ----
        gridRollComponent = std::make_unique<setle::gridroll::GridRollComponent>(songState, *edit);
        gridRollComponent->setVisible(false);   // theory tab is default
        gridRollComponent->setTheorySnap(theorySnap);
        gridRollComponent->onProgressionEdited = [this](const juce::String& progId)
        {
            const auto snapshot = createSongSnapshot();
            saveSongState();
            if (!progId.isEmpty())
                loadProgressionToEdit(progId, 0.0, true, nullptr);
            refreshTimelineData();
            captureUndoStateIfChanged(snapshot);
        };
        gridRollComponent->onPrepareProgressionForNoteMode = [this](const juce::String& progId)
        {
            ensureProgressionLoadedForNoteMode(progId);
        };
        gridRollComponent->onStatusMessage = [this](const juce::String& msg)
        {
            interactionStatus.setText(msg, juce::dontSendNotification);
        };
        gridRollComponent->onSelectionMetadataChanged = [this](const setle::gridroll::GridRollComponent::SelectionMetadata& metadata)
        {
            handleGridRollSelectionChanged(metadata);
        };
        gridRollComponent->onConstraintLockChanged = [this](bool scaleLock, bool chordLock)
        {
            setle::state::AppPreferences::get().setScaleLockEnabled(scaleLock);
            setle::state::AppPreferences::get().setChordLockEnabled(chordLock);
            interactionStatus.setText(
                "Constraints: Scale Lock " + juce::String(scaleLock ? "ON" : "OFF")
                    + " | Chord Lock " + juce::String(chordLock ? "ON" : "OFF"),
                juce::dontSendNotification);
        };
        gridRollComponent->onDrumPatternEdited = [this](const std::vector<setle::gridroll::GridRollCell>& cells,
                                                        const juce::String& progId)
        {
            applyDrumPatternToSlots(cells, progId);
        };
        gridRollComponent->setScaleLock(setle::state::AppPreferences::get().getScaleLockEnabled());
        gridRollComponent->setChordLock(setle::state::AppPreferences::get().getChordLockEnabled());
        if (!selectedProgressionId.isEmpty())
            gridRollComponent->setTargetProgression(selectedProgressionId);
        workPanel->addAndMakeVisible(*gridRollComponent);
        shellTrace("constructor: edit/timeline/gridroll initialized");
    }
    bpmEditor.setText(juce::String(songState.getBpm(), 1), juce::dontSendNotification);

    openTheoryEditor(TheoryMenuTarget::section, sectionEditTheory, "Edit Section Theory");
    updateFocusButtonState();
    updateUndoRedoButtonState();
    startTimerHz(30);
    updateInPanelQueueView();
    shellTrace("constructor: completed");
}

WorkspaceShellComponent::~WorkspaceShellComponent()
{
    stopTimer();
    if (outputCaptureTap != nullptr)
        engineRef.getDeviceManager().deviceManager.removeAudioCallback(outputCaptureTap.get());
    outputCaptureTap.reset();
    circularAudioBuffer.reset();
    saveLayoutState();
    saveSongState();
    ThemeManager::get().removeListener(this);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

std::optional<setle::timeline::TimelineStampManager::GhostTrail> WorkspaceShellComponent::getGhostTrail(const juce::String& sectionId) const
{
    if (timelineStampManager == nullptr || sectionId.isEmpty())
        return std::nullopt;

    return timelineStampManager->getGhostTrail(sectionId);
}

bool WorkspaceShellComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && themeEditorPanel != nullptr && themeEditorPanel->isVisible())
    {
        showThemeEditor(false);
        return true;
    }

    // Single-letter tool shortcuts (no modifiers)
    const auto keyMods = key.getModifiers();
    const auto modifierMask = juce::ModifierKeys::shiftModifier
                            | juce::ModifierKeys::ctrlModifier
                            | juce::ModifierKeys::altModifier
                            | juce::ModifierKeys::commandModifier;
    const bool hasNoModifiers = (keyMods.getRawFlags() & modifierMask) == 0;
    if (hasNoModifiers)
    {
        const auto code = key.getKeyCode();
        switch (code)
        {
            case 's': case 'S': EditToolManager::get().setActiveTool(EditTool::Select); return true;
            case 'd': case 'D': EditToolManager::get().setActiveTool(EditTool::Draw); return true;
            case 'e': case 'E': EditToolManager::get().setActiveTool(EditTool::Erase); return true;
            case 'b': case 'B': EditToolManager::get().setActiveTool(EditTool::Split); return true;
            case 't': case 'T': EditToolManager::get().setActiveTool(EditTool::Stretch); return true;
            case 'l': case 'L': EditToolManager::get().setActiveTool(EditTool::Listen); return true;
            case 'm': case 'M': EditToolManager::get().setActiveTool(EditTool::Marquee); return true;
            case 'g': case 'G': EditToolManager::get().setActiveTool(EditTool::ScaleSnap); return true;
            case 'h': case 'H':
                if (activeNavSection == NavSection::edit && workPanelTabIndex == 1 && gridRollComponent != nullptr)
                {
                    gridRollComponent->toggleHumanizePopover();
                    return true;
                }
                break;
            default: break;
        }
    }
    const auto commandLike = keyMods.isCommandDown() || keyMods.isCtrlDown();

    if (!commandLike)
        return false;

    if (key.getKeyCode() == 'z' || key.getKeyCode() == 'Z')
    {
        if (keyMods.isShiftDown())
            performRedo();
        else
            performUndo();
        return true;
    }

    if (key.getKeyCode() == 'y' || key.getKeyCode() == 'Y')
    {
        performRedo();
        return true;
    }

    return false;
}

void WorkspaceShellComponent::mouseDown(const juce::MouseEvent& event)
{
    if (themeEditorPanel != nullptr && themeEditorPanel->isVisible() && !themeEditorPanel->getBounds().contains(event.getPosition()))
    {
        showThemeEditor(false);
    }
}

void WorkspaceShellComponent::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    g.fillAll(setle::theme::panelBackground(theme, setle::theme::ZoneRole::neutral));

    const auto topBounds = juce::Rectangle<int>(0, 0, getWidth(), topStripHeight);
    g.setColour(setle::theme::panelHeaderBackground(theme, setle::theme::ZoneRole::workPanel));
    g.fillRect(topBounds);
    g.setColour(setle::theme::timelineGridLine(theme, true, true));
    g.fillRect(0, topStripHeight - 1, getWidth(), 1);

    auto toRoot = [this](const juce::Rectangle<int>& r)
    {
        return r.translated(topStrip.getX(), topStrip.getY());
    };

    const auto fieldStyle = setle::theme::rowStyle(theme, setle::theme::SurfaceState::hovered);
    g.setColour(fieldStyle.fill.withAlpha(0.92f));
    g.fillRoundedRectangle(toRoot(bpmEditor.getBounds()).toFloat().expanded(1.0f, 1.0f), 3.0f);
    g.fillRoundedRectangle(toRoot(sessionKeySelector.getBounds()).toFloat().expanded(1.0f, 1.0f), 3.0f);
    g.fillRoundedRectangle(toRoot(sessionModeSelector.getBounds()).toFloat().expanded(1.0f, 1.0f), 3.0f);
    g.fillRoundedRectangle(toRoot(captureSourceSelector.getBounds()).toFloat().expanded(1.0f, 1.0f), 3.0f);

    if (toolPalette != nullptr)
    {
        g.setColour(fieldStyle.fill.withAlpha(0.72f));
        g.fillRoundedRectangle(toRoot(toolPalette->getBounds()).toFloat().expanded(1.0f, 1.0f), 4.0f);
    }

    if (captureSourceActivity > 0.01f)
    {
        const auto cap = toRoot(captureSourceSelector.getBounds());
        const float pulse = juce::jlimit(0.0f, 1.0f, captureSourceActivity);
        const auto dot = juce::Rectangle<float>(static_cast<float>(cap.getRight() + 6),
                                                static_cast<float>(cap.getCentreY() - 3),
                                                6.0f, 6.0f);
        g.setColour(theme.zoneC.withAlpha(0.45f + 0.50f * pulse));
        g.fillEllipse(dot);
    }

    if (theoryEditorPanel.isVisible() && activeNavSection == NavSection::edit)
    {
        const auto panelBounds = getLocalArea(&theoryEditorPanel, theoryEditorPanel.getLocalBounds()).toFloat();
        const auto outer = panelBounds.expanded(2.0f);

        // Pebbled matte edge treatment to make the editor feel recessed into the chassis.
        g.setColour(theme.surface0.withAlpha(0.88f));
        g.fillRoundedRectangle(outer, 8.0f);
        for (int y = static_cast<int>(outer.getY()) + 2; y < static_cast<int>(outer.getBottom()) - 2; y += 2)
        {
            const auto alpha = ((y / 2) % 2 == 0) ? 0.010f : 0.018f;
            g.setColour(theme.surface1.withAlpha(alpha));
            g.fillRect(static_cast<int>(outer.getX()) + 2, y,
                       static_cast<int>(outer.getWidth()) - 4, 1);
        }

        // Fuzzy inset shell around the full editor panel.
        g.setColour(theme.surface3.withAlpha(0.82f));
        g.fillRoundedRectangle(panelBounds.reduced(1.0f), 7.0f);
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        g.drawRoundedRectangle(panelBounds.reduced(1.0f), 7.0f, 1.4f);
        g.setColour(theme.inkLight.withAlpha(0.07f));
        g.drawRoundedRectangle(panelBounds.reduced(2.5f), 6.0f, 0.8f);
    }

    drawThemePreviewHighlight(g);
}

void WorkspaceShellComponent::themeChanged()
{
    if (themeEditorPanel != nullptr)
        themeEditorPanel->refreshControls();
    repaintEntireTree();
}

void WorkspaceShellComponent::themePreviewTargetChanged(const juce::String&)
{
    repaint();
}

void WorkspaceShellComponent::repaintEntireTree()
{
    std::function<void(juce::Component*)> repaintChildren = [&](juce::Component* component)
    {
        if (component == nullptr)
            return;

        component->repaint();
        for (int i = 0; i < component->getNumChildComponents(); ++i)
            repaintChildren(component->getChildComponent(i));
    };

    repaintChildren(this);
}

void WorkspaceShellComponent::showThemeEditor(bool shouldShow)
{
    if (themeEditorPanel == nullptr)
        return;

    themeDismissOverlay.setVisible(shouldShow);
    themeEditorPanel->setVisible(shouldShow);
    if (!shouldShow)
        ThemeManager::get().setPreviewHighlightToken({});
    if (shouldShow)
        themeEditorPanel->toFront(true);
    resized();
}

juce::Rectangle<int> WorkspaceShellComponent::highlightBoundsForThemeToken(const juce::String& token) const
{
    auto normalized = token.trim();
    if (normalized.isEmpty())
        return {};

    auto toRootRect = [this](const juce::Rectangle<int>& r)
    {
        return r.translated(topStrip.getX(), topStrip.getY());
    };

    auto componentToThis = [this](const juce::Component* component) -> juce::Rectangle<int>
    {
        if (component == nullptr || !component->isVisible())
            return {};
        return getLocalArea(component, component->getLocalBounds());
    };

    auto normalizeStartsWith = [&normalized](const char* prefix)
    {
        return normalized.startsWithIgnoreCase(prefix);
    };

    auto unionIfValid = [](juce::Rectangle<int> a, juce::Rectangle<int> b)
    {
        if (a.isEmpty()) return b;
        if (b.isEmpty()) return a;
        return a.getUnion(b);
    };

    if (normalized == "surface0")
        return getLocalBounds();

    if (normalizeStartsWith("header") || normalizeStartsWith("menuBar") || normalized == "focusOutline")
        return { 0, 0, getWidth(), topStripHeight };

    if (normalized == "zoneAWidth" || normalizeStartsWith("zoneA") || normalized == "signalMidi")
        return inPanel != nullptr ? inPanel->getBounds() : juce::Rectangle<int>{};

    if (normalized == "zoneCWidth" || normalizeStartsWith("zoneC") || normalized == "signalAudio")
        return outPanelHost != nullptr ? outPanelHost->getBounds() : juce::Rectangle<int>{};

    if (normalizeStartsWith("zoneB"))
        return workPanel != nullptr ? workPanel->getBounds() : juce::Rectangle<int>{};

    if (normalized == "zoneDNormHeight"
        || normalizeStartsWith("zoneD")
        || normalizeStartsWith("tape")
        || normalizeStartsWith("playhead")
        || normalized == "housingEdge")
        return timelineShell != nullptr ? timelineShell->getBounds() : juce::Rectangle<int>{};

    if (normalized == "cardBg"
        || normalized == "rowBg"
        || normalized == "rowHover"
        || normalized == "rowSelected")
    {
        auto r = componentToThis(&theoryEditorPanel);
        r = unionIfValid(r, componentToThis(libraryBrowser.get()));
        r = unionIfValid(r, componentToThis(chordPalette.get()));
        return r.isEmpty() && workPanel != nullptr ? workPanel->getBounds() : r;
    }

    if (normalizeStartsWith("control")
        || normalizeStartsWith("btn")
        || normalizeStartsWith("slider")
        || normalizeStartsWith("knob")
        || normalizeStartsWith("switch")
        || normalized == "sizeTransport")
    {
        auto r = toRootRect(playButton.getBounds());
        r = unionIfValid(r, toRootRect(stopButton.getBounds()));
        r = unionIfValid(r, toRootRect(recordButton.getBounds()));
        r = unionIfValid(r, toRootRect(bpmEditor.getBounds()));
        r = unionIfValid(r, toRootRect(sessionKeySelector.getBounds()));
        r = unionIfValid(r, toRootRect(sessionModeSelector.getBounds()));
        r = unionIfValid(r, toRootRect(captureSourceSelector.getBounds()));
        r = unionIfValid(r, componentToThis(outPanelHost.get()));
        return r.expanded(6, 4);
    }

    if (normalizeStartsWith("surface"))
    {
        if (normalized == "surface1" && workPanel != nullptr)
            return workPanel->getBounds();
        if (normalized == "surface2" && outPanelHost != nullptr)
            return outPanelHost->getBounds();
        if (normalized == "surface3" || normalized == "surface4" || normalized == "surfaceEdge")
        {
            auto r = toRootRect(sessionKeySelector.getBounds());
            r = unionIfValid(r, toRootRect(sessionModeSelector.getBounds()));
            r = unionIfValid(r, toRootRect(captureSourceSelector.getBounds()));
            r = unionIfValid(r, toRootRect(bpmEditor.getBounds()));
            return r.expanded(6, 4);
        }
    }

    if (normalizeStartsWith("ink") || normalizeStartsWith("size") || normalizeStartsWith("space"))
    {
        auto r = toRootRect(topTitle.getBounds());
        r = unionIfValid(r, toRootRect(interactionStatus.getBounds()));
        r = unionIfValid(r, componentToThis(&theoryEditorTitle));
        r = unionIfValid(r, componentToThis(&theoryEditorHint));
        return r.isEmpty() && workPanel != nullptr ? workPanel->getBounds() : r;
    }

    if (normalized == "signalCv" || normalized == "signalGate")
    {
        auto r = componentToThis(timelineShell);
        r = unionIfValid(r, componentToThis(timelineTracks));
        return r;
    }

    if (normalized == "badgeBg")
        return componentToThis(timelineShell);

    if (normalized == "moduleSlotHeight")
        return componentToThis(outPanelHost.get());

    if (normalized == "stepHeight" || normalized == "stepWidth")
        return componentToThis(gridRollComponent.get());

    if (normalized == "menuBarHeight")
        return { 0, 0, getWidth(), topStripHeight };

    if (normalizeStartsWith("radius") || normalizeStartsWith("stroke"))
        return workPanel != nullptr ? workPanel->getBounds() : juce::Rectangle<int>{};

    return workPanel != nullptr ? workPanel->getBounds() : juce::Rectangle<int>{};
}

void WorkspaceShellComponent::drawThemePreviewHighlight(juce::Graphics& g) const
{
    const auto target = highlightBoundsForThemeToken(ThemeManager::get().getPreviewHighlightToken());
    if (target.isEmpty())
        return;

    const float pulse = 0.65f + 0.35f * std::sin(static_cast<float>(juce::Time::getMillisecondCounterHiRes() * 0.008));
    auto outline = juce::Colour(0xFFFF3EE3).withAlpha(0.55f + 0.40f * pulse);
    auto fill = juce::Colour(0xFFFF3EE3).withAlpha(0.08f + 0.08f * pulse);

    auto rect = target.toFloat().reduced(1.5f);
    g.setColour(fill);
    g.fillRoundedRectangle(rect, 6.0f);
    g.setColour(outline);
    g.drawRoundedRectangle(rect, 6.0f, 2.2f);
}

void WorkspaceShellComponent::applyFocusMode(FocusMode mode)
{
    focusMode = mode;

    const auto topWidth = juce::jmax(1, getWidth() - (2 * splitterThickness));

    if (focusMode == FocusMode::inFocused)
    {
        leftPanelWidth = static_cast<int>(topWidth * 0.34f);
        rightPanelWidth = static_cast<int>(topWidth * 0.08f);
    }
    else if (focusMode == FocusMode::outFocused)
    {
        leftPanelWidth = static_cast<int>(topWidth * 0.08f);
        rightPanelWidth = static_cast<int>(topWidth * 0.34f);
    }
    else
    {
        leftPanelWidth = static_cast<int>(topWidth * 0.22f);
        rightPanelWidth = static_cast<int>(topWidth * 0.22f);
    }

    updateFocusButtonState();
    resized();
    saveLayoutState();
}

void WorkspaceShellComponent::updateFocusButtonState()
{
    auto setButtonState = [](juce::TextButton& button, bool active)
    {
        const auto& theme = ThemeManager::get().theme();
        button.setColour(juce::TextButton::buttonColourId, active ? theme.controlOnBg : theme.controlBg);
        button.setColour(juce::TextButton::textColourOffId, active ? theme.controlTextOn : theme.controlText);
    };

    setButtonState(focusInButton, focusMode == FocusMode::inFocused);
    setButtonState(focusBalancedButton, focusMode == FocusMode::balanced);
    setButtonState(focusOutButton, focusMode == FocusMode::outFocused);
}

void WorkspaceShellComponent::configureTheoryEditorPanel()
{
    if (workPanel == nullptr)
        return;

    workPanel->addAndMakeVisible(theoryEditorPanel);
    theoryEditorPanel.addAndMakeVisible(theoryEditorTitle);
    theoryEditorPanel.addAndMakeVisible(theoryEditorHint);
    theoryEditorPanel.addAndMakeVisible(theoryObjectLabel);
    theoryEditorPanel.addAndMakeVisible(theoryObjectSelector);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel1);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel2);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel3);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel4);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel5);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor1);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor2);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor3);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor4);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor5);
    theoryEditorPanel.addAndMakeVisible(applyTheoryEditorButton);
    theoryEditorPanel.addAndMakeVisible(reloadTheoryEditorButton);

    // Create progression library browser and chord palette
    libraryBrowser = std::make_unique<ProgressionLibraryBrowser>(songState.getSessionKey(), songState.getSessionMode());
    libraryBrowser->setOnRowClicked([this](const juce::String& templateId)
    {
        handleProgressionAction(templateId, progressionCreateCandidate);
    });
    libraryBrowser->setOnRowContextRequested([this](const juce::String& progressionId, juce::Component& target)
    {
        auto menu = buildProgressionContextMenu(progressionId);
        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&target),
            [safeThis = juce::Component::SafePointer<WorkspaceShellComponent>(this), progressionId](int selectedAction)
            {
                if (safeThis == nullptr || selectedAction <= 0)
                    return;

                safeThis->handleProgressionAction(progressionId, selectedAction);
            });
    });
    theoryEditorPanel.addAndMakeVisible(*libraryBrowser);

    chordPalette = std::make_unique<ProgressionChordPalette>(songState.getSessionKey(), songState.getSessionMode());
    chordPalette->setOnCellClicked([this](int degreeIndex)
    {
        if (!selectedProgressionId.isEmpty())
        {
            handleProgressionAction(selectedProgressionId, progressionAddChord);
        }
    });
    theoryEditorPanel.addAndMakeVisible(*chordPalette);

    theoryEditorTitle.setFont(juce::FontOptions(16.0f));
    theoryEditorTitle.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
    theoryEditorTitle.setJustificationType(juce::Justification::centredLeft);

    theoryEditorHint.setFont(juce::FontOptions(12.5f));
    theoryEditorHint.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.72f));
    theoryEditorHint.setJustificationType(juce::Justification::centredLeft);

    auto configureFieldLabel = [](juce::Label& label)
    {
        label.setFont(juce::FontOptions(12.5f));
        label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.84f));
        label.setJustificationType(juce::Justification::centredLeft);
    };

    configureFieldLabel(theoryObjectLabel);
    configureFieldLabel(theoryFieldLabel1);
    configureFieldLabel(theoryFieldLabel2);
    configureFieldLabel(theoryFieldLabel3);
    configureFieldLabel(theoryFieldLabel4);
    configureFieldLabel(theoryFieldLabel5);

    auto configureEditor = [](juce::TextEditor& editor)
    {
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff151a1f));
        editor.setColour(juce::TextEditor::textColourId, juce::Colours::white.withAlpha(0.96f));
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff2f353d));
        editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffa84c32));
        editor.setTextToShowWhenEmpty("...", juce::Colours::white.withAlpha(0.35f));
    };

    configureEditor(theoryFieldEditor1);
    configureEditor(theoryFieldEditor2);
    configureEditor(theoryFieldEditor3);
    configureEditor(theoryFieldEditor4);
    configureEditor(theoryFieldEditor5);

    auto wireLiveField = [this](juce::TextEditor& editor)
    {
        editor.onTextChange = [this]
        {
            if (suppressLiveCoupledFieldEvents)
                return;
            applyLiveCoupledTheoryMutation();
        };
    };
    wireLiveField(theoryFieldEditor1);
    wireLiveField(theoryFieldEditor2);
    wireLiveField(theoryFieldEditor3);
    wireLiveField(theoryFieldEditor4);

    applyTheoryEditorButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a3b28));
    applyTheoryEditorButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.94f));
    reloadTheoryEditorButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2b323a));
    reloadTheoryEditorButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.94f));

    theoryObjectSelector.onChange = [this]
    {
        updateSelectionFromSelector();
        populateTheoryFieldsForCurrentSelection();
    };

    applyTheoryEditorButton.onClick = [this] { commitTheoryEditorAction(); };
    reloadTheoryEditorButton.onClick = [this]
    {
        ensureSelectionDefaults();
        populateTheoryObjectSelector();
        populateTheoryFieldsForCurrentSelection();
        interactionStatus.setText("Theory editor reloaded from current selection", juce::dontSendNotification);
    };

    // ---- Zone header (replaces raw TextButton tab strip) ----
    zoneHeader = std::make_unique<ZoneHeaderComponent>();
    zoneHeader->setSection(activeNavSection);
    zoneHeader->setActiveTab(0);
    zoneHeader->onTabSelected = [this](int tabIndex) { switchWorkTab(tabIndex); };
    zoneHeader->onContextControlTriggered = [this](const juce::String& controlId, int commandId, bool availableNow)
    {
        if (availableNow && commandId != 0)
        {
            commandManager.invokeDirectly(commandId, true);
            return;
        }

        interactionStatus.setText("Planned control: " + controlId + " (coming in a future phase)",
                                  juce::dontSendNotification);
    };
    zoneHeader->onOverflowRequested = [this](juce::Component& anchor)
    {
        juce::PopupMenu m;
        m.addSectionHeader("Zone options");
        m.addItem(1, "Focus IN",       true, focusMode == FocusMode::inFocused);
        m.addItem(2, "Focus Balanced", true, focusMode == FocusMode::balanced);
        m.addItem(3, "Focus OUT",      true, focusMode == FocusMode::outFocused);
        m.addSeparator();
        m.addItem(4, "Undo Theory",  true);
        m.addItem(5, "Redo Theory",  true);

        const auto controls = getContextControls(activeNavSection);
        bool hasPlanned = false;
        int plannedItemId = 7000;
        for (const auto& c : controls)
        {
            if (!c.availableNow)
            {
                if (!hasPlanned)
                {
                    m.addSeparator();
                    m.addSectionHeader("Planned");
                    hasPlanned = true;
                }

                auto line = juce::String(c.label) + " (planned)";
                if (juce::String(c.plannedNote).isNotEmpty())
                    line << " - " << c.plannedNote;
                m.addItem(plannedItemId++, line, false, false);
            }
        }

        m.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(&anchor),
            [this](int id)
            {
                switch (id)
                {
                    case 1: applyFocusMode(FocusMode::inFocused);  break;
                    case 2: applyFocusMode(FocusMode::balanced);   break;
                    case 3: applyFocusMode(FocusMode::outFocused); break;
                    case 4: performUndo();                         break;
                    case 5: performRedo();                         break;
                    default: break;
                }
            });
    };
    workPanel->addAndMakeVisible(*zoneHeader);

    // Keep legacy buttons wired but invisible — their color-update logic
    // in switchWorkTab() is harmless; ZoneHeaderComponent is the visible face.
    workTabTheoryButton.onClick   = [this] { switchWorkTab(0); };
    workTabGridRollButton.onClick = [this] { switchWorkTab(1); };
    workTabFxButton.onClick       = [this] { switchWorkTab(2); };

    // Create EffBuilderComponent
    effBuilderComponent = std::make_unique<setle::eff::EffBuilderComponent>();
    effBuilderComponent->setVisible(true);

    // Create SoundWorkComponent host for the SOUND station
    soundWorkComponent = std::make_unique<SoundWorkComponent>();
    soundWorkComponent->setEffBuilder(effBuilderComponent.get());
    soundWorkComponent->onFocusTrackRequested = [this](const juce::String& trackId)
    {
        focusSoundTrack(trackId);
    };
    soundWorkComponent->onSmartSidechainRequested = [this](const juce::String& focusedTrackId)
    {
        return buildSmartSidechainSuggestion(focusedTrackId);
    };
    soundWorkComponent->onSmartSidechainAccepted = [this](const SoundWorkComponent::SidechainSuggestion& suggestion)
    {
        applySmartSidechainSuggestion(suggestion);
    };
    soundWorkComponent->onAutoGainRequested = [this](const juce::String& focusedTrackId)
    {
        return runAutoGainForTrack(focusedTrackId);
    };
    soundWorkComponent->onAddMidiRequested = [this]
    {
        showVesselSelectionModal();
    };
    soundWorkComponent->onMonoModeChanged = [this](bool enabled)
    {
        interactionStatus.setText(enabled ? "Mono mode enabled" : "Mono mode disabled", juce::dontSendNotification);
    };
    workPanel->addAndMakeVisible(*soundWorkComponent);
    soundWorkComponent->setVisible(false);

    arrangeWorkComponent = std::make_unique<ArrangeWorkComponent>();
    arrangeWorkComponent->onSectionSelected = [this](const juce::String& sectionId)
    {
        selectedSectionId = sectionId;
        ensureSelectionDefaults();
        populateTheoryObjectSelector();
        populateTheoryFieldsForCurrentSelection();
        refreshTimelineData();
        interactionStatus.setText("Arrange focus: " + sectionId.substring(0, 8), juce::dontSendNotification);
    };
    arrangeWorkComponent->onSectionOrderChanged = [this](const juce::StringArray& orderedIds)
    {
        onSectionReordered(orderedIds);
    };
    arrangeWorkComponent->onSmartDuplicateRequested = [this](const juce::String& sectionId, ArrangeWorkComponent::DuplicateMode mode)
    {
        interactionStatus.setText(smartDuplicateSection(sectionId, mode), juce::dontSendNotification);
    };
    arrangeWorkComponent->onStructuralMeltChanged = [this](float meltFactor)
    {
        arrangeLogoMeltTarget = meltFactor;
    };
    workPanel->addAndMakeVisible(*arrangeWorkComponent);
    arrangeWorkComponent->setVisible(false);

    // gridRollComponent is created later in the constructor after edit is initialised
}

void WorkspaceShellComponent::switchWorkTab(int tabIndex)
{
    if (workPanel == nullptr)
    {
        shellTrace("switchWorkTab: fallback, workPanel is null");
        interactionStatus.setText("Fallback: WORK panel unavailable. Staying on current view.",
                                  juce::dontSendNotification);
        return;
    }

    // Only these tabs are currently implemented in the WORK panel.
    const auto requestedTab = tabIndex;
    if (tabIndex < 0 || tabIndex > 4)
    {
        shellTrace("switchWorkTab: invalid tab index=" + juce::String(tabIndex) + ", clamping to 0");
        interactionStatus.setText("Fallback: unsupported tab requested, returning to Theory.",
                                  juce::dontSendNotification);
        tabIndex = 0;
    }

    shellTrace("switchWorkTab: requested=" + juce::String(requestedTab)
        + " resolved=" + juce::String(tabIndex)
        + " section=" + getNavSectionInfo(activeNavSection).id);

    workPanelTabIndex    = tabIndex;
    workPanelShowGridRoll = (tabIndex == 1);

    theoryEditorPanel.setVisible(tabIndex == 0);

    if (gridRollComponent != nullptr)
    {
        if (tabIndex == 1)
        {
            gridRollComponent->setTheorySnap(theorySnap);
            ensureProgressionLoadedForNoteMode(selectedProgressionId);
            gridRollComponent->refreshNoteModeCache();
        }
        gridRollComponent->setVisible(tabIndex == 1);
    }

    if (soundWorkComponent != nullptr)
        soundWorkComponent->setVisible(tabIndex == 2);

    if (arrangeWorkComponent != nullptr)
    {
        arrangeWorkComponent->setVisible(tabIndex == 4);
        if (tabIndex == 4)
            refreshArrangeWorkspace();
    }

    if (effBuilderComponent != nullptr)
    {
        effBuilderComponent->setVisible(tabIndex == 2);

        if (tabIndex == 2)
        {
            refreshSoundRoster();

            juce::String targetTrackId = selectedFxTrackId;
            auto selectedIt = instrumentSlots.find(targetTrackId);
            if (selectedIt == instrumentSlots.end() || selectedIt->second == nullptr || !selectedIt->second->getInfo().isActive)
            {
                for (auto& [trackId, slot] : instrumentSlots)
                {
                    if (slot != nullptr && slot->getInfo().isActive)
                    {
                        targetTrackId = trackId;
                        break;
                    }
                }
            }

            if (targetTrackId.isNotEmpty())
                focusSoundTrack(targetTrackId);
        }
    }

    if (sessionDashboardComponent != nullptr)
    {
        sessionDashboardComponent->setVisible(tabIndex == 3);
    }

    // Update tab button visual state (legacy buttons kept invisible but still data-accurate)
    const auto activeCol   = juce::Colour(0xff2c4a67);
    const auto inactiveCol = juce::Colour(0xff2a3040);
    workTabTheoryButton.setColour(juce::TextButton::buttonColourId,
                                   tabIndex == 0 ? activeCol : inactiveCol);
    workTabGridRollButton.setColour(juce::TextButton::buttonColourId,
                                    tabIndex == 1 ? activeCol : inactiveCol);
    workTabFxButton.setColour(juce::TextButton::buttonColourId,
                               tabIndex == 2 ? activeCol : inactiveCol);

    // Sync ZoneHeaderComponent
    if (zoneHeader != nullptr)
        zoneHeader->setActiveTab(tabIndex);

    resized();
}

void WorkspaceShellComponent::switchNavSection(NavSection section)
{
    if (leftNavComponent == nullptr || workPanel == nullptr)
    {
        shellTrace("switchNavSection: fallback, core shell components not ready");
        interactionStatus.setText("Fallback: section switch requested before shell was ready.",
                                  juce::dontSendNotification);
        return;
    }

    activeNavSection = section;
    shellTrace(juce::String("switchNavSection: target=") + getNavSectionInfo(section).id);
    setle::state::AppPreferences::get().setActiveNavSection(getNavSectionInfo(section).id);

    if (leftNavComponent != nullptr)
        leftNavComponent->setActiveSection(section);

    // Determine which work tab to show for the new section and update zone header
    const auto tabs = getContextTabs(section);
    int targetTab = workPanelTabIndex; // keep current tab if valid for this section

    if (tabs.isEmpty())
    {
        targetTab = 0;
        shellTrace("switchNavSection: fallback, no context tabs found for section; forcing tab 0");
        interactionStatus.setText("Fallback: section has no tabs configured, using Theory view.",
                                  juce::dontSendNotification);
    }
    else
    {
        // check if current tab is in the new section's tab list
        bool currentTabValid = false;
        for (const auto& t : tabs)
            if (t.tabIndex == targetTab) { currentTabValid = true; break; }
        if (!currentTabValid)
            targetTab = tabs[0].tabIndex;
    }

    if (zoneHeader != nullptr)
        zoneHeader->setSection(section);
    else
        shellTrace("switchNavSection: zoneHeader not available yet");

    switchWorkTab(targetTab);
}

// -----------------------------------------------------------------------
// ApplicationCommandTarget implementation
// -----------------------------------------------------------------------
juce::ApplicationCommandTarget* WorkspaceShellComponent::getNextCommandTarget()
{
    return nullptr; // shell is top of command target chain
}

void WorkspaceShellComponent::getAllCommands(juce::Array<juce::CommandID>& commands)
{
    namespace ID = AppCommandIDs;
    const juce::CommandID ids[] = {
        ID::fileNew,     ID::fileSave,      ID::fileExportMidi, ID::fileExit,
        ID::editUndo,    ID::editRedo,
        ID::transportPlay, ID::transportStop, ID::transportRecord,
        ID::toolSelect,  ID::toolDraw,    ID::toolErase,   ID::toolSplit,
        ID::toolStretch, ID::toolListen,  ID::toolMarquee, ID::toolScaleSnap,
        ID::viewFocusIn, ID::viewFocusBalanced, ID::viewFocusOut, ID::viewThemeEditor,
        ID::navCreate,   ID::navEdit,     ID::navArrange,  ID::navSound,
        ID::navMix,      ID::navLibrary,  ID::navSession,
        ID::insertMidiTrack, ID::insertAudioTrack,
        ID::helpAbout,
    };
    commands.addArray(ids, static_cast<int>(std::size(ids)));
}

void WorkspaceShellComponent::getCommandInfo(juce::CommandID commandID,
                                              juce::ApplicationCommandInfo& result)
{
    namespace ID = AppCommandIDs;

    switch (commandID)
    {
        // ---- File ----
        case ID::fileNew:
            result.setInfo("New Song", "Start a new empty song", "File", 0);
            result.setActive(false); // not yet implemented
            break;
        case ID::fileSave:
            result.setInfo("Save Song", "Save the current song", "File", 0);
            result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier);
            break;
        case ID::fileExportMidi:
            result.setInfo("Export MIDI...", "Export song to MIDI file", "File", 0);
            result.setActive(false); // not yet implemented
            break;
        case ID::fileExit:
            result.setInfo("Exit", "Quit SETLE", "File", 0);
            break;

        // ---- Edit ----
        case ID::editUndo:
            result.setInfo("Undo Theory", "Undo the last theory change", "Edit", 0);
            result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier);
            result.setActive(!undoSnapshots.empty());
            break;
        case ID::editRedo:
            result.setInfo("Redo Theory", "Redo the last undone change", "Edit", 0);
            result.addDefaultKeypress('z', juce::ModifierKeys::commandModifier
                                            | juce::ModifierKeys::shiftModifier);
            result.setActive(!redoSnapshots.empty());
            break;

        // ---- Transport ----
        case ID::transportPlay:
            result.setInfo("Play", "Start transport playback", "Transport", 0);
            result.addDefaultKeypress(juce::KeyPress::spaceKey, juce::ModifierKeys::noModifiers);
            break;
        case ID::transportStop:
            result.setInfo("Stop", "Stop transport playback", "Transport", 0);
            break;
        case ID::transportRecord:
            result.setInfo("Record", "Start recording", "Transport", 0);
            break;

        // ---- Tools ----
        case ID::toolSelect:
            result.setInfo("Select",     "Select / move items",   "Tools", 0);
            result.addDefaultKeypress('s', juce::ModifierKeys::noModifiers);
            break;
        case ID::toolDraw:
            result.setInfo("Draw",       "Draw new items",         "Tools", 0);
            result.addDefaultKeypress('d', juce::ModifierKeys::noModifiers);
            break;
        case ID::toolErase:
            result.setInfo("Erase",      "Erase items",            "Tools", 0);
            result.addDefaultKeypress('e', juce::ModifierKeys::noModifiers);
            break;
        case ID::toolSplit:
            result.setInfo("Split",      "Split items at cursor",  "Tools", 0);
            result.addDefaultKeypress('b', juce::ModifierKeys::noModifiers);
            break;
        case ID::toolStretch:
            result.setInfo("Stretch",    "Time-stretch items",     "Tools", 0);
            result.addDefaultKeypress('t', juce::ModifierKeys::noModifiers);
            break;
        case ID::toolListen:
            result.setInfo("Listen",     "Audition / preview",     "Tools", 0);
            result.addDefaultKeypress('l', juce::ModifierKeys::noModifiers);
            break;
        case ID::toolMarquee:
            result.setInfo("Marquee",    "Marquee selection",      "Tools", 0);
            result.addDefaultKeypress('m', juce::ModifierKeys::noModifiers);
            break;
        case ID::toolScaleSnap:
            result.setInfo("Scale Snap", "Snap notes to scale",    "Tools", 0);
            result.addDefaultKeypress('g', juce::ModifierKeys::noModifiers);
            break;

        // ---- View / Focus ----
        case ID::viewFocusIn:
            result.setInfo("Focus IN",       "Emphasise the capture/queue panel",  "View", 0);
            result.setTicked(focusMode == FocusMode::inFocused);
            break;
        case ID::viewFocusBalanced:
            result.setInfo("Focus Balanced", "Balanced layout",                    "View", 0);
            result.setTicked(focusMode == FocusMode::balanced);
            break;
        case ID::viewFocusOut:
            result.setInfo("Focus OUT",      "Emphasise the instruments/mix panel","View", 0);
            result.setTicked(focusMode == FocusMode::outFocused);
            break;
        case ID::viewThemeEditor:
            result.setInfo("Theme Editor",   "Open the visual theme editor",       "View", 0);
            break;

        // ---- Nav sections ----
        case ID::navCreate:
            result.setInfo("Create",  "Go to Create section (capture, queue)", "View", 0);
            result.setTicked(activeNavSection == NavSection::create);
            break;
        case ID::navEdit:
            result.setInfo("Edit",    "Go to Edit section (theory, grid roll)", "View", 0);
            result.setTicked(activeNavSection == NavSection::edit);
            break;
        case ID::navArrange:
            result.setInfo("Arrange", "Go to Arrange section (timeline)", "View", 0);
            result.setTicked(activeNavSection == NavSection::arrange);
            break;
        case ID::navSound:
            result.setInfo("Sound",   "Go to Sound section (instruments, FX)", "View", 0);
            result.setTicked(activeNavSection == NavSection::sound);
            break;
        case ID::navMix:
            result.setInfo("Mix",     "Go to Mix section (mixer, master)", "View", 0);
            result.setTicked(activeNavSection == NavSection::mix);
            break;
        case ID::navLibrary:
            result.setInfo("Library", "Go to Library section (progressions, patterns)", "View", 0);
            result.setTicked(activeNavSection == NavSection::library);
            break;
        case ID::navSession:
            result.setInfo("Session", "Go to Session settings (key, mode, BPM)", "View", 0);
            result.setTicked(activeNavSection == NavSection::session);
            break;

        // ---- Insert ----
        case ID::insertMidiTrack:
            result.setInfo("Add MIDI Track",  "Add a new MIDI track to the timeline", "Insert", 0);
            break;
        case ID::insertAudioTrack:
            result.setInfo("Add Audio Track", "Add a new audio track to the timeline", "Insert", 0);
            break;

        // ---- Help ----
        case ID::helpAbout:
            result.setInfo("About SETLE", "Version and credits", "Help", 0);
            break;

        default:
            break;
    }
}

bool WorkspaceShellComponent::perform(const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    namespace ID = AppCommandIDs;

    switch (info.commandID)
    {
        // ---- File ----
        case ID::fileSave:
            saveSongState();
            interactionStatus.setText("Song saved.", juce::dontSendNotification);
            return true;
        case ID::fileExit:
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
            return true;

        // ---- Edit ----
        case ID::editUndo: performUndo(); return true;
        case ID::editRedo: performRedo(); return true;

        // ---- Transport ----
        case ID::transportPlay:
            if (edit != nullptr) { loadProgressionToEdit(); edit->getTransport().play(false); }
            return true;
        case ID::transportStop:
            if (edit != nullptr) edit->getTransport().stop(false, false);
            return true;
        case ID::transportRecord:
            if (edit != nullptr) { loadProgressionToEdit(); edit->getTransport().record(false); }
            return true;

        // ---- Tools ----
        case ID::toolSelect:    EditToolManager::get().setActiveTool(EditTool::Select);    return true;
        case ID::toolDraw:      EditToolManager::get().setActiveTool(EditTool::Draw);      return true;
        case ID::toolErase:     EditToolManager::get().setActiveTool(EditTool::Erase);     return true;
        case ID::toolSplit:     EditToolManager::get().setActiveTool(EditTool::Split);     return true;
        case ID::toolStretch:   EditToolManager::get().setActiveTool(EditTool::Stretch);   return true;
        case ID::toolListen:    EditToolManager::get().setActiveTool(EditTool::Listen);    return true;
        case ID::toolMarquee:   EditToolManager::get().setActiveTool(EditTool::Marquee);   return true;
        case ID::toolScaleSnap: EditToolManager::get().setActiveTool(EditTool::ScaleSnap); return true;

        // ---- View / Focus ----
        case ID::viewFocusIn:      applyFocusMode(FocusMode::inFocused);  return true;
        case ID::viewFocusBalanced:applyFocusMode(FocusMode::balanced);   return true;
        case ID::viewFocusOut:     applyFocusMode(FocusMode::outFocused); return true;
        case ID::viewThemeEditor:  showThemeEditor(!themeEditorPanel->isVisible()); return true;

        // ---- Nav sections ----
        case ID::navCreate:  switchNavSection(NavSection::create);  return true;
        case ID::navEdit:    switchNavSection(NavSection::edit);    return true;
        case ID::navArrange: switchNavSection(NavSection::arrange); return true;
        case ID::navSound:   switchNavSection(NavSection::sound);   return true;
        case ID::navMix:     switchNavSection(NavSection::mix);     return true;
        case ID::navLibrary: switchNavSection(NavSection::library); return true;
        case ID::navSession: switchNavSection(NavSection::session); return true;

        // ---- Insert ----
        case ID::insertMidiTrack:
            showVesselSelectionModal();
            return true;
        case ID::insertAudioTrack:
            if (trackManager != nullptr)
                trackManager->addAudioTrack("Audio Track");
            return true;

        // ---- Help ----
        case ID::helpAbout:
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "SETLE",
                "SETLE v0.0.1\n2nist — harmonic composition aid",
                "OK");
            return true;

        default:
            return false;
    }
}

void WorkspaceShellComponent::openTheoryEditor(TheoryMenuTarget target, int actionId, const juce::String& actionName)
{
    activeEditorTarget = target;
    activeEditorActionId = actionId;

    theoryEditorTitle.setText("Theory Editor - " + actionName, juce::dontSendNotification);
    theoryEditorHint.setText("Non-modal editing in WORK panel; right-click actions target selected objects.", juce::dontSendNotification);

    ensureSelectionDefaults();
    populateTheoryObjectSelector();
    populateTheoryFieldsForCurrentSelection();
}

void WorkspaceShellComponent::populateTheoryObjectSelector()
{
    ensureSelectionDefaults();

    theoryObjectSelector.clear(juce::dontSendNotification);
    theorySelectorObjectIds.clear();

    auto addObject = [this](const juce::String& objectId, const juce::String& label)
    {
        theorySelectorObjectIds.push_back(objectId);
        theoryObjectSelector.addItem(label, static_cast<int>(theorySelectorObjectIds.size()));
    };

    juce::String targetSelectedId;

    if (activeEditorTarget == TheoryMenuTarget::section)
    {
        theoryObjectLabel.setText("Section", juce::dontSendNotification);
        targetSelectedId = selectedSectionId;
        for (const auto& section : songState.getSections())
            addObject(section.getId(), section.getName() + "  [" + section.getId().substring(0, 8) + "]");
    }
    else if (activeEditorTarget == TheoryMenuTarget::chord)
    {
        theoryObjectLabel.setText("Chord", juce::dontSendNotification);
        targetSelectedId = selectedChordId;

        if (auto progression = getSelectedProgression())
            for (const auto& chord : progression->getChords())
                addObject(chord.getId(), chord.getSymbol() + " (" + chord.getFunction() + ")");
    }
    else if (activeEditorTarget == TheoryMenuTarget::note)
    {
        theoryObjectLabel.setText("Note", juce::dontSendNotification);
        targetSelectedId = selectedNoteId;

        if (auto chord = getSelectedChord())
        {
            for (const auto& note : chord->getNotes())
            {
                addObject(note.getId(),
                          "MIDI " + juce::String(note.getPitch())
                              + "  vel " + juce::String(note.getVelocity(), 2));
            }
        }
    }
    else
    {
        theoryObjectLabel.setText("Progression", juce::dontSendNotification);
        targetSelectedId = selectedProgressionId;
        for (const auto& progression : songState.getProgressions())
            addObject(progression.getId(), progression.getName() + " (" + progression.getKey() + " " + progression.getMode() + ")");
    }

    int selectedItemId = 0;
    for (int i = 0; i < static_cast<int>(theorySelectorObjectIds.size()); ++i)
        if (theorySelectorObjectIds[static_cast<size_t>(i)] == targetSelectedId)
            selectedItemId = i + 1;

    if (selectedItemId == 0 && !theorySelectorObjectIds.empty())
        selectedItemId = 1;

    if (selectedItemId > 0)
        theoryObjectSelector.setSelectedId(selectedItemId, juce::dontSendNotification);

    theoryObjectSelector.setEnabled(!theorySelectorObjectIds.empty());
}

void WorkspaceShellComponent::updateSelectionFromSelector()
{
    const auto selectedIndex = theoryObjectSelector.getSelectedId();
    if (selectedIndex <= 0 || selectedIndex > static_cast<int>(theorySelectorObjectIds.size()))
        return;

    const auto selectedId = theorySelectorObjectIds[static_cast<size_t>(selectedIndex - 1)];
    if (activeEditorTarget == TheoryMenuTarget::section)
        selectedSectionId = selectedId;
    else if (activeEditorTarget == TheoryMenuTarget::chord)
        selectedChordId = selectedId;
    else if (activeEditorTarget == TheoryMenuTarget::note)
        selectedNoteId = selectedId;
    else
        selectedProgressionId = selectedId;
}

void WorkspaceShellComponent::handleGridRollSelectionChanged(const setle::gridroll::GridRollComponent::SelectionMetadata& metadata)
{
    if (!metadata.valid)
    {
        liveGridRollSelection.reset();
        liveCoupledUndoBaseSnapshot.clear();
        liveCoupledLastMutationMs = 0;
        if (activeNavSection == NavSection::edit && workPanelTabIndex == 1)
            populateTheoryFieldsForCurrentSelection();
        return;
    }

    const bool selectionChanged = !liveGridRollSelection.has_value()
                                  || liveGridRollSelection->target != metadata.target
                                  || liveGridRollSelection->noteId != metadata.noteId
                                  || liveGridRollSelection->chordId != metadata.chordId;
    liveGridRollSelection = metadata;
    if (selectionChanged)
    {
        liveCoupledUndoBaseSnapshot.clear();
        liveCoupledLastMutationMs = 0;
    }

    if (activeNavSection == NavSection::edit && workPanelTabIndex == 1)
        populateTheoryFieldsForCurrentSelection();
}

void WorkspaceShellComponent::applyLiveCoupledTheoryMutation()
{
    if (gridRollComponent == nullptr || workPanelTabIndex != 1 || !liveGridRollSelection.has_value())
        return;

    const auto root = theoryFieldEditor1.getText().trim();
    const auto extension = theoryFieldEditor2.getText().trim();
    const auto inversionText = theoryFieldEditor3.getText().trim();
    const auto velocityText = theoryFieldEditor4.getText().trim();

    if (root.isEmpty())
        return;

    if (inversionText.isNotEmpty() && !isStrictInt(inversionText))
        return;
    if (velocityText.isNotEmpty() && !isStrictDouble(velocityText))
        return;

    const auto nowMs = juce::Time::getMillisecondCounter();
    const bool continueUndoTransaction = liveCoupledLastMutationMs != 0
                                         && (nowMs - liveCoupledLastMutationMs) <= 700
                                         && liveCoupledUndoBaseSnapshot.isNotEmpty();
    if (!continueUndoTransaction)
        liveCoupledUndoBaseSnapshot = createSongSnapshot();

    const auto beforeSnapshot = liveCoupledUndoBaseSnapshot;
    setle::gridroll::GridRollComponent::CoupledMutation mutation;
    mutation.root = root;
    mutation.extension = extension;
    mutation.inversion = parseIntOr(inversionText, liveGridRollSelection->inversion);
    mutation.velocity = juce::jlimit(0.0f, 1.0f,
                                     velocityText.isEmpty()
                                         ? liveGridRollSelection->velocity
                                         : static_cast<float>(velocityText.getDoubleValue()));

    if (!gridRollComponent->applyTheoryMutationToSelection(mutation))
        return;

    saveSongState();
    if (!continueUndoTransaction)
        captureUndoStateIfChanged(beforeSnapshot);
    liveCoupledLastMutationMs = nowMs;
    refreshTimelineData();
    updateUndoRedoButtonState();
}

std::optional<model::Section> WorkspaceShellComponent::getSelectedSection()
{
    ensureSelectionDefaults();
    for (const auto& section : songState.getSections())
        if (section.getId() == selectedSectionId)
            return section;

    return std::nullopt;
}

std::optional<model::Progression> WorkspaceShellComponent::getSelectedProgression()
{
    ensureSelectionDefaults();
    for (const auto& progression : songState.getProgressions())
        if (progression.getId() == selectedProgressionId)
            return progression;

    return std::nullopt;
}

std::optional<model::Chord> WorkspaceShellComponent::getSelectedChord()
{
    ensureSelectionDefaults();
    if (auto progression = getSelectedProgression())
        for (const auto& chord : progression->getChords())
            if (chord.getId() == selectedChordId)
                return chord;

    return std::nullopt;
}

std::optional<model::Note> WorkspaceShellComponent::getSelectedNote()
{
    ensureSelectionDefaults();
    if (auto chord = getSelectedChord())
        for (const auto& note : chord->getNotes())
            if (note.getId() == selectedNoteId)
                return note;

    return std::nullopt;
}

void WorkspaceShellComponent::ensureSelectionDefaults()
{
    auto sections = songState.getSections();
    auto hasSection = [sections](const juce::String& id)
    {
        for (const auto& section : sections)
            if (section.getId() == id)
                return true;
        return false;
    };

    if (sections.empty())
        selectedSectionId.clear();
    else if (!hasSection(selectedSectionId))
        selectedSectionId = sections.front().getId();

    auto progressions = songState.getProgressions();
    auto hasProgression = [progressions](const juce::String& id)
    {
        for (const auto& progression : progressions)
            if (progression.getId() == id)
                return true;
        return false;
    };

    if (progressions.empty())
        selectedProgressionId.clear();
    else if (!hasProgression(selectedProgressionId))
        selectedProgressionId = progressions.front().getId();

    for (const auto& progression : progressions)
    {
        if (progression.getId() != selectedProgressionId)
            continue;

        const auto chords = progression.getChords();
        auto hasChord = [chords](const juce::String& id)
        {
            for (const auto& chord : chords)
                if (chord.getId() == id)
                    return true;
            return false;
        };

        if (chords.empty())
            selectedChordId.clear();
        else if (!hasChord(selectedChordId))
            selectedChordId = chords.front().getId();

        for (const auto& chord : chords)
        {
            if (chord.getId() != selectedChordId)
                continue;

            const auto notes = chord.getNotes();
            auto hasNote = [notes](const juce::String& id)
            {
                for (const auto& note : notes)
                    if (note.getId() == id)
                        return true;
                return false;
            };

            if (notes.empty())
                selectedNoteId.clear();
            else if (!hasNote(selectedNoteId))
                selectedNoteId = notes.front().getId();
            break;
        }

        break;
    }
}

void WorkspaceShellComponent::populateTheoryFieldsForCurrentSelection()
{
    auto hideField = [](juce::Label& label, juce::TextEditor& editor)
    {
        label.setVisible(false);
        editor.setVisible(false);
    };

    auto setField = [](juce::Label& label, juce::TextEditor& editor, const juce::String& title, const juce::String& value)
    {
        label.setVisible(true);
        editor.setVisible(true);
        label.setText(title, juce::dontSendNotification);
        editor.setText(value, juce::dontSendNotification);
    };

    hideField(theoryFieldLabel1, theoryFieldEditor1);
    hideField(theoryFieldLabel2, theoryFieldEditor2);
    hideField(theoryFieldLabel3, theoryFieldEditor3);
    hideField(theoryFieldLabel4, theoryFieldEditor4);
    hideField(theoryFieldLabel5, theoryFieldEditor5);

    applyTheoryEditorButton.setButtonText("Apply Edit");

    if (activeNavSection == NavSection::edit && workPanelTabIndex == 1 && liveGridRollSelection.has_value())
    {
        suppressLiveCoupledFieldEvents = true;
        theoryEditorTitle.setText("Theory Editor - Coupled Focus", juce::dontSendNotification);
        theoryEditorHint.setText("GridRoll selection is coupled. Edits mutate selected notes immediately.",
                                 juce::dontSendNotification);

        setField(theoryFieldLabel1, theoryFieldEditor1, "Root", liveGridRollSelection->root);
        setField(theoryFieldLabel2, theoryFieldEditor2, "Extension", liveGridRollSelection->extension);
        setField(theoryFieldLabel3, theoryFieldEditor3, "Inversion", juce::String(liveGridRollSelection->inversion));
        setField(theoryFieldLabel4, theoryFieldEditor4, "Velocity (0-1)", juce::String(liveGridRollSelection->velocity, 3));
        hideField(theoryFieldLabel5, theoryFieldEditor5);
        applyTheoryEditorButton.setButtonText("Apply Coupled Edit");
        suppressLiveCoupledFieldEvents = false;
        return;
    }

    if (activeEditorTarget == TheoryMenuTarget::section)
    {
        if (auto section = getSelectedSection())
        {
            if (activeEditorActionId == sectionEditTheory)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Name", section->getName());
                setField(theoryFieldLabel2, theoryFieldEditor2, "Repeat count", juce::String(section->getRepeatCount()));
                setField(theoryFieldLabel3, theoryFieldEditor3, "Energy", "");
                auto progressionRefs = section->getProgressionRefs();
                setField(theoryFieldLabel4, theoryFieldEditor4, "Progression ID",
                         progressionRefs.empty() ? "" : progressionRefs.front().getProgressionId());
                applyTheoryEditorButton.setButtonText("Apply Section Edit");
            }
            else if (activeEditorActionId == sectionSetRepeatPattern)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Repeat Count", juce::String(section->getRepeatCount()));
                applyTheoryEditorButton.setButtonText("Set Repeat Pattern");
            }
            else if (activeEditorActionId == sectionAddTransitionAnchor)
            {
                setField(theoryFieldLabel1,
                         theoryFieldEditor1,
                         "Transition Name",
                         "Anchor " + juce::String(static_cast<int>(songState.getTransitions().size()) + 1));
                setField(theoryFieldLabel2, theoryFieldEditor2, "Strategy", "pushRight");
                setField(theoryFieldLabel3, theoryFieldEditor3, "To Section Name (optional)", "");
                applyTheoryEditorButton.setButtonText("Create Transition Anchor");
            }
        }
    }
    else if (activeEditorTarget == TheoryMenuTarget::chord)
    {
        if (auto chord = getSelectedChord())
        {
            if (activeEditorActionId == chordEdit)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Symbol", chord->getSymbol());
                setField(theoryFieldLabel2, theoryFieldEditor2, "Quality", chord->getQuality());
                setField(theoryFieldLabel3, theoryFieldEditor3, "Root MIDI", juce::String(chord->getRootMidi()));
                setField(theoryFieldLabel4, theoryFieldEditor4, "Duration (beats)", juce::String(chord->getDurationBeats()));
                setField(theoryFieldLabel5, theoryFieldEditor5, "Function", chord->getFunction());
                applyTheoryEditorButton.setButtonText("Apply Chord Edit");
            }
            else if (activeEditorActionId == chordSubstitution)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Profile", "qualityCycle");
                applyTheoryEditorButton.setButtonText("Apply Substitution");
            }
            else if (activeEditorActionId == chordSetFunction)
            {
                setField(theoryFieldLabel1,
                         theoryFieldEditor1,
                         "Function (T/PD/D/custom)",
                         chord->getFunction().isNotEmpty() ? chord->getFunction() : "T");
                applyTheoryEditorButton.setButtonText("Set Harmonic Function");
            }
        }
    }
    else if (activeEditorTarget == TheoryMenuTarget::note)
    {
        if (auto note = getSelectedNote())
        {
            setField(theoryFieldLabel1, theoryFieldEditor1, "Pitch (0-127)", juce::String(note->getPitch()));
            setField(theoryFieldLabel2, theoryFieldEditor2, "Velocity (0-1)", juce::String(note->getVelocity(), 3));
            setField(theoryFieldLabel3, theoryFieldEditor3, "Start Beats", juce::String(note->getStartBeats()));
            setField(theoryFieldLabel4, theoryFieldEditor4, "Duration Beats", juce::String(note->getDurationBeats()));
            applyTheoryEditorButton.setButtonText("Apply Note Edit");
        }
    }
    else
    {
        if (auto progression = getSelectedProgression())
        {
            if (activeEditorActionId == progressionCreateCandidate || activeEditorActionId == progressionGrabSampler)
            {
                setField(theoryFieldLabel1,
                         theoryFieldEditor1,
                         "Name",
                         (activeEditorActionId == progressionGrabSampler ? "Queue Grab " : "Candidate ")
                             + juce::String(static_cast<int>(songState.getProgressions().size()) + 1));
                setField(theoryFieldLabel2, theoryFieldEditor2, "Key", progression->getKey());
                setField(theoryFieldLabel3, theoryFieldEditor3, "Mode", progression->getMode());
                applyTheoryEditorButton.setButtonText(activeEditorActionId == progressionGrabSampler
                                                          ? "Create Queue Candidate"
                                                          : "Create Candidate");
            }
            else if (activeEditorActionId == progressionAnnotateKeyMode)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Name", progression->getName());
                setField(theoryFieldLabel2, theoryFieldEditor2, "Key", progression->getKey());
                setField(theoryFieldLabel3, theoryFieldEditor3, "Mode", progression->getMode());
                setField(theoryFieldLabel4, theoryFieldEditor4, "Length (beats)", juce::String(progression->getLengthBeats()));
                setField(theoryFieldLabel5, theoryFieldEditor5, "Variant of", progression->getVariantOf());
                applyTheoryEditorButton.setButtonText("Apply Key/Mode");
            }
        }
    }
}

void WorkspaceShellComponent::commitTheoryEditorAction()
{
    seedSongStateIfNeeded();
    liveCoupledUndoBaseSnapshot.clear();
    liveCoupledLastMutationMs = 0;

    const auto beforeSnapshot = createSongSnapshot();
    const auto actionResult = applyTheoryEditorAction();

    captureUndoStateIfChanged(beforeSnapshot);
    saveSongState();
    ensureSelectionDefaults();
    refreshTimelineData();
    populateTheoryObjectSelector();
    populateTheoryFieldsForCurrentSelection();
    updateUndoRedoButtonState();

    const auto message = actionResult + " | " + summarizeSongState();
    interactionStatus.setText(message, juce::dontSendNotification);
    DBG(message);
}

juce::String WorkspaceShellComponent::applyTheoryEditorAction()
{
    if (activeNavSection == NavSection::edit && workPanelTabIndex == 1
        && liveGridRollSelection.has_value() && gridRollComponent != nullptr)
    {
        const auto root = theoryFieldEditor1.getText().trim();
        const auto extension = theoryFieldEditor2.getText().trim();
        const auto inversionText = theoryFieldEditor3.getText().trim();
        const auto velocityText = theoryFieldEditor4.getText().trim();

        if (root.isEmpty())
            return "Coupled mutation rejected: Root is required";
        if (inversionText.isNotEmpty() && !isStrictInt(inversionText))
            return "Coupled mutation rejected: Inversion must be an integer";
        if (velocityText.isNotEmpty() && !isStrictDouble(velocityText))
            return "Coupled mutation rejected: Velocity must be a number";

        const auto inversion = parseIntOr(inversionText, liveGridRollSelection->inversion);
        const auto velocity = juce::jlimit(0.0f, 1.0f,
                                           velocityText.isEmpty()
                                               ? liveGridRollSelection->velocity
                                               : static_cast<float>(velocityText.getDoubleValue()));

        setle::gridroll::GridRollComponent::CoupledMutation mutation;
        mutation.root = root;
        mutation.extension = extension;
        mutation.inversion = inversion;
        mutation.velocity = velocity;

        if (gridRollComponent->applyTheoryMutationToSelection(mutation))
            return "Coupled mutation applied to GridRoll selection";

        return "Coupled mutation skipped: no editable GridRoll selection";
    }

    if (activeEditorTarget == TheoryMenuTarget::section)
    {
        auto section = getSelectedSection();
        if (!section.has_value())
            return "Editor apply failed: no section selected";

        if (activeEditorActionId == sectionEditTheory)
        {
            const auto name = theoryFieldEditor1.getText().trim();
            const auto repeatText = theoryFieldEditor2.getText().trim();
            if (repeatText.isNotEmpty() && !isStrictInt(repeatText))
                return "Section edit rejected: Repeat Count must be an integer";

            const auto repeats = juce::jlimit(1, 32, parseIntOr(theoryFieldEditor2.getText(), section->getRepeatCount()));
            section->setName(name.isNotEmpty() ? name : section->getName());
            section->setRepeatCount(repeats);
            return "Section updated from center editor";
        }

        if (activeEditorActionId == sectionSetRepeatPattern)
        {
            const auto repeatText = theoryFieldEditor1.getText().trim();
            if (repeatText.isNotEmpty() && !isStrictInt(repeatText))
                return "Repeat pattern rejected: Repeat Count must be an integer";

            const auto repeats = juce::jlimit(1, 32, parseIntOr(theoryFieldEditor1.getText(), section->getRepeatCount()));
            section->setRepeatCount(repeats);
            return "Section repeat pattern updated from center editor";
        }

        if (activeEditorActionId == sectionAddTransitionAnchor)
        {
            auto sections = songState.getSections();
            if (sections.size() < 2)
            {
                auto generated = model::Section::create("Generated Target", 2);
                if (auto progression = getSelectedProgression())
                    generated.addProgressionRef(model::SectionProgressionRef::create(progression->getId(), 0, ""));
                songState.addSection(generated);
                sections = songState.getSections();
            }

            juce::String toSectionId;
            const auto preferredToSectionName = theoryFieldEditor3.getText().trim();
            for (const auto& candidate : sections)
            {
                if (candidate.getId() == section->getId())
                    continue;

                if (preferredToSectionName.isNotEmpty() && candidate.getName().equalsIgnoreCase(preferredToSectionName))
                {
                    toSectionId = candidate.getId();
                    break;
                }

                if (toSectionId.isEmpty())
                    toSectionId = candidate.getId();
            }

            if (toSectionId.isEmpty())
                return "Transition anchor failed: no destination section";

            auto transition = model::Transition::create(
                section->getId(),
                toSectionId,
                theoryFieldEditor2.getText().trim().isNotEmpty() ? theoryFieldEditor2.getText().trim()
                                                                 : juce::String("pushRight"));
            transition.setName(theoryFieldEditor1.getText().trim().isNotEmpty()
                                   ? theoryFieldEditor1.getText().trim()
                                   : "Anchor " + juce::String(static_cast<int>(songState.getTransitions().size()) + 1));
            songState.addTransition(transition);
            return "Transition anchor created from center editor";
        }
    }
    else if (activeEditorTarget == TheoryMenuTarget::chord)
    {
        auto chord = getSelectedChord();
        if (!chord.has_value())
            return "Editor apply failed: no chord selected";

        if (activeEditorActionId == chordEdit)
        {
            const auto rootText = theoryFieldEditor3.getText().trim();
            const auto durationText = theoryFieldEditor4.getText().trim();
            if (rootText.isNotEmpty() && !isStrictInt(rootText))
                return "Chord edit rejected: Root MIDI must be an integer";
            if (durationText.isNotEmpty() && !isStrictDouble(durationText))
                return "Chord edit rejected: Duration Beats must be numeric";

            const auto beforeSymbol = chord->getSymbol();
            chord->setSymbol(theoryFieldEditor1.getText().trim().isNotEmpty()
                                 ? theoryFieldEditor1.getText().trim()
                                 : chord->getSymbol());
            chord->setName(chord->getSymbol());
            chord->setQuality(theoryFieldEditor2.getText().trim().isNotEmpty()
                                  ? theoryFieldEditor2.getText().trim()
                                  : chord->getQuality());
            chord->setRootMidi(juce::jlimit(0, 127, parseIntOr(theoryFieldEditor3.getText(), chord->getRootMidi())));
            chord->setDurationBeats(juce::jmax(0.125, parseDoubleOr(theoryFieldEditor4.getText(), chord->getDurationBeats())));
            chord->setFunction(theoryFieldEditor5.getText().trim().isNotEmpty()
                                   ? theoryFieldEditor5.getText().trim()
                                   : chord->getFunction());
            const auto afterSymbol = chord->getSymbol();
            return "Applied chord edit: " + beforeSymbol + " → " + afterSymbol;
        }

        if (activeEditorActionId == chordSubstitution)
        {
            const auto profile = theoryFieldEditor1.getText().trim().toLowerCase();
            const auto root = chordRootNameForMidi(chord->getRootMidi());
            juce::String substitutionSymbol;
            juce::String quality;

            if (profile == "tritone")
            {
                const auto tritoneRoot = juce::jlimit(0, 127, chord->getRootMidi() + 6);
                substitutionSymbol = chordRootNameForMidi(tritoneRoot) + "7";
                quality = "dominant7";
                chord->setRootMidi(tritoneRoot);
            }
            else if (profile == "relative")
            {
                if (chord->getQuality().containsIgnoreCase("major"))
                {
                    const auto relativeRoot = juce::jlimit(0, 127, chord->getRootMidi() - 3);
                    substitutionSymbol = chordRootNameForMidi(relativeRoot) + "m7";
                    quality = "minor7";
                    chord->setRootMidi(relativeRoot);
                }
                else
                {
                    const auto relativeRoot = juce::jlimit(0, 127, chord->getRootMidi() + 3);
                    substitutionSymbol = chordRootNameForMidi(relativeRoot) + "maj7";
                    quality = "major7";
                    chord->setRootMidi(relativeRoot);
                }
            }
            else if (chord->getQuality().containsIgnoreCase("major"))
            {
                substitutionSymbol = root + "m7";
                quality = "minor7";
            }
            else if (chord->getQuality().containsIgnoreCase("minor"))
            {
                substitutionSymbol = root + "7";
                quality = "dominant7";
            }
            else
            {
                substitutionSymbol = root + "maj7";
                quality = "major7";
            }

            chord->setSymbol(substitutionSymbol);
            chord->setName(substitutionSymbol);
            chord->setQuality(quality);
            return "Chord substitution applied from center editor";
        }

        if (activeEditorActionId == chordSetFunction)
        {
            const auto chordFunction = theoryFieldEditor1.getText().trim();
            chord->setFunction(chordFunction.isNotEmpty() ? chordFunction : nextFunction(chord->getFunction()));
            return "Chord function updated from center editor";
        }
    }
    else if (activeEditorTarget == TheoryMenuTarget::note)
    {
        auto note = getSelectedNote();
        if (!note.has_value())
            return "Editor apply failed: no note selected";

        if (activeEditorActionId == noteEditTheory)
        {
            const auto pitchText = theoryFieldEditor1.getText().trim();
            const auto velocityText = theoryFieldEditor2.getText().trim();
            const auto startText = theoryFieldEditor3.getText().trim();
            const auto durationText = theoryFieldEditor4.getText().trim();

            if (pitchText.isNotEmpty() && !isStrictInt(pitchText))
                return "Note edit rejected: Pitch must be an integer";
            if (velocityText.isNotEmpty() && !isStrictDouble(velocityText))
                return "Note edit rejected: Velocity must be numeric";
            if (startText.isNotEmpty() && !isStrictDouble(startText))
                return "Note edit rejected: Start Beats must be numeric";
            if (durationText.isNotEmpty() && !isStrictDouble(durationText))
                return "Note edit rejected: Duration Beats must be numeric";

            note->setPitch(juce::jlimit(0, 127, parseIntOr(theoryFieldEditor1.getText(), note->getPitch())));
            note->setVelocity(juce::jlimit(0.0f, 1.0f, parseFloatOr(theoryFieldEditor2.getText(), note->getVelocity())));
            note->setStartBeats(juce::jmax(0.0, parseDoubleOr(theoryFieldEditor3.getText(), note->getStartBeats())));
            note->setDurationBeats(juce::jmax(0.03125, parseDoubleOr(theoryFieldEditor4.getText(), note->getDurationBeats())));
            return "Note updated from center editor";
        }
    }
    else
    {
        auto progression = getSelectedProgression();
        if (!progression.has_value())
            return "Editor apply failed: no progression selected";

        if (activeEditorActionId == progressionAnnotateKeyMode)
        {
            const auto name = theoryFieldEditor1.getText().trim();
            const auto key = theoryFieldEditor2.getText().trim();
            const auto mode = theoryFieldEditor3.getText().trim();
            const auto lengthText = theoryFieldEditor4.getText().trim();
            const auto variantOf = theoryFieldEditor5.getText().trim();

            if (lengthText.isNotEmpty() && !isStrictDouble(lengthText))
                return "Progression edit rejected: Length (beats) must be numeric";

            progression->setName(name.isNotEmpty() ? name : progression->getName());
            progression->setKey(key.isNotEmpty() ? key : progression->getKey());
            progression->setMode(mode.isNotEmpty() ? mode : progression->getMode());
            progression->setLengthBeats(juce::jmax(0.125, parseDoubleOr(theoryFieldEditor4.getText(), progression->getLengthBeats())));
            progression->setVariantOf(variantOf.isNotEmpty() ? variantOf : progression->getVariantOf());
            return "Progression key/mode updated from center editor";
        }

        if (activeEditorActionId == progressionCreateCandidate || activeEditorActionId == progressionGrabSampler)
        {
            auto candidate = model::Progression::create(
                theoryFieldEditor1.getText().trim().isNotEmpty()
                    ? theoryFieldEditor1.getText().trim()
                    : (activeEditorActionId == progressionGrabSampler ? "Queue Grab " : "Candidate ")
                          + juce::String(static_cast<int>(songState.getProgressions().size()) + 1),
                theoryFieldEditor2.getText().trim().isNotEmpty()
                    ? theoryFieldEditor2.getText().trim()
                    : progression->getKey(),
                theoryFieldEditor3.getText().trim().isNotEmpty()
                    ? theoryFieldEditor3.getText().trim()
                    : progression->getMode());
            candidate.setVariantOf(progression->getId());
            candidate.setLengthBeats(progression->getLengthBeats());

            for (const auto& sourceChord : progression->getChords())
            {
                auto cloned = model::Chord::create(sourceChord.getSymbol(), sourceChord.getQuality(), sourceChord.getRootMidi());
                cloned.setName(sourceChord.getName());
                cloned.setFunction(sourceChord.getFunction());
                cloned.setStartBeats(sourceChord.getStartBeats());
                cloned.setDurationBeats(sourceChord.getDurationBeats());
                cloned.setTension(sourceChord.getTension());
                for (const auto& sourceNote : sourceChord.getNotes())
                    cloned.addNote(model::Note::create(sourceNote.getPitch(),
                                                       sourceNote.getVelocity(),
                                                       sourceNote.getStartBeats(),
                                                       sourceNote.getDurationBeats(),
                                                       sourceNote.getChannel()));
                candidate.addChord(cloned);
            }

            songState.addProgression(candidate);
            selectedProgressionId = candidate.getId();
            return activeEditorActionId == progressionGrabSampler
                       ? "Queue candidate created from center editor"
                       : "Progression candidate created from center editor";
        }
    }

    return "No center-editor action mapped for current context";
}

void WorkspaceShellComponent::refreshTimelineData()
{
    if (timelineTracks != nullptr)
        timelineTracks->refreshTracks();

    ensureInstrumentSlots();
    rebuildOutPanelStrips();

    const auto sections = songState.getSections();
    const auto progressions = songState.getProgressions();

    timelineShell->updateSections(
        sections,
        progressions,
        selectedSectionId,
        [this](const juce::String& sectionId)
        {
            selectedSectionId = sectionId;
            ensureSelectionDefaults();
            populateTheoryObjectSelector();
            populateTheoryFieldsForCurrentSelection();
            refreshTimelineData(); // refresh chord strip for newly selected section
            interactionStatus.setText("Selected section: " + sectionId.substring(0, 8), juce::dontSendNotification);
        });

    // Collect chords from active progressions referenced by the selected section
    std::vector<model::Chord> visibleChords;
    for (const auto& section : sections)
    {
        if (section.getId() != selectedSectionId)
            continue;
        for (const auto& ref : section.getProgressionRefs())
        {
            for (const auto& prog : progressions)
            {
                if (prog.getId() == ref.getProgressionId())
                {
                    for (const auto& chord : prog.getChords())
                        visibleChords.push_back(chord);
                    break;
                }
            }
        }
        break;
    }

    double selectedBeatUnit = 1.0;
    double selectedBeatsPerBar = 4.0;
    for (const auto& section : sections)
    {
        if (section.getId() != selectedSectionId)
            continue;
        selectedBeatUnit = 4.0 / juce::jmax(1, section.getTimeSigDenominator());
        selectedBeatsPerBar = (section.getTimeSigNumerator() * 4.0) / juce::jmax(1, section.getTimeSigDenominator());
        break;
    }

    timelineShell->updateChords(
        visibleChords,
        selectedChordId,
        selectedBeatUnit,
        selectedBeatsPerBar,
        [this](const juce::String& chordId)
        {
            selectedChordId = chordId;
            ensureSelectionDefaults();
            populateTheoryObjectSelector();
            populateTheoryFieldsForCurrentSelection();
            interactionStatus.setText("Selected chord: " + chordId.substring(0, 8), juce::dontSendNotification);
        });

    if (timelineShell != nullptr)
    {
        timelineShell->setHistorySourceLabel(captureSourceSelector.getText().isNotEmpty()
                                                 ? captureSourceSelector.getText()
                                                 : "Off");
    }

    refreshArrangeWorkspace();

    syncTimeSignaturesToEdit();
}

void WorkspaceShellComponent::refreshArrangeWorkspace()
{
    if (arrangeWorkComponent == nullptr)
        return;

    std::vector<ArrangeWorkComponent::TileModel> tiles;
    std::vector<ArrangeWorkComponent::CadenceState> cadenceStates;
    const auto sections = songState.getSections();
    const auto transitions = songState.getTransitions();

    for (const auto& section : sections)
    {
        ArrangeWorkComponent::TileModel tile;
        tile.section = section;
        tile.lengthBeats = getSectionLengthBeats(section);

        juce::StringArray chordSymbols;
        int chordCount = 0;
        for (const auto& ref : section.getProgressionRefs())
        {
            if (const auto progression = songState.findProgressionById(ref.getProgressionId()))
            {
                const auto progressionLength = juce::jmax(1.0, progression->getLengthBeats());
                for (const auto& chord : progression->getChords())
                {
                    if (chordSymbols.size() < 6)
                        chordSymbols.add(chord.getSymbol().isNotEmpty() ? chord.getSymbol() : chord.getName());

                    const auto x = static_cast<float>(juce::jlimit(0.0, 0.95, chord.getStartBeats() / progressionLength));
                    const auto w = static_cast<float>(juce::jlimit(0.04, 0.28, chord.getDurationBeats() / progressionLength));
                    tile.audioPips.push_back({ x, 0.22f + 0.12f * static_cast<float>(chordCount % 3), w, 0.48f });

                    for (const auto& note : chord.getNotes())
                    {
                        const auto pitchNorm = static_cast<float>(juce::jlimit(0.0, 1.0, 1.0 - ((note.getPitch() - 36.0) / 60.0)));
                        tile.notePips.push_back({ x,
                                                  0.08f + pitchNorm * 0.72f,
                                                  juce::jmax(0.035f, w * 0.75f),
                                                  0.06f });
                    }
                    ++chordCount;
                }
            }
        }

        if (chordSymbols.isEmpty())
            chordSymbols.add("No progression");
        tile.progressionSummary = chordSymbols.joinIntoString(" - ");
        tiles.push_back(std::move(tile));
    }

    auto boundaryRoot = [this](const model::Section& section, bool wantLast)
    {
        std::vector<model::Chord> chords;
        for (const auto& ref : section.getProgressionRefs())
            if (const auto progression = songState.findProgressionById(ref.getProgressionId()))
                for (const auto& chord : progression->getChords())
                    chords.push_back(chord);

        if (chords.empty())
            return -1;

        const auto& chord = wantLast ? chords.back() : chords.front();
        return ((chord.getRootMidi() % 12) + 12) % 12;
    };

    for (size_t i = 0; i + 1 < sections.size(); ++i)
    {
        ArrangeWorkComponent::CadenceState state = ArrangeWorkComponent::CadenceState::neutral;
        const auto& from = sections[i];
        const auto& to = sections[i + 1];

        const auto transitionIt = std::find_if(transitions.begin(), transitions.end(), [&from, &to](const auto& transition)
        {
            return transition.getFromSectionId() == from.getId() && transition.getToSectionId() == to.getId();
        });

        if (transitionIt != transitions.end() && transitionIt->getStrategy().containsIgnoreCase("push"))
            state = ArrangeWorkComponent::CadenceState::standard;

        const auto tonicPc = setle::theory::DiatonicHarmony::pitchClassForRoot(songState.getSessionKey());
        const auto dominantPc = (tonicPc + 7) % 12;
        const auto fromRootPc = boundaryRoot(from, true);
        const auto toRootPc = boundaryRoot(to, false);
        const auto scale = setle::theory::DiatonicHarmony::modeIntervals(songState.getSessionMode());
        const bool toInScale = std::find(scale.begin(), scale.end(), (toRootPc - tonicPc + 12) % 12) != scale.end();

        if (fromRootPc == dominantPc && toRootPc == tonicPc)
            state = ArrangeWorkComponent::CadenceState::standard;
        else if (fromRootPc >= 0 && toRootPc >= 0 && (!toInScale || std::abs(fromRootPc - toRootPc) == 1))
            state = ArrangeWorkComponent::CadenceState::clash;

        cadenceStates.push_back(state);
    }

    arrangeWorkComponent->setSongStructure(std::move(tiles),
                                           selectedSectionId,
                                           songState.getSessionKey(),
                                           songState.getSessionMode(),
                                           std::move(cadenceStates));
}

juce::String WorkspaceShellComponent::beginUndoableAction(const juce::String& actionLabel)
{
    if (edit != nullptr && actionLabel.isNotEmpty())
        edit->getUndoManager().beginNewTransaction(actionLabel);
    return createSongSnapshot();
}

void WorkspaceShellComponent::performUpdate(const juce::String& beforeSnapshot, const juce::String& statusMessage)
{
    refreshTimelineData();
    saveSongState();
    captureUndoStateIfChanged(beforeSnapshot);
    if (statusMessage.isNotEmpty())
        interactionStatus.setText(statusMessage, juce::dontSendNotification);
}

void WorkspaceShellComponent::onSectionReordered(const juce::StringArray& orderedSectionIds)
{
    if (orderedSectionIds.isEmpty())
        return;

    juce::StringArray previousOrder;
    for (const auto& section : songState.getSections())
        previousOrder.add(section.getId());

    if (orderedSectionIds == previousOrder)
        return;

    const auto beforeSnapshot = beginUndoableAction("Reorder Arrange Sections");

    juce::String movedSectionId;
    int largestIndexDelta = 0;
    for (int newIndex = 0; newIndex < orderedSectionIds.size(); ++newIndex)
    {
        const auto& sectionId = orderedSectionIds.getReference(newIndex);
        const auto oldIndex = previousOrder.indexOf(sectionId);
        if (oldIndex < 0)
            continue;

        const auto indexDelta = newIndex - oldIndex;
        if (std::abs(indexDelta) > std::abs(largestIndexDelta))
        {
            largestIndexDelta = indexDelta;
            movedSectionId = sectionId;
        }
    }

    auto sectionStartForOrder = [this](const juce::StringArray& order, const juce::String& sectionId)
    {
        double beat = 0.0;
        for (const auto& id : order)
        {
            if (id == sectionId)
                break;

            if (const auto section = songState.findSectionById(id); section.has_value())
                beat += getSectionLengthBeats(section.value());
        }
        return beat;
    };

    double movedDeltaBeats = 0.0;
    if (movedSectionId.isNotEmpty())
    {
        movedDeltaBeats = sectionStartForOrder(orderedSectionIds, movedSectionId)
                        - sectionStartForOrder(previousOrder, movedSectionId);
    }

    for (int i = 0; i < orderedSectionIds.size(); ++i)
        songState.moveSection(orderedSectionIds[i], i);

    if (timelineStampManager != nullptr)
    {
        timelineStampManager->applySectionReorder(songState,
                                                  previousOrder,
                                                  orderedSectionIds,
                                                  [this](const model::Section& section)
                                                  {
                                                      return getSectionLengthBeats(section);
                                                  });
    }

    juce::String status = "Structural mosaic reordered";
    if (movedSectionId.isNotEmpty())
    {
        status << " | Δ " << juce::String(movedDeltaBeats, 2)
               << " beats (" << movedSectionId.substring(0, 8) << ")";
    }

    performUpdate(beforeSnapshot, status);
}

juce::String WorkspaceShellComponent::smartDuplicateSection(const juce::String& sectionId, ArrangeWorkComponent::DuplicateMode mode)
{
    const auto sourceSection = songState.findSectionById(sectionId);
    if (!sourceSection.has_value())
        return "Smart Duplicate failed: source section not found";

    const auto snapshot = createSongSnapshot();
    const auto insertionBeat = getSongStructureLengthBeats();
    auto duplicatedSection = model::Section::create(sourceSection->getName() + " (Fork)", sourceSection->getRepeatCount());
    duplicatedSection.setColor(sourceSection->getColor());
    duplicatedSection.setTimeSigNumerator(sourceSection->getTimeSigNumerator());
    duplicatedSection.setTimeSigDenominator(sourceSection->getTimeSigDenominator());

    const auto originalMode = songState.getSessionMode();
    const auto targetMode = relativeModeFor(originalMode);

    for (const auto& ref : sourceSection->getProgressionRefs())
    {
        if (mode == ArrangeWorkComponent::DuplicateMode::exactObject)
        {
            duplicatedSection.addProgressionRef(ref);
            continue;
        }

        const auto sourceProgression = songState.findProgressionById(ref.getProgressionId());
        if (!sourceProgression.has_value())
            continue;

        auto newProgression = model::Progression::create(sourceProgression->getName(),
                                                         sourceProgression->getKey(),
                                                         mode == ArrangeWorkComponent::DuplicateMode::modalExtraction
                                                             ? targetMode
                                                             : sourceProgression->getMode());
        newProgression.setVariantOf(sourceProgression->getId());
        newProgression.setLengthBeats(sourceProgression->getLengthBeats()
                                      * (mode == ArrangeWorkComponent::DuplicateMode::rhythmicDecimation ? 2.0 : 1.0));

        for (const auto& chord : sourceProgression->getChords())
        {
            auto newChord = model::Chord::create(chord.getSymbol(), chord.getQuality(), chord.getRootMidi());
            newChord.setName(chord.getName());
            newChord.setFunction(chord.getFunction());
            newChord.setSource(mode == ArrangeWorkComponent::DuplicateMode::modalExtraction ? "modalExtraction" : "rhythmicDecimation");
            newChord.setConfidence(chord.getConfidence());
            newChord.setTension(chord.getTension());
            newChord.setStartBeats(chord.getStartBeats());
            newChord.setDurationBeats(chord.getDurationBeats());

            if (mode == ArrangeWorkComponent::DuplicateMode::modalExtraction)
            {
                const auto newRoot = setle::theory::TheoryEngine::applyModalTransform(chord.getRootMidi(),
                                                                                       songState.getSessionKey(),
                                                                                       sourceProgression->getMode(),
                                                                                       targetMode);
                newChord.setRootMidi(newRoot);
                const auto newSymbol = chordRootNameForMidi(newRoot) + qualitySuffixFor(chord);
                newChord.setSymbol(newSymbol);
                newChord.setName(newSymbol);
            }
            else if (mode == ArrangeWorkComponent::DuplicateMode::rhythmicDecimation)
            {
                newChord.setStartBeats(chord.getStartBeats() * 2.0);
                newChord.setDurationBeats(chord.getDurationBeats() * 2.0);
                newChord.setName(chord.getName() + " HT");
            }

            for (const auto& note : chord.getNotes())
            {
                auto newNote = model::Note::create(note.getPitch(),
                                                   note.getVelocity(),
                                                   mode == ArrangeWorkComponent::DuplicateMode::rhythmicDecimation ? note.getStartBeats() * 2.0 : note.getStartBeats(),
                                                   mode == ArrangeWorkComponent::DuplicateMode::rhythmicDecimation ? note.getDurationBeats() * 2.0 : note.getDurationBeats(),
                                                   note.getChannel());
                if (mode == ArrangeWorkComponent::DuplicateMode::modalExtraction)
                {
                    newNote.setPitch(setle::theory::TheoryEngine::applyModalTransform(note.getPitch(),
                                                                                       songState.getSessionKey(),
                                                                                       sourceProgression->getMode(),
                                                                                       targetMode));
                }
                newChord.addNote(newNote);
            }

            newProgression.addChord(newChord);
        }

        if (mode == ArrangeWorkComponent::DuplicateMode::modalExtraction)
            newProgression.setName(sourceProgression->getName() + " [" + targetMode + "]");
        else if (mode == ArrangeWorkComponent::DuplicateMode::rhythmicDecimation)
            newProgression.setName(sourceProgression->getName() + " [Half-Time]");

        songState.addProgression(newProgression);
        duplicatedSection.addProgressionRef(model::SectionProgressionRef::create(newProgression.getId(), ref.getOrderIndex(), ref.getVariantName()));
    }

    songState.addSection(duplicatedSection);

    if (timelineStampManager != nullptr)
    {
        setle::timeline::TimelineStampManager::DuplicateStampMode stampMode = setle::timeline::TimelineStampManager::DuplicateStampMode::exactObject;
        if (mode == ArrangeWorkComponent::DuplicateMode::modalExtraction)
            stampMode = setle::timeline::TimelineStampManager::DuplicateStampMode::modalExtraction;
        else if (mode == ArrangeWorkComponent::DuplicateMode::rhythmicDecimation)
            stampMode = setle::timeline::TimelineStampManager::DuplicateStampMode::rhythmicDecimation;

        timelineStampManager->stampSectionDuplicate(songState,
                                                    sourceSection->getId(),
                                                    duplicatedSection.getId(),
                                                    stampMode,
                                                    insertionBeat,
                                                    songState.getSessionKey(),
                                                    originalMode,
                                                    mode == ArrangeWorkComponent::DuplicateMode::modalExtraction ? targetMode : originalMode,
                                                    [this](const model::Section& section)
                                                    {
                                                        return getSectionLengthBeats(section);
                                                    });
    }

    selectedSectionId = duplicatedSection.getId();
    refreshTimelineData();
    saveSongState();
    captureUndoStateIfChanged(snapshot);

    switch (mode)
    {
        case ArrangeWorkComponent::DuplicateMode::exactObject:
            return "Smart Duplicate: Exact Object created";
        case ArrangeWorkComponent::DuplicateMode::modalExtraction:
            return "Smart Duplicate: Modal Extraction created in " + targetMode;
        case ArrangeWorkComponent::DuplicateMode::rhythmicDecimation:
            return "Smart Duplicate: Rhythmic Decimation created";
    }
    return "Smart Duplicate complete";
}

double WorkspaceShellComponent::getSectionLengthBeats(const model::Section& section) const
{
    double sectionLength = 0.0;
    for (const auto& ref : section.getProgressionRefs())
        if (const auto progression = songState.findProgressionById(ref.getProgressionId()))
            sectionLength += juce::jmax(1.0, progression->getLengthBeats());

    if (sectionLength <= 0.0)
        sectionLength = (section.getTimeSigNumerator() * 4.0) / juce::jmax(1, section.getTimeSigDenominator());

    return sectionLength * juce::jmax(1, section.getRepeatCount());
}

double WorkspaceShellComponent::getSongStructureLengthBeats() const
{
    double total = 0.0;
    for (const auto& section : songState.getSections())
        total += getSectionLengthBeats(section);
    return juce::jmax(1.0, total);
}

void WorkspaceShellComponent::syncTimeSignaturesToEdit()
{
    if (!edit)
        return;

    auto& tempoSequence = edit->tempoSequence;

    // Clear all existing time sig events beyond beat 0
    // (keep the default — remove any section-derived ones)
    for (int i = tempoSequence.getNumTimeSigs() - 1; i > 0; --i)
        tempoSequence.removeTimeSig(i);

    // Add one time sig event per section that has a non-default meter
    const int songDefaultNum = 4;
    const int songDefaultDen = 4;

    for (const auto& section : songState.getSections())
    {
        const int num = section.getTimeSigNumerator();   // default 4
        const int den = section.getTimeSigDenominator(); // default 4

        if (num != songDefaultNum || den != songDefaultDen)
        {
            // Calculate this section's start beat
            double sectionStartBeat = 0.0;
            const auto sections = songState.getSections();
            const auto progressions = songState.getProgressions();

            for (const auto& s : sections)
            {
                if (s.getId() == section.getId())
                    break;

                // Sum all chord durations in this section's progressions
                for (const auto& ref : s.getProgressionRefs())
                {
                    for (const auto& prog : progressions)
                    {
                        if (prog.getId() == ref.getProgressionId())
                        {
                            for (const auto& chord : prog.getChords())
                                sectionStartBeat += chord.getDurationBeats();
                            break;
                        }
                    }
                }
            }

            if (auto ts = tempoSequence.insertTimeSig(tracktion::BeatPosition::fromBeats(sectionStartBeat))) { ts->numerator = num; ts->denominator = den; }
        }
    }
}

void WorkspaceShellComponent::ensureInstrumentSlots()
{
    if (edit == nullptr || trackManager == nullptr)
        return;

    std::set<juce::String> liveTrackIds;
    const auto tracks = trackManager->getAllUserTracks();
    for (auto* track : tracks)
    {
        auto* audioTrack = dynamic_cast<te::AudioTrack*>(track);
        if (audioTrack == nullptr)
            continue;

        const auto trackId = audioTrack->itemID.toString();
        liveTrackIds.insert(trackId);

        if (instrumentSlots.find(trackId) == instrumentSlots.end())
            instrumentSlots[trackId] = std::make_unique<setle::instruments::InstrumentSlot>(*audioTrack, *edit);
    }

    for (auto it = instrumentSlots.begin(); it != instrumentSlots.end();)
    {
        if (liveTrackIds.find(it->first) == liveTrackIds.end())
            it = instrumentSlots.erase(it);
        else
            ++it;
    }
}

void WorkspaceShellComponent::applyPersistedInstrumentSlotAssignments()
{
    if (!songState.isValid() || instrumentSlots.empty())
        return;

    auto root = songState.valueTree();
    const auto container = root.getChildWithName(kInstrumentSlotsContainerId);
    if (!container.isValid())
        return;

    for (const auto& child : container)
    {
        if (!child.hasType(kInstrumentSlotEntryId))
            continue;

        const auto trackId = child.getProperty(kSlotTrackIdProp, {}).toString();
        if (trackId.isEmpty())
            continue;

        auto it = instrumentSlots.find(trackId);
        if (it == instrumentSlots.end() || it->second == nullptr)
            continue;

        const auto type = slotTypeFromString(child.getProperty(kSlotTypeProp, {}).toString());
        const auto persistentId = child.getProperty(kSlotPersistentIdProp, {}).toString();
        const auto persistentName = child.getProperty(kSlotPersistentNameProp, {}).toString();

        it->second->clear();
        switch (type)
        {
            case setle::instruments::InstrumentSlot::SlotType::PolySynth:
                it->second->loadPolySynth();
                break;
            case setle::instruments::InstrumentSlot::SlotType::DrumMachine:
                it->second->loadDrumMachine();
                break;
            case setle::instruments::InstrumentSlot::SlotType::ReelSampler:
                it->second->loadReelSampler();
                break;
            case setle::instruments::InstrumentSlot::SlotType::MidiOut:
                it->second->loadMidiOut(persistentId.isNotEmpty() ? persistentId : "default");
                break;
            case setle::instruments::InstrumentSlot::SlotType::VST3:
            {
                juce::PluginDescription resolved;
                bool found = false;
                for (const auto& desc : engineRef.getPluginManager().knownPluginList.getTypes())
                {
                    if (!desc.pluginFormatName.containsIgnoreCase("VST3"))
                        continue;

                    if ((!persistentId.isEmpty() && desc.fileOrIdentifier == persistentId)
                        || (!persistentName.isEmpty() && desc.name == persistentName))
                    {
                        resolved = desc;
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    resolved.name = persistentName.isNotEmpty() ? persistentName : "VST3";
                    resolved.fileOrIdentifier = persistentId;
                    resolved.pluginFormatName = "VST3";
                }

                it->second->loadVST3(resolved);
                break;
            }
            case setle::instruments::InstrumentSlot::SlotType::Empty:
                break;
        }
    }
}

void WorkspaceShellComponent::persistInstrumentSlotAssignments()
{
    if (!songState.isValid())
        return;

    auto root = songState.valueTree();
    if (!root.isValid())
        return;

    for (int i = root.getNumChildren(); --i >= 0;)
    {
        if (root.getChild(i).hasType(kInstrumentSlotsContainerId))
            root.removeChild(i, nullptr);
    }

    juce::ValueTree container(kInstrumentSlotsContainerId);
    for (const auto& [trackId, slot] : instrumentSlots)
    {
        if (slot == nullptr)
            continue;

        const auto info = slot->getInfo();
        if (info.type == setle::instruments::InstrumentSlot::SlotType::Empty)
            continue;

        juce::ValueTree entry(kInstrumentSlotEntryId);
        entry.setProperty(kSlotTrackIdProp, trackId, nullptr);
        entry.setProperty(kSlotTypeProp, slotTypeToString(info.type), nullptr);
        entry.setProperty(kSlotPersistentIdProp, info.persistentIdentifier, nullptr);
        entry.setProperty(kSlotPersistentNameProp, info.persistentName, nullptr);
        container.appendChild(entry, nullptr);
    }

    root.appendChild(container, nullptr);
}

void WorkspaceShellComponent::rebuildOutPanelStrips()
{
    if (outPanelHost == nullptr || trackManager == nullptr)
        return;

    outPanelHost->rebuild(
        trackManager->getAllUserTracks(),
        instrumentSlots,
        [this](const juce::String& trackId, setle::instruments::InstrumentSlot::SlotType type)
        {
            auto it = instrumentSlots.find(trackId);
            if (it == instrumentSlots.end() || it->second == nullptr)
                return;

            switch (type)
            {
                case setle::instruments::InstrumentSlot::SlotType::PolySynth:   it->second->loadPolySynth(); break;
                case setle::instruments::InstrumentSlot::SlotType::DrumMachine: it->second->loadDrumMachine(); break;
                case setle::instruments::InstrumentSlot::SlotType::ReelSampler: it->second->loadReelSampler(); break;
                case setle::instruments::InstrumentSlot::SlotType::VST3:        it->second->loadVST3(juce::PluginDescription{}); break;
                case setle::instruments::InstrumentSlot::SlotType::MidiOut:     it->second->loadMidiOut("default"); break;
                case setle::instruments::InstrumentSlot::SlotType::Empty:       it->second->clear(); break;
            }

            persistInstrumentSlotAssignments();
            rebuildOutPanelStrips();
        },
        [this]()
        {
            interactionStatus.setText("VST3 scan uses Tracktion known plugin list.", juce::dontSendNotification);
        },
        [this](const juce::String& trackId)
        {
            juce::PopupMenu menu;
            std::vector<juce::PluginDescription> vst3Descs;

            int itemId = 1;
            for (const auto& desc : engineRef.getPluginManager().knownPluginList.getTypes())
            {
                if (!desc.pluginFormatName.containsIgnoreCase("VST3"))
                    continue;

                menu.addItem(itemId++, desc.name.isNotEmpty() ? desc.name : desc.fileOrIdentifier);
                vst3Descs.push_back(desc);
            }

            if (vst3Descs.empty())
            {
                interactionStatus.setText("No VST3 plugins found in known plugin list.", juce::dontSendNotification);
                return;
            }

            const auto chosen = menu.show();
            if (chosen <= 0 || chosen > static_cast<int>(vst3Descs.size()))
                return;

            auto it = instrumentSlots.find(trackId);
            if (it == instrumentSlots.end() || it->second == nullptr)
                return;

            it->second->loadVST3(vst3Descs[static_cast<size_t>(chosen - 1)]);
            persistInstrumentSlotAssignments();
            rebuildOutPanelStrips();
        },
        [this](const juce::String& trackId, const juce::String& deviceIdentifier)
        {
            auto it = instrumentSlots.find(trackId);
            if (it == instrumentSlots.end() || it->second == nullptr)
                return;

            it->second->loadMidiOut(deviceIdentifier);
            persistInstrumentSlotAssignments();
            rebuildOutPanelStrips();
        });

    refreshSoundRoster();
}

void WorkspaceShellComponent::applyDrumPatternToSlots(const std::vector<setle::gridroll::GridRollCell>& cells,
                                                      const juce::String& progressionId)
{
    (void) progressionId;

    std::map<int, float> swingByNote;
    for (const auto& cell : cells)
    {
        if (cell.type != setle::gridroll::GridRollCell::CellType::DrumHit)
            continue;
        swingByNote[cell.midiNote] = juce::jmax(swingByNote[cell.midiNote], cell.swing);
    }

    ensureInstrumentSlots();
    for (auto& [trackId, slot] : instrumentSlots)
    {
        (void) trackId;
        if (slot != nullptr)
        {
            slot->setDrumPattern(cells);
            if (auto* processor = slot->getEffProcessor())
            {
                for (const auto& [midiNote, swing] : swingByNote)
                    processor->setDrumSwingForNote(midiNote, swing);
            }
        }
    }
}

std::vector<SoundWorkComponent::InstrumentRosterEntry> WorkspaceShellComponent::buildSoundRosterEntries() const
{
    std::vector<SoundWorkComponent::InstrumentRosterEntry> entries;
    if (trackManager == nullptr)
        return entries;

    const auto tracks = trackManager->getAllUserTracks();
    entries.reserve(static_cast<size_t>(tracks.size()));

    for (auto* track : tracks)
    {
        auto* audioTrack = dynamic_cast<te::AudioTrack*>(track);
        if (audioTrack == nullptr)
            continue;

        const auto trackId = audioTrack->itemID.toString();
        auto it = instrumentSlots.find(trackId);
        if (it == instrumentSlots.end() || it->second == nullptr)
            continue;

        SoundWorkComponent::InstrumentRosterEntry entry;
        entry.trackId = trackId;
        entry.trackName = audioTrack->getName();
        entry.slotType = it->second->getInfo().type;
        entries.push_back(std::move(entry));
    }

    return entries;
}

void WorkspaceShellComponent::refreshSoundRoster()
{
    if (soundWorkComponent == nullptr)
        return;

    soundWorkComponent->setInstrumentRoster(buildSoundRosterEntries());
    soundWorkComponent->setFocusedTrack(selectedFxTrackId);
}

void WorkspaceShellComponent::focusSoundTrack(const juce::String& trackId)
{
    auto it = instrumentSlots.find(trackId);
    if (it == instrumentSlots.end() || it->second == nullptr)
        return;

    selectedFxTrackId = trackId;
    auto* slot = it->second.get();

    auto* proc = slot->getEffProcessor();
    if (proc == nullptr)
    {
        setle::eff::EffDefinition emptyDef;
        emptyDef.effId = juce::Uuid().toString();
        emptyDef.name = "Track FX";
        emptyDef.schemaVersion = 1;
        slot->loadEffChain(emptyDef);
        proc = slot->getEffProcessor();
    }

    if (proc != nullptr && effBuilderComponent != nullptr)
    {
        const auto effFile = getEffFileForTrack(trackId);
        effBuilderComponent->loadDefinition(proc, proc->getDefinition(), effFile);
    }

    if (soundWorkComponent != nullptr)
        soundWorkComponent->setFocusedTrack(trackId);
}

SoundWorkComponent::SidechainSuggestion WorkspaceShellComponent::buildSmartSidechainSuggestion(const juce::String& focusedTrackId) const
{
    SoundWorkComponent::SidechainSuggestion suggestion;

    if (trackManager == nullptr)
        return suggestion;

    juce::String sourceId;
    juce::String sourceName;
    int bestSourceScore = -1;

    juce::String targetId;
    juce::String targetName;
    int bestTargetScore = -1;

    const auto tracks = trackManager->getAllUserTracks();
    for (auto* track : tracks)
    {
        auto* audioTrack = dynamic_cast<te::AudioTrack*>(track);
        if (audioTrack == nullptr)
            continue;

        const auto name = audioTrack->getName();
        const auto id = audioTrack->itemID.toString();
        auto slotIt = instrumentSlots.find(id);
        const auto slotType = (slotIt != instrumentSlots.end() && slotIt->second != nullptr)
            ? slotIt->second->getInfo().type
            : setle::instruments::InstrumentSlot::SlotType::Empty;

        const auto sourceScore = scoreSidechainSourceCandidate(name, slotType);
        if (sourceScore > bestSourceScore)
        {
            bestSourceScore = sourceScore;
            sourceId = id;
            sourceName = name;
        }

        if (id != sourceId)
        {
            const auto targetScore = scoreSidechainTargetCandidate(name, slotType, id == focusedTrackId);
            if (targetScore > bestTargetScore)
            {
                bestTargetScore = targetScore;
                targetId = id;
                targetName = name;
            }
        }
    }

    if (sourceId == targetId)
    {
        targetId.clear();
        targetName.clear();
    }

    if (bestSourceScore <= 0 || bestTargetScore <= 0)
        return suggestion;

    suggestion.sourceTrackId = sourceId;
    suggestion.sourceTrackName = sourceName;
    suggestion.targetTrackId = targetId;
    suggestion.targetTrackName = targetName;
    return suggestion;
}

void WorkspaceShellComponent::applySmartSidechainSuggestion(const SoundWorkComponent::SidechainSuggestion& suggestion)
{
    if (!suggestion.isValid())
        return;

    auto* sourceTrack = findAudioTrackById(trackManager.get(), suggestion.sourceTrackId);
    auto* targetTrack = findAudioTrackById(trackManager.get(), suggestion.targetTrackId);
    if (sourceTrack == nullptr || targetTrack == nullptr || edit == nullptr)
        return;

    auto* compressor = ensureTracktionCompressor(*targetTrack, *edit);
    if (compressor == nullptr || !compressor->canSidechain())
    {
        interactionStatus.setText("Smart Sidechain failed: Tracktion compressor sidechain unavailable",
                                  juce::dontSendNotification);
        return;
    }

    const auto sidechainChoice = findSidechainSourceChoice(*compressor, suggestion.sourceTrackName);
    if (sidechainChoice.isEmpty())
    {
        interactionStatus.setText("Smart Sidechain failed: source track is not available as a sidechain input",
                                  juce::dontSendNotification);
        return;
    }

    compressor->useSidechainTrigger = true;
    compressor->setThreshold(juce::Decibels::decibelsToGain(-18.0f));
    compressor->setRatio(0.25f);
    compressor->attackMs.setParameter(25.0f, juce::sendNotification);
    compressor->releaseMs.setParameter(180.0f, juce::sendNotification);
    compressor->outputDb.setParameter(0.0f, juce::sendNotification);
    compressor->sidechainDb.setParameter(0.0f, juce::sendNotification);
    compressor->setSidechainSourceByName(sidechainChoice);
    compressor->guessSidechainRouting();

    focusSoundTrack(suggestion.targetTrackId);
    interactionStatus.setText("Smart Sidechain: " + suggestion.sourceTrackName + " routed into "
                              + suggestion.targetTrackName + " Tracktion compressor",
                              juce::dontSendNotification);
}

std::optional<float> WorkspaceShellComponent::runAutoGainForTrack(const juce::String& trackId)
{
    if (trackManager == nullptr || circularAudioBuffer == nullptr)
        return std::nullopt;

    auto* targetTrack = findAudioTrackById(trackManager.get(), trackId);
    if (targetTrack == nullptr)
        return std::nullopt;

    auto analysisBuffer = circularAudioBuffer->getLastSeconds(8.0);
    if (analysisBuffer.getNumChannels() == 0 || analysisBuffer.getNumSamples() == 0)
        return std::nullopt;

    const int windowSize = juce::jmin(2048, analysisBuffer.getNumSamples());
    if (windowSize <= 32)
        return std::nullopt;

    const int startSample = analysisBuffer.getNumSamples() - windowSize;
    double energy = 0.0;
    for (int ch = 0; ch < analysisBuffer.getNumChannels(); ++ch)
    {
        const auto* data = analysisBuffer.getReadPointer(ch, startSample);
        for (int i = 0; i < windowSize; ++i)
        {
            const auto s = static_cast<double>(data[i]);
            energy += s * s;
        }
    }

    energy /= static_cast<double>(windowSize * analysisBuffer.getNumChannels());
    if (energy <= 1.0e-10)
        return std::nullopt;

    const float measuredDb = static_cast<float>(20.0 * std::log10(std::sqrt(energy)));
    const float targetLufs = -18.0f;
    const float gainOffsetDb = juce::jlimit(-12.0f, 12.0f, targetLufs - measuredDb);

    if (auto* volumePlugin = targetTrack->getVolumePlugin())
    {
        volumePlugin->setVolumeDb(volumePlugin->getVolumeDb() + gainOffsetDb);
    }

    interactionStatus.setText("Auto Gain (RMS 2048): " + targetTrack->getName() + " -> "
                              + juce::String(gainOffsetDb, 1) + " dB toward "
                              + juce::String(targetLufs, 1) + " LUFS",
                              juce::dontSendNotification);
    return gainOffsetDb;
}

void WorkspaceShellComponent::showVesselSelectionModal()
{
    if (trackManager == nullptr)
        return;

    juce::PopupMenu menu;
    menu.addSectionHeader("Vessel Selection");
    menu.addItem(1, "PolySynth");
    menu.addItem(2, "DrumMachine");
    menu.addItem(3, "ReelSampler");
    menu.addItem(4, "VST3");

    auto options = juce::PopupMenu::Options();
    if (soundWorkComponent != nullptr)
        options = options.withTargetComponent(soundWorkComponent.get());

    const auto selected = menu.showMenu(options);
    if (selected <= 0)
        return;

    juce::PluginDescription vst3Description;
    if (selected == 4)
    {
        for (const auto& known : engineRef.getPluginManager().knownPluginList.getTypes())
        {
            if (known.pluginFormatName.containsIgnoreCase("VST3"))
            {
                vst3Description = known;
                break;
            }
        }

        if (vst3Description.fileOrIdentifier.isEmpty())
        {
            interactionStatus.setText("Add MIDI skipped: no scanned VST3 plugin available",
                                      juce::dontSendNotification);
            if (soundWorkComponent != nullptr)
                soundWorkComponent->setFocusedTrack(selectedFxTrackId);
            return;
        }
    }

    auto* track = trackManager->addMidiTrack("MIDI Track");
    if (track == nullptr)
        return;

    ensureInstrumentSlots();

    const auto trackId = track->itemID.toString();
    auto it = instrumentSlots.find(trackId);
    if (it == instrumentSlots.end() || it->second == nullptr)
        return;

    switch (selected)
    {
        case 1: it->second->loadPolySynth(); track->setName("PolySynth"); break;
        case 2: it->second->loadDrumMachine(); track->setName("DrumMachine"); break;
        case 3: it->second->loadReelSampler(); track->setName("ReelSampler"); break;
        case 4:
        {
            it->second->loadVST3(vst3Description);
            track->setName(vst3Description.name.isNotEmpty() ? vst3Description.name : "VST3");
            break;
        }
        default:
            break;
    }

    persistInstrumentSlotAssignments();
    refreshTimelineData();
    rebuildOutPanelStrips();

    switchNavSection(NavSection::sound);
    switchWorkTab(2);
    focusSoundTrack(trackId);
    interactionStatus.setText("Added MIDI vessel: " + track->getName(), juce::dontSendNotification);
}

juce::String WorkspaceShellComponent::getTrackIdForTrack(const te::Track& track) const
{
    return track.itemID.toString();
}

void WorkspaceShellComponent::initialiseSongState()
{
    loadSongState();
    if (grabSamplerQueue != nullptr)
        grabSamplerQueue->setSong(&songState);
    seedSongStateIfNeeded();
    ensureSelectionDefaults();

    refreshCaptureSourceSelector(setle::state::AppPreferences::get().getCaptureSource());

    // Restore session key/mode selectors from songState
    {
        auto sessionKey = songState.getSessionKey();
        auto sessionMode = songState.getSessionMode();

        const auto keys = juce::StringArray { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        const auto modes = juce::StringArray { "ionian", "dorian", "phrygian", "lydian", "mixolydian", "aeolian", "locrian" };

        for (int i = 0; i < keys.size(); ++i)
        {
            if (keys[i] == sessionKey)
            {
                sessionKeySelector.setSelectedId(i + 1, juce::dontSendNotification);
                break;
            }
        }

        for (int i = 0; i < modes.size(); ++i)
        {
            if (modes[i] == sessionMode)
            {
                sessionModeSelector.setSelectedId(i + 1, juce::dontSendNotification);
                break;
            }
        }
    }

    saveSongState();
    refreshTimelineData();
    loadEffChains();

    interactionStatus.setText("Theory state ready - " + summarizeSongState(), juce::dontSendNotification);
}

juce::File WorkspaceShellComponent::getSongStateFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SETLE")
        .getChildFile("song_state.xml");
}

void WorkspaceShellComponent::loadSongState()
{
    if (auto loaded = model::Song::loadFromFile(getSongStateFile(), model::StorageFormat::xml))
    {
        songState = *loaded;
        return;
    }

    songState = model::Song::create("SETLE Session", 120.0);
}

void WorkspaceShellComponent::seedSongStateIfNeeded()
{
    if (!songState.isValid())
        songState = model::Song::create("SETLE Session", 120.0);

    if (songState.getProgressions().empty())
    {
        auto progression = model::Progression::create("Core Loop", "C", "ionian");

        auto cmaj7 = model::Chord::create("Cmaj7", "major7", 60);
        cmaj7.setFunction("T");
        cmaj7.addNote(model::Note::create(60, 0.85f, 0.0, 1.0, 1));
        cmaj7.addNote(model::Note::create(64, 0.78f, 0.0, 1.0, 1));
        cmaj7.addNote(model::Note::create(67, 0.74f, 0.0, 1.0, 1));
        cmaj7.addNote(model::Note::create(71, 0.70f, 0.0, 1.0, 1));

        auto dm7 = model::Chord::create("Dm7", "minor7", 62);
        dm7.setFunction("PD");
        dm7.setStartBeats(4.0);
        dm7.addNote(model::Note::create(62, 0.80f, 4.0, 1.0, 1));
        dm7.addNote(model::Note::create(65, 0.75f, 4.0, 1.0, 1));
        dm7.addNote(model::Note::create(69, 0.72f, 4.0, 1.0, 1));
        dm7.addNote(model::Note::create(72, 0.68f, 4.0, 1.0, 1));

        auto g7 = model::Chord::create("G7", "dominant7", 67);
        g7.setFunction("D");
        g7.setStartBeats(8.0);
        g7.addNote(model::Note::create(67, 0.82f, 8.0, 1.0, 1));
        g7.addNote(model::Note::create(71, 0.76f, 8.0, 1.0, 1));
        g7.addNote(model::Note::create(74, 0.73f, 8.0, 1.0, 1));
        g7.addNote(model::Note::create(77, 0.69f, 8.0, 1.0, 1));

        auto cResolve = model::Chord::create("Cmaj7", "major7", 60);
        cResolve.setFunction("T");
        cResolve.setStartBeats(12.0);
        cResolve.addNote(model::Note::create(60, 0.85f, 12.0, 1.0, 1));
        cResolve.addNote(model::Note::create(64, 0.78f, 12.0, 1.0, 1));
        cResolve.addNote(model::Note::create(67, 0.74f, 12.0, 1.0, 1));
        cResolve.addNote(model::Note::create(71, 0.70f, 12.0, 1.0, 1));

        progression.addChord(cmaj7);
        progression.addChord(dm7);
        progression.addChord(g7);
        progression.addChord(cResolve);
        songState.addProgression(progression);
    }

    auto progressions = songState.getProgressions();
    if (progressions.empty())
        return;

    if (songState.getSections().empty())
    {
        auto section = model::Section::create("Verse", 4);
        section.addProgressionRef(model::SectionProgressionRef::create(progressions.front().getId(), 0, ""));
        songState.addSection(section);

        auto chorus = model::Section::create("Chorus", 2);
        chorus.addProgressionRef(model::SectionProgressionRef::create(progressions.front().getId(), 0, ""));
        songState.addSection(chorus);
    }

    auto sections = songState.getSections();
    if (songState.getTransitions().empty() && sections.size() >= 2)
        songState.addTransition(model::Transition::create(sections.front().getId(), sections[1].getId(), "pushRight"));
}

void WorkspaceShellComponent::saveSongState()
{
    if (!songState.isValid())
        return;

    persistInstrumentSlotAssignments();
    saveEffChains();

    const auto result = songState.saveToFile(getSongStateFile(), model::StorageFormat::xml);
    if (result.failed())
        DBG("Failed to save song state: " + result.getErrorMessage());

    syncTimeSignaturesToEdit();
}

juce::File WorkspaceShellComponent::getEffectsFolder() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SETLE")
        .getChildFile("effects");
}

juce::File WorkspaceShellComponent::getEffFileForTrack(const juce::String& trackId) const
{
    return getEffectsFolder().getChildFile(trackId + ".eff");
}

void WorkspaceShellComponent::saveEffChains()
{
    for (auto& [trackId, slot] : instrumentSlots)
    {
        if (slot == nullptr) continue;
        auto* proc = slot->getEffProcessor();
        if (proc == nullptr) continue;
        const auto& def = proc->getDefinition();
        if (def.effId.isEmpty()) continue;

        const auto file = getEffFileForTrack(trackId);
        file.getParentDirectory().createDirectory();
        setle::eff::EffSerializer::saveToFile(def, file);
    }
}

void WorkspaceShellComponent::loadEffChains()
{
    for (auto& [trackId, slot] : instrumentSlots)
    {
        if (slot == nullptr) continue;
        const auto file = getEffFileForTrack(trackId);
        if (!file.existsAsFile()) continue;

        const auto def = setle::eff::EffSerializer::loadFromFile(file);
        if (def.effId.isEmpty()) continue;

        slot->loadEffChain(def);
    }
}

juce::String WorkspaceShellComponent::createSongSnapshot() const
{
    if (!songState.isValid())
        return {};

    return songState.valueTree().toXmlString();
}

bool WorkspaceShellComponent::restoreSongFromSnapshot(const juce::String& xmlSnapshot)
{
    if (xmlSnapshot.isEmpty())
        return false;

    auto restored = juce::ValueTree::fromXml(xmlSnapshot);
    if (!restored.isValid() || !restored.hasType(model::Schema::songType))
        return false;

    songState = model::Song(restored);
    seedSongStateIfNeeded();
    ensureSelectionDefaults();
    saveSongState();
    return true;
}

void WorkspaceShellComponent::captureUndoStateIfChanged(const juce::String& beforeSnapshot)
{
    const auto afterSnapshot = createSongSnapshot();
    if (beforeSnapshot.isEmpty() || afterSnapshot.isEmpty() || beforeSnapshot == afterSnapshot)
        return;

    undoSnapshots.push_back(beforeSnapshot);
    if (undoSnapshots.size() > static_cast<size_t>(maxUndoDepth))
        undoSnapshots.erase(undoSnapshots.begin());

    redoSnapshots.clear();
    updateUndoRedoButtonState();
}

void WorkspaceShellComponent::performUndo()
{
    if (undoSnapshots.empty())
    {
        interactionStatus.setText("Undo not available", juce::dontSendNotification);
        updateUndoRedoButtonState();
        return;
    }

    const auto currentSnapshot = createSongSnapshot();
    const auto targetSnapshot = undoSnapshots.back();
    undoSnapshots.pop_back();

    if (restoreSongFromSnapshot(targetSnapshot))
    {
        if (currentSnapshot.isNotEmpty())
        {
            redoSnapshots.push_back(currentSnapshot);
            if (redoSnapshots.size() > static_cast<size_t>(maxUndoDepth))
                redoSnapshots.erase(redoSnapshots.begin());
        }
        refreshTimelineData();
        populateTheoryObjectSelector();
        populateTheoryFieldsForCurrentSelection();
        interactionStatus.setText("Undid theory action | " + summarizeSongState(), juce::dontSendNotification);
    }
    else
    {
        interactionStatus.setText("Undo failed: invalid snapshot", juce::dontSendNotification);
    }

    updateUndoRedoButtonState();
}

void WorkspaceShellComponent::performRedo()
{
    if (redoSnapshots.empty())
    {
        interactionStatus.setText("Redo not available", juce::dontSendNotification);
        updateUndoRedoButtonState();
        return;
    }

    const auto currentSnapshot = createSongSnapshot();
    const auto targetSnapshot = redoSnapshots.back();
    redoSnapshots.pop_back();

    if (restoreSongFromSnapshot(targetSnapshot))
    {
        if (currentSnapshot.isNotEmpty())
        {
            undoSnapshots.push_back(currentSnapshot);
            if (undoSnapshots.size() > static_cast<size_t>(maxUndoDepth))
                undoSnapshots.erase(undoSnapshots.begin());
        }
        refreshTimelineData();
        populateTheoryObjectSelector();
        populateTheoryFieldsForCurrentSelection();
        interactionStatus.setText("Redid theory action | " + summarizeSongState(), juce::dontSendNotification);
    }
    else
    {
        interactionStatus.setText("Redo failed: invalid snapshot", juce::dontSendNotification);
    }

    updateUndoRedoButtonState();
}

void WorkspaceShellComponent::updateUndoRedoButtonState()
{
    const auto canUndo = !undoSnapshots.empty();
    const auto canRedo = !redoSnapshots.empty();

    undoTheoryButton.setEnabled(canUndo);
    redoTheoryButton.setEnabled(canRedo);

    undoTheoryButton.setColour(
        juce::TextButton::buttonColourId,
        canUndo ? juce::Colour(0xff3d4654) : juce::Colour(0xff2a2d32));
    redoTheoryButton.setColour(
        juce::TextButton::buttonColourId,
        canRedo ? juce::Colour(0xff3d4654) : juce::Colour(0xff2a2d32));

    undoTheoryButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.9f));
    redoTheoryButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.9f));
}

juce::String WorkspaceShellComponent::summarizeSongState() const
{
    if (!songState.isValid())
        return "No song state";

    const auto progressionCount = static_cast<int>(songState.getProgressions().size());
    const auto sectionCount = static_cast<int>(songState.getSections().size());
    const auto transitionCount = static_cast<int>(songState.getTransitions().size());

    return "P:" + juce::String(progressionCount)
         + " S:" + juce::String(sectionCount)
         + " T:" + juce::String(transitionCount);
}

void WorkspaceShellComponent::handleTimelineTheoryAction(TheoryMenuTarget target, int actionId, const juce::String& actionName)
{
    if (target == TheoryMenuTarget::progression)
    {
        handleProgressionAction(selectedProgressionId, actionId);
        return;
    }

    seedSongStateIfNeeded();
    ensureSelectionDefaults();
    const auto beforeSnapshot = createSongSnapshot();
    const auto actionResult = runTheoryAction(target, actionId, actionName);
    captureUndoStateIfChanged(beforeSnapshot);
    saveSongState();
    ensureSelectionDefaults();
    refreshTimelineData();
    populateTheoryObjectSelector();
    populateTheoryFieldsForCurrentSelection();
    updateUndoRedoButtonState();

    const auto message = actionResult + " | " + summarizeSongState();
    interactionStatus.setText(message, juce::dontSendNotification);
    DBG(message);
}

juce::String WorkspaceShellComponent::runTheoryAction(TheoryMenuTarget target, int actionId, const juce::String& actionName)
{
    switch (target)
    {
        case TheoryMenuTarget::section:
            return runSectionAction(actionId);
        case TheoryMenuTarget::chord:
            return runChordAction(actionId);
        case TheoryMenuTarget::note:
            return runNoteAction(actionId);
        case TheoryMenuTarget::progression:
            return "Progression actions are handled via progression dispatch";
        case TheoryMenuTarget::historyBuffer:
            return runHistoryBufferAction(actionId);
    }

    return "Unknown action: " + actionName;
}

juce::String WorkspaceShellComponent::runSectionAction(int actionId)
{
    if (actionId == sectionEditTheory || actionId == sectionSetRepeatPattern || actionId == sectionAddTransitionAnchor)
    {
        openTheoryEditor(TheoryMenuTarget::section, actionId, "Section Action");
        return "Section editor opened in WORK panel";
    }

    auto sections = songState.getSections();
    if (sections.empty())
        return "Section action skipped: no sections available";

    ensureSelectionDefaults();
    if (auto selectedSection = getSelectedSection())
        selectedSectionId = selectedSection->getId();

    if (actionId == sectionConflictCheck)
    {
        int missingProgressionRefs = 0;
        for (const auto& candidateSection : sections)
        {
            for (const auto& progressionRef : candidateSection.getProgressionRefs())
            {
                if (!songState.findProgressionById(progressionRef.getProgressionId()).has_value())
                    ++missingProgressionRefs;
            }
        }

        if (missingProgressionRefs == 0)
            return "Section conflict check passed";

        return "Section conflict check found " + juce::String(missingProgressionRefs) + " missing progression reference(s)";
    }

    auto selectedSection = getSelectedSection();
    if (!selectedSection.has_value())
        return "Section action skipped: no section selected";

    if (actionId == sectionRename)
    {
        juce::AlertWindow window("Rename Section", "Enter new section name:", juce::AlertWindow::NoIcon);
        window.addTextEditor("name", selectedSection->getName(), "Name");
        window.addButton("Rename", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            const auto newName = window.getTextEditorContents("name").trim();
            if (newName.isNotEmpty())
            {
                selectedSection->setName(newName);
                refreshTheoryLanes();
                return "Section renamed to " + newName;
            }
        }
        return "Rename cancelled";
    }

    if (actionId == sectionDelete)
    {
        songState.removeSection(selectedSection->getId());
        selectedSectionId = {};
        refreshTheoryLanes();
        return "Section deleted";
    }

    if (actionId == sectionDuplicate)
    {
        auto copy = model::Section::create(selectedSection->getName() + " (Copy)", selectedSection->getRepeatCount());
        copy.setColor(selectedSection->getColor());
        copy.setTimeSigNumerator(selectedSection->getTimeSigNumerator());
        copy.setTimeSigDenominator(selectedSection->getTimeSigDenominator());
        for (const auto& ref : selectedSection->getProgressionRefs())
            copy.addProgressionRef(ref);
        songState.addSection(copy);
        selectedSectionId = copy.getId();
        refreshTheoryLanes();
        return "Section duplicated";
    }

    if (actionId == sectionSetColor)
    {
        juce::AlertWindow window("Set Section Color", "Enter a hex color (e.g. #FF6B35):", juce::AlertWindow::NoIcon);
        window.addTextEditor("color", selectedSection->getColor().isEmpty() ? "#888888" : selectedSection->getColor(), "Hex Color");
        window.addButton("Set", 1);
        window.addButton("Clear", 2);
        window.addButton("Cancel", 0);

        const int result = window.runModalLoop();
        if (result == 1)
        {
            selectedSection->setColor(window.getTextEditorContents("color").trim());
            refreshTheoryLanes();
            return "Section color updated";
        }
        if (result == 2)
        {
            selectedSection->setColor({});
            refreshTheoryLanes();
            return "Section color cleared";
        }
        return "Set color cancelled";
    }

    if (actionId == sectionSetTimeSig)
    {
        juce::AlertWindow window("Set Time Signature", "Enter numerator and denominator:", juce::AlertWindow::NoIcon);
        window.addTextEditor("num", juce::String(selectedSection->getTimeSigNumerator()), "Numerator");
        window.addTextEditor("den", juce::String(selectedSection->getTimeSigDenominator()), "Denominator");
        window.addButton("Set", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            const int num = juce::jlimit(1, 32, parseIntOr(window.getTextEditorContents("num"), 4));
            const int den = juce::jlimit(1, 32, parseIntOr(window.getTextEditorContents("den"), 4));
            selectedSection->setTimeSigNumerator(num);
            selectedSection->setTimeSigDenominator(den);
            syncTimeSignaturesToEdit();
            refreshTheoryLanes();
            return "Time signature set to " + juce::String(num) + "/" + juce::String(den);
        }
        return "Set time signature cancelled";
    }

    if (actionId == sectionJumpTo)
    {
        interactionStatus.setText("Jump to section: " + selectedSection->getName(), juce::dontSendNotification);
        return "Jumped to section " + selectedSection->getName();
    }

    if (actionId == sectionExportMidi)
    {
        interactionStatus.setText("Export MIDI: feature coming soon", juce::dontSendNotification);
        return "Export MIDI not yet implemented for sections";
    }

    return "Section action unavailable for current selection";
}

juce::String WorkspaceShellComponent::runChordAction(int actionId)
{
    if (actionId == chordEdit || actionId == chordSubstitution || actionId == chordSetFunction)
    {
        openTheoryEditor(TheoryMenuTarget::chord, actionId, "Chord Action");
        return "Chord editor opened in WORK panel";
    }

    auto progression = getSelectedProgression();
    if (!progression.has_value())
        return "Chord action skipped: no selected progression available";

    auto chords = progression->getChords();

    if (chords.empty())
    {
        auto bootstrapChord = model::Chord::create("Cmaj7", "major7", 60);
        bootstrapChord.setFunction("T");
        bootstrapChord.addNote(model::Note::create(60, 0.8f, 0.0, 1.0, 1));
        bootstrapChord.addNote(model::Note::create(64, 0.75f, 0.0, 1.0, 1));
        bootstrapChord.addNote(model::Note::create(67, 0.72f, 0.0, 1.0, 1));
        progression->addChord(bootstrapChord);
        chords = progression->getChords();
        selectedChordId = bootstrapChord.getId();
    }

    auto chord = getSelectedChord();
    if (!chord.has_value() && !chords.empty())
    {
        selectedChordId = chords.front().getId();
        chord = getSelectedChord();
    }

    if (!chord.has_value())
        return "Chord action skipped: no selected chord available";

    if (actionId == chordScopeOccurrence || actionId == chordScopeRepeat
        || actionId == chordScopeSelectedRepeats || actionId == chordScopeAllRepeats)
    {
        auto section = getSelectedSection();
        if (!section.has_value())
            return "Scope change skipped: no sections available";

        auto refs = section->getProgressionRefs();
        if (refs.empty())
        {
            section->addProgressionRef(model::SectionProgressionRef::create(progression->getId(), 0, ""));
            refs = section->getProgressionRefs();
        }

        if (refs.empty())
            return "Scope change skipped: progression mapping unavailable";

        const auto selectedProgressionIdLocal = progression->getId();
        const auto selectedBaseProgressionId = progression->getVariantOf().isNotEmpty()
                                                   ? progression->getVariantOf()
                                                   : selectedProgressionIdLocal;
        const auto currentRepeatIndex = juce::jmax(1, theoryCurrentRepeatIndex);
        const auto currentRepeatIndices = juce::String(currentRepeatIndex);
        const auto selectedRepeatIndices = normalizeRepeatIndices(
            theorySelectedRepeatIndices.isNotEmpty() ? theorySelectedRepeatIndices : currentRepeatIndices,
            currentRepeatIndices);

        auto refMatchesSelection = [this, &selectedProgressionIdLocal, &selectedBaseProgressionId](const model::SectionProgressionRef& candidate)
        {
            const auto candidateProgressionId = candidate.getProgressionId();
            if (candidateProgressionId == selectedProgressionIdLocal || candidateProgressionId == selectedBaseProgressionId)
                return true;

            if (auto candidateProgression = songState.findProgressionById(candidateProgressionId))
                return candidateProgression->getVariantOf() == selectedBaseProgressionId;

            return false;
        };

        int targetRefIndex = -1;
        for (size_t i = 0; i < refs.size(); ++i)
        {
            if (refMatchesSelection(refs[i]))
            {
                targetRefIndex = static_cast<int>(i);
                break;
            }
        }

        if (targetRefIndex < 0)
        {
            // New ref created with default scope="all"; the actionId blocks below
            // immediately override it with the requested scope.
            const auto nextOrderIndex = static_cast<int>(refs.size());
            section->addProgressionRef(model::SectionProgressionRef::create(selectedProgressionIdLocal, nextOrderIndex, ""));
            refs = section->getProgressionRefs();
            targetRefIndex = static_cast<int>(refs.size()) - 1;
        }

        if (actionId == chordScopeAllRepeats)
        {
            for (auto& sectionRef : refs)
            {
                if (!refMatchesSelection(sectionRef))
                    continue;

                if (auto currentProgression = songState.findProgressionById(sectionRef.getProgressionId()))
                {
                    if (currentProgression->getVariantOf().isNotEmpty())
                        sectionRef.setProgressionId(currentProgression->getVariantOf());
                }

                sectionRef.setVariantName("");
                sectionRef.setRepeatScope("all");
                sectionRef.setRepeatSelection("all");
                sectionRef.setRepeatIndices("");
            }

            theoryScope = "allRepeats";
            return "Theory scope set to all repeats (base progression)";
        }

        auto variant = model::Progression::create(
            progression->getName() + " Variant " + juce::String(static_cast<int>(songState.getProgressions().size()) + 1),
            progression->getKey(),
            progression->getMode());
        variant.setVariantOf(selectedBaseProgressionId);
        variant.setLengthBeats(progression->getLengthBeats());

        for (const auto& sourceChord : progression->getChords())
        {
            auto copiedChord = model::Chord::create(sourceChord.getSymbol(), sourceChord.getQuality(), sourceChord.getRootMidi());
            copiedChord.setName(sourceChord.getName());
            copiedChord.setFunction(sourceChord.getFunction());
            copiedChord.setStartBeats(sourceChord.getStartBeats());
            copiedChord.setDurationBeats(sourceChord.getDurationBeats());
            copiedChord.setTension(sourceChord.getTension());

            for (const auto& sourceNote : sourceChord.getNotes())
                copiedChord.addNote(model::Note::create(sourceNote.getPitch(),
                                                        sourceNote.getVelocity(),
                                                        sourceNote.getStartBeats(),
                                                        sourceNote.getDurationBeats(),
                                                        sourceNote.getChannel()));

            variant.addChord(copiedChord);
        }

        songState.addProgression(variant);
        refs[static_cast<size_t>(targetRefIndex)].setProgressionId(variant.getId());

        if (actionId == chordScopeOccurrence)
        {
            refs[static_cast<size_t>(targetRefIndex)].setVariantName("occurrenceOnly");
            refs[static_cast<size_t>(targetRefIndex)].setRepeatScope("occurrence");
            refs[static_cast<size_t>(targetRefIndex)].setRepeatSelection("single");
            refs[static_cast<size_t>(targetRefIndex)].setRepeatIndices(currentRepeatIndices);
            theoryScope = "thisOccurrence";
            return "Theory scope set to this occurrence (variant generated)";
        }

        // Guard: selectedRepeats requires an explicit selection — never fall silently through to repeat 1.
        if (actionId == chordScopeSelectedRepeats && theorySelectedRepeatIndices.trim().isEmpty())
            return "Scope change skipped: no repeat indices selected. Set repeat indices in the theory editor first.";

        if (actionId == chordScopeSelectedRepeats)
        {
            for (auto& sectionRef : refs)
            {
                if (!refMatchesSelection(sectionRef))
                    continue;

                sectionRef.setProgressionId(variant.getId());
                sectionRef.setVariantName("selectedRepeats");
                sectionRef.setRepeatScope("selectedRepeats");
                sectionRef.setRepeatSelection("selected");
                sectionRef.setRepeatIndices(selectedRepeatIndices);
            }

            theorySelectedRepeatIndices = selectedRepeatIndices;
            theoryScope = "selectedRepeats";
            return "Theory scope set to selected repeats (variant generated)";
        }

        refs[static_cast<size_t>(targetRefIndex)].setVariantName("thisRepeat");
        refs[static_cast<size_t>(targetRefIndex)].setRepeatScope("repeat");
        refs[static_cast<size_t>(targetRefIndex)].setRepeatSelection("current");
        refs[static_cast<size_t>(targetRefIndex)].setRepeatIndices(currentRepeatIndices);
        theoryScope = "thisRepeat";
        return "Theory scope set to this repeat (variant generated)";
    }

    if (actionId >= chordSnapWholeBar && actionId <= chordSnapThirtySecond)
    {
        juce::String snapValue = "1/16";

        if (actionId == chordSnapWholeBar) snapValue = "bar";
        else if (actionId == chordSnapHalfBar) snapValue = "1/2";
        else if (actionId == chordSnapBeat) snapValue = "1/4";
        else if (actionId == chordSnapHalfBeat) snapValue = "halfBeat";
        else if (actionId == chordSnapEighth) snapValue = "1/8";
        else if (actionId == chordSnapSixteenth) snapValue = "1/16";
        else if (actionId == chordSnapThirtySecond) snapValue = "1/32";

        theorySnap = snapValue;
        return "Theory snap set to " + snapValue;
    }

    if (actionId == chordDelete)
    {
        if (!progression.has_value())
            return "Chord delete skipped: no progression selected";

        progression->removeChord(chord->getId());
        selectedChordId = {};
        refreshTheoryLanes();
        return "Chord deleted";
    }

    if (actionId == chordDuplicate)
    {
        if (!progression.has_value())
            return "Chord duplicate skipped: no progression selected";

        auto copy = model::Chord::create(chord->getSymbol(), chord->getQuality(), chord->getRootMidi());
        copy.setName(chord->getName());
        copy.setFunction(chord->getFunction());
        copy.setStartBeats(chord->getStartBeats() + chord->getDurationBeats());
        copy.setDurationBeats(chord->getDurationBeats());
        copy.setTension(chord->getTension());
        for (const auto& note : chord->getNotes())
            copy.addNote(model::Note::create(note.getPitch(), note.getVelocity(), note.getStartBeats(), note.getDurationBeats(), note.getChannel()));
        progression->addChord(copy);
        selectedChordId = copy.getId();
        refreshTheoryLanes();
        return "Chord duplicated";
    }

    if (actionId == chordInsertBefore || actionId == chordInsertAfter)
    {
        if (!progression.has_value())
            return "Chord insert skipped: no progression selected";

        const double insertAt = actionId == chordInsertBefore
                                    ? chord->getStartBeats()
                                    : chord->getStartBeats() + chord->getDurationBeats();
        auto blank = model::Chord::create("?", "unknown", 60);
        blank.setStartBeats(insertAt);
        blank.setDurationBeats(chord->getDurationBeats());
        progression->addChord(blank);
        selectedChordId = blank.getId();
        refreshTheoryLanes();
        return juce::String(actionId == chordInsertBefore ? "Chord inserted before" : "Chord inserted after");
    }

    if (actionId == chordNudgeLeft || actionId == chordNudgeRight)
    {
        if (!progression.has_value())
            return "Chord nudge skipped: no progression selected";

        const double snapBeats = theorySnap == "bar" ? 4.0
                               : theorySnap == "1/2" ? 2.0
                               : theorySnap == "1/4" ? 1.0
                               : theorySnap == "halfBeat" ? 0.5
                               : theorySnap == "1/8" ? 0.5
                               : theorySnap == "1/16" ? 0.25
                               : theorySnap == "1/32" ? 0.125
                               : 1.0;
        const double delta = (actionId == chordNudgeLeft) ? -snapBeats : snapBeats;
        chord->setStartBeats(juce::jmax(0.0, chord->getStartBeats() + delta));
        refreshTheoryLanes();
        return juce::String(actionId == chordNudgeLeft ? "Chord nudged left" : "Chord nudged right");
    }

    if (actionId == chordSetDuration)
    {
        if (!progression.has_value())
            return "Chord duration skipped: no progression selected";

        juce::AlertWindow window("Set Chord Duration", "Enter duration in beats:", juce::AlertWindow::NoIcon);
        window.addTextEditor("dur", juce::String(chord->getDurationBeats()), "Beats");
        window.addButton("Set", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            const double dur = juce::jmax(0.0625, window.getTextEditorContents("dur").trim().getDoubleValue());
            chord->setDurationBeats(dur);
            refreshTheoryLanes();
            return "Chord duration set to " + juce::String(dur) + " beats";
        }
        return "Set duration cancelled";
    }

    if (actionId == chordInvertVoicing)
    {
        for (auto& note : chord->getNotes())
            note.setPitch(note.getPitch() > 60 ? note.getPitch() - 12 : note.getPitch() + 12);
        refreshTheoryLanes();
        return "Chord voicing inverted";
    }

    if (actionId == chordSendToGridRoll)
    {
        interactionStatus.setText("Opening chord in Grid Roll...", juce::dontSendNotification);
        switchWorkTab(1);
        return "Chord sent to Grid Roll";
    }

    return "Chord action unavailable for current selection";
}

juce::String WorkspaceShellComponent::runNoteAction(int actionId)
{
    if (actionId == noteEditTheory)
    {
        openTheoryEditor(TheoryMenuTarget::note, actionId, "Note Action");
        return "Note editor opened in WORK panel";
    }

    auto progression = getSelectedProgression();
    if (!progression.has_value())
        return "Note action skipped: no selected progression available";

    auto chord = getSelectedChord();
    if (!chord.has_value())
        return "Note action skipped: no selected chord available";

    auto notes = chord->getNotes();

    if (notes.empty())
    {
        chord->addNote(model::Note::create(chord->getRootMidi(), 0.8f, 0.0, 1.0, 1));
        notes = chord->getNotes();
    }

    if (notes.empty())
        return "Note action skipped: unable to build note context";

    auto note = getSelectedNote();
    if (!note.has_value())
    {
        selectedNoteId = notes.front().getId();
        note = getSelectedNote();
    }

    if (!note.has_value())
        return "Note action skipped: no selected note available";

    if (actionId == noteQuantizeScale)
    {
        const auto quantizedPitch = quantizeToCMajor(note->getPitch());
        note->setPitch(quantizedPitch);
        return "Lead note quantized to C-major tone " + chordRootNameForMidi(quantizedPitch);
    }

    if (actionId == noteConvertToChord)
    {
        if (notes.size() < 3)
        {
            chord->addNote(model::Note::create(chord->getRootMidi() + 4, 0.74f, 0.0, 1.0, 1));
            chord->addNote(model::Note::create(chord->getRootMidi() + 7, 0.70f, 0.0, 1.0, 1));
            notes = chord->getNotes();
        }

        std::vector<int> pitches;
        for (const auto& sourceNote : notes)
            pitches.push_back(sourceNote.getPitch());

        std::sort(pitches.begin(), pitches.end());
        const auto root = pitches.front();
        const auto third = pitches[juce::jmin(1, static_cast<int>(pitches.size() - 1))];
        const auto interval = (third - root + 12) % 12;
        const bool minorTriad = (interval == 3);

        const auto rootName = chordRootNameForMidi(root);
        const auto symbol = rootName + (minorTriad ? "m" : "maj");

        chord->setRootMidi(root);
        chord->setSymbol(symbol);
        chord->setName(symbol);
        chord->setQuality(minorTriad ? "minorTriad" : "majorTriad");

        return "Notes converted to chord symbol " + symbol;
    }

    if (actionId == noteDeriveProgression)
    {
        auto derived = model::Progression::create(
            "Derived " + juce::String(static_cast<int>(songState.getProgressions().size()) + 1),
            progression->getKey(),
            progression->getMode());
        derived.setLengthBeats(progression->getLengthBeats());

        auto derivedChord = model::Chord::create(chord->getSymbol(), chord->getQuality(), chord->getRootMidi());
        derivedChord.setName(chord->getName());
        derivedChord.setFunction(chord->getFunction());
        derivedChord.setDurationBeats(4.0);
        for (const auto& sourceNote : chord->getNotes())
            derivedChord.addNote(model::Note::create(sourceNote.getPitch(),
                                                     sourceNote.getVelocity(),
                                                     sourceNote.getStartBeats(),
                                                     sourceNote.getDurationBeats(),
                                                     sourceNote.getChannel()));

        derived.addChord(derivedChord);
        songState.addProgression(derived);

        auto sections = songState.getSections();
        if (!sections.empty())
        {
            auto section = getSelectedSection().value_or(sections.front());
            section.addProgressionRef(model::SectionProgressionRef::create(
                derived.getId(),
                static_cast<int>(section.getProgressionRefs().size()),
                "derived"));
        }

        return "Derived progression created from note selection";
    }

    return "Note action unavailable for current selection";
}

juce::String WorkspaceShellComponent::runProgressionAction(int actionId, const juce::String& targetProgressionId)
{
    if (actionId == progressionCreateCandidate || actionId == progressionAnnotateKeyMode)
    {
        openTheoryEditor(TheoryMenuTarget::progression, actionId, "Progression Action");
        return "Progression editor opened in WORK panel";
    }

    auto progression = songState.findProgressionById(targetProgressionId);
    if (!progression.has_value())
        return "Progression action skipped: progression not found";

    if (actionId == progressionGrabSampler)
    {
        if (grabSamplerQueue == nullptr)
            return "Sampler queue unavailable";

        const auto slot = grabSamplerQueue->loadProgression(
            progression->getId(),
            progression->getName(),
            0.75f);

        bool appliedCoupledAudio = false;
        if (pendingGrabCoupledAudio != nullptr
            && pendingGrabCoupledSampleRate > 0.0
            && pendingGrabCoupledAudio->getNumSamples() > 0)
        {
            appliedCoupledAudio = grabSamplerQueue->setSlotAudioBuffer(
                slot,
                *pendingGrabCoupledAudio,
                pendingGrabCoupledSampleRate);
        }

        if (!appliedCoupledAudio)
        {
            // Fallback preview for non-grab entry points.
            constexpr double sr = 44100.0;
            const auto secPerBeat = 60.0 / juce::jmax(1.0, songState.getBpm());

            double totalBeats = 0.0;
            for (const auto& chord : progression->getChords())
                totalBeats += juce::jmax(0.25, chord.getDurationBeats());
            totalBeats = juce::jmax(1.0, totalBeats);

            const int totalSamples = static_cast<int>(std::ceil(totalBeats * secPerBeat * sr));
            juce::AudioBuffer<float> preview(2, juce::jmax(1, totalSamples));
            preview.clear();

            double beatCursor = 0.0;
            for (const auto& chord : progression->getChords())
            {
                const auto chordBeats = juce::jmax(0.25, chord.getDurationBeats());
                const int startSample = juce::jlimit(0, preview.getNumSamples() - 1,
                    static_cast<int>(std::floor(beatCursor * secPerBeat * sr)));
                const int endSample = juce::jlimit(startSample + 1, preview.getNumSamples(),
                    static_cast<int>(std::ceil((beatCursor + chordBeats) * secPerBeat * sr)));

                std::vector<int> pitches;
                for (const auto& note : chord.getNotes())
                    pitches.push_back(note.getPitch());
                if (pitches.empty())
                    pitches.push_back(chord.getRootMidi());

                std::vector<double> phase(static_cast<size_t>(pitches.size()), 0.0);
                std::vector<double> inc(static_cast<size_t>(pitches.size()), 0.0);
                for (size_t i = 0; i < pitches.size(); ++i)
                    inc[i] = juce::MathConstants<double>::twoPi
                             * juce::MidiMessage::getMidiNoteInHertz(pitches[i]) / sr;

                for (int s = startSample; s < endSample; ++s)
                {
                    const double t = static_cast<double>(s - startSample) / juce::jmax(1, endSample - startSample);
                    const float env = static_cast<float>(std::pow(1.0 - juce::jmin(0.999, t), 0.45));
                    float sample = 0.0f;

                    for (size_t i = 0; i < pitches.size(); ++i)
                    {
                        sample += static_cast<float>(std::sin(phase[i]));
                        phase[i] += inc[i];
                        if (phase[i] >= juce::MathConstants<double>::twoPi)
                            phase[i] -= juce::MathConstants<double>::twoPi;
                    }

                    sample /= static_cast<float>(juce::jmax(1, static_cast<int>(pitches.size())));
                    sample *= env * 0.23f;
                    preview.addSample(0, s, sample);
                    preview.addSample(1, s, sample);
                }

                beatCursor += chordBeats;
            }

            grabSamplerQueue->setSlotAudioBuffer(slot, preview, sr);
        }

        updateInPanelQueueView();

        return "Progression moved to sampler queue slot " + juce::String(slot + 1);
    }

    if (actionId == progressionTagTransition)
    {
        auto sections = songState.getSections();
        if (sections.size() < 2)
            return "Transition tagging skipped: requires at least two sections";

        auto transition = model::Transition::create(sections.front().getId(), sections[1].getId(), "historyTagged");
        transition.setName("Tagged Transition " + juce::String(static_cast<int>(songState.getTransitions().size()) + 1));
        transition.setTargetTension(6);
        songState.addTransition(transition);
        return "Progression tagged as transition material";
    }

    if (actionId == progressionEditIdentity)
    {
        openTheoryEditor(TheoryMenuTarget::progression, progressionAnnotateKeyMode, "Edit Progression Identity");
        return "Progression identity editor opened";
    }

    if (actionId == progressionForkVariant)
    {
        auto variant = model::Progression::create(
            progression->getName() + " Variant " + juce::String(static_cast<int>(songState.getProgressions().size()) + 1),
            progression->getKey(),
            progression->getMode());
        variant.setVariantOf(progression->getId());
        variant.setLengthBeats(progression->getLengthBeats());

        for (const auto& sourceChord : progression->getChords())
        {
            auto cloned = model::Chord::create(sourceChord.getSymbol(), sourceChord.getQuality(), sourceChord.getRootMidi());
            cloned.setName(sourceChord.getName());
            cloned.setFunction(sourceChord.getFunction());
            cloned.setSource(sourceChord.getSource());
            cloned.setStartBeats(sourceChord.getStartBeats());
            cloned.setDurationBeats(sourceChord.getDurationBeats());
            cloned.setTension(sourceChord.getTension());
            for (const auto& sourceNote : sourceChord.getNotes())
                cloned.addNote(model::Note::create(sourceNote.getPitch(),
                                                   sourceNote.getVelocity(),
                                                   sourceNote.getStartBeats(),
                                                   sourceNote.getDurationBeats(),
                                                   sourceNote.getChannel()));
            variant.addChord(cloned);
        }

        songState.addProgression(variant);
        selectedProgressionId = variant.getId();
        return "Forked progression as variant";
    }

    if (actionId == progressionAddChord)
    {
        const auto existing = progression->getChords();
        double nextStart = 0.0;
        for (const auto& chord : existing)
        {
            const auto duration = chord.getDurationBeats() > 0.0 ? chord.getDurationBeats() : 1.0;
            nextStart += duration;
        }

        auto newChord = model::Chord::create("Cmaj7", "major7", 60);
        if (!existing.empty())
        {
            const auto& last = existing.back();
            const auto newRoot = juce::jlimit(0, 127, last.getRootMidi() + 5);
            newChord.setRootMidi(newRoot);
            newChord.setSymbol(cycleChordSymbol(last.getSymbol()));
            newChord.setName(newChord.getSymbol());
            newChord.setQuality(last.getQuality());
            newChord.setFunction(last.getFunction());
        }
        newChord.setStartBeats(nextStart);
        newChord.setDurationBeats(4.0);
        progression->addChord(newChord);
        return "Added chord to progression";
    }

    if (actionId == progressionClearChords)
    {
        auto tree = progression->valueTree();
        for (int i = tree.getNumChildren(); --i >= 0;)
        {
            if (tree.getChild(i).hasType(model::Schema::chordType))
                tree.removeChild(i, nullptr);
        }
        return "Cleared progression chords";
    }

    if (actionId == progressionAssignToSection)
    {
        auto sections = songState.getSections();
        if (sections.empty())
        {
            auto generated = model::Section::create("Verse", 4);
            generated.addProgressionRef(model::SectionProgressionRef::create(progression->getId(), 0, ""));
            songState.addSection(generated);
            selectedSectionId = generated.getId();
            return "Created section and assigned progression";
        }

        auto section = getSelectedSection().value_or(sections.front());
        auto refs = section.getProgressionRefs();
        for (const auto& ref : refs)
        {
            if (ref.getProgressionId() == progression->getId())
                return "Progression already assigned to section";
        }

        section.addProgressionRef(model::SectionProgressionRef::create(
            progression->getId(),
            static_cast<int>(refs.size()),
            ""));
        selectedSectionId = section.getId();
        return "Assigned progression to section";
    }

    if (actionId == progressionSubAllDominants)
    {
        int substitutions = 0;
        for (auto chord : progression->getChords())
        {
            const auto dominantByFunction = chord.getFunction().containsIgnoreCase("D");
            const auto dominantBySymbol = chord.getSymbol().containsChar('7');
            if (!dominantByFunction && !dominantBySymbol)
                continue;

            const auto transposedRoot = juce::jlimit(0, 127, chord.getRootMidi() + 6);
            chord.setRootMidi(transposedRoot);
            chord.setSymbol(transposeChordSymbolRoot(chord.getSymbol(), 6));
            chord.setName(chord.getSymbol());
            ++substitutions;
        }

        return substitutions > 0
                   ? "Applied dominant substitutions: " + juce::String(substitutions)
                   : "No dominant chords found to substitute";
    }

    if (actionId == progressionTransposeToKey)
    {
        const auto fromPc = pitchClassForKeyName(progression->getKey());
        const auto toPc = pitchClassForKeyName(songState.getSessionKey());
        const auto semitones = ((toPc - fromPc) + 12) % 12;

        for (auto chord : progression->getChords())
        {
            chord.setRootMidi(juce::jlimit(0, 127, chord.getRootMidi() + semitones));
            chord.setSymbol(transposeChordSymbolRoot(chord.getSymbol(), semitones));
            chord.setName(chord.getSymbol());

            for (auto note : chord.getNotes())
                note.setPitch(juce::jlimit(0, 127, note.getPitch() + semitones));
        }

        progression->setKey(songState.getSessionKey());
        progression->setMode(songState.getSessionMode());
        return "Transposed progression to session key";
    }

    if (actionId == progressionSaveAsTemplate)
    {
        juce::DynamicObject::Ptr payload = new juce::DynamicObject();
        payload->setProperty("id", progression->getId());
        payload->setProperty("name", progression->getName());
        payload->setProperty("key", progression->getKey());
        payload->setProperty("mode", progression->getMode());
        payload->setProperty("lengthBeats", progression->getLengthBeats());

        juce::Array<juce::var> chordArray;
        for (const auto& chord : progression->getChords())
        {
            juce::DynamicObject::Ptr chordObj = new juce::DynamicObject();
            chordObj->setProperty("symbol", chord.getSymbol());
            chordObj->setProperty("quality", chord.getQuality());
            chordObj->setProperty("rootMidi", chord.getRootMidi());
            chordObj->setProperty("durationBeats", chord.getDurationBeats());
            chordObj->setProperty("function", chord.getFunction());
            chordArray.add(juce::var(chordObj.get()));
        }
        payload->setProperty("chords", chordArray);

        auto targetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("SETLE")
            .getChildFile("templates")
            .getChildFile("progressions");
        targetDir.createDirectory();

        auto safeName = progression->getName();
        safeName = safeName.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
        safeName = safeName.replaceCharacter(' ', '_');
        if (safeName.isEmpty())
            safeName = progression->getId().substring(0, 8);

        const auto file = targetDir.getChildFile(safeName + ".setle-prg");
        const auto serialized = juce::JSON::toString(juce::var(payload.get()), true);
        if (!file.replaceWithText(serialized))
            return "Failed to save progression template";

        return "Saved progression template: " + file.getFileName();
    }

    return "Progression action unavailable for current selection";
}

juce::String WorkspaceShellComponent::runHistoryBufferAction(int actionId)
{
    if (historyBuffer == nullptr)
        return "History buffer unavailable";

    if (actionId == historyGrab4)
    {
        grabFromBuffer(4);
        return "Grab requested: last 4 beats";
    }

    if (actionId == historyGrab8)
    {
        grabFromBuffer(8);
        return "Grab requested: last 8 beats";
    }

    if (actionId == historyGrab16)
    {
        grabFromBuffer(16);
        return "Grab requested: last 16 beats";
    }

    if (actionId == historyGrabCustom)
    {
        juce::AlertWindow window("Grab Custom Beats", "Enter beat count to grab", juce::AlertWindow::NoIcon);
        window.addTextEditor("beats", "8", "Beats");
        window.addButton("Grab", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            const auto beats = juce::jlimit(1, 128, parseIntOr(window.getTextEditorContents("beats"), 8));
            grabFromBuffer(beats);
            return "Grab requested: custom " + juce::String(beats) + " beats";
        }

        return "Grab custom cancelled";
    }

    if (actionId == historySetLength)
    {
        juce::AlertWindow window("Set Buffer Length", "Enter max buffer length in beats", juce::AlertWindow::NoIcon);
        window.addTextEditor("length", juce::String(historyBuffer->getMaxBeats()), "Beats (1-128)");
        window.addButton("Set", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            const auto beats = juce::jlimit(1, 128, parseIntOr(window.getTextEditorContents("length"), historyBuffer->getMaxBeats()));
            historyBuffer->setMaxBeats(beats);
            setle::state::AppPreferences::get().setHistoryBufferMaxBeats(beats);
            return "History buffer length set to " + juce::String(beats) + " beats";
        }

        return "Set buffer length cancelled";
    }

    if (actionId == historyChangeSource)
    {
        captureSourceSelector.showPopup();
        return "Capture source selector opened";
    }

    if (actionId == historyClear)
    {
        historyBuffer->clear();
        interactionStatus.setText("History buffer cleared", juce::dontSendNotification);
        return "History buffer cleared";
    }

    if (actionId == historyAutoGrabToggle)
    {
        const bool enabled = !setle::state::AppPreferences::get().getAutoGrabEnabled();
        setle::state::AppPreferences::get().setAutoGrabEnabled(enabled);
        interactionStatus.setText(juce::String("Auto-grab after stop: ") + (enabled ? "ON" : "OFF"), juce::dontSendNotification);
        return juce::String("Auto-grab toggled ") + (enabled ? "on" : "off");
    }

    if (actionId == historySetBpm)
    {
        juce::AlertWindow window("Override Buffer BPM", "Enter BPM (0 = use transport):", juce::AlertWindow::NoIcon);
        window.addTextEditor("bpm", "0", "BPM");
        window.addButton("Set", 1);
        window.addButton("Cancel", 0);

        if (window.runModalLoop() == 1)
        {
            const auto bpm = juce::jlimit(0, 300, parseIntOr(window.getTextEditorContents("bpm"), 0));
            interactionStatus.setText("Buffer BPM override: " + juce::String(bpm == 0 ? "transport" : juce::String(bpm)), juce::dontSendNotification);
            return "Buffer BPM set to " + juce::String(bpm);
        }
        return "Set buffer BPM cancelled";
    }

    if (actionId == historySyncToTransport)
    {
        interactionStatus.setText("Sync to transport: feature coming soon", juce::dontSendNotification);
        return "Sync to transport not yet implemented";
    }

    if (actionId == historyPreviewMode)
    {
        interactionStatus.setText("Preview mode: feature coming soon", juce::dontSendNotification);
        return "Preview mode not yet implemented";
    }

    if (actionId == historyExportMidi)
    {
        interactionStatus.setText("Export buffer as MIDI: feature coming soon", juce::dontSendNotification);
        return "Export buffer as MIDI not yet implemented";
    }

    if (actionId == historyCaptureSingle)
    {
        grabFromBuffer(historyBuffer->getMaxBeats());
        return "Single take captured";
    }

    return "History action unavailable: " + juce::String(actionId);
}

void WorkspaceShellComponent::handleProgressionAction(const juce::String& progressionId, int actionId)
{
    if (progressionId.isEmpty())
    {
        interactionStatus.setText("Progression action skipped: no progression selected", juce::dontSendNotification);
        return;
    }

    selectedProgressionId = progressionId;

    const auto result = runProgressionAction(actionId, progressionId);
    saveSongState();
    refreshTimelineData();
    populateTheoryObjectSelector();
    populateTheoryFieldsForCurrentSelection();

    const auto message = result + " | " + summarizeSongState();
    interactionStatus.setText(message, juce::dontSendNotification);
    DBG(message);
}

void WorkspaceShellComponent::grabFromBuffer(int beats)
{
    pendingGrabCoupledAudio.reset();
    pendingGrabCoupledSampleRate = 0.0;

    if (historyBuffer == nullptr || !historyBuffer->isCapturing())
    {
        interactionStatus.setText("Capture source is Off - select a source first", juce::dontSendNotification);
        return;
    }

    const auto bpm = songState.getBpm();
    const auto captured = historyBuffer->getLastNBeats(bpm, beats);
    const auto coupledAudio = (circularAudioBuffer != nullptr)
                                  ? circularAudioBuffer->getLastNBeats(bpm, beats)
                                  : juce::AudioBuffer<float>();
    const auto coupledSampleRate = (circularAudioBuffer != nullptr)
                                       ? circularAudioBuffer->getSampleRate()
                                       : 0.0;

    if (captured.empty())
    {
        interactionStatus.setText("Buffer is empty - play something first", juce::dontSendNotification);
        return;
    }

    struct TimedPitch
    {
        double timeSeconds {};
        int pitch {};
    };

    std::vector<TimedPitch> notes;
    notes.reserve(captured.size());

    for (const auto& event : captured)
    {
        if (!event.message.isNoteOn())
            continue;

        notes.push_back({ event.wallClockSeconds, event.message.getNoteNumber() });
    }

    if (notes.empty())
    {
        interactionStatus.setText("Buffer has no note-on events in that range", juce::dontSendNotification);
        return;
    }

    std::sort(notes.begin(), notes.end(), [](const auto& a, const auto& b)
    {
        return a.timeSeconds < b.timeSeconds;
    });

    const auto toleranceSeconds = (60.0 / juce::jmax(1.0, bpm)) * 0.15;
    std::vector<std::vector<TimedPitch>> clusters;

    for (const auto& note : notes)
    {
        if (clusters.empty())
        {
            clusters.push_back({ note });
            continue;
        }

        const auto& lastCluster = clusters.back();
        const auto anchor = lastCluster.front().timeSeconds;
        if (std::abs(note.timeSeconds - anchor) <= toleranceSeconds)
            clusters.back().push_back(note);
        else
            clusters.push_back({ note });
    }

    auto progression = model::Progression::create(
        "Grab " + juce::Time::getCurrentTime().toString(false, true, false, true),
        songState.getSessionKey(),
        songState.getSessionMode());

    double startBeat = 0.0;
    for (const auto& cluster : clusters)
    {
        if (cluster.empty())
            continue;

        std::set<int> pitchClasses;
        int minPitch = 127;
        int maxPitch = 0;
        for (const auto& note : cluster)
        {
            pitchClasses.insert(((note.pitch % 12) + 12) % 12);
            minPitch = juce::jmin(minPitch, note.pitch);
            maxPitch = juce::jmax(maxPitch, note.pitch);
        }

        const auto rootMidi = minPitch;
        const auto rootName = chordRootNameForMidi(rootMidi);
        const auto hasMinorThird = pitchClasses.count((rootMidi + 3) % 12) > 0;
        const auto hasMajorThird = pitchClasses.count((rootMidi + 4) % 12) > 0;

        juce::String quality = "cluster";
        juce::String symbol = rootName;
        if (hasMinorThird)
        {
            quality = "minor";
            symbol += "m";
        }
        else if (hasMajorThird)
        {
            quality = "major";
        }

        auto chord = model::Chord::create(symbol, quality, rootMidi);
        chord.setName(symbol);
        chord.setStartBeats(startBeat);
        chord.setDurationBeats(4.0);
        chord.setSource("midi");

        for (const auto& note : cluster)
            chord.addNote(model::Note::create(note.pitch, 0.8f, startBeat, 1.0, 1));

        progression.addChord(chord);
        startBeat += 4.0;
    }

    if (progression.getChords().empty())
    {
        interactionStatus.setText("No chord groups could be derived from buffer", juce::dontSendNotification);
        return;
    }

    songState.addProgression(progression);
    pendingGrabCoupledAudio.reset();
    pendingGrabCoupledSampleRate = 0.0;
    if (coupledAudio.getNumSamples() > 0 && coupledSampleRate > 0.0)
    {
        pendingGrabCoupledAudio = std::make_shared<juce::AudioBuffer<float>>(coupledAudio);
        pendingGrabCoupledSampleRate = coupledSampleRate;
    }
    handleProgressionAction(progression.getId(), progressionGrabSampler);
    pendingGrabCoupledAudio.reset();
    pendingGrabCoupledSampleRate = 0.0;

    interactionStatus.setText("Grabbed " + juce::String(beats)
                                  + " beats -> " + juce::String(static_cast<int>(progression.getChords().size()))
                                  + " chords | " + summarizeSongState(),
                              juce::dontSendNotification);
}

void WorkspaceShellComponent::refreshCaptureSourceSelector(const juce::String& preferredSourceIdentifier)
{
    juce::String desiredIdentifier = preferredSourceIdentifier;
    if (desiredIdentifier.isEmpty())
    {
        const auto selectedIndex = captureSourceSelector.getSelectedItemIndex();
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(captureSourceIdentifiers.size()))
            desiredIdentifier = captureSourceIdentifiers[static_cast<size_t>(selectedIndex)];
        else if (historyBuffer != nullptr)
            desiredIdentifier = historyBuffer->getCurrentSourceIdentifier();
    }

    captureSourceSelector.clear(juce::dontSendNotification);
    captureSourceIdentifiers.clear();

    int itemId = 1;
    captureSourceSelector.addItem("Off", itemId++);
    captureSourceIdentifiers.push_back("");

    const auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& device : devices)
    {
        captureSourceSelector.addItem(device.name, itemId++);
        captureSourceIdentifiers.push_back(device.identifier);
    }

    captureSourceSelector.addItem("All Inputs", itemId);
    captureSourceIdentifiers.push_back("__all__");

    int selectionIndex = 0;
    for (int i = 0; i < static_cast<int>(captureSourceIdentifiers.size()); ++i)
    {
        if (captureSourceIdentifiers[static_cast<size_t>(i)] == desiredIdentifier)
        {
            selectionIndex = i;
            break;
        }
    }

    captureSourceSelector.setSelectedItemIndex(selectionIndex, juce::dontSendNotification);
    applyCaptureSourceSelection();
}

void WorkspaceShellComponent::applyCaptureSourceSelection()
{
    if (historyBuffer == nullptr)
        return;

    const auto idx = captureSourceSelector.getSelectedItemIndex();
    if (idx < 0 || idx >= static_cast<int>(captureSourceIdentifiers.size()))
        return;

    const auto identifier = captureSourceIdentifiers[static_cast<size_t>(idx)];
    if (identifier.isEmpty())
        historyBuffer->clearSource();
    else if (identifier == "__all__")
        historyBuffer->setAllSources(true);
    else
        historyBuffer->setSource(identifier);

    setle::state::AppPreferences::get().setCaptureSource(identifier);
}

juce::String WorkspaceShellComponent::buildMidiDeviceSignature() const
{
    juce::StringArray ids;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
        ids.add(device.identifier + ":" + device.name);

    ids.sort(true);
    return ids.joinIntoString("|");
}

juce::PopupMenu WorkspaceShellComponent::buildProgressionContextMenu(const juce::String& progressionId)
{
    juce::PopupMenu menu;

    menu.addItem(progressionEditIdentity, "Edit Identity...");
    menu.addItem(progressionForkVariant, "Fork as Variant");
    menu.addSeparator();
    menu.addItem(progressionAddChord, "Add Chord at End");
    menu.addItem(progressionClearChords, "Clear All Chords");
    menu.addSeparator();
    menu.addItem(progressionAssignToSection, "Assign to Section...");

    auto sections = songState.getSections();
    std::vector<juce::String> usedInSections;
    for (const auto& section : sections)
    {
        for (const auto& ref : section.getProgressionRefs())
        {
            if (ref.getProgressionId() == progressionId)
            {
                usedInSections.push_back(section.getName());
                break;
            }
        }
    }

    if (!usedInSections.empty())
    {
        juce::String usedInText = "Used in: ";
        for (size_t i = 0; i < usedInSections.size(); ++i)
        {
            usedInText += usedInSections[i];
            if (i < usedInSections.size() - 1)
                usedInText += ", ";
        }
        menu.addItem(-1, usedInText, false);
    }

    menu.addSeparator();
    menu.addItem(progressionAnnotateKeyMode, "Annotate Key / Mode");
    menu.addItem(progressionSubAllDominants, "Sub All Dominants");
    menu.addItem(progressionTransposeToKey, "Transpose to Session Key");
    menu.addSeparator();
    menu.addItem(progressionGrabSampler, "Grab to Sampler Queue");
    menu.addItem(progressionTagTransition, "Tag as Transition Material");
    menu.addItem(progressionSaveAsTemplate, "Save as Template");

    return menu;
}

void WorkspaceShellComponent::clampLayoutValues(int totalTopWidth, int totalBodyHeight)
{
    timelineHeight = juce::jlimit(minTimelineHeight, juce::jmax(minTimelineHeight, totalBodyHeight - minTopWorkHeight), timelineHeight);

    leftPanelWidth = juce::jlimit(minSideWidth, maxSideWidth, leftPanelWidth);
    rightPanelWidth = juce::jlimit(minSideWidth, maxSideWidth, rightPanelWidth);

    auto availableForPanels = totalTopWidth - (2 * splitterThickness);
    auto maxSidesSum = juce::jmax(0, availableForPanels - minCenterWidth);
    auto sidesSum = leftPanelWidth + rightPanelWidth;

    if (sidesSum > maxSidesSum && sidesSum > 0)
    {
        auto leftRatio = static_cast<float>(leftPanelWidth) / static_cast<float>(sidesSum);
        leftPanelWidth = static_cast<int>(maxSidesSum * leftRatio);
        rightPanelWidth = maxSidesSum - leftPanelWidth;
    }
}

void WorkspaceShellComponent::loadLayoutState()
{
    leftPanelWidth = setle::state::AppPreferences::get().getUiLeftWidth(leftPanelWidth);
    rightPanelWidth = setle::state::AppPreferences::get().getUiRightWidth(rightPanelWidth);
    timelineHeight = setle::state::AppPreferences::get().getUiTimelineHeight(timelineHeight);

    const auto modeRaw = setle::state::AppPreferences::get().getUiFocusMode(static_cast<int>(FocusMode::balanced));
    if (modeRaw == static_cast<int>(FocusMode::inFocused))
        focusMode = FocusMode::inFocused;
    else if (modeRaw == static_cast<int>(FocusMode::outFocused))
        focusMode = FocusMode::outFocused;
    else
        focusMode = FocusMode::balanced;
}

void WorkspaceShellComponent::saveLayoutState()
{
    setle::state::AppPreferences::get().setUiLeftWidth(leftPanelWidth);
    setle::state::AppPreferences::get().setUiRightWidth(rightPanelWidth);
    setle::state::AppPreferences::get().setUiTimelineHeight(timelineHeight);
    setle::state::AppPreferences::get().setUiFocusMode(static_cast<int>(focusMode));
}

void WorkspaceShellComponent::timerCallback()
{
    const auto signature = buildMidiDeviceSignature();
    if (signature != midiDeviceSignature)
    {
        midiDeviceSignature = signature;

        refreshCaptureSourceSelector(setle::state::AppPreferences::get().getCaptureSource());
    }

    if (edit == nullptr)
        return;

    captureSourceActivity *= 0.88f;
    if (historyBuffer != nullptr)
    {
        const int eventCount = static_cast<int>(historyBuffer->getAll().size());
        if (eventCount != lastHistoryEventCount)
        {
            lastHistoryEventCount = eventCount;
            captureSourceActivity = 1.0f;
        }
    }

    const auto* tempo = edit->tempoSequence.getTempo(0);
    const auto bpm = tempo != nullptr ? tempo->getBpm() : 120.0;
    arrangeLogoMeltCurrent += (arrangeLogoMeltTarget - arrangeLogoMeltCurrent) * 0.18f;
    if (arrangeLogoMeltTarget > 0.01f)
        arrangeLogoMeltTarget *= 0.92f;
    else
        arrangeLogoMeltTarget = 0.0f;

    if (quesoLogoComponent != nullptr)
    {
        quesoLogoComponent->setBpm(songState.getBpm());

        float dominantHz = 0.0f;
        float toneEnergy = 0.0f;
        if (sessionDashboardComponent != nullptr)
        {
            dominantHz = sessionDashboardComponent->getDominantFrequencyHz();
            toneEnergy = juce::jlimit(0.0f, 1.0f, sessionDashboardComponent->getDominantMagnitude() / 400.0f);
        }

        float strongestSubHz = 0.0f;
        float strongestSubIntensity = 0.0f;
        for (const auto& [trackId, slot] : instrumentSlots)
        {
            (void) trackId;
            if (slot == nullptr)
                continue;

            float subHz = 0.0f;
            float subIntensity = 0.0f;
            if (slot->getDrumSubSyncHint(subHz, subIntensity) && subIntensity > strongestSubIntensity)
            {
                strongestSubIntensity = subIntensity;
                strongestSubHz = subHz;
            }
        }

        if (strongestSubIntensity > 0.02f && strongestSubHz > 0.0f)
        {
            dominantHz = strongestSubHz;
            toneEnergy = juce::jmax(toneEnergy, juce::jlimit(0.0f, 1.0f, strongestSubIntensity));
        }

        quesoLogoComponent->setToneDetectorState(dominantHz, toneEnergy);
        quesoLogoComponent->setMeltFactor(arrangeLogoMeltCurrent);
    }

    const auto secPerBeat = 60.0 / juce::jmax(1.0, bpm);
    const auto playheadBeat = edit->getTransport().getPosition().inSeconds() / secPerBeat;
    const auto fraction = juce::jlimit(0.0, 1.0, playheadBeat / 32.0);
    const auto structureFraction = juce::jlimit(0.0, 1.0, playheadBeat / getSongStructureLengthBeats());

    if (timelineShell != nullptr)
        timelineShell->setPlayheadFraction(fraction);

    if (timelineTracks != nullptr)
        timelineTracks->setPlayheadFraction(fraction);

    if (gridRollComponent != nullptr)
        gridRollComponent->setPlayheadBeat(playheadBeat);

    if (arrangeWorkComponent != nullptr)
        arrangeWorkComponent->setPlayheadFraction(structureFraction);

    if (quesoLogoComponent != nullptr)
        quesoLogoComponent->setStructureProgress(static_cast<float>(structureFraction));

    for (auto& [trackId, slot] : instrumentSlots)
    {
        (void) trackId;
        if (slot != nullptr)
            slot->setTransportBeat(playheadBeat);
    }
}

void WorkspaceShellComponent::updateInPanelQueueView()
{
    if (auto* panel = dynamic_cast<InDevicePanel*>(inPanel))
        panel->repaint();
}

void WorkspaceShellComponent::playQueueSlot(int slotIndex)
{
    if (grabSamplerQueue == nullptr || edit == nullptr)
        return;

    const auto& slot = grabSamplerQueue->getSlot(slotIndex);
    grabSamplerQueue->playSlot(slotIndex, *edit);
    updateInPanelQueueView();
    const auto playbackMode = slot.hasCoupledAudio() ? "Reel sampler" : "MIDI synth";
    interactionStatus.setText("Playing sampler slot " + juce::String(slotIndex + 1)
                                  + " (" + playbackMode + ")",
                              juce::dontSendNotification);
}

void WorkspaceShellComponent::stopQueuePlayback()
{
    if (grabSamplerQueue == nullptr || edit == nullptr)
        return;

    grabSamplerQueue->stopPlayback(*edit);
    updateInPanelQueueView();
    if (setle::state::AppPreferences::get().getAutoGrabEnabled())
    {
        const int beats = setle::state::AppPreferences::get().getHistoryBufferMaxBeats(8);
        grabFromBuffer(juce::jlimit(1, 128, beats));
        interactionStatus.setText("Sampler playback stopped and auto-grabbed", juce::dontSendNotification);
        return;
    }

    interactionStatus.setText("Sampler playback stopped", juce::dontSendNotification);
}

void WorkspaceShellComponent::promoteQueueSlotToLibrary(int slotIndex)
{
    if (grabSamplerQueue == nullptr)
        return;

    const auto progressionId = grabSamplerQueue->promoteToLibrary(slotIndex, songState);
    if (progressionId.isEmpty())
    {
        interactionStatus.setText("Promote failed: progression missing", juce::dontSendNotification);
        return;
    }

    selectedProgressionId = progressionId;
    openTheoryEditor(TheoryMenuTarget::progression, progressionAnnotateKeyMode, "Annotate Key / Mode");
    grabSamplerQueue->clearSlot(slotIndex);
    updateInPanelQueueView();
    interactionStatus.setText("Promoted slot " + juce::String(slotIndex + 1) + " to library", juce::dontSendNotification);
}

void WorkspaceShellComponent::dropQueueSlotToTimeline(int slotIndex)
{
    if (grabSamplerQueue == nullptr || edit == nullptr)
        return;

    const auto& slot = grabSamplerQueue->getSlot(slotIndex);
    if (slot.state == setle::capture::GrabSlot::State::Empty || slot.progressionId.isEmpty())
        return;

    te::Track* targetTrack = nullptr;
    if (trackManager != nullptr)
    {
        const auto userTracks = trackManager->getAllUserTracks();
        if (!userTracks.isEmpty())
            targetTrack = userTracks.getFirst();
    }

    const auto playheadSeconds = edit->getTransport().getPosition().inSeconds();
    loadProgressionToEdit(slot.progressionId, playheadSeconds, true, targetTrack);
    interactionStatus.setText("Dropped slot " + juce::String(slotIndex + 1) + " to timeline at playhead", juce::dontSendNotification);
}

void WorkspaceShellComponent::clearQueueSlot(int slotIndex)
{
    if (grabSamplerQueue == nullptr)
        return;

    grabSamplerQueue->clearSlot(slotIndex);
    updateInPanelQueueView();
    interactionStatus.setText("Cleared sampler slot " + juce::String(slotIndex + 1), juce::dontSendNotification);
}

void WorkspaceShellComponent::editQueueSlotInTheoryPanel(int slotIndex)
{
    if (grabSamplerQueue == nullptr)
        return;

    const auto& slot = grabSamplerQueue->getSlot(slotIndex);
    if (slot.progressionId.isEmpty())
        return;

    selectedProgressionId = slot.progressionId;
    openTheoryEditor(TheoryMenuTarget::progression, progressionAnnotateKeyMode, "Annotate Key / Mode");
}

void WorkspaceShellComponent::renameQueueSlot(int slotIndex)
{
    if (grabSamplerQueue == nullptr)
        return;

    auto& slot = grabSamplerQueue->getSlotMutable(slotIndex);
    if (slot.progressionId.isEmpty())
        return;

    juce::AlertWindow window("Rename Grab Slot", "Enter new slot label", juce::AlertWindow::NoIcon);
    window.addTextEditor("name", slot.displayName, "Name");
    window.addButton("Rename", 1);
    window.addButton("Cancel", 0);

    if (window.runModalLoop() == 1)
    {
        auto newName = window.getTextEditorContents("name").trim();
        if (newName.isNotEmpty())
            slot.displayName = newName;
        updateInPanelQueueView();
    }
}

void WorkspaceShellComponent::loadProgressionToEdit()
{
    auto progressionOpt = getSelectedProgression();
    if (!progressionOpt.has_value())
        return;

    loadProgressionToEdit(progressionOpt->getId(), 0.0, false);
}

void WorkspaceShellComponent::loadProgressionToEdit(const juce::String& progressionId,
                                                    double startTimeSeconds,
                                                    bool preferNonSystemTrack,
                                                    te::Track* preferredTrack)
{
    if (edit == nullptr)
        return;

    auto tracks = te::getAudioTracks(*edit);
    if (tracks.isEmpty())
        return;

    auto* track = dynamic_cast<te::AudioTrack*>(preferredTrack);
    if (track == nullptr && preferNonSystemTrack)
    {
        for (auto* t : tracks)
        {
            if (t != nullptr && t->state.getProperty("setleSystemTag").toString().isEmpty())
            {
                track = t;
                break;
            }
        }
    }

    if (track == nullptr)
        track = tracks.getFirst();

    if (track == nullptr)
        return;

    if (!preferNonSystemTrack)
    {
        juce::Array<te::Clip*> oldClips;
        for (auto* clip : track->getClips())
            oldClips.add(clip);
        for (auto* clip : oldClips)
            clip->removeFromParent();
    }

    auto progressionOpt = songState.findProgressionById(progressionId);
    if (!progressionOpt.has_value())
        return;

    const auto chords = progressionOpt->getChords();
    if (chords.empty())
        return;

    const double bpm     = (edit->tempoSequence.getTempo(0) != nullptr)
                               ? edit->tempoSequence.getTempo(0)->getBpm()
                               : 120.0;
    const double secPerBeat = 60.0 / bpm;

    // Calculate total duration in seconds
    double totalBeats = 0.0;
    for (const auto& chord : chords)
    {
        double d = chord.getDurationBeats();
        totalBeats += (d > 0.0 ? d : 1.0);
    }
    const double totalSecs = totalBeats * secPerBeat;

    // Insert one MIDI clip spanning the whole progression
    const auto clipRange = tracktion::TimeRange (
        tracktion::TimePosition::fromSeconds(startTimeSeconds),
        tracktion::TimePosition::fromSeconds(startTimeSeconds + totalSecs));

    auto clip = track->insertMIDIClip(clipRange, nullptr);
    if (clip == nullptr)
        return;

    clip->setName(progressionId);
    clip->state.setProperty("progressionId", progressionId, nullptr);
    clip->state.setProperty("progressionName", progressionOpt->getName(), nullptr);
    juce::String symbols;
    for (int i = 0; i < static_cast<int>(chords.size()); ++i)
    {
        if (i > 0)
            symbols << " ";
        symbols << chords[static_cast<size_t>(i)].getSymbol();
        if (symbols.length() > 48)
            break;
    }
    clip->state.setProperty("progressionSymbols", symbols.trim(), nullptr);

    juce::String clipSectionId;
    if (selectedSectionId.isNotEmpty())
        clipSectionId = selectedSectionId;

    if (clipSectionId.isEmpty())
    {
        juce::String uniqueSection;
        bool ambiguous = false;
        for (const auto& section : songState.getSections())
        {
            for (const auto& ref : section.getProgressionRefs())
            {
                if (ref.getProgressionId() != progressionId)
                    continue;

                if (uniqueSection.isEmpty())
                    uniqueSection = section.getId();
                else if (uniqueSection != section.getId())
                    ambiguous = true;
            }
        }

        if (!ambiguous)
            clipSectionId = uniqueSection;
    }

    if (clipSectionId.isNotEmpty())
    {
        const auto clipStartBeat = startTimeSeconds / secPerBeat;
        double sectionStartBeat = 0.0;
        for (const auto& section : songState.getSections())
        {
            if (section.getId() == clipSectionId)
                break;
            sectionStartBeat += getSectionLengthBeats(section);
        }

        clip->state.setProperty("setleSectionId", clipSectionId, nullptr);
        clip->state.setProperty("setleSectionLocalStartBeats", clipStartBeat - sectionStartBeat, nullptr);
    }

    auto& seq = clip->getSequence();
    double beatPos = 0.0;

    for (const auto& chord : chords)
    {
        double d = chord.getDurationBeats();
        if (d <= 0.0)
            d = 1.0;

        const auto startBeat = tracktion::BeatPosition::fromBeats(beatPos);
        const auto beatLen   = tracktion::BeatDuration::fromBeats(d * 0.95);

        const auto notes = chord.getNotes();
        if (!notes.empty())
        {
            for (const auto& note : notes)
                seq.addNote(note.getPitch(),
                            startBeat,
                            beatLen,
                            static_cast<int>(note.getVelocity() * 127.0f),
                            0,
                            nullptr);
        }
        else
        {
            auto chordPitchClasses = setle::theory::BachTheory::getChordPitchClasses(chord.getSymbol());
            if (chordPitchClasses.empty())
                chordPitchClasses = { (juce::jlimit(0, 127, chord.getRootMidi()) % 12 + 12) % 12 };

            int rootMidi = chord.getRootMidi();
            if (rootMidi <= 0 || rootMidi > 127)
                rootMidi = 60 + chordPitchClasses.front();

            for (auto pc : chordPitchClasses)
            {
                int noteNum = juce::jlimit(0, 127, rootMidi);
                int guard = 0;
                while (((noteNum % 12) + 12) % 12 != pc && guard++ < 24)
                    ++noteNum;

                while (noteNum > rootMidi + 12)
                    noteNum -= 12;

                seq.addNote(juce::jlimit(0, 127, noteNum),
                            startBeat,
                            beatLen,
                            90,
                            0,
                            nullptr);
            }
        }

        beatPos += d;
    }

    if (gridRollComponent != nullptr
        && selectedProgressionId == progressionId
        && gridRollComponent->isNoteMode())
    {
        gridRollComponent->refreshNoteModeCache();
    }
}

void WorkspaceShellComponent::ensureProgressionLoadedForNoteMode(const juce::String& progressionId)
{
    if (progressionId.isEmpty() || edit == nullptr)
        return;

    bool hasClipWithNotes = false;
    for (auto* track : te::getAudioTracks(*edit))
    {
        if (track == nullptr)
            continue;

        for (auto* clipBase : track->getClips())
        {
            auto* midiClip = dynamic_cast<te::MidiClip*>(clipBase);
            if (midiClip == nullptr)
                continue;

            const auto clipPropId = midiClip->state.getProperty("progressionId").toString();
            if (clipPropId != progressionId && midiClip->getName() != progressionId)
                continue;

            if (!midiClip->getSequence().getNotes().isEmpty())
            {
                hasClipWithNotes = true;
                break;
            }
        }

        if (hasClipWithNotes)
            break;
    }

    if (!hasClipWithNotes)
        loadProgressionToEdit(progressionId, 0.0, true, nullptr);
}

void WorkspaceShellComponent::resized()
{
    auto bounds = getLocalBounds();

    // ---- 1. Classic menu bar (top-most strip) ----
    if (menuBarComponent != nullptr)
        menuBarComponent->setBounds(bounds.removeFromTop(menuBarHeight));

    // ---- 2. Left-nav section strip ----
    if (leftNavComponent != nullptr)
        leftNavComponent->setBounds(bounds.removeFromTop(navBarHeight));

    clampLayoutValues(bounds.getWidth(), bounds.getHeight() - topStripHeight);

    auto topBounds = bounds.removeFromTop(topStripHeight);
    topStrip.setBounds(topBounds);

    // Reserve logo space first so it cannot disappear when controls compress on narrower windows.
    auto logoSlot = topBounds.removeFromLeft(juce::jlimit(120, 360, topBounds.getWidth() / 3));
    if (quesoLogoComponent != nullptr)
    {
        auto logoBounds = logoSlot.reduced(4, 4);
        quesoLogoComponent->setBounds(logoBounds);
        quesoLogoComponent->setVisible(!logoBounds.isEmpty());
    }

    auto buttonArea = topBounds.removeFromRight(juce::jmin(1410, topBounds.getWidth()));
    themeButton.setBounds(buttonArea.removeFromRight(72).reduced(4, 6));
    redoTheoryButton.setBounds(buttonArea.removeFromRight(112).reduced(4, 6));
    undoTheoryButton.setBounds(buttonArea.removeFromRight(122).reduced(4, 6));
    focusOutButton.setBounds(buttonArea.removeFromRight(118).reduced(4, 6));
    focusBalancedButton.setBounds(buttonArea.removeFromRight(148).reduced(4, 6));
    focusInButton.setBounds(buttonArea.removeFromRight(104).reduced(4, 6));

    // Session key/mode selectors (right of focus buttons)
    sessionModeLabel.setBounds(buttonArea.removeFromRight(40).reduced(4, 6));
    sessionModeSelector.setBounds(buttonArea.removeFromRight(90).reduced(4, 6));
    sessionKeyLabel.setBounds(buttonArea.removeFromRight(35).reduced(4, 6));
    sessionKeySelector.setBounds(buttonArea.removeFromRight(75).reduced(4, 6));

    if (toolPalette != nullptr)
    {
        auto toolArea = buttonArea.removeFromRight(228).reduced(4, 6);
        toolPalette->setBounds(toolArea);
    }

    // Transport controls: BPM + Capture + Play/Stop/Rec
    recordButton.setBounds(buttonArea.removeFromRight(46).reduced(4, 6));
    stopButton.setBounds(buttonArea.removeFromRight(46).reduced(4, 6));
    playButton.setBounds(buttonArea.removeFromRight(52).reduced(4, 6));

    auto captureArea = buttonArea.removeFromRight(188).reduced(4, 6);
    captureSourceLabel.setBounds(captureArea.removeFromLeft(48));
    captureSourceSelector.setBounds(captureArea.removeFromLeft(140));

    auto bpmArea = buttonArea.removeFromRight(100).reduced(4, 6);
    bpmLabel.setBounds(bpmArea.removeFromLeft(32));
    bpmEditor.setBounds(bpmArea);

    topTitle.setBounds(topBounds.removeFromLeft(146).reduced(8, 6));
    interactionStatus.setBounds(topBounds.reduced(8, 8));

    auto timelineArea = bounds.removeFromBottom(timelineHeight);
    auto timelineSplitterArea = bounds.removeFromBottom(splitterThickness);
    if (timelineResizeBar != nullptr)
        timelineResizeBar->setBounds(timelineSplitterArea);
    if (timelineShell != nullptr)
    {
        timelineShell->setBounds(timelineArea);
        if (timelineTracks != nullptr)
            timelineTracks->setBounds(timelineShell->getTrackAreaBoundsInParent());
    }

    clampLayoutValues(bounds.getWidth(), bounds.getHeight() + timelineHeight);

    auto inArea = bounds.removeFromLeft(leftPanelWidth);
    if (inPanel != nullptr)
        inPanel->setBounds(inArea);

    auto leftSplitterArea = bounds.removeFromLeft(splitterThickness);
    if (leftResizeBar != nullptr)
        leftResizeBar->setBounds(leftSplitterArea);

    auto outArea = bounds.removeFromRight(rightPanelWidth);
    if (outPanelHost != nullptr)
        outPanelHost->setBounds(outArea);

    auto rightSplitterArea = bounds.removeFromRight(splitterThickness);
    if (rightResizeBar != nullptr)
        rightResizeBar->setBounds(rightSplitterArea);

    if (workPanel == nullptr)
        return;

    workPanel->setBounds(bounds);

    // ---- WORK panel zone header + content area ----
    auto workLocal = workPanel->getLocalBounds();
    {
        // Zone header (tabs + section label + hamburger)
        if (zoneHeader != nullptr)
            zoneHeader->setBounds(workLocal.removeFromTop(ZoneHeaderComponent::kPreferredHeight));
    }
    workLocal.reduce(12, 0);
    workLocal.removeFromTop(6); // gap between header and content

    auto editorBounds = workLocal;
    theoryEditorPanel.setBounds(editorBounds);

    if (gridRollComponent != nullptr)
        gridRollComponent->setBounds(editorBounds);

    if (effBuilderComponent != nullptr)
        effBuilderComponent->setBounds(editorBounds);

    if (soundWorkComponent != nullptr)
        soundWorkComponent->setBounds(editorBounds);

    if (arrangeWorkComponent != nullptr)
        arrangeWorkComponent->setBounds(editorBounds);

    auto panelBounds = theoryEditorPanel.getLocalBounds().reduced(10);

    theoryEditorTitle.setBounds(panelBounds.removeFromTop(24));
    theoryEditorHint.setBounds(panelBounds.removeFromTop(20));
    panelBounds.removeFromTop(6);

    auto objectRow = panelBounds.removeFromTop(28);
    theoryObjectLabel.setBounds(objectRow.removeFromLeft(120));
    theoryObjectSelector.setBounds(objectRow);

    auto placeFieldRow = [&panelBounds](juce::Label& label, juce::TextEditor& editor)
    {
        auto row = panelBounds.removeFromTop(28);
        label.setBounds(row.removeFromLeft(220));
        editor.setBounds(row);
    };

    panelBounds.removeFromTop(6);
    placeFieldRow(theoryFieldLabel1, theoryFieldEditor1);
    placeFieldRow(theoryFieldLabel2, theoryFieldEditor2);
    placeFieldRow(theoryFieldLabel3, theoryFieldEditor3);
    placeFieldRow(theoryFieldLabel4, theoryFieldEditor4);
    placeFieldRow(theoryFieldLabel5, theoryFieldEditor5);

    panelBounds.removeFromTop(10);
    auto buttonRow = panelBounds.removeFromTop(30);
    reloadTheoryEditorButton.setBounds(buttonRow.removeFromRight(96));
    buttonRow.removeFromRight(6);
    applyTheoryEditorButton.setBounds(buttonRow.removeFromRight(140));

    if (themeEditorPanel != nullptr)
    {
        themeDismissOverlay.setBounds(getLocalBounds());
        themeEditorPanel->setBounds(getWidth() - 440, topStripHeight, 440, getHeight() - topStripHeight);
    }
}

} // namespace setle::ui
