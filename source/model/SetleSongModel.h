#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <optional>
#include <vector>

namespace setle::model
{

enum class StorageFormat
{
    xml,
    binary,
    gzipBinary
};

struct Schema
{
    static constexpr int version = 1;

    static inline const juce::Identifier songType { "setleSong" };
    static inline const juce::Identifier noteType { "note" };
    static inline const juce::Identifier chordType { "chord" };
    static inline const juce::Identifier progressionType { "progression" };
    static inline const juce::Identifier sectionType { "section" };
    static inline const juce::Identifier transitionType { "transition" };
    static inline const juce::Identifier sectionProgressionRefType { "sectionProgressionRef" };

    static inline const juce::Identifier progressionsContainerType { "progressions" };
    static inline const juce::Identifier sectionsContainerType { "sections" };
    static inline const juce::Identifier transitionsContainerType { "transitions" };

    static inline const juce::Identifier idProp { "id" };
    static inline const juce::Identifier nameProp { "name" };
    static inline const juce::Identifier titleProp { "title" };
    static inline const juce::Identifier schemaVersionProp { "schemaVersion" };

    static inline const juce::Identifier pitchProp { "pitch" };
    static inline const juce::Identifier velocityProp { "velocity" };
    static inline const juce::Identifier startBeatsProp { "startBeats" };
    static inline const juce::Identifier durationBeatsProp { "durationBeats" };
    static inline const juce::Identifier channelProp { "channel" };

    static inline const juce::Identifier symbolProp { "symbol" };
    static inline const juce::Identifier qualityProp { "quality" };
    static inline const juce::Identifier functionProp { "function" };
    static inline const juce::Identifier sourceProp { "source" };
    static inline const juce::Identifier confidenceProp { "confidence" };
    static inline const juce::Identifier rootMidiProp { "rootMidi" };
    static inline const juce::Identifier tensionProp { "tension" };

    static inline const juce::Identifier keyProp { "key" };
    static inline const juce::Identifier modeProp { "mode" };
    static inline const juce::Identifier lengthBeatsProp { "lengthBeats" };
    static inline const juce::Identifier variantOfProp { "variantOf" };

    static inline const juce::Identifier repeatCountProp { "repeatCount" };
    static inline const juce::Identifier progressionIdProp { "progressionId" };
    static inline const juce::Identifier orderIndexProp { "orderIndex" };
    static inline const juce::Identifier variantNameProp { "variantName" };
    static inline const juce::Identifier repeatScopeProp { "repeatScope" };
    // NOTE: repeatSelection is redundant with repeatScope + repeatIndices.
    // Preserved for schema v1 compatibility. Do not add new reads of this
    // field. Scheduled for removal in schema v2.
    static inline const juce::Identifier repeatSelectionProp { "repeatSelection" };
    static inline const juce::Identifier repeatIndicesProp { "repeatIndices" };

    static inline const juce::Identifier fromSectionIdProp { "fromSectionId" };
    static inline const juce::Identifier toSectionIdProp { "toSectionId" };
    static inline const juce::Identifier strategyProp { "strategy" };
    static inline const juce::Identifier bpmProp { "bpm" };
};

class Note
{
public:
    Note() = default;
    explicit Note(juce::ValueTree tree);

    static Note create(int pitch = 60, float velocity = 0.8f, double startBeats = 0.0, double durationBeats = 1.0, int channel = 1);

    bool isValid() const;
    juce::ValueTree valueTree() const;

    juce::String getId() const;
    int getPitch() const;
    float getVelocity() const;
    double getStartBeats() const;
    double getDurationBeats() const;
    int getChannel() const;

    void setPitch(int pitch);
    void setVelocity(float velocity);
    void setStartBeats(double beats);
    void setDurationBeats(double beats);
    void setChannel(int channel);

private:
    juce::ValueTree state;
};

class Chord
{
public:
    Chord() = default;
    explicit Chord(juce::ValueTree tree);

    static Chord create(const juce::String& symbol = "Cmaj7", const juce::String& quality = "major7", int rootMidi = 60);

    bool isValid() const;
    juce::ValueTree valueTree() const;

    juce::String getId() const;
    juce::String getName() const;
    juce::String getSymbol() const;
    juce::String getQuality() const;
    juce::String getFunction() const;
    juce::String getSource() const;
    float getConfidence() const;
    int getRootMidi() const;
    double getStartBeats() const;
    double getDurationBeats() const;
    int getTension() const;

