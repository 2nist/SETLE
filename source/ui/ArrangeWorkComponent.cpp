#include "ArrangeWorkComponent.h"

#include "../theme/ThemeStyleHelpers.h"
#include "../theory/DiatonicHarmony.h"

#include <algorithm>
#include <cmath>

namespace setle::ui
{

namespace
{
juce::String cadenceLabelForState(ArrangeWorkComponent::CadenceState state)
{
    switch (state)
    {
        case ArrangeWorkComponent::CadenceState::standard: return "Cadence Locked";
        case ArrangeWorkComponent::CadenceState::clash: return "Harmonic Clash";
        case ArrangeWorkComponent::CadenceState::neutral: break;
    }
    return "Floating Link";
}
}

void ArrangeWorkComponent::SectionBlockComponent::setTileModel(TileModel newModel)
{
    model = std::move(newModel);
    repaint();
}

void ArrangeWorkComponent::SectionBlockComponent::setSelected(bool isSelected)
{
    if (selected == isSelected)
        return;
    selected = isSelected;
    repaint();
}

void ArrangeWorkComponent::SectionBlockComponent::setDragging(bool isDragging)
{
    if (dragging == isDragging)
        return;
    dragging = isDragging;
    repaint();
}

void ArrangeWorkComponent::SectionBlockComponent::paint(juce::Graphics& g)
{
    auto& themeManager = ThemeManager::get();
    const auto& theme = themeManager.theme();
    auto area = getLocalBounds().toFloat().reduced(2.0f);
    auto glassColour = themeManager.getColor(ThemeManager::ThemeKey::surfaceLow)
                          .interpolatedWith(themeManager.getColor(ThemeManager::ThemeKey::windowBackground), 0.25f);
    setle::theme::drawGlassPanel(g, area, glassColour, theme, true, 14.0f);

    if (selected || dragging)
    {
        setle::theme::drawIndicatorGlow(g,
                                        area.getCentre(),
                                        juce::jmax(area.getWidth(), area.getHeight()) * 0.34f,
                                        theme.primaryAccent,
                                        theme,
                                        dragging ? 1.0f : 0.72f,
                                        true);
        g.setColour(theme.primaryAccent.withAlpha(dragging ? 0.92f : 0.72f));
        g.drawRoundedRectangle(area.reduced(1.0f), 14.0f, dragging ? 2.8f : 2.0f);
    }

    auto content = area.reduced(14.0f, 12.0f);
    auto titleArea = content.removeFromTop(22.0f);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkDark).withAlpha(0.35f));
    g.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    g.drawText(model.section.getName(), titleArea.translated(0.0f, 1.0f).toNearestInt(), juce::Justification::centredLeft, true);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.92f));
    g.drawText(model.section.getName(), titleArea.toNearestInt(), juce::Justification::centredLeft, true);

    auto summaryArea = content.removeFromTop(20.0f);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.84f));
    g.setFont(juce::FontOptions(13.0f).withKerningFactor(0.05f));
    g.drawFittedText(model.progressionSummary, summaryArea.toNearestInt(), juce::Justification::centredLeft, 1);

    auto pipBounds = content.removeFromTop(86.0f);
    setle::theme::drawFuzzyInsetPanel(g, pipBounds, theme.surfaceLow, theme, true, 10.0f);

    auto pipInner = pipBounds.reduced(10.0f, 10.0f);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.08f));
    for (int i = 1; i < 8; ++i)
    {
        const auto y = pipInner.getY() + pipInner.getHeight() * (static_cast<float>(i) / 8.0f);
        g.drawHorizontalLine(static_cast<int>(std::round(y)), pipInner.getX(), pipInner.getRight());
    }

    for (const auto& noteRect : model.notePips)
    {
        auto scaled = juce::Rectangle<float>(pipInner.getX() + noteRect.getX() * pipInner.getWidth(),
                                             pipInner.getY() + noteRect.getY() * pipInner.getHeight(),
                                             juce::jmax(4.0f, noteRect.getWidth() * pipInner.getWidth()),
                                             juce::jmax(3.0f, noteRect.getHeight() * pipInner.getHeight()));
        g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.16f));
        g.fillRoundedRectangle(scaled, 2.0f);
    }

    for (const auto& audioRect : model.audioPips)
    {
        auto scaled = juce::Rectangle<float>(pipInner.getX() + audioRect.getX() * pipInner.getWidth(),
                                             pipInner.getY() + audioRect.getY() * pipInner.getHeight(),
                                             juce::jmax(2.0f, audioRect.getWidth() * pipInner.getWidth()),
                                             juce::jmax(5.0f, audioRect.getHeight() * pipInner.getHeight()));
        g.setColour(theme.secondaryAccent.withAlpha(0.18f));
        g.fillRoundedRectangle(scaled, 2.0f);
    }

    auto footer = content.removeFromBottom(24.0f);
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkMid).withAlpha(0.68f));
    g.setFont(juce::FontOptions(11.5f));
    g.drawText("Repeat x" + juce::String(model.section.getRepeatCount())
                   + "  |  " + juce::String(model.section.getTimeSigNumerator())
                   + "/" + juce::String(model.section.getTimeSigDenominator()),
               footer.toNearestInt(),
               juce::Justification::centredLeft,
               false);
}

