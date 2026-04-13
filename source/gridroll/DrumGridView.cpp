#include "DrumGridView.h"
#include "DrumPatternMidiReader.h"
#include "../theme/ThemeManager.h"
#include "../theme/ThemeStyleHelpers.h"

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

static bool promptTextInput(const juce::String& title,
                            const juce::String& prompt,
                            const juce::String& initialValue,
                            juce::String& outValue)
{
    juce::AlertWindow window(title, prompt, juce::AlertWindow::NoIcon);
    window.addTextEditor("value", initialValue, {});
    window.addButton("OK", 1);
    window.addButton("Cancel", 0);
    if (window.runModalLoop() != 1)
        return false;

    outValue = window.getTextEditorContents("value");
    return true;
}

// ---------------------------------------------------------------
DrumGridView::DrumGridView()
{
    addAndMakeVisible(fillButton);
    const auto& theme = ThemeManager::get().theme();
    const auto fillChip = setle::theme::chipStyle(theme, setle::theme::SurfaceState::normal, theme.zoneD);
    fillButton.setColour(juce::TextButton::buttonColourId, fillChip.fill);
    fillButton.setColour(juce::TextButton::textColourOffId, fillChip.text.withAlpha(0.92f));
    fillButton.setColour(juce::TextButton::buttonOnColourId,
                         setle::theme::chipStyle(theme, setle::theme::SurfaceState::selected, theme.zoneD).fill);
    fillButton.onClick = [this]
    {
        setFillMode(!fillModeActive);
    };

    addAndMakeVisible(subdivSelector);
    subdivSelector.onChange = [this]
    {
        const int newSubdiv = subdivSelector.getSelectedId();
        if (newSubdiv <= 0)
            return;

        currentStepCount = newSubdiv;
        for (auto& row : rows)
        {
            row.steps = newSubdiv;
            ensureSteps(row);
        }
        repaint();
    };

    updateStepOptions(currentMeter);

    addAndMakeVisible(patternsButton);
    patternsButton.onClick = [this]
    {
        if (patternBrowser == nullptr)
        {
            patternBrowser = std::make_unique<DrumPatternBrowser>(patternLibrary);
            patternBrowser->onPatternSelected = [this](const DrumPattern& pattern) { applyPattern(pattern); };
            addAndMakeVisible(*patternBrowser);
        }

        patternBrowser->setVisible(!patternBrowser->isVisible());
        resized();
    };

    addChildComponent(groovePanel);
    groovePanel.onGrooveChanged = [this](int rowIndex, GroovePanel::RowGroove groove)
    {
        if (rowIndex < 0 || rowIndex >= static_cast<int>(rows.size()))
            return;

        auto& row = rows[static_cast<size_t>(rowIndex)];
        auto& cells = fillModeActive ? row.fillCells : row.cells;
        for (auto& cell : cells)
            cell.swing = groove.swingPercent;
        if (onCellsChanged)
            onCellsChanged();
    };

    auto appDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
    auto patternsRoot = appDir.getChildFile("assets").getChildFile("patterns");
    auto manifest = patternsRoot.getChildFile("pattern_manifest_batch.json");
    patternLibrary.loadFromManifest(manifest, patternsRoot);
}

void DrumGridView::setMeterContext(const setle::theory::MeterContext& meter)
{
    if (meter.numerator == currentMeter.numerator && meter.denominator == currentMeter.denominator)
        return;

    currentMeter = meter;
    updateStepOptions(currentMeter);
}

