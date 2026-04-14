#include "MidiFileImporter.h"

#include <algorithm>

namespace setle::midi
{
namespace
{
constexpr const char* kTrackKindProperty = "setleTrackKind";
constexpr const char* kTrackKindMidi = "midi";
}

MidiFileImporter::ImportResult MidiFileImporter::importFile(const juce::File& file,
                                                            te::Edit& edit,
                                                            te::Engine& engine)
{
    (void) engine;

    ImportResult result;

    if (!file.existsAsFile())
    {
        result.errorMessage = "MIDI file does not exist: " + file.getFullPathName();
        return result;
    }

    juce::MidiFile midiFile;
    juce::FileInputStream stream(file);
    if (!stream.openedOk() || !midiFile.readFrom(stream))
    {
        result.errorMessage = "Could not read MIDI file: " + file.getFileName();
        return result;
    }

    midiFile.convertTimestampTicksToSeconds();

    const int numTracks = midiFile.getNumTracks();
    result.numTracks = numTracks;
    result.suggestedName = file.getFileNameWithoutExtension();

    for (int t = 0; t < numTracks; ++t)
    {
        const auto* seq = midiFile.getTrack(t);
        if (seq == nullptr)
            continue;

        for (int i = 0; i < seq->getNumEvents(); ++i)
        {
            const auto& msg = seq->getEventPointer(i)->message;
            if (msg.isTempoMetaEvent())
            {
                const auto secPerQuarter = msg.getTempoSecondsPerQuarterNote();
                if (secPerQuarter > 0.0)
                    result.detectedBpm = 60.0 / secPerQuarter;
            }

            if (msg.isTimeSignatureMetaEvent())
            {
                int num = 4;
                int den = 4;
                msg.getTimeSignatureInfo(num, den);
                result.timeSigNumerator = juce::jmax(1, num);
                result.timeSigDenominator = juce::jmax(1, den);
            }
        }
    }

    if (auto* tempo = edit.tempoSequence.getTempo(0))
        tempo->setBpm(result.detectedBpm);

    const double secPerBeat = 60.0 / juce::jmax(1.0, result.detectedBpm);

    double maxEndTime = 0.0;
    int totalNotes = 0;

    for (int t = 0; t < numTracks; ++t)
    {
        const auto* seq = midiFile.getTrack(t);
        if (seq == nullptr)
            continue;

        juce::MidiMessageSequence working(*seq);
        working.updateMatchedPairs();

        int noteCount = 0;
        double trackEnd = 0.0;
        for (int i = 0; i < working.getNumEvents(); ++i)
        {
            const auto* ev = working.getEventPointer(i);
            if (ev == nullptr)
                continue;

            trackEnd = juce::jmax(trackEnd, ev->message.getTimeStamp());
            if (ev->message.isNoteOn())
                ++noteCount;
        }

        if (noteCount == 0)
            continue;

        maxEndTime = juce::jmax(maxEndTime, trackEnd);

        auto created = edit.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(edit), nullptr, false);
        auto* track = created.get();
        if (track == nullptr)
            continue;

        track->setName("Track " + juce::String(t + 1));
        track->state.setProperty(kTrackKindProperty, kTrackKindMidi, nullptr);
        track->state.setProperty("importedFromMidi", true, nullptr);
        track->state.setProperty("sourceFile", file.getFullPathName(), nullptr);

        const auto clipRange = tracktion::TimeRange(tracktion::TimePosition::fromSeconds(0.0),
                                                    tracktion::TimePosition::fromSeconds(trackEnd + 0.1));

        auto clip = track->insertMIDIClip(clipRange, nullptr);
        if (clip == nullptr)
            continue;

        clip->setName(result.suggestedName + "_t" + juce::String(t + 1));
        clip->state.setProperty("importedFromMidi", true, nullptr);
        clip->state.setProperty("sourceFile", file.getFullPathName(), nullptr);

        auto& midiSeq = clip->getSequence();

        for (int i = 0; i < working.getNumEvents(); ++i)
        {
            const auto* ev = working.getEventPointer(i);
            if (ev == nullptr)
                continue;

            const auto& msg = ev->message;
            if (!msg.isNoteOn())
                continue;

            const double startSec = msg.getTimeStamp();
            const double startBeat = startSec / secPerBeat;

            double endSec = startSec + 0.5;
            if (ev->noteOffObject != nullptr)
                endSec = juce::jmax(startSec + 0.001, ev->noteOffObject->message.getTimeStamp());

            const double durationBeats = juce::jmax(0.0625, (endSec - startSec) / secPerBeat * 0.95);
            const int velocity = juce::jlimit(1, 127, static_cast<int>(msg.getVelocity()));

            midiSeq.addNote(msg.getNoteNumber(),
                            tracktion::BeatPosition::fromBeats(startBeat),
                            tracktion::BeatDuration::fromBeats(durationBeats),
                            velocity,
                            0,
                            nullptr);

            ++totalNotes;
        }
    }

    result.numNotes = totalNotes;
    result.durationBeats = maxEndTime / secPerBeat;
    result.success = totalNotes > 0;

    if (!result.success)
        result.errorMessage = "No note data found in file";

    return result;
}

std::optional<MidiFileImporter::ImportResult> MidiFileImporter::importWithChooser(te::Edit& edit,
                                                                                   te::Engine& engine,
                                                                                   juce::Component* parentComponent)
{
    juce::FileChooser chooser("Import MIDI File",
                              juce::File(),
                              "*.mid;*.midi",
                              true,
                              false,
                              parentComponent);

    if (!chooser.browseForFileToOpen())
        return std::nullopt;

    return importFile(chooser.getResult(), edit, engine);
}

} // namespace setle::midi
