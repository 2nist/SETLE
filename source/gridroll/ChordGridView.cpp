#include "ChordGridView.h"
#include "../theme/ThemeManager.h"

#include <algorithm>
#include <cmath>

namespace setle::gridroll
{

// ---------------------------------------------------------------
// Colour helpers
// ---------------------------------------------------------------
static constexpr int kResizeHandleWidth = 8;
static constexpr int kMinCellPixelWidth = 20;

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
ChordGridView::ChordGridView(model::Song& songRef)
    : song(songRef)
{
    addAndMakeVisible(addChordButton);
    const auto& theme = ThemeManager::get().theme();
    addChordButton.setColour(juce::TextButton::buttonColourId, theme.controlBg);
    addChordButton.setColour(juce::TextButton::textColourOffId, theme.controlText.withAlpha(0.9f));
    addChordButton.onClick = [this]
    {
        showAddChordPopover(addChordButton.getScreenBounds().getBottomLeft());
    };
    setSize(400, kHeight);
    startTimerHz(30);
}

// ---------------------------------------------------------------
void ChordGridView::loadProgression(const juce::String& id)
{
    progressionId = id;
    cells.clear();
    selectedCellIndex = -1;

    auto progOpt = song.findProgressionById(id);
    if (!progOpt.has_value())
    {
        rebuildLayoutCache();
        repaint();
        return;
    }

    const auto& prog = *progOpt;
    double cursor = 0.0;
    for (const auto& chord : prog.getChords())
    {
        GridRollCell cell;
        cell.cellId        = chord.getId();
        cell.sourceId      = id;
        cell.startBeat     = chord.getStartBeats();
        cell.durationBeats = chord.getDurationBeats() > 0.0 ? chord.getDurationBeats() : 1.0;
        cell.type          = GridRollCell::CellType::Chord;
        cell.chordSymbol   = chord.getSymbol();
        cell.chordFunction = chord.getFunction();
        cell.romanNumeral  = chord.getName();   // name stores Roman numeral
        cell.source        = chord.getSource();
        cell.confidence    = chord.getConfidence();
        cell.midiNote      = chord.getRootMidi();
        cursor += cell.durationBeats;
        cells.push_back(std::move(cell));
    }

    rebuildLayoutCache();
    repaint();
}

// ---------------------------------------------------------------
void ChordGridView::setPlayheadBeat(double beat)
{
    if (std::abs(beat - playheadBeat) > 0.001)
    {
        playheadBeat = beat;
        repaint();
    }
}

void ChordGridView::setMeterContext(const setle::theory::MeterContext& meter)
{
    if (meterContext.numerator == meter.numerator
        && meterContext.denominator == meter.denominator)
        return;

    meterContext = meter;
    repaint();
}

// ---------------------------------------------------------------
double ChordGridView::totalBeats() const
{
    double total = 0.0;
    for (const auto& c : cells)
        total += c.durationBeats;
    return total > 0.0 ? total : 4.0;
}

double ChordGridView::getTotalBeats() const
{
    return totalBeats();
}

double ChordGridView::beatsPerPixel() const
{
    const int availW = getWidth() - 32;  // leave room for + button
    return availW > 0 ? totalBeats() / static_cast<double>(availW) : 1.0;
}

double ChordGridView::snapBeat(double rawBeat) const
{
    double gridSize = 0.25;
    if      (theorySnap == "bar" || theorySnap == "1/1") gridSize = meterContext.beatsPerBar();
    else if (theorySnap == "1/2")                          gridSize = meterContext.beatsPerBar() / 2.0;
    else if (theorySnap == "1/4")                          gridSize = meterContext.beatUnit();
    else if (theorySnap == "halfBeat")                     gridSize = meterContext.beatUnit() / 2.0;
    else if (theorySnap == "1/8")                          gridSize = 0.5;
    else if (theorySnap == "1/16")                         gridSize = 0.25;
    else if (theorySnap == "1/32")                         gridSize = 0.125;

    return std::round(rawBeat / gridSize) * gridSize;
}

void ChordGridView::rebuildLayoutCache()
{
    cellBounds.clear();
    if (cells.empty())
        return;

    const int h = getHeight();
    const double bpp = beatsPerPixel();
    double cursor = 0.0;
    for (const auto& c : cells)
    {
        const int x = static_cast<int>(cursor / bpp);
        const int w = juce::jmax(kMinCellPixelWidth, static_cast<int>(c.durationBeats / bpp));
        cellBounds.push_back({ x, 0, w, h });
        cursor += c.durationBeats;
    }
}

// ---------------------------------------------------------------
juce::Colour ChordGridView::functionColour(const juce::String& fn) const
{
    const auto& theme = ThemeManager::get().theme();
    if (fn == "T" || fn == "tonic" || fn == "t")
        return theme.zoneB;
    if (fn == "PD" || fn == "subdominant" || fn == "pd")
        return theme.zoneA;
    if (fn == "D" || fn == "dominant" || fn == "d")
        return theme.accentWarm;
    return theme.surfaceEdge;
}

juce::String ChordGridView::functionTag(const juce::String& fn) const
{
    if (fn.equalsIgnoreCase("tonic") || fn.equalsIgnoreCase("t"))
        return "T";
    if (fn.equalsIgnoreCase("subdominant") || fn.equalsIgnoreCase("pd"))
        return "PD";
    if (fn.equalsIgnoreCase("dominant") || fn.equalsIgnoreCase("d"))
        return "D";

    return fn.toUpperCase().trim();
}

// ---------------------------------------------------------------
void ChordGridView::paintCell(juce::Graphics& g,
                               const GridRollCell& cell,
                               juce::Rectangle<int> bounds,
                               bool selected,
                               bool active) const
{
    const auto& theme = ThemeManager::get().theme();
    const auto base = functionColour(cell.chordFunction);
    const auto body = bounds.toFloat().reduced(1.0f);
    const float pulse = 0.5f + 0.5f * std::sin(pulsePhase);

    juce::Colour bg = theme.surface3.interpolatedWith(base, 0.38f);
    if (active)
        bg = bg.brighter(0.20f);
    g.setColour(bg);
    g.fillRoundedRectangle(body, 4.0f);

    // Keep function tint behavior from the existing implementation.
    g.setColour(base.withAlpha(active ? 0.46f : 0.36f));
    g.fillRoundedRectangle(body, 4.0f);

    if (cell.accented)
    {
        g.setColour(theme.accentWarm.withAlpha(0.95f));
        g.fillRect(bounds.getX() + 1, bounds.getY() + 1, bounds.getWidth() - 2, 3);
    }

    if (selected)
    {
        g.setColour(theme.accent);
        g.drawRoundedRectangle(body, 4.0f, 2.0f);
    }
    else
    {
        g.setColour(theme.surfaceEdge.withAlpha(0.55f));
        g.drawRoundedRectangle(body, 4.0f, 1.0f);
    }

    if (active)
    {
        g.setColour(theme.accent.withAlpha(0.45f + 0.45f * pulse));
        g.drawRoundedRectangle(body.expanded(0.5f), 4.0f, 1.8f);
    }

    auto content = bounds.reduced(6, 4);
    auto velocityStrip = content.removeFromBottom(10);
    auto topRow = content.removeFromTop(14);
    auto arpRow = content.removeFromBottom(14);
    auto symbolArea = content;

    const auto fn = functionTag(cell.chordFunction);
    juce::String leftText = cell.romanNumeral.isNotEmpty() ? cell.romanNumeral : "-";
    if (fn.isNotEmpty())
        leftText << "  [" << fn << "]";
    g.setColour(theme.inkLight.withAlpha(0.92f));
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    g.drawText(leftText, topRow.removeFromLeft(juce::jmax(36, topRow.getWidth() - 72)),
               juce::Justification::centredLeft, true);

    juce::String rightText;
    if (cell.ratchetCount > 1)
        rightText << juce::String::fromUTF8("\xc3\x97") << juce::String(cell.ratchetCount);
    if (cell.probability < 0.999f)
    {
        if (rightText.isNotEmpty())
            rightText << "  ";
        rightText << juce::String(juce::roundToInt(cell.probability * 100.0f)) << "%";
    }
    g.setColour(theme.inkLight.withAlpha(0.78f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText(rightText, topRow, juce::Justification::centredRight, false);

    g.setColour(theme.inkLight.withAlpha(0.96f));
    g.setFont(juce::FontOptions(std::min(22.0f, static_cast<float>(symbolArea.getHeight()) * 0.75f)).withStyle("Bold"));
    g.drawText(cell.chordSymbol, symbolArea, juce::Justification::centred, true);

    if (cell.arpMode != GridRollCell::ArpMode::Off)
    {
        juce::String arpText = "♪ ";
        switch (cell.arpMode)
        {
            case GridRollCell::ArpMode::Up: arpText += "Up"; break;
            case GridRollCell::ArpMode::Down: arpText += "Down"; break;
            case GridRollCell::ArpMode::UpDown: arpText += "UpDown"; break;
            case GridRollCell::ArpMode::Random: arpText += "Random"; break;
            case GridRollCell::ArpMode::Off: break;
        }
        g.setColour(theme.inkMid.withAlpha(0.92f));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(arpText, arpRow, juce::Justification::centredLeft, true);
    }

    g.setColour(theme.surfaceEdge.withAlpha(0.45f));
    g.fillRect(velocityStrip);
    auto filledVelocity = velocityStrip.withWidth(static_cast<int>(velocityStrip.getWidth() * juce::jlimit(0.0f, 1.0f, cell.velocity)));
    g.setColour(base.brighter(0.35f).withAlpha(0.9f));
    g.fillRect(filledVelocity);

    // Confidence dot: shown for MIDI-derived content.
    if (cell.source.equalsIgnoreCase("midi"))
    {
        auto dotArea = juce::Rectangle<float>(static_cast<float>(bounds.getRight() - 8),
                                              static_cast<float>(bounds.getY() + 4),
                                              4.0f, 4.0f);
        const auto dotColor = cell.confidence >= 0.9f ? theme.zoneC : theme.zoneD;
        g.setColour(dotColor.withAlpha(0.95f));
        g.fillEllipse(dotArea);
    }

    // Muted cells are visually suppressed with overlay + diagonal hatch.
    if (cell.muted)
    {
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.fillRoundedRectangle(body, 4.0f);

        g.setColour(theme.inkLight.withAlpha(0.08f));
        const auto clip = bounds.reduced(2);
        for (int x = clip.getX() - clip.getHeight(); x < clip.getRight(); x += 6)
            g.drawLine(static_cast<float>(x), static_cast<float>(clip.getBottom()),
                       static_cast<float>(x + clip.getHeight()), static_cast<float>(clip.getY()),
                       1.0f);
    }

    // Resize handle triangle in the bottom-right corner.
    juce::Path handle;
    const float hx = static_cast<float>(bounds.getRight() - 2);
    const float hy = static_cast<float>(bounds.getBottom() - 2);
    handle.startNewSubPath(hx - 8.0f, hy);
    handle.lineTo(hx, hy - 8.0f);
    handle.lineTo(hx, hy);
    handle.closeSubPath();
    g.setColour(theme.inkGhost.withAlpha(0.85f));
    g.fillPath(handle);
}

// ---------------------------------------------------------------
void ChordGridView::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    g.fillAll(theme.surface2);

    // Beat grid lines (denominator unit spacing)
    const double bpp = beatsPerPixel();
    const double total = totalBeats();
    const double beatUnit = meterContext.beatUnit();
    const double barSize  = meterContext.beatsPerBar();

    g.setColour(theme.inkLight.withAlpha(0.14f));
    for (double beat = 0.0; beat <= total + 0.0001; beat += beatUnit)
    {
        const int x = static_cast<int>(beat / bpp);
        g.fillRect(x, 0, 1, getHeight());
    }

    g.setColour(theme.inkLight.withAlpha(0.30f));
    for (double beat = 0.0; beat <= total + 0.0001; beat += barSize)
    {
        const int x = static_cast<int>(beat / bpp);
        g.fillRect(x, 0, 1, getHeight());
    }

    // Cells
    const size_t n = cells.size();
    double cellBeatStart = 0.0;
    for (size_t i = 0; i < n && i < cellBounds.size(); ++i)
    {
        const auto& cell = cells[i];
        const bool active = playheadBeat >= cellBeatStart
                            && playheadBeat < (cellBeatStart + juce::jmax(0.001, cell.durationBeats));
        paintCell(g,
                  cell,
                  cellBounds[i],
                  static_cast<int>(i) == selectedCellIndex,
                  active);

        if (i + 1 < cellBounds.size())
        {
            g.setColour(theme.surfaceEdge.withAlpha(0.85f));
            g.fillRect(cellBounds[i].getRight() - 1, 0, 1, getHeight());
        }

        cellBeatStart += cell.durationBeats;
    }

    // Playhead
    if (playheadBeat >= 0.0)
    {
        const int px = static_cast<int>(playheadBeat / bpp);
        g.setColour(theme.playheadColor.withAlpha(0.9f));
        g.fillRect(px, 0, 2, getHeight());
    }

    // Snap unit readout in header area.
    juce::String snapLabel = theorySnap;
    if (theorySnap == "1/4")
        snapLabel = (std::abs(meterContext.beatUnit() - 0.5) < 0.001) ? "1/8" : "1 beat";
    else if (theorySnap == "halfBeat")
        snapLabel = (std::abs(meterContext.beatUnit() - 0.5) < 0.001) ? "1/16" : "1/8";

    g.setColour(theme.inkMuted.withAlpha(0.85f));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("Snap: " + snapLabel,
               6, 4, 96, 14,
               juce::Justification::centredLeft,
               false);
}

// ---------------------------------------------------------------
void ChordGridView::resized()
{
    rebuildLayoutCache();
    // Place + button at the right
    addChordButton.setBounds(getWidth() - 28, 4, 24, getHeight() - 8);
}

// ---------------------------------------------------------------
void ChordGridView::mouseMove(const juce::MouseEvent& e)
{
    const auto hit = hitTest(e.getPosition());
    const bool shouldResize = hit.cellIndex >= 0 && hit.isRightEdge;
    if (hoverResizeHandle == shouldResize)
        return;

    hoverResizeHandle = shouldResize;
    setMouseCursor(hoverResizeHandle
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
}

// ---------------------------------------------------------------
ChordGridView::CellLayout ChordGridView::hitTest(juce::Point<int> pos) const
{
    for (int i = static_cast<int>(cellBounds.size()) - 1; i >= 0; --i)
    {
        const auto& b = cellBounds[i];
        if (!b.contains(pos))
            continue;
        const bool edge = pos.getX() >= b.getRight() - kResizeHandleWidth;
        return { i, b, edge };
    }
    return { -1, {}, false };
}

// ---------------------------------------------------------------
void ChordGridView::mouseDown(const juce::MouseEvent& e)
{
    dragKind = DragKind::None;
    dragCellIndex = -1;

    const auto hit = hitTest(e.getPosition());
    if (hit.cellIndex < 0)
        return;

    selectedCellIndex = hit.cellIndex;
    repaint();

    if (e.mods.isRightButtonDown())
    {
        showCellContextMenu(hit.cellIndex, e.getScreenPosition());
        return;
    }

    dragStartPos   = e.getPosition();
    dragCellIndex  = hit.cellIndex;
    dragIsDuplicate = e.mods.isCtrlDown();

    if (hit.isRightEdge)
    {
        dragKind         = DragKind::ResizeRight;
        dragOrigDuration = cells[static_cast<size_t>(hit.cellIndex)].durationBeats;
        dragStartBeat    = cells[static_cast<size_t>(hit.cellIndex)].startBeat;
    }
    else
    {
        dragKind      = DragKind::MoveCell;
        dragOrigStart = cells[static_cast<size_t>(hit.cellIndex)].startBeat;
    }
}

// ---------------------------------------------------------------
void ChordGridView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragKind == DragKind::None || dragCellIndex < 0)
        return;

    const double bpp = beatsPerPixel();
    const double deltaPx = static_cast<double>(e.getPosition().getX() - dragStartPos.getX());
    const double deltaBeats = deltaPx * bpp;

    if (dragKind == DragKind::ResizeRight)
    {
        const double minDur = snapBeat(1.0 / 32.0);
        double newDur = snapBeat(dragOrigDuration + deltaBeats);
        newDur = juce::jmax(minDur, newDur);
        cells[static_cast<size_t>(dragCellIndex)].durationBeats = newDur;
        rebuildLayoutCache();
        repaint();
    }
    else if (dragKind == DragKind::MoveCell)
    {
        // Reorder: swap cells while dragging left/right
        const auto hit = hitTest(e.getPosition());
        if (hit.cellIndex >= 0 && hit.cellIndex != dragCellIndex)
        {
            const int from = dragCellIndex;
            const int to   = hit.cellIndex;
            std::swap(cells[static_cast<size_t>(from)], cells[static_cast<size_t>(to)]);
            std::swap(cellBounds[static_cast<size_t>(from)], cellBounds[static_cast<size_t>(to)]);
            dragCellIndex  = to;
            selectedCellIndex = to;
            rebuildLayoutCache();
            repaint();
        }
    }
}

// ---------------------------------------------------------------
void ChordGridView::mouseUp(const juce::MouseEvent&)
{
    if (dragKind != DragKind::None && onCellsChanged)
        onCellsChanged();

    if (dragIsDuplicate && dragKind == DragKind::MoveCell && dragCellIndex >= 0)
    {
        // Insert a copy after current position
        auto copy = cells[static_cast<size_t>(dragCellIndex)];
        copy.cellId = juce::Uuid().toString();
        cells.insert(cells.begin() + dragCellIndex + 1, copy);
        rebuildLayoutCache();
        repaint();
        if (onCellsChanged)
            onCellsChanged();
    }

    dragKind      = DragKind::None;
    dragCellIndex = -1;
    dragIsDuplicate = false;
}

// ---------------------------------------------------------------
void ChordGridView::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto hit = hitTest(e.getPosition());
    if (hit.cellIndex >= 0 && onCellDoubleClicked)
        onCellDoubleClicked(hit.cellIndex);
}

