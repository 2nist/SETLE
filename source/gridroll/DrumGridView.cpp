#include "DrumGridView.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace setle::gridroll
{

// GM drum colours
static juce::Colour gmDrumColour(int midiNote)
{
    if (midiNote == 36) return juce::Colour(0xffcc3333); // Kick — red
    if (midiNote == 38) return juce::Colour(0xffddaa22); // Snare — yellow
    if (midiNote == 39) return juce::Colour(0xffddaa22); // Clap
    if (midiNote == 42) return juce::Colour(0xff3377cc); // HH Closed — blue
    if (midiNote == 46) return juce::Colour(0xff5599ee); // HH Open
    return juce::Colour(0xff557799);                      // Other — slate
}

// ---------------------------------------------------------------
DrumGridView::DrumGridView()
{
    addAndMakeVisible(fillButton);
    fillButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a3040));
    fillButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.8f));
    fillButton.onClick = [this]
    {
        setFillMode(!fillModeActive);
        fillButton.setColour(juce::TextButton::buttonColourId,
                             fillModeActive ? juce::Colour(0xff6633aa) : juce::Colour(0xff2a3040));
    };

    addAndMakeVisible(subdivSelector);
    subdivSelector.addItem("8 steps",  8);
    subdivSelector.addItem("16 steps", 16);
    subdivSelector.addItem("32 steps", 32);
    subdivSelector.setSelectedId(16, juce::dontSendNotification);
    subdivSelector.onChange = [this]
    {
        const int newSubdiv = subdivSelector.getSelectedId();
        for (auto& row : rows)
        {
            row.steps = newSubdiv;
            ensureSteps(row);
        }
        repaint();
    };
}

// ---------------------------------------------------------------
void DrumGridView::buildDefaultRows()
{
    rows.clear();
    struct DefaultRow { const char* name; int note; };
    static const DefaultRow defaults[] = {
        { "Kick",      36 },
        { "Snare",     38 },
        { "HH Closed", 42 },
        { "HH Open",   46 },
        { "Clap",      39 },
    };
    for (const auto& d : defaults)
        addRow(d.name, d.note);
}

// ---------------------------------------------------------------
void DrumGridView::addRow(const juce::String& name, int midiNote)
{
    DrumRow row;
    row.name     = name;
    row.midiNote = midiNote;
    ensureSteps(row);
    rows.push_back(std::move(row));
    setSize(getWidth(), totalHeight() + kHeaderControlH);
}

void DrumGridView::removeRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size()))
        return;
    rows.erase(rows.begin() + rowIndex);
    setSize(getWidth(), totalHeight() + kHeaderControlH);
    repaint();
}

void DrumGridView::ensureSteps(DrumRow& row)
{
    const int total = row.steps * row.patternBars;
    while (static_cast<int>(row.cells.size()) < total)
    {
        GridRollCell cell;
        cell.type     = GridRollCell::CellType::DrumHit;
        cell.midiNote = row.midiNote;
        cell.muted    = true; // inactive by default
        row.cells.push_back(std::move(cell));
    }
    row.cells.resize(static_cast<size_t>(total));

    while (static_cast<int>(row.fillCells.size()) < total)
    {
        GridRollCell cell;
        cell.type     = GridRollCell::CellType::DrumHit;
        cell.midiNote = row.midiNote;
        cell.muted    = true;
        row.fillCells.push_back(std::move(cell));
    }
    row.fillCells.resize(static_cast<size_t>(total));
}

// ---------------------------------------------------------------
void DrumGridView::setFillMode(bool active)
{
    fillModeActive = active;
    repaint();
}

void DrumGridView::setPlayheadBeat(double beat)
{
    if (std::abs(beat - playheadBeat) > 0.001)
    {
        playheadBeat = beat;
        repaint();
    }
}

void DrumGridView::setVisibleBeatRange(double start, double end)
{
    visibleStart = start;
    visibleEnd   = end;
    repaint();
}

// ---------------------------------------------------------------
int DrumGridView::totalHeight() const
{
    return static_cast<int>(rows.size()) * kRowHeight;
}

