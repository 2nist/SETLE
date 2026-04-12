#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GridRollCell.h"
#include "../model/SetleSongModel.h"

namespace setle::gridroll
{

/**
 * Step-sequencer chord row for the GridRoll editor (Phase 14A).
 *
 * Displays a horizontal strip of GridRollCell blocks. Each block is proportional
 * to durationBeats and shows function color, Roman numeral, chord symbol, and
 * a velocity bar. Supports drag-resize, drag-reorder, Ctrl+drag-duplicate, and
 * a right-click context menu.
 */
class ChordGridView : public juce::Component
{
public:
    static constexpr int kHeight = 80;

    /** Called when a cell is selected (single-click). */
    std::function<void(int cellIndex)> onCellSelected;

    /** Called when a cell is double-clicked — expand into note detail. */
    std::function<void(int cellIndex)> onCellDoubleClicked;

    /** Called after any structural change (resize, reorder, divide, delete). */
    std::function<void()> onCellsChanged;

    /** Called when user requests adding a chord from the diatonic palette. */
    std::function<void(const juce::String& chordSymbol)> onAddChordRequested;

    /** Called when a status message should be shown. */
    std::function<void(const juce::String&)> onStatusMessage;

    explicit ChordGridView(model::Song& song);

    /** Replace all cells for one progression. */
    void loadProgression(const juce::String& progressionId);

    /** Return the current cell list (reflects edits). */
    const std::vector<GridRollCell>& getCells() const { return cells; }

    /** Where the playhead is, as a fraction 0–1 across the total beat range. */
    void setPlayheadBeat(double beat);

    /** Snap grid setting inherited from workspace. */
    void setTheorySnap(const juce::String& snap) { theorySnap = snap; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    // ---------------------------------------------------------------
    // Internal helpers
    // ---------------------------------------------------------------
    struct CellLayout
    {
        int cellIndex { -1 };
        juce::Rectangle<int> bounds;
        bool isRightEdge { false };  // mouse hit the resize handle
    };

    CellLayout hitTest(juce::Point<int> pos) const;
    double snapBeat(double rawBeat) const;
    double beatsPerPixel() const;
    double totalBeats() const;
    void   rebuildLayoutCache();
    void   showCellContextMenu(int cellIndex, juce::Point<int> screenPos);
    void   showAddChordPopover(juce::Point<int> screenPos);
    void   divideCells(int cellIndex, int n);
    juce::Colour functionColour(const juce::String& fn) const;
    void   paintCell(juce::Graphics& g, const GridRollCell& cell,
                     juce::Rectangle<int> bounds, bool selected) const;

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    model::Song& song;
    juce::String progressionId;
    std::vector<GridRollCell> cells;
    std::vector<juce::Rectangle<int>> cellBounds; // parallel to cells

    int  selectedCellIndex { -1 };
    double playheadBeat    { 0.0 };
    juce::String theorySnap { "1/16" };

    // Drag state
    enum class DragKind { None, ResizeRight, MoveCell };
    DragKind     dragKind      { DragKind::None };
    int          dragCellIndex { -1 };
    double       dragStartBeat { 0.0 };
    double       dragOrigDuration { 0.0 };
    double       dragOrigStart    { 0.0 };
    bool         dragIsDuplicate  { false };
    juce::Point<int> dragStartPos;

    // Add-chord "+" button
    juce::TextButton addChordButton { "+" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordGridView)
};

} // namespace setle::gridroll
