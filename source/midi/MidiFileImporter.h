#pragma once

#include <juce_core/juce_core.h>
#include <tracktion_engine/tracktion_engine.h>

#include <optional>

namespace te = tracktion::engine;

namespace setle::midi
{

class MidiFileImporter
{
public:
    struct ImportResult
    {
        bool success { false };
        juce::String errorMessage;
        int numTracks { 0 };
        int numNotes { 0 };
        double durationBeats { 0.0 };
        double detectedBpm { 120.0 };
        int timeSigNumerator { 4 };
        int timeSigDenominator { 4 };
        juce::String suggestedName;
    };

    // Import a MIDI file into the Tracktion Edit.
    // Creates one te::AudioTrack per MIDI track with note data.
    static ImportResult importFile(const juce::File& midiFile,
                                   te::Edit& edit,
                                   te::Engine& engine);

    // Show a FileChooser and import the selected file.
    // Returns empty optional if user cancelled.
    static std::optional<ImportResult> importWithChooser(te::Edit& edit,
                                                          te::Engine& engine,
                                                          juce::Component* parentComponent);
};

} // namespace setle::midi
