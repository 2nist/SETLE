#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GridRollCell.h"
#include "../model/SetleSongModel.h"

namespace setle::gridroll
{

/**
 * Expanded note-editing panel that opens below a selected chord cell (Phase 14B).
 *
 * Height: 160px. Left 28px: mini piano keyboard strip.
 * Right: note grid (time × pitch) spanning 2 octaves centred on chord root.
 * Scale tones in key are highlighted. Chord tones get a warmer tint.
 */
class NoteDetailView : public juce::Component
{
public:
    static constexpr int kHeight      = 160;
    static constexpr int kKeyWidth    = 28;
    static constexpr int kOctaveSpan  = 2;   // number of octaves shown
    static constexpr int kNoteRows    = kOctaveSpan * 12;

    /** Called after any note edit so the parent can re-run ChordIdentifier. */
    std::function<void()> onNotesChanged;

    /** Called when the view should be closed (Escape or click outside). */
    std::function<void()> onCloseRequested;

    /** Called when a status message should be shown. */
    std::function<void(const juce::String&)> onStatusMessage;

    explicit NoteDetailView(model::Song& song);

    /**
     * Open the view for a chord cell.
     * @param cell       The chord cell to edit notes for.
     * @param sessionKey The current session key (e.g. "C").
     * @param sessionMode The current mode (e.g. "ionian").
     */
    void loadCell(const GridRollCell& cell,
                  const juce::String& sessionKey,
                  const juce::String& sessionMode);

    const std::vector<model::Note>& getNotes() const { return notes; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    // ---------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------
    juce::Rectangle<int> noteGridBounds() const;
    juce::Rectangle<int> keyBounds() const;

    /** pixel row for a given midi pitch (bottom = low pitch) */
    int pitchToRow(int pitch) const;
    int rowToPitch(int row) const;
    double pixelToTime(int px) const;
    int    timeToPx(double beat) const;

    bool isChordTone(int pitch) const;
    bool isScaleTone(int pitch) const;

    struct NoteRect
    {
        int noteIndex { -1 };
        juce::Rectangle<int> bounds;
        bool resizeHandle { false };
    };
    NoteRect hitTestGrid(juce::Point<int> pos) const;
    void rebuildNoteRects();
    void showNoteContext(int noteIdx, juce::Point<int> screenPos);

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    model::Song& song;
    GridRollCell sourceCell;
    std::vector<model::Note> notes;
    std::vector<juce::Rectangle<int>> noteRects;

    int  selectedNoteIdx { -1 };
    int  lowestPitch     { 48 };   // C3 — recomputed on loadCell

    juce::String sessionKey  { "C" };
    juce::String sessionMode { "ionian" };
    std::vector<int> scaleIntervals; // 0–11 pitch classes in key
    std::vector<int> chordTones;     // pitch class indices from cell

    // Drag state
    enum class DragKind { None, AddNote, MoveNote, ResizeNote };
    DragKind     dragKind      { DragKind::None };
    int          dragNoteIdx   { -1 };
    double       dragOrigStart { 0.0 };
    int          dragOrigPitch { 60 };
    juce::Point<int> dragStartPos;
    double       dragOrigDuration { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteDetailView)
};

} // namespace setle::gridroll