juce::Rectangle<int> DrumGridView::headerBounds(int rowIndex) const
{
    return { 0, kHeaderControlH + rowIndex * kRowHeight, kHeaderWidth, kRowHeight };
}

juce::Rectangle<int> DrumGridView::gridBounds(int rowIndex) const
{
    return { kHeaderWidth,
             kHeaderControlH + rowIndex * kRowHeight,
             getWidth() - kHeaderWidth,
             kRowHeight };
}

juce::Rectangle<int> DrumGridView::stepCellBounds(int rowIndex, int stepIndex) const
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size()))
        return {};
    const auto& row = rows[static_cast<size_t>(rowIndex)];
    const auto grid = gridBounds(rowIndex);
    const int total = row.steps * row.patternBars;
    if (total <= 0) return {};
    const int cellW = juce::jmax(4, grid.getWidth() / total);
    return { grid.getX() + stepIndex * cellW,
             grid.getY(),
             cellW,
             kRowHeight };
}

// ---------------------------------------------------------------
DrumGridView::HitResult DrumGridView::hitTest(juce::Point<int> pos) const
{
    for (int r = 0; r < static_cast<int>(rows.size()); ++r)
    {
        const auto hb = headerBounds(r);
        if (hb.contains(pos))
            return { r, -1, true };

        const auto gb = gridBounds(r);
        if (!gb.contains(pos)) continue;

        const auto& row = rows[static_cast<size_t>(r)];
        const int total = row.steps * row.patternBars;
        if (total <= 0) continue;
        const int cellW = juce::jmax(4, gb.getWidth() / total);
        const int step  = (pos.getX() - gb.getX()) / cellW;
        return { r, std::min(step, total - 1), false };
    }
    return { -1, -1, false };
}

// ---------------------------------------------------------------
juce::Colour DrumGridView::rowColour(int midiNote) const
{
    return gmDrumColour(midiNote);
}

// ---------------------------------------------------------------
void DrumGridView::paintStep(juce::Graphics& g,
                              const GridRollCell& cell,
                              juce::Rectangle<int> bounds,
                              bool active) const
{
    if (!active) return;

    const auto centre = bounds.getCentre().toFloat();
    const float maxR  = static_cast<float>(std::min(bounds.getWidth(), bounds.getHeight())) * 0.44f;
    const float r     = maxR * juce::jlimit(0.3f, 1.0f, cell.velocity);

    juce::Colour col = gmDrumColour(cell.midiNote);
    if (cell.muted)    col = col.withAlpha(0.3f);
    if (cell.accented) col = col.brighter(0.5f);

    g.setColour(col);
    g.fillEllipse(centre.getX() - r, centre.getY() - r, r * 2.0f, r * 2.0f);

    if (cell.probability < 0.99f)
    {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.drawEllipse(centre.getX() - maxR, centre.getY() - maxR,
                      maxR * 2.0f, maxR * 2.0f, 1.0f);
    }

    if (cell.ratchetCount > 1)
    {
        const float subR = r * 0.25f;
        const int n = juce::jmin(cell.ratchetCount, 4);
        for (int i = 0; i < n; ++i)
        {
            const float angle = static_cast<float>(i) * juce::MathConstants<float>::twoPi / static_cast<float>(n);
            const float cx = centre.getX() + (r * 0.6f) * std::cos(angle);
            const float cy = centre.getY() + (r * 0.6f) * std::sin(angle);
            g.setColour(juce::Colours::white.withAlpha(0.6f));
            g.fillEllipse(cx - subR, cy - subR, subR * 2.0f, subR * 2.0f);
        }
    }
}