// ---------------------------------------------------------------
void ChordGridView::showCellContextMenu(int cellIndex, juce::Point<int> screenPos)
{
    auto& cell = cells[static_cast<size_t>(cellIndex)];

    juce::PopupMenu menu;
    menu.addSectionHeader(cell.chordSymbol.isNotEmpty() ? cell.chordSymbol : "Cell");

    // Velocity
    menu.addItem(1, "Set Velocity...");
    // Probability
    menu.addItem(2, "Set Probability...");

    // Ratchet sub-menu
    {
        juce::PopupMenu ratchetMenu;
        for (int n : { 1, 2, 3, 4, 8 })
            ratchetMenu.addItem(100 + n, juce::String(n) + "x",
                                true, cell.ratchetCount == n);
        menu.addSubMenu("Ratchet", ratchetMenu);
    }

    // Arp sub-menu
    {
        juce::PopupMenu arpMenu;
        using AM = GridRollCell::ArpMode;
        arpMenu.addItem(200, "Off",    true, cell.arpMode == AM::Off);
        arpMenu.addItem(201, "Up",     true, cell.arpMode == AM::Up);
        arpMenu.addItem(202, "Down",   true, cell.arpMode == AM::Down);
        arpMenu.addItem(203, "UpDown", true, cell.arpMode == AM::UpDown);
        arpMenu.addItem(204, "Random", true, cell.arpMode == AM::Random);
        menu.addSubMenu("Arp Mode", arpMenu);
    }

    // Strum sub-menu
    {
        juce::PopupMenu strumMenu;
        using SD = GridRollCell::StrumDir;
        strumMenu.addItem(300, "None", true, cell.strumDir == SD::None);
        strumMenu.addItem(301, "Up",   true, cell.strumDir == SD::Up);
        strumMenu.addItem(302, "Down", true, cell.strumDir == SD::Down);
        menu.addSubMenu("Strum", strumMenu);
    }

    // Divide sub-menu
    {
        juce::PopupMenu divMenu;
        divMenu.addItem(401, "÷ 2");
        divMenu.addItem(402, "÷ 3");
        divMenu.addItem(403, "÷ 4");
        menu.addSubMenu("Divide", divMenu);
    }

    menu.addSeparator();
    menu.addItem(500, "Mute Cell",   true, cell.muted);
    menu.addItem(501, "Accent Cell", true, cell.accented);
    menu.addSeparator();
    menu.addItem(700, "Duplicate Cell");
    menu.addItem(701, "Transpose...");
    menu.addItem(702, "Set Swing...");
    menu.addItem(703, "Humanize", true, std::abs(cell.swing) > 0.0001f);
    menu.addItem(704, "Open Note Mode");
    menu.addItem(705, "Assign FX Block...");
    menu.addSeparator();
    menu.addItem(600, "Delete Cell");

    auto safeThis = juce::Component::SafePointer<ChordGridView>(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(
            juce::Rectangle<int>(screenPos.getX(), screenPos.getY(), 1, 1)),
        [safeThis, cellIndex](int result)
        {
            if (safeThis != nullptr)
                safeThis->handleCellContextMenuResult(cellIndex, result);
        });
}

