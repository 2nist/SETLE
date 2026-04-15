#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

#include "../model/SetleSongModel.h"
#include "../theme/ThemeManager.h"

namespace setle::ui
{

class ArrangeWorkComponent final : public juce::Component,
                                   private juce::Timer,
                                   private ThemeManager::Listener
{
public:
    enum class DuplicateMode
    {
        exactObject = 0,
        modalExtraction,
        rhythmicDecimation
    };

    enum class CadenceState
    {
        neutral = 0,
        standard,
        clash
    };

    struct TileModel
    {
        setle::model::Section section;
        juce::String progressionSummary;
        double lengthBeats { 16.0 };
        std::vector<juce::Rectangle<float>> notePips;
        std::vector<juce::Rectangle<float>> audioPips;
    };

    ArrangeWorkComponent();
    ~ArrangeWorkComponent() override;

    void setSongStructure(std::vector<TileModel> tiles,
                          juce::String selectedSectionId,
                          juce::String sessionKey,
                          juce::String sessionMode,
                          std::vector<CadenceState> connectorStates);
    void setPlayheadFraction(double fraction);

    std::function<void(const juce::String& sectionId)> onSectionSelected;
    std::function<void(const juce::StringArray& orderedSectionIds)> onSectionOrderChanged;
    std::function<void(const juce::String& sectionId, DuplicateMode mode)> onSmartDuplicateRequested;
    std::function<void(float meltFactor)> onStructuralMeltChanged;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

private:
    class SectionBlockComponent final : public juce::Component
    {
    public:
        void setTileModel(TileModel newModel);
        void setSelected(bool isSelected);
        void setDragging(bool isDragging);

        std::function<void(const juce::String& sectionId)> onSelected;
        std::function<void(const juce::String& sectionId, juce::Point<float> positionInParent)> onDragStarted;
        std::function<void(const juce::String& sectionId, juce::Point<float> positionInParent)> onDragged;
        std::function<void(const juce::String& sectionId, juce::Point<float> positionInParent)> onDragEnded;

        juce::String getSectionId() const { return model.section.getId(); }

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;

    private:
        TileModel model;
        bool selected { false };
        bool dragging { false };
    };

    class TransitionConnector final : public juce::Component
    {
    public:
        void setCadenceState(CadenceState newState);
        void setPulsePhase(float newPhase);

        void paint(juce::Graphics& g) override;

    private:
        CadenceState state { CadenceState::neutral };
        float pulsePhase { 0.0f };
    };

    void timerCallback() override;
    void themeChanged() override;
    void showDuplicateMenu();
    void rebuildTiles();
    void rebuildConnectors();
    void layoutTiles();
    juce::Rectangle<int> getHeaderBounds() const;
    juce::Rectangle<int> getFloorBounds() const;
    int insertionIndexForX(float x) const;
    void commitCurrentDragOrder();

    std::vector<TileModel> tiles;
    std::vector<std::unique_ptr<SectionBlockComponent>> tileComponents;
    std::vector<std::unique_ptr<TransitionConnector>> connectorComponents;
    juce::TextButton smartDuplicateButton { "Smart Duplicate" };
    juce::Label statusLabel;

    juce::String selectedSectionId;
    juce::String sessionKey;
    juce::String sessionMode;
    std::vector<CadenceState> connectorStates;

    juce::String draggedSectionId;
    juce::StringArray draggingOrder;
    float dragCurrentX { 0.0f };
    int dragInsertIndex { -1 };
    double playheadFraction { 0.0 };
    float pulsePhase { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArrangeWorkComponent)
};

} // namespace setle::ui