void DrumGridView::updateStepOptions(const setle::theory::MeterContext& meter)
{
    subdivSelector.clear(juce::dontSendNotification);

    // Keep 4/4 behavior close to previous defaults while supporting odd meters.
    const int primarySteps = meter.denominator <= 4
                                 ? meter.stepsPerBarSixteenths()
                                 : meter.stepsPerBarEighths();

    subdivSelector.addItem(
        juce::String(primarySteps) + " steps (" + meter.toString() + ")",
        primarySteps);

    const int doubleSteps = primarySteps * 2;
    subdivSelector.addItem(
        juce::String(doubleSteps) + " steps (double)",
        doubleSteps);

    if (meter.isSimple() && meter.numerator % 2 == 0 && primarySteps > 2)
    {
        const int halfSteps = juce::jmax(1, primarySteps / 2);
        subdivSelector.addItem(
            juce::String(halfSteps) + " steps (half)",
            halfSteps);
    }

    currentStepCount = primarySteps;
    subdivSelector.setSelectedId(primarySteps, juce::dontSendNotification);

    for (auto& row : rows)
    {
        row.steps = currentStepCount;
        ensureSteps(row);
    }

    repaint();
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

std::vector<GridRollCell> DrumGridView::getActivePatternCells() const
{
    std::vector<GridRollCell> out;

    for (const auto& row : rows)
    {
        const auto& cells = fillModeActive ? row.fillCells : row.cells;
        if (cells.empty() || row.steps <= 0)
            continue;

        const float stepBeats = 1.0f / static_cast<float>(row.steps);
        for (size_t i = 0; i < cells.size(); ++i)
        {
            const auto& src = cells[i];
            if (src.muted)
                continue;

            auto cell = src;
            cell.type = GridRollCell::CellType::DrumHit;
            cell.midiNote = row.midiNote;
            cell.startBeat = static_cast<float>(i) * stepBeats;
            cell.durationBeats = stepBeats;
            out.push_back(std::move(cell));
        }
    }

    return out;
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
    const auto& theme = ThemeManager::get().theme();
    const auto chip = setle::theme::chipStyle(theme,
                                               fillModeActive ? setle::theme::SurfaceState::selected
                                                              : setle::theme::SurfaceState::normal,
                                               theme.zoneD);
    fillButton.setColour(juce::TextButton::buttonColourId, chip.fill);
    fillButton.setColour(juce::TextButton::textColourOffId, chip.text.withAlpha(0.95f));
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

juce::Rectangle<int> DrumGridView::patternLengthMarkerBounds(int rowIndex) const
{
    const auto grid = gridBounds(rowIndex);
    return { grid.getRight() - 9, grid.getCentreY() - 4, 8, 8 };
}

bool DrumGridView::isBeatGroupStart(const DrumRow& row, int stepIndex) const
{
    if (stepIndex <= 0 || row.steps <= 0 || currentMeter.numerator <= 0)
        return stepIndex == 0;

    // Map row steps to denominator-unit groups using meter context.
    const int stepsPerBarAtMeterUnit = currentMeter.denominator == 8
                                           ? currentMeter.stepsPerBarEighths()
                                           : currentMeter.numerator;
    if (stepsPerBarAtMeterUnit <= 0)
        return false;

    const float scale = static_cast<float>(row.steps) / static_cast<float>(stepsPerBarAtMeterUnit);
    std::vector<int> groups;
    if (currentMeter.denominator == 8 && currentMeter.numerator > 3)
    {
        int remaining = currentMeter.numerator;
        while (remaining > 0)
        {
            int group = (remaining == 2 || remaining == 4) ? 2 : juce::jmin(3, remaining);
            groups.push_back(group);
            remaining -= group;
        }
    }
    else
    {
        groups.assign(currentMeter.numerator, 1);
    }

    int cursor = 0;
    for (int group : groups)
    {
        const int rowStep = static_cast<int>(std::round(cursor * scale));
        if (stepIndex == rowStep)
            return true;
        cursor += group;
    }
    return false;
}

// ---------------------------------------------------------------
void DrumGridView::paintStep(juce::Graphics& g,
                              const GridRollCell& cell,
                              juce::Rectangle<int> bounds,
                              bool active,
                              bool hovered) const
{
    const auto& theme = ThemeManager::get().theme();
    const auto centre = bounds.getCentre().toFloat();
    const float maxR = static_cast<float>(std::min(bounds.getWidth(), bounds.getHeight())) * 0.45f;

    if (!active)
    {
        g.setColour(theme.surfaceEdge.withAlpha(hovered ? 0.70f : 0.40f));
        g.drawEllipse(centre.getX() - maxR * 0.42f, centre.getY() - maxR * 0.42f,
                      maxR * 0.84f, maxR * 0.84f, 1.1f);
        return;
    }

    const float vel = juce::jlimit(0.0f, 1.0f, cell.velocity);
    float diameterScale = juce::jmap(vel, 0.5f, 1.0f, 0.60f, 0.90f);
    diameterScale = juce::jlimit(0.40f, 0.90f, diameterScale);
    if (vel <= 0.35f)
        diameterScale = 0.40f;

    const float r = maxR * diameterScale;
    juce::Colour col = gmDrumColour(cell.midiNote);
    if (cell.muted)
        col = col.withAlpha(0.4f);

    g.setColour(col);
    g.fillEllipse(centre.getX() - r, centre.getY() - r, r * 2.0f, r * 2.0f);

    if (cell.probability < 0.99f)
    {
        juce::Path ringPath;
        ringPath.addEllipse(centre.getX() - maxR * 0.9f, centre.getY() - maxR * 0.9f,
                            maxR * 1.8f, maxR * 1.8f);
        juce::Path dashed;
        const float dashes[] = { 2.0f, 2.0f };
        juce::PathStrokeType(1.1f).createDashedStroke(dashed, ringPath, dashes, 2);
        g.setColour(theme.inkLight.withAlpha(0.70f));
        g.fillPath(dashed);
    }

    if (cell.accented)
    {
        g.setColour(theme.inkLight.withAlpha(0.90f));
        g.drawEllipse(centre.getX() - (r + 2.2f), centre.getY() - (r + 2.2f),
                      (r + 2.2f) * 2.0f, (r + 2.2f) * 2.0f, 1.6f);
    }

    if (cell.ratchetCount > 1)
    {
        const float subR = juce::jmax(1.3f, r * 0.20f);
        const int n = juce::jmin(cell.ratchetCount, 6);
        for (int i = 0; i < n; ++i)
        {
            const float angle = static_cast<float>(i) * juce::MathConstants<float>::twoPi / static_cast<float>(n);
            const float cx = centre.getX() + (r * 0.55f) * std::cos(angle);
            const float cy = centre.getY() + (r * 0.55f) * std::sin(angle);
            g.setColour(theme.inkLight.withAlpha(0.85f));
            g.fillEllipse(cx - subR, cy - subR, subR * 2.0f, subR * 2.0f);
        }
    }
}

void DrumGridView::paintHeader(juce::Graphics& g, int rowIndex, juce::Rectangle<int> bounds)
{
    const auto& row = rows[static_cast<size_t>(rowIndex)];
    const auto& theme = ThemeManager::get().theme();
    const juce::Colour col = rowColour(row.midiNote);
    const auto headerStyle = setle::theme::rowStyle(theme,
                                                     row.muted ? setle::theme::SurfaceState::muted
                                                               : setle::theme::SurfaceState::normal);

    g.setColour(headerStyle.fill);
    g.fillRoundedRectangle(bounds.toFloat().reduced(0.5f),
                           setle::theme::radius(theme, setle::theme::RadiusRole::xs));
    g.setColour(col.withAlpha(0.92f));
    g.fillRect(bounds.removeFromLeft(3));

    g.setColour(setle::theme::textForRole(theme, setle::theme::TextRole::muted).withAlpha(0.98f));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(row.name,
               bounds.getX() + 6, bounds.getY(), 72, bounds.getHeight(),
               juce::Justification::centredLeft, true);

    // M & S buttons (16x16 each, right side of header)
    juce::Rectangle<int> mBtn(bounds.getRight() - 38, bounds.getCentreY() - 8, 16, 16);
    juce::Rectangle<int> sBtn(bounds.getRight() - 20, bounds.getCentreY() - 8, 16, 16);

    const auto muteChip = setle::theme::chipStyle(theme,
                                                   row.muted ? setle::theme::SurfaceState::warning
                                                             : setle::theme::SurfaceState::normal,
                                                   theme.accentWarm);
    g.setColour(muteChip.fill);
    g.fillRoundedRectangle(mBtn.toFloat(), 2.5f);
    const auto soloChip = setle::theme::chipStyle(theme,
                                                   row.soloed ? setle::theme::SurfaceState::selected
                                                              : setle::theme::SurfaceState::normal,
                                                   theme.zoneD);
    g.setColour(soloChip.fill);
    g.fillRoundedRectangle(sBtn.toFloat(), 2.5f);

    g.setColour(setle::theme::textForRole(theme, setle::theme::TextRole::primary).withAlpha(0.94f));
    g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
    g.drawText("M", mBtn, juce::Justification::centred, false);
    g.drawText("S", sBtn, juce::Justification::centred, false);
}

void DrumGridView::paintRow(juce::Graphics& g, int rowIndex)
{
    const auto& row = rows[static_cast<size_t>(rowIndex)];
    const auto& theme = ThemeManager::get().theme();
    const auto grid = gridBounds(rowIndex);
    const int total = row.steps * row.patternBars;

    // Background
    const auto rowStyle = setle::theme::rowStyle(theme,
                                                  row.muted ? setle::theme::SurfaceState::muted
                                                            : setle::theme::SurfaceState::normal);
    g.setColour(rowStyle.fill);
    g.fillRect(grid);

    // Beat separator lines + meter-group emphasis.
    for (int i = 0; i <= total; ++i)
    {
        const int x = grid.getX() + (grid.getWidth() * i) / juce::jmax(1, total);
        float alpha = 0.08f;
        if (i % juce::jmax(1, row.steps) == 0)
            alpha = 0.24f;
        else if (isBeatGroupStart(row, i))
            alpha = 0.16f;

        g.setColour(setle::theme::timelineGridLine(theme, i % juce::jmax(1, row.steps) == 0).withAlpha(alpha + 0.08f));
        g.fillRect(x, grid.getY(), 1, kRowHeight);
    }

    // Step cells
    const auto& activeCells = fillModeActive ? row.fillCells : row.cells;
    for (int s = 0; s < total && s < static_cast<int>(activeCells.size()); ++s)
    {
        const auto bounds = stepCellBounds(rowIndex, s);
        if (isBeatGroupStart(row, s))
        {
            g.setColour(setle::theme::stateOverlay(theme, setle::theme::SurfaceState::hovered).withAlpha(0.65f));
            g.fillRect(bounds);
        }

        const bool active = !activeCells[static_cast<size_t>(s)].muted;
        paintStep(g,
                  activeCells[static_cast<size_t>(s)],
                  bounds,
                  active,
                  hoverRowIndex == rowIndex && hoverStepIndex == s);
    }

    // Pattern length marker.
    {
        const auto marker = patternLengthMarkerBounds(rowIndex);
        juce::Path tri;
        tri.startNewSubPath(static_cast<float>(marker.getX()), static_cast<float>(marker.getY()));
        tri.lineTo(static_cast<float>(marker.getRight()), static_cast<float>(marker.getCentreY()));
        tri.lineTo(static_cast<float>(marker.getX()), static_cast<float>(marker.getBottom()));
        tri.closeSubPath();
        g.setColour(theme.accent.withAlpha(0.95f));
        g.fillPath(tri);
    }

    // Header
    paintHeader(g, rowIndex, headerBounds(rowIndex));

    if (row.muted)
    {
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.fillRect(headerBounds(rowIndex).getUnion(gridBounds(rowIndex)));
    }
}

// ---------------------------------------------------------------
void DrumGridView::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    g.fillAll(setle::theme::panelBackground(theme, setle::theme::ZoneRole::timeline));

    // Top controls area
    g.setColour(setle::theme::panelHeaderBackground(theme, setle::theme::ZoneRole::timeline));
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
        g.setColour(setle::theme::textForRole(theme, setle::theme::TextRole::muted).withAlpha(0.94f));
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
    g.setColour(theme.playheadColor.brighter(0.45f));
    g.fillRect(px, kHeaderControlH, 2, totalHeight());
}

// ---------------------------------------------------------------
void DrumGridView::resized()
{
    // Place top controls
    fillButton.setBounds(kHeaderWidth, 3, 44, 18);
    subdivSelector.setBounds(kHeaderWidth + 50, 3, 80, 18);
    patternsButton.setBounds(kHeaderWidth + 136, 3, 66, 18);

    if (patternBrowser != nullptr && patternBrowser->isVisible())
        patternBrowser->setBounds(getWidth() - 360, kHeaderControlH + 2, 356, juce::jmin(240, getHeight() - kHeaderControlH - 4));

    if (groovePanel.isVisible())
        groovePanel.setBounds(getWidth() - 210, kHeaderControlH + 2, 206, 96);

    setSize(getWidth(), totalHeight() + kHeaderControlH);
}

// ---------------------------------------------------------------
void DrumGridView::mouseMove(const juce::MouseEvent& e)
{
    const auto hit = hitTest(e.getPosition());
    hoverRowIndex = hit.rowIndex;
    hoverStepIndex = hit.stepIndex;

    bool overPatternHandle = false;
    if (hit.rowIndex >= 0)
        overPatternHandle = patternLengthMarkerBounds(hit.rowIndex).contains(e.getPosition());

    setMouseCursor(overPatternHandle
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
    repaint();
}

void DrumGridView::mouseExit(const juce::MouseEvent&)
{
    hoverRowIndex = -1;
    hoverStepIndex = -1;
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

// ---------------------------------------------------------------
void DrumGridView::mouseDown(const juce::MouseEvent& e)
{
    dragRowIndex  = -1;
    dragStepIndex = -1;
    patternResizeRowIndex = -1;

    const auto hit = hitTest(e.getPosition());
    if (hit.rowIndex < 0) return;

    if (patternLengthMarkerBounds(hit.rowIndex).contains(e.getPosition()))
    {
        patternResizeRowIndex = hit.rowIndex;
        patternResizeStartBars = rows[static_cast<size_t>(hit.rowIndex)].patternBars;
        patternResizeStartPos = e.getPosition();
        return;
    }

    if (hit.inHeader)
    {
        // Check M/S buttons
        const auto hb = headerBounds(hit.rowIndex);
        juce::Rectangle<int> mBtn(hb.getRight() - 38, hb.getCentreY() - 8, 16, 16);
        juce::Rectangle<int> sBtn(hb.getRight() - 20, hb.getCentreY() - 8, 16, 16);

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
    if (patternResizeRowIndex >= 0)
    {
        auto& row = rows[static_cast<size_t>(patternResizeRowIndex)];
        const auto grid = gridBounds(patternResizeRowIndex);
        const int barWidthPx = juce::jmax(8, grid.getWidth() / juce::jmax(1, patternResizeStartBars));
        const int deltaX = e.getPosition().getX() - patternResizeStartPos.getX();
        const int deltaBars = juce::roundToInt(static_cast<float>(deltaX) / static_cast<float>(barWidthPx));
        const int newBars = juce::jlimit(1, 8, patternResizeStartBars + deltaBars);
        if (newBars != row.patternBars)
        {
            row.patternBars = newBars;
            ensureSteps(row);
            resized();
            repaint();
        }
        return;
    }

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
    if (patternResizeRowIndex >= 0)
    {
        patternResizeRowIndex = -1;
        if (onCellsChanged)
            onCellsChanged();
    }

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
    menu.addItem(4, "Rename Row...");
    menu.addItem(5, "Duplicate Row");
    menu.addItem(6, "Move Row Up");
    menu.addItem(7, "Move Row Down");
    menu.addItem(9, "Fill All Steps");
    menu.addItem(14, "Clear All Steps");
    menu.addItem(15, "Randomize Steps");
    menu.addItem(16, "Set Color...");
    menu.addItem(17, "Set Bus...");
    menu.addItem(8, "Set Groove...");
    menu.addSeparator();
    menu.addItem(10, "Set Pattern Length → 1 bar",  true,
                 rows[static_cast<size_t>(rowIndex)].patternBars == 1);
    menu.addItem(11, "Set Pattern Length → 2 bars", true,
                 rows[static_cast<size_t>(rowIndex)].patternBars == 2);
    menu.addItem(12, "Set Pattern Length → 3 bars", true,
                 rows[static_cast<size_t>(rowIndex)].patternBars == 3);
    menu.addItem(13, "Set Pattern Length → 4 bars", true,
                 rows[static_cast<size_t>(rowIndex)].patternBars == 4);

    const int result = menu.show();

    if (result == 1)
    {
        juce::String name;
        if (promptTextInput("Add Row", "Instrument name:", "New Drum", name) && name.isNotEmpty())
        {
            addRow(name, 60);
            resized();
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
    }
    else if (result == 2)
    {
        removeRow(rowIndex);
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 3)
    {
        juce::String text;
        if (promptTextInput("Assign MIDI Note",
                            "Enter MIDI note number (0-127):",
                            juce::String(rows[static_cast<size_t>(rowIndex)].midiNote),
                            text)
            && text.isNotEmpty())
        {
            rows[static_cast<size_t>(rowIndex)].midiNote =
                juce::jlimit(0, 127, text.getIntValue());
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
    }
    else if (result == 8)
    {
        groovePanel.openForRow(rowIndex);
        groovePanel.setVisible(true);
        resized();
        groovePanel.grabKeyboardFocus();
    }
    else if (result == 4)
    {
        juce::String text;
        if (promptTextInput("Rename Row", "Row name:", rows[static_cast<size_t>(rowIndex)].name, text))
        {
            const auto name = text.trim();
            if (name.isNotEmpty())
            {
                rows[static_cast<size_t>(rowIndex)].name = name;
                repaint();
                if (onCellsChanged) onCellsChanged();
            }
        }
    }
    else if (result == 5)
    {
        auto copy = rows[static_cast<size_t>(rowIndex)];
        copy.name += " Copy";
        rows.insert(rows.begin() + rowIndex + 1, std::move(copy));
        resized();
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 6 && rowIndex > 0)
    {
        std::swap(rows[static_cast<size_t>(rowIndex)], rows[static_cast<size_t>(rowIndex - 1)]);
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 7 && rowIndex + 1 < static_cast<int>(rows.size()))
    {
        std::swap(rows[static_cast<size_t>(rowIndex)], rows[static_cast<size_t>(rowIndex + 1)]);
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 9)
    {
        auto& row = rows[static_cast<size_t>(rowIndex)];
        ensureSteps(row);
        auto& cells = fillModeActive ? row.fillCells : row.cells;
        for (auto& c : cells)
            c.muted = false;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 14)
    {
        auto& row = rows[static_cast<size_t>(rowIndex)];
        ensureSteps(row);
        auto& cells = fillModeActive ? row.fillCells : row.cells;
        for (auto& c : cells)
            c.muted = true;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 15)
    {
        auto& row = rows[static_cast<size_t>(rowIndex)];
        ensureSteps(row);
        auto& cells = fillModeActive ? row.fillCells : row.cells;
        juce::Random rng;
        for (auto& c : cells)
        {
            c.muted = rng.nextFloat() > 0.35f;
            c.velocity = juce::jlimit(0.2f, 1.0f, rng.nextFloat());
        }
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 16)
    {
        if (onStatusMessage)
            onStatusMessage("Row color is currently derived from MIDI note mapping.");
    }
    else if (result == 17)
    {
        if (onStatusMessage)
            onStatusMessage("Bus routing is available in Track/OUT routing panel.");
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

void DrumGridView::applyPattern(const DrumPattern& pattern)
{
    auto parsed = DrumPatternMidiReader::readFile(pattern.midiFile, kDefaultSteps);
    if (!parsed.success)
        return;

    for (auto& row : rows)
    {
        ensureSteps(row);
        auto& cells = fillModeActive ? row.fillCells : row.cells;
        for (auto& cell : cells)
            cell.muted = true;
    }

    for (const auto& [step, velocity] : parsed.stepEvents)
    {
        for (auto& row : rows)
        {
            auto& cells = fillModeActive ? row.fillCells : row.cells;
            if (step < 0 || step >= static_cast<int>(cells.size()))
                continue;
            auto& cell = cells[static_cast<size_t>(step)];
            cell.muted = false;
            cell.velocity = juce::jlimit(0.0f, 1.0f, static_cast<float>(velocity) / 127.0f);
        }
    }

    if (onCellsChanged)
        onCellsChanged();
    repaint();
}

// ---------------------------------------------------------------
void DrumGridView::showStepContextMenu(int rowIndex, int stepIndex, juce::Point<int> screenPos)
{
    static GridRollCell copiedStep;
    static bool hasCopiedStep = false;

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
    menu.addItem(6, "Set Swing Offset...");
    menu.addItem(7, "Override Pitch...");
    menu.addItem(8, "Ghost Note", true, cell.probability < 0.5f);
    menu.addItem(9, "Copy Step");
    menu.addItem(10, "Paste Step");
    menu.addItem(11, "Fill to End");
    menu.addItem(12, "Clear from Here");
    menu.addSeparator();
    menu.addItem(500, "Mute",   true, cell.muted);
    menu.addItem(501, "Accent", true, cell.accented);

    const int result = menu.show();

    if (result == 1)
    {
        juce::String text;
        if (promptTextInput("Set Velocity",
                            "Enter velocity (0.0 - 1.0):",
                            juce::String(cell.velocity),
                            text)
            && text.isNotEmpty())
        {
            auto& r2 = rows[static_cast<size_t>(rowIndex)];
            auto& c2 = fillModeActive ? r2.fillCells : r2.cells;
            c2[static_cast<size_t>(stepIndex)].velocity =
                juce::jlimit(0.0f, 1.0f, text.getFloatValue());
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
    }
    else if (result == 2)
    {
        juce::String text;
        if (promptTextInput("Set Probability",
                            "Enter probability (0.0 - 1.0):",
                            juce::String(cell.probability),
                            text)
            && text.isNotEmpty())
        {
            auto& r2 = rows[static_cast<size_t>(rowIndex)];
            auto& c2 = fillModeActive ? r2.fillCells : r2.cells;
            c2[static_cast<size_t>(stepIndex)].probability =
                juce::jlimit(0.0f, 1.0f, text.getFloatValue());
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
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
    else if (result == 6)
    {
        juce::String text;
        if (promptTextInput("Set Swing Offset",
                            "Swing (-1.0 to 1.0):",
                            juce::String(cell.swing),
                            text)
            && text.isNotEmpty())
        {
            auto& r2 = rows[static_cast<size_t>(rowIndex)];
            auto& c2 = fillModeActive ? r2.fillCells : r2.cells;
            c2[static_cast<size_t>(stepIndex)].swing = juce::jlimit(-1.0f, 1.0f, text.getFloatValue());
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
    }
    else if (result == 7)
    {
        juce::String text;
        if (promptTextInput("Override Pitch",
                            "MIDI note (0-127):",
                            juce::String(cell.midiNote),
                            text)
            && text.isNotEmpty())
        {
            auto& r2 = rows[static_cast<size_t>(rowIndex)];
            auto& c2 = fillModeActive ? r2.fillCells : r2.cells;
            c2[static_cast<size_t>(stepIndex)].midiNote = juce::jlimit(0, 127, text.getIntValue());
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
    }
    else if (result == 8)
    {
        cell.probability = (cell.probability < 0.5f) ? 1.0f : 0.3f;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 9)
    {
        copiedStep = cell;
        hasCopiedStep = true;
        if (onStatusMessage)
            onStatusMessage("Step copied");
    }
    else if (result == 10)
    {
        if (hasCopiedStep)
        {
            auto pasted = copiedStep;
            pasted.cellId = juce::Uuid().toString();
            cells[static_cast<size_t>(stepIndex)] = pasted;
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
    }
    else if (result == 11)
    {
        for (int i = stepIndex; i < static_cast<int>(cells.size()); ++i)
            cells[static_cast<size_t>(i)].muted = false;
        repaint();
        if (onCellsChanged) onCellsChanged();
    }
    else if (result == 12)
    {
        for (int i = stepIndex; i < static_cast<int>(cells.size()); ++i)
            cells[static_cast<size_t>(i)].muted = true;
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