void ArrangeWorkComponent::SectionBlockComponent::mouseDown(const juce::MouseEvent& event)
{
    if (onSelected)
        onSelected(model.section.getId());
    if (onDragStarted)
        onDragStarted(model.section.getId(), event.getEventRelativeTo(getParentComponent()).position);
}

void ArrangeWorkComponent::SectionBlockComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (onDragged)
        onDragged(model.section.getId(), event.getEventRelativeTo(getParentComponent()).position);
}

void ArrangeWorkComponent::SectionBlockComponent::mouseUp(const juce::MouseEvent& event)
{
    if (onDragEnded)
        onDragEnded(model.section.getId(), event.getEventRelativeTo(getParentComponent()).position);
}

void ArrangeWorkComponent::TransitionConnector::setCadenceState(CadenceState newState)
{
    state = newState;
    repaint();
}

void ArrangeWorkComponent::TransitionConnector::setPulsePhase(float newPhase)
{
    pulsePhase = newPhase;
    repaint();
}

void ArrangeWorkComponent::TransitionConnector::paint(juce::Graphics& g)
{
    auto& themeManager = ThemeManager::get();
    const auto& theme = themeManager.theme();
    auto area = getLocalBounds().toFloat().reduced(4.0f, 18.0f);
    const auto centre = area.getCentre();

    juce::Colour accent = theme.surfaceLow.withAlpha(0.18f);
    float glow = 0.18f;
    if (state == CadenceState::standard)
    {
        accent = themeManager.getColor(ThemeManager::ThemeKey::secondaryAccent);
        glow = 0.55f + 0.35f * (0.5f + 0.5f * std::sin(pulsePhase));
    }
    else if (state == CadenceState::clash)
    {
        accent = themeManager.getColor(ThemeManager::ThemeKey::primaryAccent);
        glow = 0.24f + 0.20f * (0.5f + 0.5f * std::sin(pulsePhase * 1.7f));
    }

    setle::theme::drawIndicatorGlow(g, centre, area.getHeight() * 0.65f, accent, theme, glow, true);
    g.setColour(accent.withAlpha(0.68f));
    g.drawLine(area.getX(), centre.y, area.getRight(), centre.y, 3.0f);
    g.drawLine(area.getRight() - 11.0f, centre.y - 7.0f, area.getRight(), centre.y, 3.0f);
    g.drawLine(area.getRight() - 11.0f, centre.y + 7.0f, area.getRight(), centre.y, 3.0f);
}

ArrangeWorkComponent::ArrangeWorkComponent()
{
    ThemeManager::get().addListener(this);
    addAndMakeVisible(smartDuplicateButton);
    addAndMakeVisible(statusLabel);

    smartDuplicateButton.onClick = [this] { showDuplicateMenu(); };
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setFont(juce::FontOptions(12.5f));
    statusLabel.setText("Structural Mosaic ready", juce::dontSendNotification);

    startTimerHz(30);
}

ArrangeWorkComponent::~ArrangeWorkComponent()
{
    stopTimer();
    ThemeManager::get().removeListener(this);
}

void ArrangeWorkComponent::setSongStructure(std::vector<TileModel> newTiles,
                                            juce::String newSelectedSectionId,
                                            juce::String newSessionKey,
                                            juce::String newSessionMode,
                                            std::vector<CadenceState> newConnectorStates)
{
    tiles = std::move(newTiles);
    selectedSectionId = std::move(newSelectedSectionId);
    sessionKey = std::move(newSessionKey);
    sessionMode = std::move(newSessionMode);
    connectorStates = std::move(newConnectorStates);

    if (selectedSectionId.isEmpty() && !tiles.empty())
        selectedSectionId = tiles.front().section.getId();

    rebuildTiles();
    rebuildConnectors();
    layoutTiles();
    repaint();
}

void ArrangeWorkComponent::setPlayheadFraction(double fraction)
{
    playheadFraction = juce::jlimit(0.0, 1.0, fraction);
    repaint();
}

