#include "SetleSongModel.h"

namespace setle::model
{
namespace
{
juce::String makeId()
{
    return juce::Uuid().toString();
}

juce::String getString(const juce::ValueTree& tree, const juce::Identifier& property, const juce::String& defaultValue = {})
{
    return tree.getProperty(property, defaultValue).toString();
}

int getInt(const juce::ValueTree& tree, const juce::Identifier& property, int defaultValue)
{
    return static_cast<int>(tree.getProperty(property, defaultValue));
}

double getDouble(const juce::ValueTree& tree, const juce::Identifier& property, double defaultValue)
{
    return static_cast<double>(tree.getProperty(property, defaultValue));
}

float getFloat(const juce::ValueTree& tree, const juce::Identifier& property, float defaultValue)
{
    return static_cast<float>(tree.getProperty(property, defaultValue));
}
} // namespace

Note::Note(juce::ValueTree tree)
    : state(std::move(tree))
{
}

Note Note::create(int pitch, float velocity, double startBeats, double durationBeats, int channel)
{
    juce::ValueTree tree(Schema::noteType);
    tree.setProperty(Schema::idProp, makeId(), nullptr);
    tree.setProperty(Schema::pitchProp, pitch, nullptr);
    tree.setProperty(Schema::velocityProp, velocity, nullptr);
    tree.setProperty(Schema::startBeatsProp, startBeats, nullptr);
    tree.setProperty(Schema::durationBeatsProp, durationBeats, nullptr);
    tree.setProperty(Schema::channelProp, channel, nullptr);
    return Note(tree);
}

bool Note::isValid() const { return state.isValid() && state.hasType(Schema::noteType); }
juce::ValueTree Note::valueTree() const { return state; }

juce::String Note::getId() const { return getString(state, Schema::idProp); }
int Note::getPitch() const { return getInt(state, Schema::pitchProp, 60); }
float Note::getVelocity() const { return getFloat(state, Schema::velocityProp, 0.8f); }
double Note::getStartBeats() const { return getDouble(state, Schema::startBeatsProp, 0.0); }
double Note::getDurationBeats() const { return getDouble(state, Schema::durationBeatsProp, 1.0); }
int Note::getChannel() const { return getInt(state, Schema::channelProp, 1); }

void Note::setPitch(int pitch) { state.setProperty(Schema::pitchProp, pitch, nullptr); }
void Note::setVelocity(float velocity) { state.setProperty(Schema::velocityProp, velocity, nullptr); }
void Note::setStartBeats(double beats) { state.setProperty(Schema::startBeatsProp, beats, nullptr); }
void Note::setDurationBeats(double beats) { state.setProperty(Schema::durationBeatsProp, beats, nullptr); }
void Note::setChannel(int channel) { state.setProperty(Schema::channelProp, channel, nullptr); }

Chord::Chord(juce::ValueTree tree)
    : state(std::move(tree))
{
}

Chord Chord::create(const juce::String& symbol, const juce::String& quality, int rootMidi)
{
    juce::ValueTree tree(Schema::chordType);
    tree.setProperty(Schema::idProp, makeId(), nullptr);
    tree.setProperty(Schema::nameProp, symbol, nullptr);
    tree.setProperty(Schema::symbolProp, symbol, nullptr);
    tree.setProperty(Schema::qualityProp, quality, nullptr);
    tree.setProperty(Schema::functionProp, "", nullptr);
    tree.setProperty(Schema::sourceProp, "manual", nullptr);
    tree.setProperty(Schema::rootMidiProp, rootMidi, nullptr);
    tree.setProperty(Schema::startBeatsProp, 0.0, nullptr);
    tree.setProperty(Schema::durationBeatsProp, 4.0, nullptr);
    tree.setProperty(Schema::tensionProp, 0, nullptr);
    return Chord(tree);
}

bool Chord::isValid() const { return state.isValid() && state.hasType(Schema::chordType); }
juce::ValueTree Chord::valueTree() const { return state; }

juce::String Chord::getId() const { return getString(state, Schema::idProp); }
juce::String Chord::getName() const { return getString(state, Schema::nameProp); }
juce::String Chord::getSymbol() const { return getString(state, Schema::symbolProp); }
juce::String Chord::getQuality() const { return getString(state, Schema::qualityProp); }
juce::String Chord::getFunction() const { return getString(state, Schema::functionProp); }
juce::String Chord::getSource() const { return getString(state, Schema::sourceProp, "manual"); }
int Chord::getRootMidi() const { return getInt(state, Schema::rootMidiProp, 60); }
double Chord::getStartBeats() const { return getDouble(state, Schema::startBeatsProp, 0.0); }
double Chord::getDurationBeats() const { return getDouble(state, Schema::durationBeatsProp, 4.0); }
int Chord::getTension() const { return getInt(state, Schema::tensionProp, 0); }

void Chord::setName(const juce::String& name) { state.setProperty(Schema::nameProp, name, nullptr); }
void Chord::setSymbol(const juce::String& symbol) { state.setProperty(Schema::symbolProp, symbol, nullptr); }
void Chord::setQuality(const juce::String& quality) { state.setProperty(Schema::qualityProp, quality, nullptr); }
void Chord::setFunction(const juce::String& chordFunction) { state.setProperty(Schema::functionProp, chordFunction, nullptr); }
void Chord::setSource(const juce::String& source) { state.setProperty(Schema::sourceProp, source, nullptr); }
void Chord::setRootMidi(int rootMidi) { state.setProperty(Schema::rootMidiProp, rootMidi, nullptr); }
void Chord::setStartBeats(double beats) { state.setProperty(Schema::startBeatsProp, beats, nullptr); }
void Chord::setDurationBeats(double beats) { state.setProperty(Schema::durationBeatsProp, beats, nullptr); }
void Chord::setTension(int tension) { state.setProperty(Schema::tensionProp, tension, nullptr); }

void Chord::addNote(const Note& note)
{
    if (!isValid() || !note.isValid())
        return;

    state.appendChild(note.valueTree().createCopy(), nullptr);
}

std::vector<Note> Chord::getNotes() const
{
    std::vector<Note> notes;

    if (!isValid())
        return notes;

    for (const auto& child : state)
        if (child.hasType(Schema::noteType))
            notes.emplace_back(child);

    return notes;
}

Progression::Progression(juce::ValueTree tree)
    : state(std::move(tree))
{
}

Progression Progression::create(const juce::String& name, const juce::String& key, const juce::String& mode)
{
    juce::ValueTree tree(Schema::progressionType);
    tree.setProperty(Schema::idProp, makeId(), nullptr);
    tree.setProperty(Schema::nameProp, name, nullptr);
    tree.setProperty(Schema::keyProp, key, nullptr);
    tree.setProperty(Schema::modeProp, mode, nullptr);
    tree.setProperty(Schema::lengthBeatsProp, 16.0, nullptr);
    tree.setProperty(Schema::variantOfProp, "", nullptr);
    return Progression(tree);
}

bool Progression::isValid() const { return state.isValid() && state.hasType(Schema::progressionType); }
juce::ValueTree Progression::valueTree() const { return state; }

juce::String Progression::getId() const { return getString(state, Schema::idProp); }
juce::String Progression::getName() const { return getString(state, Schema::nameProp); }
juce::String Progression::getKey() const { return getString(state, Schema::keyProp); }
juce::String Progression::getMode() const { return getString(state, Schema::modeProp); }
double Progression::getLengthBeats() const { return getDouble(state, Schema::lengthBeatsProp, 16.0); }
juce::String Progression::getVariantOf() const { return getString(state, Schema::variantOfProp); }

void Progression::setName(const juce::String& name) { state.setProperty(Schema::nameProp, name, nullptr); }
void Progression::setKey(const juce::String& key) { state.setProperty(Schema::keyProp, key, nullptr); }
void Progression::setMode(const juce::String& mode) { state.setProperty(Schema::modeProp, mode, nullptr); }
void Progression::setLengthBeats(double beats) { state.setProperty(Schema::lengthBeatsProp, beats, nullptr); }
void Progression::setVariantOf(const juce::String& baseProgressionId) { state.setProperty(Schema::variantOfProp, baseProgressionId, nullptr); }

void Progression::addChord(const Chord& chord)
{
    if (!isValid() || !chord.isValid())
        return;

    state.appendChild(chord.valueTree().createCopy(), nullptr);
}

std::vector<Chord> Progression::getChords() const
{
    std::vector<Chord> chords;

    if (!isValid())
        return chords;

    for (const auto& child : state)
        if (child.hasType(Schema::chordType))
            chords.emplace_back(child);

    return chords;
}

SectionProgressionRef::SectionProgressionRef(juce::ValueTree tree)
    : state(std::move(tree))
{
}

SectionProgressionRef SectionProgressionRef::create(const juce::String& progressionId, int orderIndex, const juce::String& variantName)
{
    juce::ValueTree tree(Schema::sectionProgressionRefType);
    tree.setProperty(Schema::progressionIdProp, progressionId, nullptr);
    tree.setProperty(Schema::orderIndexProp, orderIndex, nullptr);
    tree.setProperty(Schema::variantNameProp, variantName, nullptr);
    tree.setProperty(Schema::repeatScopeProp, "all", nullptr);
    tree.setProperty(Schema::repeatSelectionProp, "all", nullptr);
    tree.setProperty(Schema::repeatIndicesProp, "", nullptr);
    return SectionProgressionRef(tree);
}

bool SectionProgressionRef::isValid() const { return state.isValid() && state.hasType(Schema::sectionProgressionRefType); }
juce::ValueTree SectionProgressionRef::valueTree() const { return state; }

juce::String SectionProgressionRef::getProgressionId() const { return getString(state, Schema::progressionIdProp); }
int SectionProgressionRef::getOrderIndex() const { return getInt(state, Schema::orderIndexProp, 0); }
juce::String SectionProgressionRef::getVariantName() const { return getString(state, Schema::variantNameProp); }
juce::String SectionProgressionRef::getRepeatScope() const { return getString(state, Schema::repeatScopeProp, "all"); }
juce::String SectionProgressionRef::getRepeatSelection() const { return getString(state, Schema::repeatSelectionProp, "all"); }
juce::String SectionProgressionRef::getRepeatIndices() const { return getString(state, Schema::repeatIndicesProp, ""); }

void SectionProgressionRef::setProgressionId(const juce::String& progressionId) { state.setProperty(Schema::progressionIdProp, progressionId, nullptr); }
void SectionProgressionRef::setOrderIndex(int orderIndex) { state.setProperty(Schema::orderIndexProp, orderIndex, nullptr); }
void SectionProgressionRef::setVariantName(const juce::String& variantName) { state.setProperty(Schema::variantNameProp, variantName, nullptr); }
void SectionProgressionRef::setRepeatScope(const juce::String& repeatScope) { state.setProperty(Schema::repeatScopeProp, repeatScope, nullptr); }
void SectionProgressionRef::setRepeatSelection(const juce::String& repeatSelection) { state.setProperty(Schema::repeatSelectionProp, repeatSelection, nullptr); }
void SectionProgressionRef::setRepeatIndices(const juce::String& repeatIndices) { state.setProperty(Schema::repeatIndicesProp, repeatIndices, nullptr); }

Section::Section(juce::ValueTree tree)
    : state(std::move(tree))
{
}

Section Section::create(const juce::String& name, int repeatCount)
{
    juce::ValueTree tree(Schema::sectionType);
    tree.setProperty(Schema::idProp, makeId(), nullptr);
    tree.setProperty(Schema::nameProp, name, nullptr);
    tree.setProperty(Schema::repeatCountProp, repeatCount, nullptr);
    return Section(tree);
}

bool Section::isValid() const { return state.isValid() && state.hasType(Schema::sectionType); }
juce::ValueTree Section::valueTree() const { return state; }

juce::String Section::getId() const { return getString(state, Schema::idProp); }
juce::String Section::getName() const { return getString(state, Schema::nameProp); }
int Section::getRepeatCount() const { return getInt(state, Schema::repeatCountProp, 1); }

void Section::setName(const juce::String& name) { state.setProperty(Schema::nameProp, name, nullptr); }
void Section::setRepeatCount(int repeats) { state.setProperty(Schema::repeatCountProp, repeats, nullptr); }

void Section::addProgressionRef(const SectionProgressionRef& progressionRef)
{
    if (!isValid() || !progressionRef.isValid())
        return;

    state.appendChild(progressionRef.valueTree().createCopy(), nullptr);
}

std::vector<SectionProgressionRef> Section::getProgressionRefs() const
{
    std::vector<SectionProgressionRef> refs;

    if (!isValid())
        return refs;

    for (const auto& child : state)
        if (child.hasType(Schema::sectionProgressionRefType))
            refs.emplace_back(child);

    return refs;
}

Transition::Transition(juce::ValueTree tree)
    : state(std::move(tree))
{
}

Transition Transition::create(const juce::String& fromSectionId, const juce::String& toSectionId, const juce::String& strategy)
{
    juce::ValueTree tree(Schema::transitionType);
    tree.setProperty(Schema::idProp, makeId(), nullptr);
    tree.setProperty(Schema::nameProp, "Transition", nullptr);
    tree.setProperty(Schema::fromSectionIdProp, fromSectionId, nullptr);
    tree.setProperty(Schema::toSectionIdProp, toSectionId, nullptr);
    tree.setProperty(Schema::strategyProp, strategy, nullptr);
    tree.setProperty(Schema::tensionProp, 0, nullptr);
    return Transition(tree);
}

bool Transition::isValid() const { return state.isValid() && state.hasType(Schema::transitionType); }
juce::ValueTree Transition::valueTree() const { return state; }

juce::String Transition::getId() const { return getString(state, Schema::idProp); }
juce::String Transition::getName() const { return getString(state, Schema::nameProp); }
juce::String Transition::getFromSectionId() const { return getString(state, Schema::fromSectionIdProp); }
juce::String Transition::getToSectionId() const { return getString(state, Schema::toSectionIdProp); }
juce::String Transition::getStrategy() const { return getString(state, Schema::strategyProp); }
int Transition::getTargetTension() const { return getInt(state, Schema::tensionProp, 0); }

void Transition::setName(const juce::String& name) { state.setProperty(Schema::nameProp, name, nullptr); }
void Transition::setFromSectionId(const juce::String& sectionId) { state.setProperty(Schema::fromSectionIdProp, sectionId, nullptr); }
void Transition::setToSectionId(const juce::String& sectionId) { state.setProperty(Schema::toSectionIdProp, sectionId, nullptr); }
void Transition::setStrategy(const juce::String& strategy) { state.setProperty(Schema::strategyProp, strategy, nullptr); }
void Transition::setTargetTension(int tension) { state.setProperty(Schema::tensionProp, tension, nullptr); }

Song::Song(juce::ValueTree tree)
    : state(std::move(tree))
{
    ensureSchema();
}

Song Song::create(const juce::String& title, double bpm)
{
    juce::ValueTree tree(Schema::songType);
    tree.setProperty(Schema::schemaVersionProp, Schema::version, nullptr);
    tree.setProperty(Schema::titleProp, title, nullptr);
    tree.setProperty(Schema::bpmProp, bpm, nullptr);
    tree.setProperty(Schema::sessionKeyProp, "C", nullptr);
    tree.setProperty(Schema::sessionModeProp, "ionian", nullptr);
    return Song(tree);
}

std::optional<Song> Song::loadFromFile(const juce::File& file, StorageFormat format)
{
    if (!file.existsAsFile())
        return std::nullopt;

    juce::ValueTree tree;

    if (format == StorageFormat::xml)
    {
        tree = juce::ValueTree::fromXml(file.loadFileAsString());
    }
    else
    {
        juce::MemoryBlock data;
        if (!file.loadFileAsData(data))
            return std::nullopt;

        if (data.getSize() == 0)
            return std::nullopt;

        if (format == StorageFormat::binary)
            tree = juce::ValueTree::readFromData(data.getData(), data.getSize());
        else
            tree = juce::ValueTree::readFromGZIPData(data.getData(), data.getSize());
    }

    if (!tree.isValid() || !tree.hasType(Schema::songType))
        return std::nullopt;

    return Song(tree);
}

bool Song::isValid() const
{
    return state.isValid() && state.hasType(Schema::songType);
}

juce::ValueTree Song::valueTree() const { return state; }

int Song::getSchemaVersion() const { return getInt(state, Schema::schemaVersionProp, 0); }
juce::String Song::getTitle() const { return getString(state, Schema::titleProp); }
double Song::getBpm() const { return getDouble(state, Schema::bpmProp, 120.0); }
juce::String Song::getSessionKey() const { return getString(state, Schema::sessionKeyProp, "C"); }
juce::String Song::getSessionMode() const { return getString(state, Schema::sessionModeProp, "ionian"); }

void Song::setTitle(const juce::String& title) { state.setProperty(Schema::titleProp, title, nullptr); }
void Song::setBpm(double bpm) { state.setProperty(Schema::bpmProp, bpm, nullptr); }
void Song::setSessionKey(const juce::String& sessionKey) { state.setProperty(Schema::sessionKeyProp, sessionKey, nullptr); }
void Song::setSessionMode(const juce::String& sessionMode) { state.setProperty(Schema::sessionModeProp, sessionMode, nullptr); }

void Song::addProgression(const Progression& progression)
{
    if (!isValid() || !progression.isValid())
        return;

    getOrCreateContainer(Schema::progressionsContainerType).appendChild(progression.valueTree().createCopy(), nullptr);
}

void Song::addSection(const Section& section)
{
    if (!isValid() || !section.isValid())
        return;

    getOrCreateContainer(Schema::sectionsContainerType).appendChild(section.valueTree().createCopy(), nullptr);
}

void Song::addTransition(const Transition& transition)
{
    if (!isValid() || !transition.isValid())
        return;

    getOrCreateContainer(Schema::transitionsContainerType).appendChild(transition.valueTree().createCopy(), nullptr);
}

std::vector<Progression> Song::getProgressions() const
{
    std::vector<Progression> progressions;
    auto container = getContainer(Schema::progressionsContainerType);

    if (!container.isValid())
        return progressions;

    for (const auto& child : container)
        if (child.hasType(Schema::progressionType))
            progressions.emplace_back(child);

    return progressions;
}

std::vector<Section> Song::getSections() const
{
    std::vector<Section> sections;
    auto container = getContainer(Schema::sectionsContainerType);

    if (!container.isValid())
        return sections;

    for (const auto& child : container)
        if (child.hasType(Schema::sectionType))
            sections.emplace_back(child);

    return sections;
}

std::vector<Transition> Song::getTransitions() const
{
    std::vector<Transition> transitions;
    auto container = getContainer(Schema::transitionsContainerType);

    if (!container.isValid())
        return transitions;

    for (const auto& child : container)
        if (child.hasType(Schema::transitionType))
            transitions.emplace_back(child);

    return transitions;
}

std::optional<Progression> Song::findProgressionById(const juce::String& id) const
{
    for (const auto& progression : getProgressions())
        if (progression.getId() == id)
            return progression;

    return std::nullopt;
}

std::optional<Section> Song::findSectionById(const juce::String& id) const
{
    for (const auto& section : getSections())
        if (section.getId() == id)
            return section;

    return std::nullopt;
}

juce::Result Song::saveToFile(const juce::File& file, StorageFormat format) const
{
    if (!isValid())
        return juce::Result::fail("Cannot save invalid song model");

    auto parent = file.getParentDirectory();
    if (parent != juce::File() && !parent.exists())
    {
        const auto createResult = parent.createDirectory();
        if (createResult.failed())
            return juce::Result::fail("Failed creating song directory: " + createResult.getErrorMessage());
    }

    if (format == StorageFormat::xml)
    {
        if (file.replaceWithText(state.toXmlString()))
            return juce::Result::ok();

        return juce::Result::fail("Failed writing XML song file");
    }

    juce::FileOutputStream stream(file);
    if (!stream.openedOk())
        return juce::Result::fail("Failed opening song file for write");

    if (format == StorageFormat::binary)
    {
        state.writeToStream(stream);
        stream.flush();
        if (stream.getStatus().failed())
            return juce::Result::fail("Failed writing binary song file");

        return juce::Result::ok();
    }

    {
        juce::GZIPCompressorOutputStream gzip(stream);
        state.writeToStream(gzip);
        gzip.flush();
    }

    stream.flush();
    if (stream.getStatus().failed())
        return juce::Result::fail("Failed writing GZIP song file");

    return juce::Result::ok();
}

juce::ValueTree Song::getOrCreateContainer(const juce::Identifier& containerType)
{
    for (const auto& child : state)
        if (child.hasType(containerType))
            return child;

    juce::ValueTree container(containerType);
    state.appendChild(container, nullptr);
    return container;
}

juce::ValueTree Song::getContainer(const juce::Identifier& containerType) const
{
    for (const auto& child : state)
        if (child.hasType(containerType))
            return child;

    return {};
}

void Song::ensureSchema()
{
    if (!isValid())
        return;

    const int persistedVersion = static_cast<int>(state.getProperty(Schema::schemaVersionProp, 0));

    // v0 → v1: baseline fields, container scaffolding, and scope metadata on
    // SectionProgressionRef nodes.  Applies to any file saved before
    // schemaVersion was stamped (persistedVersion == 0).
    //
    // Scope semantics introduced in v1:
    //   repeatScope      — "all" | "occurrence" | "repeat" | "selectedRepeats"
    //   repeatSelection  — "all" | "single" | "current" | "selected"
    //   repeatIndices    — comma-separated 1-based repeat numbers, empty == all
    if (persistedVersion < 1)
    {
        if (!state.hasProperty(Schema::titleProp))
            state.setProperty(Schema::titleProp, "Untitled", nullptr);

        if (!state.hasProperty(Schema::bpmProp))
            state.setProperty(Schema::bpmProp, 120.0, nullptr);

        // Add session key/mode defaults during v0→1 migration
        if (!state.hasProperty(Schema::sessionKeyProp))
            state.setProperty(Schema::sessionKeyProp, "C", nullptr);

        if (!state.hasProperty(Schema::sessionModeProp))
            state.setProperty(Schema::sessionModeProp, "ionian", nullptr);

        getOrCreateContainer(Schema::progressionsContainerType);
        auto sectionsContainer = getOrCreateContainer(Schema::sectionsContainerType);
        getOrCreateContainer(Schema::transitionsContainerType);

        for (auto sectionNode : sectionsContainer)
        {
            if (!sectionNode.hasType(Schema::sectionType))
                continue;

            for (auto sectionRefNode : sectionNode)
            {
                if (!sectionRefNode.hasType(Schema::sectionProgressionRefType))
                    continue;

                if (!sectionRefNode.hasProperty(Schema::repeatScopeProp))
                    sectionRefNode.setProperty(Schema::repeatScopeProp, "all", nullptr);

                if (!sectionRefNode.hasProperty(Schema::repeatSelectionProp))
                    sectionRefNode.setProperty(Schema::repeatSelectionProp, "all", nullptr);

                if (!sectionRefNode.hasProperty(Schema::repeatIndicesProp))
                    sectionRefNode.setProperty(Schema::repeatIndicesProp, "", nullptr);
            }
        }
    }
    else
    {
        // For files already at v1+, ensure the containers exist in case any
        // were omitted by an older serialiser.
        getOrCreateContainer(Schema::progressionsContainerType);
        getOrCreateContainer(Schema::sectionsContainerType);
        getOrCreateContainer(Schema::transitionsContainerType);

        // Also ensure session key/mode exist for v1 songs
        if (!state.hasProperty(Schema::sessionKeyProp))
            state.setProperty(Schema::sessionKeyProp, "C", nullptr);

        if (!state.hasProperty(Schema::sessionModeProp))
            state.setProperty(Schema::sessionModeProp, "ionian", nullptr);
    }

    // Future migration blocks go here:
    // if (persistedVersion < 2) { ... }

    state.setProperty(Schema::schemaVersionProp, Schema::version, nullptr);
}

} // namespace setle::model