void ChordGridView::handleCellContextMenuResult(int cellIndex, int result)
{
    if (result == 0 || cellIndex < 0 || cellIndex >= static_cast<int>(cells.size()))
        return;

    auto& cell = cells[static_cast<size_t>(cellIndex)];

    if (result == 1)
    {
        juce::String text;
        if (promptTextInput("Set Velocity",
                            "Enter velocity (0.0 - 1.0):",
                            juce::String(cell.velocity),
                            text)
            && text.isNotEmpty())
        {
            cells[static_cast<size_t>(cellIndex)].velocity =
                juce::jlimit(0.0f, 1.0f, text.getFloatValue());
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
        return;
    }

    if (result == 2)
    {
        juce::String text;
        if (promptTextInput("Set Probability",
                            "Enter probability (0.0 - 1.0):",
                            juce::String(cell.probability),
                            text)
            && text.isNotEmpty())
        {
            cells[static_cast<size_t>(cellIndex)].probability =
                juce::jlimit(0.0f, 1.0f, text.getFloatValue());
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
        return;
    }

    if (result >= 101 && result <= 108)
    {
        cell.ratchetCount = result - 100;
    }
    else if (result >= 200 && result <= 204)
    {
        using AM = GridRollCell::ArpMode;
        const AM modes[] = { AM::Off, AM::Up, AM::Down, AM::UpDown, AM::Random };
        cell.arpMode = modes[result - 200];
    }
    else if (result >= 300 && result <= 302)
    {
        using SD = GridRollCell::StrumDir;
        const SD dirs[] = { SD::None, SD::Up, SD::Down };
        cell.strumDir = dirs[result - 300];
    }
    else if (result == 401) divideCells(cellIndex, 2);
    else if (result == 402) divideCells(cellIndex, 3);
    else if (result == 403) divideCells(cellIndex, 4);
    else if (result == 500) cell.muted    = !cell.muted;
    else if (result == 501) cell.accented = !cell.accented;
    else if (result == 700)
    {
        auto copy = cell;
        copy.cellId = juce::Uuid().toString();
        cells.insert(cells.begin() + cellIndex + 1, copy);
    }
    else if (result == 701)
    {
        juce::String text;
        if (promptTextInput("Transpose Cell",
                            "Enter semitone offset (+/-):",
                            "0",
                            text)
            && text.isNotEmpty())
        {
            auto& c = cells[static_cast<size_t>(cellIndex)];
            c.midiNote = juce::jlimit(0, 127, c.midiNote + juce::jlimit(-48, 48, text.getIntValue()));
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
        return;
    }
    else if (result == 702)
    {
        juce::String text;
        if (promptTextInput("Set Swing",
                            "Enter swing amount (-1.0 to 1.0):",
                            juce::String(cell.swing),
                            text)
            && text.isNotEmpty())
        {
            cells[static_cast<size_t>(cellIndex)].swing =
                juce::jlimit(-1.0f, 1.0f, text.getFloatValue());
            repaint();
            if (onCellsChanged) onCellsChanged();
        }
        return;
    }
    else if (result == 703)
    {
        cell.swing = (std::abs(cell.swing) < 0.0001f) ? 0.15f : 0.0f;
    }
    else if (result == 704)
    {
        if (onOpenNoteDetail)
            onOpenNoteDetail(cellIndex);
        return;
    }
    else if (result == 705)
    {
        if (onStatusMessage)
            onStatusMessage("Assign FX Block is available in the FX tab workflow.");
        return;
    }
    else if (result == 600)
    {
        cells.erase(cells.begin() + static_cast<size_t>(cellIndex));
        if (selectedCellIndex >= static_cast<int>(cells.size()))
            selectedCellIndex = static_cast<int>(cells.size()) - 1;
    }

    rebuildLayoutCache();
    repaint();
    if (onCellsChanged)
        onCellsChanged();
}

// ---------------------------------------------------------------
void ChordGridView::divideCells(int cellIndex, int n)
{
    if (n < 2 || cellIndex < 0 || cellIndex >= static_cast<int>(cells.size()))
        return;

    const GridRollCell parent = cells[static_cast<size_t>(cellIndex)];
    const double subDur = parent.durationBeats / static_cast<double>(n);

    std::vector<GridRollCell> replacements;
    for (int i = 0; i < n; ++i)
    {
        GridRollCell sub  = parent;
        sub.cellId        = juce::Uuid().toString();
        sub.startBeat     = parent.startBeat + i * subDur;
        sub.durationBeats = subDur;
        replacements.push_back(std::move(sub));
    }

    cells.erase(cells.begin() + cellIndex);
    cells.insert(cells.begin() + cellIndex,
                 replacements.begin(), replacements.end());

    rebuildLayoutCache();
    repaint();
}

// ---------------------------------------------------------------
void ChordGridView::showAddChordPopover(juce::Point<int> screenPos)
{
    // Build diatonic chord palette based on progression's key/mode
    juce::PopupMenu menu;
    menu.addSectionHeader("Add Chord");

    auto progOpt = song.findProgressionById(progressionId);
    const juce::String key  = progOpt.has_value() ? progOpt->getKey()  : "C";
    const juce::String mode = progOpt.has_value() ? progOpt->getMode() : "ionian";

    // Seven diatonic scale degrees
    static const juce::String degrees[]    = { "I", "ii", "iii", "IV", "V", "vi", "vii°" };
    static const int          semitones[]  = {  0,   2,    4,    5,   7,   9,    11 };
    static const juce::String qualities[]  = { "maj", "min", "min", "maj", "maj", "min", "dim" };

    // Root semitone from note name
    static const juce::String noteNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    int rootSemitone = 0;
    for (int i = 0; i < 12; ++i)
        if (noteNames[i].equalsIgnoreCase(key)) { rootSemitone = i; break; }

    for (int deg = 0; deg < 7; ++deg)
    {
        const int noteIdx = (rootSemitone + semitones[deg]) % 12;
        const juce::String noteName  = noteNames[noteIdx];
        const juce::String symbol    = noteName + qualities[deg];
        const juce::String label     = degrees[deg] + "  " + symbol;
        menu.addItem(deg + 1, label);
    }

    menu.addSeparator();
    menu.addItem(20, "Type chord symbol...");

    auto safeThis = juce::Component::SafePointer<ChordGridView>(this);
    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetScreenArea(
            juce::Rectangle<int>(screenPos.getX(), screenPos.getY(), 1, 1)),
        [safeThis, rootSemitone](int result)
        {
            if (safeThis != nullptr)
                safeThis->handleAddChordPopoverResult(result, rootSemitone);
        });
}

void ChordGridView::handleAddChordPopoverResult(int result, int rootSemitone)
{
    if (result == 0)
        return;

    static const juce::String degrees[]    = { "I", "ii", "iii", "IV", "V", "vi", "vii°" };
    static const int          semitones[]  = {  0,   2,    4,    5,   7,   9,    11 };
    static const juce::String qualities[]  = { "maj", "min", "min", "maj", "maj", "min", "dim" };
    static const juce::String noteNames[]  = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

    if (result >= 1 && result <= 7)
    {
        const int deg = result - 1;
        const int noteIdx = (rootSemitone + semitones[deg]) % 12;
        const juce::String noteName = noteNames[noteIdx];
        const juce::String symbol   = noteName + qualities[deg];

        GridRollCell cell;
        cell.cellId        = juce::Uuid().toString();
        cell.sourceId      = progressionId;
        cell.startBeat     = totalBeats();
        cell.durationBeats = 1.0;
        cell.type          = GridRollCell::CellType::Chord;
        cell.chordSymbol   = symbol;
        cell.chordFunction = (deg == 0 || deg == 5) ? "T" :
                             (deg == 1 || deg == 3) ? "PD" : "D";
        cell.romanNumeral  = degrees[deg];
        cells.push_back(std::move(cell));

        rebuildLayoutCache();
        repaint();

        if (onAddChordRequested)
            onAddChordRequested(symbol);
        if (onCellsChanged)
            onCellsChanged();
    }
    else if (result == 20)
    {
        juce::String sym;
        if (promptTextInput("Add Chord",
                            "Enter chord symbol (e.g. Dm7, G#maj9):",
                            {},
                            sym)
            && sym.isNotEmpty())
        {
            GridRollCell cell;
            cell.cellId        = juce::Uuid().toString();
            cell.sourceId      = progressionId;
            cell.startBeat     = totalBeats();
            cell.durationBeats = 1.0;
            cell.type          = GridRollCell::CellType::Chord;
            cell.chordSymbol   = sym;
            cells.push_back(std::move(cell));

            rebuildLayoutCache();
            repaint();

            if (onAddChordRequested)
                onAddChordRequested(sym);
            if (onCellsChanged)
                onCellsChanged();
        }
    }
}

void ChordGridView::timerCallback()
{
    pulsePhase += 0.17f;
    if (pulsePhase >= juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    if (playheadBeat >= 0.0)
        repaint();
}

} // namespace setle::gridroll