void DrumGridView::paintHeader(juce::Graphics& g, int rowIndex, juce::Rectangle<int> bounds)
{
    const auto& row = rows[static_cast<size_t>(rowIndex)];
    const juce::Colour col = rowColour(row.midiNote);

    g.setColour(col.withAlpha(0.25f));
    g.fillRect(bounds);

    g.setColour(col.withAlpha(0.9f));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(row.name,
               bounds.getX() + 4, bounds.getY(), 72, bounds.getHeight(),
               juce::Justification::centredLeft, true);

    // M & S buttons (12×12 each, right side of header)
    juce::Rectangle<int> mBtn(bounds.getRight() - 28, bounds.getCentreY() - 7, 12, 14);
    juce::Rectangle<int> sBtn(bounds.getRight() - 14, bounds.getCentreY() - 7, 12, 14);

    g.setColour(row.muted  ? juce::Colour(0xffcc4444) : juce::Colour(0xff334455));
    g.fillRect(mBtn);
    g.setColour(row.soloed ? juce::Colour(0xffddaa22) : juce::Colour(0xff334455));
    g.fillRect(sBtn);

    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.drawText("M", mBtn, juce::Justification::centred, false);
    g.drawText("S", sBtn, juce::Justification::centred, false);
}

void DrumGridView::paintRow(juce::Graphics& g, int rowIndex)
{
    const auto& row = rows[static_cast<size_t>(rowIndex)];
    const auto grid = gridBounds(rowIndex);
    const int total = row.steps * row.patternBars;

    // Background
    g.setColour(juce::Colour(0xff1a2332));
    g.fillRect(grid);

    // Beat separator lines
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    for (int i = 0; i <= total; ++i)
    {
        const int x = grid.getX() + (grid.getWidth() * i) / juce::jmax(1, total);
        const float alpha = (i % row.steps == 0) ? 0.2f : 0.06f;
        g.setColour(juce::Colours::white.withAlpha(alpha));
        g.fillRect(x, grid.getY(), 1, kRowHeight);
    }

    // Step cells
    const auto& activeCells = fillModeActive ? row.fillCells : row.cells;
    for (int s = 0; s < total && s < static_cast<int>(activeCells.size()); ++s)
    {
        const auto bounds = stepCellBounds(rowIndex, s);
        const bool active = !activeCells[static_cast<size_t>(s)].muted;
        if (active)
            paintStep(g, activeCells[static_cast<size_t>(s)], bounds, true);
    }

    // Header
    paintHeader(g, rowIndex, headerBounds(rowIndex));
}

// ---------------------------------------------------------------
void DrumGridView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141e2a));

    // Top controls area
    g.setColour(juce::Colour(0xff1a2332));
    g.fillRect(0, 0, getWidth(), kHeaderControlH);

    // LCM cycle indicator
    if (!rows.empty())
    {
        // Compute LCM of all row total steps
        int lcm = 1;
        for (const auto& row : rows)
        {
            const int n = row.steps * row.patternBars;
            lcm = std::lcm(lcm, n);
        }
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("Cycle: " + juce::String(lcm) + " steps",
                   getWidth() - 120, 4, 116, 16,
                   juce::Justification::right, false);
    }

    // Rows
    for (int r = 0; r < static_cast<int>(rows.size()); ++r)
        paintRow(g, r);

    // Playhead
    if (rows.empty()) return;
    const auto& firstRow = rows[0];
    const int total = firstRow.steps * firstRow.patternBars;
    const double beatsTotal = static_cast<double>(total) / static_cast<double>(firstRow.steps);
    const double fraction = (beatsTotal > 0.0) ? playheadBeat / beatsTotal : 0.0;
    const int px = kHeaderWidth + static_cast<int>(fraction * (getWidth() - kHeaderWidth));
    g.setColour(juce::Colour(0xffeeee22));
    g.fillRect(px, kHeaderControlH, 2, totalHeight());
}

// ---------------------------------------------------------------
void DrumGridView::resized()
{
    // Place top controls
    fillButton.setBounds(kHeaderWidth, 3, 44, 18);
    subdivSelector.setBounds(kHeaderWidth + 50, 3, 80, 18);
    setSize(getWidth(), totalHeight() + kHeaderControlH);
}