void ArrangeWorkComponent::paint(juce::Graphics& g)
{
    auto& themeManager = ThemeManager::get();
    const auto& theme = themeManager.theme();
    auto area = getLocalBounds().toFloat();
    const auto chassisBase = themeManager.getColor(ThemeManager::ThemeKey::windowBackground)
                                 .interpolatedWith(themeManager.getColor(ThemeManager::ThemeKey::surfaceVariant), 0.22f);
    setle::theme::drawChassis(g, area, chassisBase, theme, 12.0f);

    auto headerBounds = getHeaderBounds().toFloat();
    setle::theme::drawFuzzyInsetPanel(g, headerBounds, theme.surface1, theme, true, 10.0f);

    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.88f));
    g.setFont(juce::FontOptions(13.5f).withStyle("Bold"));
    g.drawText("Quick Utility", getHeaderBounds().reduced(14, 0).removeFromLeft(120), juce::Justification::centredLeft, false);

    auto floorBounds = getFloorBounds().toFloat();
    g.setColour(themeManager.getColor(ThemeManager::ThemeKey::inkLight).withAlpha(0.08f));
    for (int x = static_cast<int>(floorBounds.getX()) + 24; x < static_cast<int>(floorBounds.getRight()); x += 36)
        g.drawVerticalLine(x, floorBounds.getY(), floorBounds.getBottom());
}

void ArrangeWorkComponent::paintOverChildren(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    auto floorBounds = getFloorBounds().toFloat();
    const auto playheadX = floorBounds.getX() + static_cast<float>(playheadFraction) * floorBounds.getWidth();
    setle::theme::drawIndicatorGlow(g,
                                    { playheadX, floorBounds.getCentreY() },
                                    18.0f,
                                    theme.secondaryAccent,
                                    theme,
                                    0.82f,
                                    true);
    g.setColour(theme.secondaryAccent.withAlpha(0.92f));
    g.drawLine(playheadX, floorBounds.getY() + 8.0f, playheadX, floorBounds.getBottom() - 8.0f, 2.4f);
}

void ArrangeWorkComponent::resized()
{
    auto header = getHeaderBounds();
    smartDuplicateButton.setBounds(header.removeFromLeft(162).reduced(8, 8));
    statusLabel.setBounds(header.reduced(8, 4));
    layoutTiles();
}

void ArrangeWorkComponent::timerCallback()
{
    pulsePhase += 0.15f;
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    for (auto& connector : connectorComponents)
        connector->setPulsePhase(pulsePhase);
}

void ArrangeWorkComponent::themeChanged()
{
    repaint();
}

void ArrangeWorkComponent::showDuplicateMenu()
{
    if (selectedSectionId.isEmpty())
    {
        statusLabel.setText("Select a section tile first", juce::dontSendNotification);
        return;
    }

    juce::PopupMenu menu;
    menu.addSectionHeader("Smart Duplicate");
    menu.addItem(1, "Exact Object");
    menu.addItem(2, "Modal Extraction");
    menu.addItem(3, "Rhythmic Decimation");

    const auto result = menu.showMenu(juce::PopupMenu::Options().withTargetComponent(&smartDuplicateButton));
    if (result == 0 || onSmartDuplicateRequested == nullptr)
        return;

    DuplicateMode mode = DuplicateMode::exactObject;
    if (result == 2)
        mode = DuplicateMode::modalExtraction;
    else if (result == 3)
        mode = DuplicateMode::rhythmicDecimation;

    onSmartDuplicateRequested(selectedSectionId, mode);
}

void ArrangeWorkComponent::rebuildTiles()
{
    for (auto& tile : tileComponents)
        removeChildComponent(tile.get());
    tileComponents.clear();

    for (const auto& tile : tiles)
    {
        auto block = std::make_unique<SectionBlockComponent>();
        block->setTileModel(tile);
        block->setSelected(tile.section.getId() == selectedSectionId);
        block->onSelected = [this](const juce::String& sectionId)
        {
            selectedSectionId = sectionId;
            for (auto& component : tileComponents)
                component->setSelected(component->getSectionId() == selectedSectionId);
            statusLabel.setText("Selected section: " + sectionId.substring(0, 8), juce::dontSendNotification);
            if (onSectionSelected)
                onSectionSelected(sectionId);
        };
        block->onDragStarted = [this](const juce::String& sectionId, juce::Point<float> position)
        {
            draggedSectionId = sectionId;
            dragCurrentX = position.x;
            draggingOrder.clear();
            for (const auto& tileModel : tiles)
                draggingOrder.add(tileModel.section.getId());
            dragInsertIndex = insertionIndexForX(position.x);
            for (auto& component : tileComponents)
                component->setDragging(component->getSectionId() == draggedSectionId);
            statusLabel.setText("Re-shaping structure...", juce::dontSendNotification);
            if (onStructuralMeltChanged)
                onStructuralMeltChanged(1.0f);
            layoutTiles();
        };
        block->onDragged = [this](const juce::String& sectionId, juce::Point<float> position)
        {
            if (draggedSectionId != sectionId)
                return;
            dragCurrentX = position.x;
            dragInsertIndex = insertionIndexForX(position.x);
            layoutTiles();
        };
        block->onDragEnded = [this](const juce::String& sectionId, juce::Point<float> position)
        {
            if (draggedSectionId != sectionId)
                return;
            dragCurrentX = position.x;
            dragInsertIndex = insertionIndexForX(position.x);
            commitCurrentDragOrder();
            draggedSectionId.clear();
            if (onStructuralMeltChanged)
                onStructuralMeltChanged(0.0f);
            for (auto& component : tileComponents)
                component->setDragging(false);
            layoutTiles();
        };
        addAndMakeVisible(*block);
        tileComponents.push_back(std::move(block));
    }
}

