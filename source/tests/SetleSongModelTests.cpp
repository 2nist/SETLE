#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include "../model/SetleSongModel.h"

namespace
{
bool expect(bool condition, const juce::String& message)
{
    if (condition)
        return true;

    juce::Logger::writeToLog("FAIL: " + message);
    return false;
}

bool testSectionProgressionRefDefaults()
{
    auto ref = setle::model::SectionProgressionRef::create("p1", 0, "");
    bool ok = true;
    ok &= expect(ref.isValid(), "SectionProgressionRef should be valid");
    ok &= expect(ref.getRepeatScope() == "all", "repeatScope should default to 'all'");
    ok &= expect(ref.getRepeatSelection() == "all", "repeatSelection should default to 'all'");
    ok &= expect(ref.getRepeatIndices().isEmpty(), "repeatIndices should default to empty");
    return ok;
}

bool testSchemaMigrationAddsRepeatFields()
{
    using namespace setle::model;

    auto song = Song::create("Migration Test", 120.0);
    auto section = Section::create("Verse", 4);
    auto ref = SectionProgressionRef::create("progression-A", 0, "legacy");

    auto refTree = ref.valueTree();
    refTree.removeProperty(Schema::repeatScopeProp, nullptr);
    refTree.removeProperty(Schema::repeatSelectionProp, nullptr);
    refTree.removeProperty(Schema::repeatIndicesProp, nullptr);

    section.valueTree().appendChild(refTree.createCopy(), nullptr);
    song.addSection(section);

    auto xml = song.valueTree().toXmlString();
    auto rawTree = juce::ValueTree::fromXml(xml);
    if (!rawTree.isValid())
        return expect(false, "Failed to create raw migration tree");

    // Simulate a v0 file: remove the schemaVersion stamp so the v0→v1
    // migration block fires and backfills the scope fields.
    rawTree.removeProperty(Schema::schemaVersionProp, nullptr);

    auto migrated = Song(rawTree);
    const auto sections = migrated.getSections();
    if (sections.empty())
        return expect(false, "Migrated song should have sections");

    const auto refs = sections.front().getProgressionRefs();
    if (refs.empty())
        return expect(false, "Migrated section should have progression refs");

    bool ok = true;
    ok &= expect(refs.front().getRepeatScope() == "all", "Migration should add repeatScope=all");
    ok &= expect(refs.front().getRepeatSelection() == "all", "Migration should add repeatSelection=all");
    ok &= expect(refs.front().getRepeatIndices().isEmpty(), "Migration should add repeatIndices as empty");
    return ok;
}

bool testXmlRoundTrip()
{
    using namespace setle::model;

    auto song = Song::create("RoundTrip", 128.0);
    auto progression = Progression::create("Prog A", "C", "ionian");
    progression.addChord(Chord::create("Cmaj7", "major7", 60));
    song.addProgression(progression);

    auto section = Section::create("Verse", 4);
    auto ref = SectionProgressionRef::create(progression.getId(), 0, "base");
    ref.setRepeatScope("repeat");
    ref.setRepeatSelection("current");
    ref.setRepeatIndices("2-4");
    section.addProgressionRef(ref);
    song.addSection(section);

    const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("setle_song_model_tests.xml");
    tempFile.deleteFile();

    const auto saveResult = song.saveToFile(tempFile, StorageFormat::xml);
    if (saveResult.failed())
        return expect(false, "XML save should succeed");

    const auto loaded = Song::loadFromFile(tempFile, StorageFormat::xml);
    tempFile.deleteFile();

    if (!loaded.has_value())
        return expect(false, "XML load should succeed");

    const auto loadedSections = loaded->getSections();
    if (loadedSections.empty())
        return expect(false, "Loaded song should contain section data");

    const auto loadedRefs = loadedSections.front().getProgressionRefs();
    if (loadedRefs.empty())
        return expect(false, "Loaded section should contain progression refs");

    bool ok = true;
    ok &= expect(loadedRefs.front().getRepeatScope() == "repeat", "Roundtrip should preserve repeatScope");
    ok &= expect(loadedRefs.front().getRepeatSelection() == "current", "Roundtrip should preserve repeatSelection");
    ok &= expect(loadedRefs.front().getRepeatIndices() == "2-4", "Roundtrip should preserve repeatIndices");
    return ok;
}

bool testBinaryAndGzipRoundTrip()
{
    using namespace setle::model;

    auto makeSong = []()
    {
        auto song = Song::create("BinaryGzipRoundTrip", 110.0);
        auto progression = Progression::create("Prog BG", "D", "dorian");
        progression.addChord(Chord::create("Dm7", "minor7", 62));
        song.addProgression(progression);

        auto section = Section::create("Section BG", 3);
        auto ref = SectionProgressionRef::create(progression.getId(), 0, "bg");
        ref.setRepeatScope("selectedRepeats");
        ref.setRepeatSelection("selected");
        ref.setRepeatIndices("1,3");
        section.addProgressionRef(ref);
        song.addSection(section);

        return song;
    };

    auto verifyLoaded = [](const Song& loaded)
    {
        const auto sections = loaded.getSections();
        if (sections.empty())
            return expect(false, "Loaded song should contain sections");

        const auto refs = sections.front().getProgressionRefs();
        if (refs.empty())
            return expect(false, "Loaded section should contain refs");

        bool ok = true;
        ok &= expect(refs.front().getRepeatScope() == "selectedRepeats", "Roundtrip should preserve repeatScope for non-XML formats");
        ok &= expect(refs.front().getRepeatSelection() == "selected", "Roundtrip should preserve repeatSelection for non-XML formats");
        ok &= expect(refs.front().getRepeatIndices() == "1,3", "Roundtrip should preserve repeatIndices for non-XML formats");
        return ok;
    };

    bool ok = true;

    {
        auto song = makeSong();
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("setle_song_model_tests.bin");
        file.deleteFile();

        const auto saveResult = song.saveToFile(file, StorageFormat::binary);
        ok &= expect(saveResult.wasOk(), "Binary save should succeed");

        const auto loaded = Song::loadFromFile(file, StorageFormat::binary);
        ok &= expect(loaded.has_value(), "Binary load should succeed");
        if (loaded.has_value())
            ok &= verifyLoaded(*loaded);

        file.deleteFile();
    }

    {
        auto song = makeSong();
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("setle_song_model_tests.gzbin");
        file.deleteFile();

        const auto saveResult = song.saveToFile(file, StorageFormat::gzipBinary);
        ok &= expect(saveResult.wasOk(), "GZIP save should succeed");

        const auto loaded = Song::loadFromFile(file, StorageFormat::gzipBinary);
        ok &= expect(loaded.has_value(), "GZIP load should succeed");
        if (loaded.has_value())
            ok &= verifyLoaded(*loaded);

        file.deleteFile();
    }

    return ok;
}

bool testInvalidNonXmlDataRejected()
{
    using namespace setle::model;

    const auto binaryFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("setle_song_model_tests_invalid.bin");
    binaryFile.replaceWithText("not-a-valuetree");

    const auto gzipFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("setle_song_model_tests_invalid.gzbin");
    gzipFile.replaceWithText("not-a-gzip-valuetree");

    bool ok = true;
    ok &= expect(!Song::loadFromFile(binaryFile, StorageFormat::binary).has_value(), "Invalid binary data should be rejected");
    ok &= expect(!Song::loadFromFile(gzipFile, StorageFormat::gzipBinary).has_value(), "Invalid gzip data should be rejected");

    binaryFile.deleteFile();
    gzipFile.deleteFile();
    return ok;
}

// ---- A1: Song title + BPM survive XML roundtrip ----
bool testSongTopLevelFieldsXmlRoundtrip()
{
    using namespace setle::model;

    auto song = Song::create("TitleCheck", 142.0);

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A1.xml");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::xml).failed())
        return expect(false, "A1: XML save failed");

    auto loaded = Song::loadFromFile(file, StorageFormat::xml);
    file.deleteFile();

    if (!loaded.has_value())
        return expect(false, "A1: XML load returned nullopt");

    bool ok = true;
    ok &= expect(loaded->getTitle() == "TitleCheck", "A1: title survives XML roundtrip");
    ok &= expect(loaded->getBpm() == 142.0, "A1: BPM survives XML roundtrip");
    return ok;
}

// ---- A2: Song title + BPM survive binary roundtrip ----
bool testSongTopLevelFieldsBinaryRoundtrip()
{
    using namespace setle::model;

    auto song = Song::create("BinaryTitle", 99.5);

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A2.bin");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::binary).failed())
        return expect(false, "A2: binary save failed");

    auto loaded = Song::loadFromFile(file, StorageFormat::binary);
    file.deleteFile();

    if (!loaded.has_value())
        return expect(false, "A2: binary load returned nullopt");

    bool ok = true;
    ok &= expect(loaded->getTitle() == "BinaryTitle", "A2: title survives binary roundtrip");
    ok &= expect(loaded->getBpm() == 99.5, "A2: BPM survives binary roundtrip");
    return ok;
}

// ---- A3: Song title + BPM survive gzip roundtrip ----
bool testSongTopLevelFieldsGzipRoundtrip()
{
    using namespace setle::model;

    auto song = Song::create("GzipTitle", 88.0);

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A3.gzbin");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::gzipBinary).failed())
        return expect(false, "A3: gzip save failed");

    auto loaded = Song::loadFromFile(file, StorageFormat::gzipBinary);
    file.deleteFile();

    if (!loaded.has_value())
        return expect(false, "A3: gzip load returned nullopt");

    bool ok = true;
    ok &= expect(loaded->getTitle() == "GzipTitle", "A3: title survives gzip roundtrip");
    ok &= expect(loaded->getBpm() == 88.0, "A3: BPM survives gzip roundtrip");
    return ok;
}

// ---- A4: Progression key/mode/name survive XML roundtrip ----
bool testProgressionFieldsXmlRoundtrip()
{
    using namespace setle::model;

    auto song = Song::create("ProgRoundtrip", 120.0);
    auto prog = Progression::create("Minor Verse", "D", "aeolian");
    prog.setLengthBeats(16.0);
    song.addProgression(prog);

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A4.xml");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::xml).failed())
        return expect(false, "A4: XML save failed");

    auto loaded = Song::loadFromFile(file, StorageFormat::xml);
    file.deleteFile();

    if (!loaded.has_value())
        return expect(false, "A4: XML load returned nullopt");

    const auto progs = loaded->getProgressions();
    if (progs.empty())
        return expect(false, "A4: no progressions in loaded song");

    bool ok = true;
    ok &= expect(progs.front().getName() == "Minor Verse", "A4: progression name survives");
    ok &= expect(progs.front().getKey() == "D", "A4: progression key survives");
    ok &= expect(progs.front().getMode() == "aeolian", "A4: progression mode survives");
    ok &= expect(progs.front().getLengthBeats() == 16.0, "A4: progression length survives");
    return ok;
}

// ---- A5: Section name + repeatCount survive XML roundtrip ----
bool testSectionFieldsXmlRoundtrip()
{
    using namespace setle::model;

    auto song = Song::create("SectionRoundtrip", 120.0);
    auto section = Section::create("Bridge", 3);
    song.addSection(section);

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A5.xml");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::xml).failed())
        return expect(false, "A5: XML save failed");

    auto loaded = Song::loadFromFile(file, StorageFormat::xml);
    file.deleteFile();

    if (!loaded.has_value())
        return expect(false, "A5: XML load returned nullopt");

    const auto sections = loaded->getSections();
    if (sections.empty())
        return expect(false, "A5: no sections in loaded song");

    bool ok = true;
    ok &= expect(sections.front().getName() == "Bridge", "A5: section name survives");
    ok &= expect(sections.front().getRepeatCount() == 3, "A5: section repeatCount survives");
    return ok;
}

// ---- A6+A7: Transition fields survive XML roundtrip ----
bool testTransitionFieldsXmlRoundtrip()
{
    using namespace setle::model;

    auto song = Song::create("TransitionRoundtrip", 120.0);
    auto section1 = Section::create("Verse", 2);
    auto section2 = Section::create("Chorus", 1);
    song.addSection(section1);
    song.addSection(section2);

    auto transition = Transition::create(section1.getId(), section2.getId(), "dominantPivot");
    transition.setName("Verse to Chorus");
    transition.setTargetTension(7);
    song.addTransition(transition);

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A6.xml");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::xml).failed())
        return expect(false, "A6: XML save failed");

    auto loaded = Song::loadFromFile(file, StorageFormat::xml);
    file.deleteFile();

    if (!loaded.has_value())
        return expect(false, "A6: XML load returned nullopt");

    const auto transitions = loaded->getTransitions();
    if (transitions.empty())
        return expect(false, "A6: no transitions in loaded song");

    bool ok = true;
    ok &= expect(transitions.front().getFromSectionId() == section1.getId(),
                 "A6: transition fromSectionId survives");
    ok &= expect(transitions.front().getToSectionId() == section2.getId(),
                 "A6: transition toSectionId survives");
    ok &= expect(transitions.front().getStrategy() == "dominantPivot",
                 "A7: transition strategy survives");
    return ok;
}

// ---- A8: Empty binary payload rejected ----
bool testEmptyBinaryPayloadRejected()
{
    using namespace setle::model;

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A8_empty.bin");
    file.create();

    const auto loaded = Song::loadFromFile(file, StorageFormat::binary);
    file.deleteFile();

    return expect(!loaded.has_value(), "A8: empty binary payload should return nullopt");
}

// ---- A9: Chord symbol/quality/rootMidi/function/duration survive XML roundtrip ----
bool testChordFieldsXmlRoundtrip()
{
    using namespace setle::model;

    auto song = Song::create("ChordRoundtrip", 120.0);
    auto prog = Progression::create("Test Prog", "C", "ionian");

    auto chord = Chord::create("Dm7", "minor7", 62);
    chord.setFunction("ii");
    chord.setDurationBeats(4.0);
    chord.setTension(3);
    prog.addChord(chord);
    song.addProgression(prog);

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A9.xml");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::xml).failed())
        return expect(false, "A9: XML save failed");

    auto loaded = Song::loadFromFile(file, StorageFormat::xml);
    file.deleteFile();

    if (!loaded.has_value())
        return expect(false, "A9: XML load returned nullopt");

    const auto progs = loaded->getProgressions();
    if (progs.empty())
        return expect(false, "A9: no progressions in loaded song");

    const auto chords = progs.front().getChords();
    if (chords.empty())
        return expect(false, "A9: no chords in loaded progression");

    bool ok = true;
    ok &= expect(chords.front().getSymbol() == "Dm7", "A9: chord symbol survives");
    ok &= expect(chords.front().getQuality() == "minor7", "A9: chord quality survives");
    ok &= expect(chords.front().getRootMidi() == 62, "A9: chord rootMidi survives");
    ok &= expect(chords.front().getFunction() == "ii", "A9: chord function survives");
    ok &= expect(chords.front().getDurationBeats() == 4.0, "A9: chord duration survives");
    return ok;
}

// ---- A10: schemaVersion is stamped in saved XML ----
bool testSchemaVersionStampedInXml()
{
    using namespace setle::model;

    auto song = Song::create("SchemaVersion", 120.0);

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_A10.xml");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::xml).failed())
        return expect(false, "A10: XML save failed");

    const auto text = file.loadFileAsString();
    file.deleteFile();

    return expect(text.contains("schemaVersion"), "A10: schemaVersion should appear in saved XML");
}

// ---- U1: Snapshot restores previous state ----
bool testSnapshotRestoresPreviousTitle()
{
    using namespace setle::model;

    auto song = Song::create("OriginalTitle", 120.0);
    const auto snapshot = song.valueTree().toXmlString();

    song.setTitle("MutatedTitle");

    auto restored = juce::ValueTree::fromXml(snapshot);
    if (!restored.isValid())
        return expect(false, "U1: snapshot XML should be valid");

    song = Song(restored);
    return expect(song.getTitle() == "OriginalTitle", "U1: restore should roll back title mutation");
}

// ---- U2: Redo re-applies mutation ----
bool testSnapshotRedoReappliesMutation()
{
    using namespace setle::model;

    auto song = Song::create("BeforeMutation", 120.0);
    const auto snapshot1 = song.valueTree().toXmlString();

    song.setTitle("AfterMutation");
    const auto snapshot2 = song.valueTree().toXmlString();

    song = Song(juce::ValueTree::fromXml(snapshot1));
    bool ok = expect(song.getTitle() == "BeforeMutation", "U2: undo should restore original title");

    song = Song(juce::ValueTree::fromXml(snapshot2));
    ok &= expect(song.getTitle() == "AfterMutation", "U2: redo should restore mutated title");
    return ok;
}

// ---- U3: Multi-cycle undo/redo stays consistent ----
bool testMultiCycleUndoRedoConsistency()
{
    using namespace setle::model;

    auto song = Song::create("State0", 100.0);
    const auto s0 = song.valueTree().toXmlString();

    song.setBpm(120.0);
    const auto s1 = song.valueTree().toXmlString();

    song.setBpm(140.0);
    const auto s2 = song.valueTree().toXmlString();

    song = Song(juce::ValueTree::fromXml(s1));
    bool ok = expect(song.getBpm() == 120.0, "U3: first undo lands at 120 bpm");

    song = Song(juce::ValueTree::fromXml(s0));
    ok &= expect(song.getBpm() == 100.0, "U3: second undo lands at 100 bpm");

    song = Song(juce::ValueTree::fromXml(s1));
    ok &= expect(song.getBpm() == 120.0, "U3: first redo lands at 120 bpm");

    song = Song(juce::ValueTree::fromXml(s2));
    ok &= expect(song.getBpm() == 140.0, "U3: second redo lands at 140 bpm");

    return ok;
}

// ---- U5: Undo in memory does not overwrite a saved file ----
bool testUndoAfterSaveDoesNotCorruptFile()
{
    using namespace setle::model;

    auto song = Song::create("PreSave", 120.0);
    const auto snapshot = song.valueTree().toXmlString();

    song.setTitle("PostSave");

    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("setle_U5.xml");
    file.deleteFile();

    if (song.saveToFile(file, StorageFormat::xml).failed())
        return expect(false, "U5: save after mutation should succeed");

    song = Song(juce::ValueTree::fromXml(snapshot));
    bool ok = expect(song.getTitle() == "PreSave", "U5: memory should show pre-save title after undo");

    auto fromDisk = Song::loadFromFile(file, StorageFormat::xml);
    file.deleteFile();

    if (!fromDisk.has_value())
        return expect(false, "U5: saved file should still be loadable");

    ok &= expect(fromDisk->getTitle() == "PostSave", "U5: file should be unaffected by in-memory undo");
    return ok;
}

// P1 – section beat widths are proportional to repeatCount × progression.lengthBeats
bool testSectionBeatWidthProportionality()
{
    using namespace setle::model;

    auto song = Song::create("BeatWidthTest", 120.0);

    // Two progressions with known lengths
    auto progA = Progression::create("A", "C", "ionian");
    progA.setLengthBeats(8.0);
    song.addProgression(progA);

    auto progB = Progression::create("B", "C", "ionian");
    progB.setLengthBeats(16.0);
    song.addProgression(progB);

    // Verse: 4 repeats of progA → 32 beats
    auto verse = Section::create("Verse", 4);
    verse.addProgressionRef(SectionProgressionRef::create(progA.getId(), 0, ""));
    song.addSection(verse);

    // Chorus: 2 repeats of progB → 32 beats
    auto chorus = Section::create("Chorus", 2);
    chorus.addProgressionRef(SectionProgressionRef::create(progB.getId(), 0, ""));
    song.addSection(chorus);

    // Re-read sections from model (same IDs)
    const auto sections = song.getSections();
    const auto progressions = song.getProgressions();
    bool ok = true;
    ok &= expect(sections.size() == 2, "P1: should have 2 sections");

    // Mirror the UI beat-width calculation
    double totalBeats = 0.0;
    std::vector<double> sectionBeats;
    for (const auto& section : sections)
    {
        double beats = 0.0;
        for (const auto& ref : section.getProgressionRefs())
            for (const auto& prog : progressions)
                if (prog.getId() == ref.getProgressionId())
                {
                    double len = prog.getLengthBeats();
                    if (len <= 0.0) len = 8.0;
                    beats += len;
                    break;
                }
        if (beats <= 0.0) beats = 8.0;
        const double withRepeats = beats * static_cast<double>(juce::jmax(1, section.getRepeatCount()));
        sectionBeats.push_back(withRepeats);
        totalBeats += withRepeats;
    }

    ok &= expect(std::abs(totalBeats - 64.0) < 0.001, "P1: total beats should be 64");
    ok &= expect(std::abs(sectionBeats[0] - 32.0) < 0.001, "P1: Verse should be 32 beats (4 × 8)");
    ok &= expect(std::abs(sectionBeats[1] - 32.0) < 0.001, "P1: Chorus should be 32 beats (2 × 16)");

    const float verseWidth = static_cast<float>(sectionBeats[0] / totalBeats);
    const float chorusWidth = static_cast<float>(sectionBeats[1] / totalBeats);
    ok &= expect(std::abs(verseWidth - 0.5f) < 0.001f, "P1: Verse width fraction should be 0.5");
    ok &= expect(std::abs(chorusWidth - 0.5f) < 0.001f, "P1: Chorus width fraction should be 0.5");
    return ok;
}

// P2 – unequal section durations produce correct relative width fractions
bool testSectionUnequalWidths()
{
    using namespace setle::model;

    auto song = Song::create("UnequalWidths", 120.0);

    auto prog = Progression::create("Loop", "C", "ionian");
    prog.setLengthBeats(4.0);
    song.addProgression(prog);

    // Intro: 1 repeat = 4 beats
    auto intro = Section::create("Intro", 1);
    intro.addProgressionRef(SectionProgressionRef::create(prog.getId(), 0, ""));
    song.addSection(intro);

    // Chorus: 3 repeats = 12 beats
    auto chorus = Section::create("Chorus", 3);
    chorus.addProgressionRef(SectionProgressionRef::create(prog.getId(), 0, ""));
    song.addSection(chorus);

    const auto sections = song.getSections();
    const auto progressions = song.getProgressions();
    bool ok = true;
    ok &= expect(sections.size() == 2, "P2: should have 2 sections");

    double totalBeats = 0.0;
    std::vector<double> sectionBeats;
    for (const auto& section : sections)
    {
        double beats = 0.0;
        for (const auto& ref : section.getProgressionRefs())
            for (const auto& prog2 : progressions)
                if (prog2.getId() == ref.getProgressionId())
                {
                    double len = prog2.getLengthBeats();
                    if (len <= 0.0) len = 8.0;
                    beats += len;
                    break;
                }
        if (beats <= 0.0) beats = 8.0;
        const double withRepeats = beats * static_cast<double>(juce::jmax(1, section.getRepeatCount()));
        sectionBeats.push_back(withRepeats);
        totalBeats += withRepeats;
    }

    ok &= expect(std::abs(totalBeats - 16.0) < 0.001, "P2: total should be 16 beats");
    const float introFraction = static_cast<float>(sectionBeats[0] / totalBeats);
    const float chorusFraction = static_cast<float>(sectionBeats[1] / totalBeats);
    ok &= expect(std::abs(introFraction - 0.25f) < 0.001f, "P2: Intro fraction should be 0.25");
    ok &= expect(std::abs(chorusFraction - 0.75f) < 0.001f, "P2: Chorus fraction should be 0.75");
    return ok;
}

// P3 – chord block widths are proportional to durationBeats
bool testChordBeatWidthProportionality()
{
    using namespace setle::model;

    auto progression = Progression::create("Test", "C", "ionian");

    auto c = Chord::create("Cmaj7", "major7", 60);
    c.setDurationBeats(4.0);
    progression.addChord(c);

    auto dm = Chord::create("Dm7", "minor7", 62);
    dm.setDurationBeats(8.0);
    progression.addChord(dm);

    auto g7 = Chord::create("G7", "dominant7", 67);
    g7.setDurationBeats(4.0);
    progression.addChord(g7);

    const auto chords = progression.getChords();
    bool ok = true;
    ok &= expect(chords.size() == 3, "P3: should have 3 chords");

    double totalDuration = 0.0;
    std::vector<double> durations;
    for (const auto& chord : chords)
    {
        double d = chord.getDurationBeats();
        if (d <= 0.0) d = 1.0;
        durations.push_back(d);
        totalDuration += d;
    }

    ok &= expect(std::abs(totalDuration - 16.0) < 0.001, "P3: total duration should be 16 beats");
    ok &= expect(std::abs(static_cast<float>(durations[0] / totalDuration) - 0.25f) < 0.001f, "P3: Cmaj7 fraction should be 0.25");
    ok &= expect(std::abs(static_cast<float>(durations[1] / totalDuration) - 0.50f) < 0.001f, "P3: Dm7 fraction should be 0.50");
    ok &= expect(std::abs(static_cast<float>(durations[2] / totalDuration) - 0.25f) < 0.001f, "P3: G7 fraction should be 0.25");
    return ok;
}

// P4 – zero chord duration falls back to 1.0 beat in width calculation
bool testChordZeroDurationFallback()
{
    using namespace setle::model;

    auto progression = Progression::create("FallbackTest", "C", "ionian");

    // Explicitly set duration to 0 to trigger the fallback guard
    auto c1 = Chord::create("C", "major", 60);
    c1.setDurationBeats(0.0);
    auto c2 = Chord::create("G", "major", 67);
    c2.setDurationBeats(0.0);
    progression.addChord(c1);
    progression.addChord(c2);

    const auto chords = progression.getChords();
    bool ok = true;
    ok &= expect(chords.size() == 2, "P4: should have 2 chords");

    // Verify getDurationBeats returns 0.0 (the stored value, not the default)
    ok &= expect(std::abs(chords[0].getDurationBeats()) < 0.001, "P4: c1 duration should be 0.0");
    ok &= expect(std::abs(chords[1].getDurationBeats()) < 0.001, "P4: c2 duration should be 0.0");

    double totalDuration = 0.0;
    for (const auto& chord : chords)
    {
        double d = chord.getDurationBeats();
        if (d <= 0.0) d = 1.0;
        totalDuration += d;
    }

    // Both fall back to 1.0 → total = 2.0, each fraction = 0.5
    ok &= expect(std::abs(totalDuration - 2.0) < 0.001, "P4: fallback total should be 2.0");
    for (const auto& chord : chords)
    {
        double d = chord.getDurationBeats();
        if (d <= 0.0) d = 1.0;
        const float fraction = static_cast<float>(d / totalDuration);
        ok &= expect(std::abs(fraction - 0.5f) < 0.001f, "P4: each fallback chord fraction should be 0.5");
    }
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= testSectionProgressionRefDefaults();
    ok &= testSchemaMigrationAddsRepeatFields();
    ok &= testXmlRoundTrip();
    ok &= testBinaryAndGzipRoundTrip();
    ok &= testInvalidNonXmlDataRejected();
    ok &= testSongTopLevelFieldsXmlRoundtrip();      // A1
    ok &= testSongTopLevelFieldsBinaryRoundtrip();   // A2
    ok &= testSongTopLevelFieldsGzipRoundtrip();     // A3
    ok &= testProgressionFieldsXmlRoundtrip();       // A4
    ok &= testSectionFieldsXmlRoundtrip();           // A5
    ok &= testTransitionFieldsXmlRoundtrip();        // A6+A7
    ok &= testEmptyBinaryPayloadRejected();          // A8
    ok &= testChordFieldsXmlRoundtrip();             // A9
    ok &= testSchemaVersionStampedInXml();           // A10
    ok &= testSnapshotRestoresPreviousTitle();       // U1
    ok &= testSnapshotRedoReappliesMutation();       // U2
    ok &= testMultiCycleUndoRedoConsistency();       // U3
    ok &= testUndoAfterSaveDoesNotCorruptFile();     // U5
    ok &= testSectionBeatWidthProportionality();     // P1
    ok &= testSectionUnequalWidths();                // P2
    ok &= testChordBeatWidthProportionality();       // P3
    ok &= testChordZeroDurationFallback();           // P4

    if (!ok)
        return 1;

    juce::Logger::writeToLog("All setle_model_tests passed.");
    return 0;
}