// ---------------------------------------------------------------
void DrumGridView::mouseDown(const juce::MouseEvent& e)
{
    dragRowIndex  = -1;
    dragStepIndex = -1;

    const auto hit = hitTest(e.getPosition());
    if (hit.rowIndex < 0) return;

    if (hit.inHeader)
    {
        // Check M/S buttons
        const auto hb = headerBounds(hit.rowIndex);
        juce::Rectangle<int> mBtn(hb.getRight() - 28, hb.getCentreY() - 7, 12, 14);
        juce::Rectangle<int> sBtn(hb.getRight() - 14, hb.getCentreY() - 7, 12, 14);

        if (mBtn.contains(e.getPosition()))
        {
            rows[static_cast<size_t>(hit.rowIndex)].muted =
                !rows[static_cast<size_t>(hit.rowIndex)].muted;
            repaint();
            return;
        }
        if (sBtn.contains(e.getPosition()))
        {
            rows[static_cast<size_t>(hit.rowIndex)].soloed =
                !rows[static_cast<size_t>(hit.rowIndex)].soloed;
            repaint();
            return;
        }

        if (e.mods.isRightButtonDown())
        {
            showRowContextMenu(hit.rowIndex, e.getScreenPosition());
            return;
        }
        return;
    }

    if (hit.stepIndex < 0) return;

    if (e.mods.isRightButtonDown())
    {
        showStepContextMenu(hit.rowIndex, hit.stepIndex, e.getScreenPosition());
        return;
    }

    // Toggle step
    auto& row = rows[static_cast<size_t>(hit.rowIndex)];
    auto& cells = fillModeActive ? row.fillCells : row.cells;
    if (hit.stepIndex >= static_cast<int>(cells.size())) return;

    auto& cell = cells[static_cast<size_t>(hit.stepIndex)];
    const bool wasActive = !cell.muted;

    if (wasActive)
    {
        cell.muted = true;
    }
    else
    {
        cell.muted    = false;
        cell.velocity = 0.8f;
        // Set up drag for velocity
        dragRowIndex   = hit.rowIndex;
        dragStepIndex  = hit.stepIndex;
        dragStartVel   = cell.velocity;
        dragStartPos   = e.getPosition();
    }

    repaint();
    if (onCellsChanged) onCellsChanged();
}

// ---------------------------------------------------------------
void DrumGridView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragRowIndex < 0 || dragStepIndex < 0) return;

    auto& row   = rows[static_cast<size_t>(dragRowIndex)];
    auto& cells = fillModeActive ? row.fillCells : row.cells;
    if (dragStepIndex >= static_cast<int>(cells.size())) return;

    const float dyNorm = static_cast<float>(dragStartPos.getY() - e.getPosition().getY()) / 60.0f;
    cells[static_cast<size_t>(dragStepIndex)].velocity =
        juce::jlimit(0.0f, 1.0f, dragStartVel + dyNorm);
    repaint();
}

// ---------------------------------------------------------------
void DrumGridView::mouseUp(const juce::MouseEvent&)
{
    if (dragRowIndex >= 0 && onCellsChanged)
        onCellsChanged();
    dragRowIndex  = -1;
    dragStepIndex = -1;
}