    void setName(const juce::String& name);
    void setSymbol(const juce::String& symbol);
    void setQuality(const juce::String& quality);
    void setFunction(const juce::String& chordFunction);
    void setSource(const juce::String& source);
    void setConfidence(float confidence);
    void setRootMidi(int rootMidi);
    void setStartBeats(double beats);
    void setDurationBeats(double beats);
    void setTension(int tension);

    void addNote(const Note& note);
    std::vector<Note> getNotes() const;

private:
    juce::ValueTree state;
};

class Progression
{
public:
    Progression() = default;
    explicit Progression(juce::ValueTree tree);

    static Progression create(const juce::String& name, const juce::String& key = "C", const juce::String& mode = "ionian");

    bool isValid() const;
    juce::ValueTree valueTree() const;

    juce::String getId() const;
    juce::String getName() const;
    juce::String getKey() const;
    juce::String getMode() const;
    double getLengthBeats() const;
    juce::String getVariantOf() const;

    void setName(const juce::String& name);
    void setKey(const juce::String& key);
    void setMode(const juce::String& mode);
    void setLengthBeats(double beats);
    void setVariantOf(const juce::String& baseProgressionId);

    void addChord(const Chord& chord);
    std::vector<Chord> getChords() const;

private:
    juce::ValueTree state;
};

class SectionProgressionRef
{
public:
    SectionProgressionRef() = default;
    explicit SectionProgressionRef(juce::ValueTree tree);

    static SectionProgressionRef create(const juce::String& progressionId, int orderIndex, const juce::String& variantName = "");

    bool isValid() const;
    juce::ValueTree valueTree() const;

    juce::String getProgressionId() const;
    int getOrderIndex() const;
    juce::String getVariantName() const;
    juce::String getRepeatScope() const;
    juce::String getRepeatSelection() const;
    juce::String getRepeatIndices() const;

    void setProgressionId(const juce::String& progressionId);
    void setOrderIndex(int orderIndex);
    void setVariantName(const juce::String& variantName);
    void setRepeatScope(const juce::String& repeatScope);
    void setRepeatSelection(const juce::String& repeatSelection);
    void setRepeatIndices(const juce::String& repeatIndices);

private:
    juce::ValueTree state;
};

class Section
{
public:
    Section() = default;
    explicit Section(juce::ValueTree tree);

    static Section create(const juce::String& name, int repeatCount = 4);

    bool isValid() const;
    juce::ValueTree valueTree() const;

    juce::String getId() const;
    juce::String getName() const;
    int getRepeatCount() const;

    void setName(const juce::String& name);
    void setRepeatCount(int repeats);

    void addProgressionRef(const SectionProgressionRef& progressionRef);
    std::vector<SectionProgressionRef> getProgressionRefs() const;

private:
    juce::ValueTree state;
};

class Transition
{
public:
    Transition() = default;
    explicit Transition(juce::ValueTree tree);

    static Transition create(const juce::String& fromSectionId, const juce::String& toSectionId, const juce::String& strategy = "dominantPivot");

    bool isValid() const;
    juce::ValueTree valueTree() const;

    juce::String getId() const;
    juce::String getName() const;
    juce::String getFromSectionId() const;
    juce::String getToSectionId() const;
    juce::String getStrategy() const;
    int getTargetTension() const;

    void setName(const juce::String& name);
    void setFromSectionId(const juce::String& sectionId);
    void setToSectionId(const juce::String& sectionId);
    void setStrategy(const juce::String& strategy);
    void setTargetTension(int tension);

private:
    juce::ValueTree state;
};

class Song
{
public:
    Song() = default;
    explicit Song(juce::ValueTree tree);

    static Song create(const juce::String& title, double bpm = 120.0);
    static std::optional<Song> loadFromFile(const juce::File& file, StorageFormat format = StorageFormat::xml);

    bool isValid() const;
    juce::ValueTree valueTree() const;

    int getSchemaVersion() const;
    juce::String getTitle() const;
    double getBpm() const;

    void setTitle(const juce::String& title);
    void setBpm(double bpm);

    void addProgression(const Progression& progression);
    void addSection(const Section& section);
    void addTransition(const Transition& transition);

    std::vector<Progression> getProgressions() const;
    std::vector<Section> getSections() const;
    std::vector<Transition> getTransitions() const;

    std::optional<Progression> findProgressionById(const juce::String& id) const;
    std::optional<Section> findSectionById(const juce::String& id) const;

    juce::Result saveToFile(const juce::File& file, StorageFormat format = StorageFormat::xml) const;

private:
    juce::ValueTree getOrCreateContainer(const juce::Identifier& containerType);
    juce::ValueTree getContainer(const juce::Identifier& containerType) const;
    void ensureSchema();

    juce::ValueTree state;
};

} // namespace setle::model

