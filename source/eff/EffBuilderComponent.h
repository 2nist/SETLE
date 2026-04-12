#pragma once

#include "EffModel.h"
#include "EffProcessor.h"
#include "EffSerializer.h"
#include "EffTemplates.h"
#include "EffBlockPickerOverlay.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include <memory>

namespace setle::eff
{

/**
 * FX Builder — the [FX] tab content in the WORK panel.
 *
 * Layout (see spec):
 *   - Header strip:  [Effect name] [Save] [Template ▼]
 *   - Chain list (left 40px): one rect per block, active highlighted, [+] at bottom
 *   - Main area (right): IN strip → current block controls → OUT strip → MACRO strip
 *
 * Controls are generated from EffParam definitions at runtime — no hardcoded UI per block.
 * Edits are written back to the EffProcessor via setParam() immediately, and the .eff
 * file is auto-saved (debounced 500 ms) when a saveFile is set.
 */
class EffBuilderComponent final : public juce::Component,
                                  private juce::Timer
{
public:
    EffBuilderComponent();
    ~EffBuilderComponent() override;

    // Load / unload the processor and definition being edited
    void loadDefinition(EffProcessor* processor, const EffDefinition& def,
                        const juce::File& effFile = {});
    void unload();

    // Called whenever any param changes (so the owning panel can react)
    std::function<void(const EffDefinition&)> onDefinitionChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // ─── State ───────────────────────────────────────────────────────────────
    EffProcessor*  effProcessor { nullptr };
    EffDefinition  definition;
    juce::File     saveFile;
    int            activeBlockIndex { 0 };  // index into definition.blocks

    // ─── Header controls ──────────────────────────────────────────────────────
    juce::TextEditor nameEditor;
    juce::TextButton saveButton  { "Save" };
    juce::ComboBox   templateBox;

    // ─── Chain list ───────────────────────────────────────────────────────────
    juce::TextButton addBlockButton { "+" };
    juce::Component  chainListArea;   // hit-test area for block rects

    // ─── Main block editor ────────────────────────────────────────────────────
    juce::Label inStripLabel;
    juce::Label outStripLabel;
    juce::Label bypassButton { {}, "Bypass" };   // acts as toggle label
    juce::TextButton removeButton { "Remove" };
    juce::Component controlArea;   // holds generated param controls

    // ─── Macro strip ──────────────────────────────────────────────────────────
    std::array<juce::Slider, 4> macroKnobs;
    std::array<juce::Label,  4> macroLabels;

    // ─── Generated param controls ─────────────────────────────────────────────
    // Rebuilt every time the active block changes
    struct ParamControl
    {
        juce::String blockId;
        juce::String paramId;
        std::unique_ptr<juce::Component> widget;
        std::unique_ptr<juce::Label> label;
    };
    std::vector<ParamControl> paramControls;

    // ─── Auto-save debounce ────────────────────────────────────────────────────
    bool pendingSave { false };
    void timerCallback() override;

    // ─── Internal helpers ─────────────────────────────────────────────────────
    void rebuildParamControls();
    void rebuildTemplateBox();
    void rebuildChainList();
    void updateInOutStrips();
    void updateMacroStrip();
    void insertBlock(BlockType type);
    void removeActiveBlock();
    void toggleBypass();
    void saveToFile();
    void scheduleAutoSave();
    void navigateTo(int blockIndex);
    void handleMacroRightClick(int macroIndex);

    juce::Rectangle<int> getChainListBounds() const;
    juce::Rectangle<int> getMainAreaBounds() const;
    juce::Rectangle<int> getHeaderBounds() const;

    static constexpr int kChainListWidth = 44;
    static constexpr int kHeaderHeight   = 32;
    static constexpr int kStripHeight    = 24;
    static constexpr int kMacroStripH    = 60;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EffBuilderComponent)
};

} // namespace setle::eff