// ---------------------------------------------------------------
void DrumGridView::showRowContextMenu(int rowIndex, juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.addSectionHeader(rows[static_cast<size_t>(rowIndex)].name);
    menu.addItem(1, "Add Row");
    menu.addItem(2, "Remove Row");
    menu.addItem(3, "Assign MIDI Note...");
    menu.addSeparator();
    menu.addItem(10, "Set Pattern Length → 1 bar",  true,
                 rows[static_cast<size_t>(rowIndex)].patternBars == 1);
    menu.addItem(11, "Set Pattern Length → 2 bars", true,
                 rows[static_cast<size_t>(rowIndex)].patternBars == 2);
    menu.addItem(12, "Set Pattern Length → 3 bars", true,
                 rows[static_cast<size_t>(rowIndex)].patternBars == 3);
    menu.addItem(13, "Set Pattern Length → 4 bars", true,
                 rows[static_cast<size_t>(rowIndex)].patternBars == 4);

    const int result = menu.showAt(juce::Rectangle<int>(screenPos.getX(), screenPos.getY(), 1, 1));

    if (result == 1)
    {
        juce::AlertWindow::showInputBoxAsync(
            "Add Row", "Instrument name:", "New Drum", nullptr,
            [this](const juce::String& name)
            {
                if (name.isNotEmpty())
                {
                    addRow(name, 60);
                    resized();
                    repaint();
                    if (onCellsChanged) onCellsChanged();
                }
            });
    }
    else if (result == 2)
    {
        removeRow(rowIndex);
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 3)
    {
        juce::AlertWindow::showInputBoxAsync(
            "Assign MIDI Note",
            "Enter MIDI note number (0–127):",
            juce::String(rows[static_cast<size_t>(rowIndex)].midiNote),
            nullptr,
            [this, rowIndex](const juce::String& text)
            {
                if (text.isNotEmpty())
                {
                    rows[static_cast<size_t>(rowIndex)].midiNote =
                        juce::jlimit(0, 127, text.getIntValue());
                    repaint();
                    if (onCellsChanged) onCellsChanged();
                }
            });
    }
    else if (result >= 10 && result <= 13)
    {
        rows[static_cast<size_t>(rowIndex)].patternBars = result - 9;
        ensureSteps(rows[static_cast<size_t>(rowIndex)]);
        setSize(getWidth(), totalHeight() + kHeaderControlH);
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
}

// ---------------------------------------------------------------
void DrumGridView::showStepContextMenu(int rowIndex, int stepIndex, juce::Point<int> screenPos)
{
    auto& row   = rows[static_cast<size_t>(rowIndex)];
    auto& cells = fillModeActive ? row.fillCells : row.cells;
    if (stepIndex >= static_cast<int>(cells.size())) return;
    auto& cell = cells[static_cast<size_t>(stepIndex)];

    juce::PopupMenu menu;
    menu.addItem(1, "Set Velocity...");
    menu.addItem(2, "Set Probability...");

    {
        juce::PopupMenu ratchetMenu;
        for (int n : { 1, 2, 3, 4, 8 })
            ratchetMenu.addItem(100 + n, juce::String(n) + "x", true, cell.ratchetCount == n);
        menu.addSubMenu("Ratchet", ratchetMenu);
    }

    menu.addItem(3, "Roll (32nd fill)");
    menu.addItem(4, "Flam");
    menu.addSeparator();
    menu.addItem(500, "Mute",   true, cell.muted);
    menu.addItem(501, "Accent", true, cell.accented);

    const int result = menu.showAt(juce::Rectangle<int>(screenPos.getX(), screenPos.getY(), 1, 1));

    if (result == 1)
    {
        juce::AlertWindow::showInputBoxAsync(
            "Set Velocity", "Enter velocity (0.0 – 1.0):",
            juce::String(cell.velocity), nullptr,
            [this, rowIndex, stepIndex](const juce::String& text)
            {
                if (text.isNotEmpty())
                {
                    auto& r2 = rows[static_cast<size_t>(rowIndex)];
                    auto& c2 = fillModeActive ? r2.fillCells : r2.cells;
                    c2[static_cast<size_t>(stepIndex)].velocity =
                        juce::jlimit(0.0f, 1.0f, text.getFloatValue());
                    repaint();
                    if (onCellsChanged) onCellsChanged();
                }
            });
    }
    else if (result == 2)
    {
        juce::AlertWindow::showInputBoxAsync(
            "Set Probability", "Enter probability (0.0 – 1.0):",
            juce::String(cell.probability), nullptr,
            [this, rowIndex, stepIndex](const juce::String& text)
            {
                if (text.isNotEmpty())
                {
                    auto& r2 = rows[static_cast<size_t>(rowIndex)];
                    auto& c2 = fillModeActive ? r2.fillCells : r2.cells;
                    c2[static_cast<size_t>(stepIndex)].probability =
                        juce::jlimit(0.0f, 1.0f, text.getFloatValue());
                    repaint();
                    if (onCellsChanged) onCellsChanged();
                }
            });
    }
    else if (result >= 101 && result <= 108)
    {
        cell.ratchetCount = result - 100;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 3)
    {
        // Roll: set ratchet to 8 (32nd note fill)
        cell.ratchetCount = 8;
        cell.muted = false;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 4)
    {
        // Flam: just mark accented and keep existing
        cell.accented = true;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 500)
    {
        cell.muted = !cell.muted;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 501)
    {
        cell.accented = !cell.accented;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
}

} // namespace setle::gridroll
