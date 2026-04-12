#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GridRollCell.h"

namespace setle::gridroll
{

/**
 * Multi-row drum step sequencer for the GridRoll editor (Phase 14C).
 *
 * Each row is 36px tall and represents one drum instrument (MIDI note).
 * The grid shows equal-width step cells across the visible beat range.
 * Rows can have independent pattern lengths for polyrhythm.
 */
class DrumGridView : public juce::Component
{
public:
    static constexpr int kRowHeight         = 36;
    static constexpr int kHeaderWidth       = 120;
    static constexpr int kDefaultSteps      = 16;
    static constexpr int kDefaultPatternBars = 1;

    struct DrumRow
    {
        juce::String name;
        int  midiNote     { 36 };
        bool muted        { false };
        bool soloed       { false };
        int  steps        { kDefaultSteps };      // steps per bar
        int  patternBars  { kDefaultPatternBars }; // bars per cycle
        int  subdivision  { kDefaultSteps };       // can override per-row

        // Cells: outer = beat index (steps * patternBars), inner = active states
        std::vector<GridRollCell> cells; // one per step slot (type DrumHit or empty)

        // Fill pattern (alternate when Fill mode active)
        std::vector<GridRollCell> fillCells;
    };

    /** Called when drum cells change (for upstream sync). */
    std::function<void()>  onCellsChanged;

    /** Called when a status message should be shown. */
    std::function<void(const juce::String&)> onStatusMessage;

    explicit DrumGridView();

    /** Seed with default GM drum rows. */
    void buildDefaultRows();

    /** Add a row. */
    void addRow(const juce::String& name, int midiNote);

    /** Remove row by index. */
    void removeRow(int rowIndex);

    const std::vector<DrumRow>& getRows() const { return rows; }

    /** Enable fill pattern mode for this frame. */
    void setFillMode(bool active);

    void setPlayheadBeat(double beat);

    /** Beat range for horizontal scroll. */
    void setVisibleBeatRange(double start, double end);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    // ---------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------
    int totalHeight() const;
    juce::Rectangle<int> stepCellBounds(int rowIndex, int stepIndex) const;
    juce::Rectangle<int> headerBounds(int rowIndex) const;
    juce::Rectangle<int> gridBounds(int rowIndex) const;

    struct HitResult
    {
        int rowIndex  { -1 };
        int stepIndex { -1 };
        bool inHeader { false };
    };
    HitResult hitTest(juce::Point<int> pos) const;

    void paintRow(juce::Graphics& g, int rowIndex);
    void paintHeader(juce::Graphics& g, int rowIndex, juce::Rectangle<int> bounds);
    void paintStep(juce::Graphics& g, const GridRollCell& cell,
                   juce::Rectangle<int> bounds, bool active) const;

    void showRowContextMenu(int rowIndex, juce::Point<int> screenPos);
    void showStepContextMenu(int rowIndex, int stepIndex, juce::Point<int> screenPos);
    void ensureSteps(DrumRow& row);

    juce::Colour rowColour(int midiNote) const;

    // ---------------------------------------------------------------
    // State
    // ---------------------------------------------------------------
    std::vector<DrumRow> rows;
    double playheadBeat  { 0.0 };
    double visibleStart  { 0.0 };
    double visibleEnd    { 4.0 };
    bool   fillModeActive { false };

    // Velocity drag state
    int  dragRowIndex  { -1 };
    int  dragStepIndex { -1 };
    float dragStartVel { 0.8f };
    juce::Point<int> dragStartPos;

    juce::TextButton fillButton { "Fill" };
    juce::ComboBox subdivSelector;

    static constexpr int kHeaderControlH = 24;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrumGridView)
};

} // namespace setle::gridroll