void ArrangeWorkComponent::rebuildConnectors()
{
    for (auto& connector : connectorComponents)
        removeChildComponent(connector.get());
    connectorComponents.clear();

    const auto count = tiles.size() > 0 ? tiles.size() - 1 : 0;
    for (size_t i = 0; i < count; ++i)
    {
        auto connector = std::make_unique<TransitionConnector>();
        connector->setCadenceState(i < connectorStates.size() ? connectorStates[i] : CadenceState::neutral);
        connector->setPulsePhase(pulsePhase);
        addAndMakeVisible(*connector);
        connectorComponents.push_back(std::move(connector));
    }
}

void ArrangeWorkComponent::layoutTiles()
{
    if (tileComponents.empty())
        return;

    auto floor = getFloorBounds().reduced(12, 12);
    const int connectorWidth = 38;
    const int tileCount = static_cast<int>(tileComponents.size());
    const int totalConnectorWidth = juce::jmax(0, tileCount - 1) * connectorWidth;
    const int tileWidth = juce::jmax(160, (floor.getWidth() - totalConnectorWidth) / juce::jmax(1, tileCount));
    const int tileHeight = juce::jmin(220, floor.getHeight() - 24);
    const int y = floor.getY() + (floor.getHeight() - tileHeight) / 2;

    juce::StringArray orderedIds;
    if (draggingOrder.isEmpty())
    {
        for (const auto& tile : tiles)
            orderedIds.add(tile.section.getId());
    }
    else
    {
        orderedIds = draggingOrder;
        orderedIds.removeString(draggedSectionId);
        const auto insertAt = juce::jlimit(0, orderedIds.size(), dragInsertIndex);
        orderedIds.insert(insertAt, draggedSectionId);
    }

    int x = floor.getX();
    for (int orderedIndex = 0; orderedIndex < orderedIds.size(); ++orderedIndex)
    {
        const auto& sectionId = orderedIds.getReference(orderedIndex);
        auto it = std::find_if(tileComponents.begin(), tileComponents.end(), [&sectionId](const auto& component)
        {
            return component->getSectionId() == sectionId;
        });
        if (it == tileComponents.end())
            continue;

        auto bounds = juce::Rectangle<int>(x, y, tileWidth, tileHeight);
        if (sectionId == draggedSectionId)
            bounds.setPosition(static_cast<int>(std::round(dragCurrentX - bounds.getWidth() * 0.5f)), y - 8);
        (*it)->setBounds(bounds);
        x += tileWidth;

        if (orderedIndex < static_cast<int>(connectorComponents.size()))
        {
            connectorComponents[static_cast<size_t>(orderedIndex)]->setBounds(x, y + 34, connectorWidth, tileHeight - 68);
            x += connectorWidth;
        }
    }

    repaint();
}

juce::Rectangle<int> ArrangeWorkComponent::getHeaderBounds() const
{
    return getLocalBounds().reduced(10, 8).removeFromTop(50);
}

juce::Rectangle<int> ArrangeWorkComponent::getFloorBounds() const
{
    auto area = getLocalBounds().reduced(10, 8);
    area.removeFromTop(58);
    return area;
}

int ArrangeWorkComponent::insertionIndexForX(float x) const
{
    if (tileComponents.empty())
        return 0;

    int index = 0;
    for (const auto& tile : tileComponents)
    {
        if (tile->getSectionId() == draggedSectionId)
            continue;
        if (x < tile->getBounds().getCentreX())
            return index;
        ++index;
    }
    return index;
}

void ArrangeWorkComponent::commitCurrentDragOrder()
{
    if (draggedSectionId.isEmpty())
        return;

    juce::StringArray orderedIds;
    for (const auto& tile : tiles)
        orderedIds.add(tile.section.getId());

    orderedIds.removeString(draggedSectionId);
    orderedIds.insert(juce::jlimit(0, orderedIds.size(), dragInsertIndex), draggedSectionId);

    if (onSectionOrderChanged)
        onSectionOrderChanged(orderedIds);
}

} // namespace setle::ui
