#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <tracktion_engine/tracktion_engine.h>

#include "../model/SetleSongModel.h"

#include <vector>

namespace te = tracktion::engine;

namespace setle::gridroll
{

class NoteModeView : public juce::Component,
                     private juce::Timer
{
public:
    explicit NoteModeView(model::Song& song, te::Edit& edit);

    void setTargetProgression(const juce::String& progressionId);
    void setPlayheadBeat(double beat);

    void setVisibleBeatRange(double startBeat, double endBeat);
    void setVisiblePitchRange(int lowestMidi, int highestMidi);

    double getVisibleStartBeat() const noexcept { return visibleStartBeat; }
    double getVisibleEndBeat() const noexcept { return visibleEndBeat; }
    int getLowestVisibleMidi() const noexcept { return lowestMidiVisible; }
    int getHighestVisibleMidi() const noexcept { return highestMidiVisible; }

    void scrollToBeat(double beat, bool centreBeat);
    void scrollByBeats(double deltaBeats);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    std::function<void()> onNotesChanged;
    std::function<void(double startBeat, double endBeat)> onVisibleBeatRangeChanged;
    std::function<void(int lowMidi, int highMidi)> onVisiblePitchRangeChanged;
    std::function<void(const juce::String&)> onStatusMessage;

private:
    struct NoteEntry
    {
        juce::String noteId;
        double startBeat { 0.0 };
        double durationBeats { 1.0 };
        int midiNote { 60 };
        float velocity { 0.8f };
        juce::String chordId;
        juce::String chordSymbol;
        bool isChordTone { false };
        bool isScaleTone { false };
    };

    struct ChordBoundary
    {
        juce::String chordId;
        double startBeat { 0.0 };
        double endBeat { 0.0 };
        juce::String symbol;
        juce::String romanNumeral;
        juce::String function;
    };

    void timerCallback() override;
    void rebuildNoteCache();
    void updateActiveChordPitchClasses();

    juce::Rectangle<int> keyBounds() const;
    juce::Rectangle<int> gridBounds() const;
    void paintPianoKeys(juce::Graphics& g, juce::Rectangle<int> bounds);

    double pxPerBeat() const;
    double pxPerPitch() const;
    double progressionLengthBeats() const;
    double visibleBeatSpan() const;
    int visiblePitchSpan() const;
    double clampBeatSpan(double span) const;

    double xToBeat(int x) const;
    int yToMidi(int y) const;
    double snapToBeatGrid(double beat) const;
    int snapMidiToScale(int midiNote) const;

    int findNoteIndexAt(juce::Point<int> pos) const;
    juce::Rectangle<float> noteRectFor(const NoteEntry& note) const;
    int noteRightEdgeX(const NoteEntry& note) const;

    const ChordBoundary* findChordBoundaryForBeat(double beat) const;
    bool deleteNoteById(const juce::String& chordId, const juce::String& noteId);
    bool updateNoteInModel(const juce::String& sourceChordId,
                           const juce::String& noteId,
                           double absoluteStartBeat,
                           double durationBeats,
                           int midiNote,
                           float velocity);
    juce::String createNoteAtBeatAndPitch(double beat, int midiNote);
    void auditionNote(int midiNote);

    void emitBeatRangeChanged();
    void emitPitchRangeChanged();

    model::Song& song;
    te::Edit& edit;

    juce::String progressionId;
    std::vector<NoteEntry> notes;
    std::vector<ChordBoundary> chordBoundaries;
    std::vector<int> scalePitchClasses;
    std::vector<int> activeChordPitchClasses;

    double playheadBeat { 0.0 };
    double visibleStartBeat { 0.0 };
    double visibleEndBeat { 16.0 };
    int lowestMidiVisible { 48 };
    int highestMidiVisible { 72 };

    static constexpr int kPianoKeyWidth = 36;

    juce::String selectedNoteId;
    bool isDragging { false };
    bool isResizing { false };
    bool dragChanged { false };
    juce::Point<int> dragStart;
    double dragOrigStartBeat { 0.0 };
    double dragOrigDuration { 0.25 };
    int dragOrigMidiNote { 60 };
    juce::String dragNoteId;
    juce::String dragSourceChordId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteModeView)
};

} // namespace setle::gridroll

