#include "EffBuilderComponent.h"

namespace setle::eff
{

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

EffBuilderComponent::EffBuilderComponent()
{
    // Header
    addAndMakeVisible(nameEditor);
    nameEditor.setFont(juce::FontOptions(14.0f));
    nameEditor.setText("Untitled Effect");
    nameEditor.onTextChange = [this]
    {
        definition.name = nameEditor.getText();
        scheduleAutoSave();
    };

    addAndMakeVisible(saveButton);
    saveButton.onClick = [this] { saveToFile(); };

    addAndMakeVisible(templateBox);
    templateBox.setTextWhenNothingSelected("Template \xe2\x96\xbc");
    templateBox.onChange = [this]
    {
        const int idx = templateBox.getSelectedItemIndex();
        if (idx < 0) return;
        const auto templates = EffTemplates::getBuiltInTemplates();
        if (idx < static_cast<int>(templates.size()))
        {
            auto tmpl = templates[static_cast<size_t>(idx)];
            // Preserve name and effId from current definition
            tmpl.effId = definition.effId;
            tmpl.name  = definition.name;
            definition  = std::move(tmpl);
            if (effProcessor)
                effProcessor->loadDefinition(definition);
            activeBlockIndex = 0;
            rebuildParamControls();
            rebuildChainList();
            updateInOutStrips();
            updateMacroStrip();
            resized();
            scheduleAutoSave();
            if (onDefinitionChanged)
                onDefinitionChanged(definition);
        }
        templateBox.setSelectedId(-1, juce::dontSendNotification);
    };

    // Chain list
    addAndMakeVisible(chainListArea);
    chainListArea.setInterceptsMouseClicks(true, false);

    addAndMakeVisible(addBlockButton);
    addBlockButton.onClick = [this]
    {
        // Show block picker overlay over the whole component
        auto* picker = new EffBlockPickerOverlay();
        picker->onBlockChosen = [this](BlockType t) { insertBlock(t); };
        picker->setBounds(getLocalBounds());
        addAndMakeVisible(picker);
        picker->toFront(true);
    };

    // IN / OUT labels (clickable strips)
    addAndMakeVisible(inStripLabel);
    inStripLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a2535));
    inStripLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    inStripLabel.setFont(juce::FontOptions(12.0f));
    inStripLabel.addMouseListener(this, false);

    addAndMakeVisible(outStripLabel);
    outStripLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a2535));
    outStripLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    outStripLabel.setFont(juce::FontOptions(12.0f));
    outStripLabel.addMouseListener(this, false);

    // Bypass / Remove
    addAndMakeVisible(bypassButton);
    bypassButton.setColour(juce::Label::backgroundColourId, juce::Colour(0xff2a3545));
    bypassButton.setColour(juce::Label::textColourId, juce::Colours::white);
    bypassButton.setFont(juce::FontOptions(12.0f));
    bypassButton.setJustificationType(juce::Justification::centred);
    bypassButton.addMouseListener(this, false);

    addAndMakeVisible(removeButton);
    removeButton.onClick = [this] { removeActiveBlock(); };

    // Macro knobs
    for (int i = 0; i < 4; ++i)
    {
        macroKnobs[static_cast<size_t>(i)].setSliderStyle(juce::Slider::RotaryVerticalDrag);
        macroKnobs[static_cast<size_t>(i)].setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        macroKnobs[static_cast<size_t>(i)].setRange(0.0, 1.0);
        macroKnobs[static_cast<size_t>(i)].setValue(0.0, juce::dontSendNotification);
        macroKnobs[static_cast<size_t>(i)].onValueChange = [this, i]
        {
            if (effProcessor)
                effProcessor->setMacro(i, static_cast<float>(macroKnobs[static_cast<size_t>(i)].getValue()));
        };
        macroKnobs[static_cast<size_t>(i)].addMouseListener(this, false);
        addAndMakeVisible(macroKnobs[static_cast<size_t>(i)]);

        macroLabels[static_cast<size_t>(i)].setFont(juce::FontOptions(10.0f));
        macroLabels[static_cast<size_t>(i)].setJustificationType(juce::Justification::centred);
        macroLabels[static_cast<size_t>(i)].setColour(juce::Label::textColourId,
                                                       juce::Colours::white.withAlpha(0.65f));
        addAndMakeVisible(macroLabels[static_cast<size_t>(i)]);
    }

    addAndMakeVisible(controlArea);

    // populate template list
    rebuildTemplateBox();

    startTimer(500);
}

EffBuilderComponent::~EffBuilderComponent()
{
    stopTimer();
}

// ─────────────────────────────────────────────────────────────────────────────
// loadDefinition / unload
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::loadDefinition(EffProcessor* proc,
                                         const EffDefinition& def,
                                         const juce::File& file)
{
    effProcessor     = proc;
    definition       = def;
    saveFile         = file;
    activeBlockIndex = 0;
    pendingSave      = false;

    nameEditor.setText(definition.name, juce::dontSendNotification);

    rebuildParamControls();
    rebuildChainList();
    updateInOutStrips();
    updateMacroStrip();
    resized();
}

void EffBuilderComponent::unload()
{
    effProcessor = nullptr;
    definition   = {};
    saveFile     = {};
    paramControls.clear();
    nameEditor.setText("", juce::dontSendNotification);
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// paint
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141a25));

    // Chain list background
    const auto chainArea = getChainListBounds();
    g.setColour(juce::Colour(0xff0f1520));
    g.fillRect(chainArea);

    // Draw block rects in chain list
    const int blockH  = 38;
    const int blockW  = chainArea.getWidth() - 6;
    int y = chainArea.getY() + 4;

    g.setFont(juce::FontOptions(9.0f));
    for (int i = 0; i < static_cast<int>(definition.blocks.size()); ++i)
    {
        const auto& block = definition.blocks[static_cast<size_t>(i)];
        const auto br = juce::Rectangle<int>(chainArea.getX() + 3, y, blockW, blockH - 2);

        const bool isActive = (i == activeBlockIndex);
        g.setColour(isActive ? juce::Colour(0xff2c5080) : juce::Colour(0xff1e2a3a));
        g.fillRoundedRectangle(br.toFloat(), 3.0f);

        if (isActive)
        {
            g.setColour(juce::Colour(0xff5090d0));
            g.drawRoundedRectangle(br.toFloat().reduced(0.5f), 3.0f, 1.0f);
        }

        const auto textCol = block.bypassed ? juce::Colours::grey
                                            : juce::Colours::white.withAlpha(0.85f);
        g.setColour(textCol);
        // Abbreviate label to fit narrow strip
        const juce::String abbrev = block.label.substring(0, 4);
        g.drawText(abbrev, br.reduced(2), juce::Justification::centred, true);

        y += blockH;
    }

    // Main area border
    const auto mainArea = getMainAreaBounds();
    g.setColour(juce::Colour(0xff2a3545));
    g.drawRect(mainArea, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// resized
// ─────────────────────────────────────────────────────────────────────────────

juce::Rectangle<int> EffBuilderComponent::getHeaderBounds() const
{
    return getLocalBounds().removeFromTop(kHeaderHeight);
}

juce::Rectangle<int> EffBuilderComponent::getChainListBounds() const
{
    return getLocalBounds()
               .withTrimmedTop(kHeaderHeight)
               .removeFromLeft(kChainListWidth);
}

juce::Rectangle<int> EffBuilderComponent::getMainAreaBounds() const
{
    return getLocalBounds()
               .withTrimmedTop(kHeaderHeight)
               .withTrimmedLeft(kChainListWidth);
}

void EffBuilderComponent::resized()
{
    // ── Header ───────────────────────────────────────────────────────────────
    auto header = getHeaderBounds();
    nameEditor.setBounds(header.removeFromLeft(header.getWidth() - 180).reduced(2, 3));
    saveButton.setBounds(header.removeFromLeft(60).reduced(2, 3));
    templateBox.setBounds(header.reduced(2, 3));

    // ── Chain list ────────────────────────────────────────────────────────────
    chainListArea.setBounds(getChainListBounds());
    {
        const int btnH = 24;
        auto chainB    = getChainListBounds();
        addBlockButton.setBounds(chainB.removeFromBottom(btnH).reduced(3, 2));
    }

    // ── Main area ─────────────────────────────────────────────────────────────
    auto main = getMainAreaBounds().reduced(4);

    inStripLabel.setBounds(main.removeFromTop(kStripHeight));
    main.removeFromTop(4);

    // Bypass / Remove row
    {
        auto row = main.removeFromBottom(28);
        removeButton.setBounds(row.removeFromRight(80).reduced(2, 2));
        bypassButton.setBounds(row.removeFromLeft(70).reduced(2, 2));
    }
    main.removeFromBottom(4);

    // Macro strip
    {
        auto macroStrip = main.removeFromBottom(kMacroStripH);
        const int slotW = macroStrip.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto slot = macroStrip.removeFromLeft(slotW);
            macroLabels[static_cast<size_t>(i)].setBounds(slot.removeFromBottom(16));
            macroKnobs[static_cast<size_t>(i)].setBounds(slot.reduced(4));
        }
    }
    main.removeFromBottom(4);

    outStripLabel.setBounds(main.removeFromBottom(kStripHeight));
    main.removeFromBottom(4);

    controlArea.setBounds(main);

    // Layout generated param controls inside controlArea
    auto ctrlBounds = controlArea.getLocalBounds().reduced(4);
    const int controlW = 72;
    const int controlH = 64;
    int cx = 0, cy = 0;

    for (auto& pc : paramControls)
    {
        if (pc.widget == nullptr) continue;

        const auto box = juce::Rectangle<int>(cx, cy, controlW, controlH);
        pc.widget->setBounds(box.withTrimmedBottom(16));
        if (pc.label != nullptr)
            pc.label->setBounds(box.removeFromBottom(16));

        cx += controlW + 4;
        if (cx + controlW > ctrlBounds.getWidth())
        {
            cx = 0;
            cy += controlH + 4;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildParamControls — generate widgets from active block's EffParam list
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::rebuildParamControls()
{
    // Remove old widgets
    for (auto& pc : paramControls)
    {
        if (pc.widget) controlArea.removeChildComponent(pc.widget.get());
        if (pc.label)  controlArea.removeChildComponent(pc.label.get());
    }
    paramControls.clear();

    if (definition.blocks.empty() ||
        activeBlockIndex >= static_cast<int>(definition.blocks.size()))
        return;

    const auto& block = definition.blocks[static_cast<size_t>(activeBlockIndex)];

    for (const auto& param : block.params)
    {
        ParamControl pc;
        pc.blockId  = block.blockId;
        pc.paramId  = param.paramId;

        // Label
        pc.label = std::make_unique<juce::Label>();
        pc.label->setFont(juce::FontOptions(10.0f));
        pc.label->setJustificationType(juce::Justification::centred);
        pc.label->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.75f));
        pc.label->setText(param.label, juce::dontSendNotification);
        controlArea.addAndMakeVisible(*pc.label);

        switch (param.hint)
        {
            case EffParam::ControlHint::Knob:
            case EffParam::ControlHint::Slider:
            {
                const bool isKnob = (param.hint == EffParam::ControlHint::Knob);
                auto* slider = new juce::Slider();
                slider->setSliderStyle(isKnob ? juce::Slider::RotaryVerticalDrag
                                              : juce::Slider::LinearHorizontal);
                slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                slider->setRange(static_cast<double>(param.minVal),
                                 static_cast<double>(param.maxVal));
                slider->setValue(static_cast<double>(param.value), juce::dontSendNotification);
                slider->setTooltip(param.label + " [" + param.unit + "]");

                const juce::String capturedBlockId  = block.blockId;
                const juce::String capturedParamId  = param.paramId;

                slider->onValueChange = [this, slider, capturedBlockId, capturedParamId]
                {
                    const float v = static_cast<float>(slider->getValue());
                    if (effProcessor)
                        effProcessor->setParam(capturedBlockId, capturedParamId, v);
                    // Update local definition mirror
                    for (auto& blk : definition.blocks)
                        if (blk.blockId == capturedBlockId)
                            for (auto& p : blk.params)
                                if (p.paramId == capturedParamId)
                                    p.value = v;
                    scheduleAutoSave();
                    if (onDefinitionChanged) onDefinitionChanged(definition);
                };

                pc.widget.reset(slider);
                break;
            }

            case EffParam::ControlHint::Toggle:
            {
                auto* btn = new juce::ToggleButton();
                btn->setToggleState(param.value > 0.5f, juce::dontSendNotification);
                const juce::String capturedBlockId = block.blockId;
                const juce::String capturedParamId = param.paramId;

                btn->onStateChange = [this, btn, capturedBlockId, capturedParamId]
                {
                    const float v = btn->getToggleState() ? 1.0f : 0.0f;
                    if (effProcessor)
                        effProcessor->setParam(capturedBlockId, capturedParamId, v);
                    scheduleAutoSave();
                    if (onDefinitionChanged) onDefinitionChanged(definition);
                };

                pc.widget.reset(btn);
                break;
            }

            case EffParam::ControlHint::Selector:
            {
                auto* box = new juce::ComboBox();
                // Populate with integer options between min and max
                const int mn = static_cast<int>(param.minVal);
                const int mx = static_cast<int>(param.maxVal);
                for (int opt = mn; opt <= mx; ++opt)
                    box->addItem(juce::String(opt), opt - mn + 1);
                box->setSelectedId(static_cast<int>(param.value) - mn + 1,
                                   juce::dontSendNotification);

                const juce::String capturedBlockId = block.blockId;
                const juce::String capturedParamId = param.paramId;

                box->onChange = [this, box, mn, capturedBlockId, capturedParamId]
                {
                    const float v = static_cast<float>(box->getSelectedItemIndex() + mn);
                    if (effProcessor)
                        effProcessor->setParam(capturedBlockId, capturedParamId, v);
                    scheduleAutoSave();
                    if (onDefinitionChanged) onDefinitionChanged(definition);
                };

                pc.widget.reset(box);
                break;
            }
        }

        controlArea.addAndMakeVisible(*pc.widget);
        paramControls.push_back(std::move(pc));
    }

    resized();
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildTemplateBox
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::rebuildTemplateBox()
{
    templateBox.clear(juce::dontSendNotification);
    const auto templates = EffTemplates::getBuiltInTemplates();
    for (int i = 0; i < static_cast<int>(templates.size()); ++i)
        templateBox.addItem(templates[static_cast<size_t>(i)].name, i + 1);
    templateBox.setTextWhenNothingSelected("Template \xe2\x96\xbc");
}

// ─────────────────────────────────────────────────────────────────────────────
// rebuildChainList — just triggers a repaint (paint() draws the chain list)
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::rebuildChainList()
{
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// updateInOutStrips
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::updateInOutStrips()
{
    const int n = static_cast<int>(definition.blocks.size());

    if (n == 0)
    {
        inStripLabel.setText("IN  (empty chain)", juce::dontSendNotification);
        outStripLabel.setText("OUT (empty chain)", juce::dontSendNotification);
        return;
    }

    const juce::String prevName = (activeBlockIndex > 0)
        ? ("\xe2\x86\x90 " + definition.blocks[static_cast<size_t>(activeBlockIndex - 1)].label)
        : "IN  (start of chain)";

    const juce::String nextName = (activeBlockIndex < n - 1)
        ? ("\xe2\x86\x92 " + definition.blocks[static_cast<size_t>(activeBlockIndex + 1)].label)
        : "OUT (end of chain)";

    inStripLabel.setText(prevName,  juce::dontSendNotification);
    outStripLabel.setText(nextName, juce::dontSendNotification);

    const bool isBypassed = !definition.blocks.empty() &&
        definition.blocks[static_cast<size_t>(activeBlockIndex)].bypassed;
    bypassButton.setText(isBypassed ? "[Bypassed]" : "[Active]", juce::dontSendNotification);
    bypassButton.setColour(juce::Label::backgroundColourId,
                           isBypassed ? juce::Colour(0xff4a2020) : juce::Colour(0xff1a3a1a));
}

// ─────────────────────────────────────────────────────────────────────────────
// updateMacroStrip
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::updateMacroStrip()
{
    for (int i = 0; i < 4; ++i)
    {
        juce::String label = "M" + juce::String(i + 1) + " [\xe2\x80\x94]";
        for (const auto& m : definition.macros)
        {
            if (m.macroIndex == i)
            {
                label = "M" + juce::String(i + 1) + " [" + m.macroLabel + "]";
                break;
            }
        }
        macroLabels[static_cast<size_t>(i)].setText(label, juce::dontSendNotification);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// navigateTo
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::navigateTo(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(definition.blocks.size()))
        return;
    activeBlockIndex = idx;
    rebuildParamControls();
    updateInOutStrips();
    repaint();
}

// ─────────────────────────────────────────────────────────────────────────────
// insertBlock — called by EffBlockPickerOverlay
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::insertBlock(BlockType type)
{
    auto newBlock = EffProcessor::createDefaultBlock(type);

    // Enforce ordering: MIDI blocks before audio blocks
    const bool isMidi = (static_cast<int>(type) < static_cast<int>(BlockType::AudioGain));

    int insertPos = static_cast<int>(definition.blocks.size()); // default: end

    if (isMidi)
    {
        // Insert after last MIDI block (or at start)
        insertPos = 0;
        for (int i = 0; i < static_cast<int>(definition.blocks.size()); ++i)
        {
            if (static_cast<int>(definition.blocks[static_cast<size_t>(i)].type)
                    < static_cast<int>(BlockType::AudioGain))
                insertPos = i + 1;
            else
                break;
        }
    }
    else
    {
        // Insert after last MIDI block (i.e., first audio position or end)
        insertPos = 0;
        for (int i = 0; i < static_cast<int>(definition.blocks.size()); ++i)
        {
            if (static_cast<int>(definition.blocks[static_cast<size_t>(i)].type)
                    < static_cast<int>(BlockType::AudioGain))
                insertPos = i + 1;
        }
        // insertPos now points just after last MIDI block, append at active or end
        insertPos = std::max(insertPos, static_cast<int>(definition.blocks.size()));
    }

    definition.blocks.insert(definition.blocks.begin() + insertPos, std::move(newBlock));
    activeBlockIndex = insertPos;

    if (effProcessor)
        effProcessor->loadDefinition(definition);

    rebuildParamControls();
    rebuildChainList();
    updateInOutStrips();
    updateMacroStrip();
    resized();
    scheduleAutoSave();

    if (onDefinitionChanged)
        onDefinitionChanged(definition);
}

// ─────────────────────────────────────────────────────────────────────────────
// removeActiveBlock
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::removeActiveBlock()
{
    if (definition.blocks.empty()) return;
    if (activeBlockIndex >= static_cast<int>(definition.blocks.size())) return;

    definition.blocks.erase(definition.blocks.begin() + activeBlockIndex);
    activeBlockIndex = std::max(0, activeBlockIndex - 1);

    if (effProcessor)
        effProcessor->loadDefinition(definition);

    rebuildParamControls();
    rebuildChainList();
    updateInOutStrips();
    updateMacroStrip();
    resized();
    scheduleAutoSave();

    if (onDefinitionChanged)
        onDefinitionChanged(definition);
}

// ─────────────────────────────────────────────────────────────────────────────
// toggleBypass
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::toggleBypass()
{
    if (definition.blocks.empty()) return;
    if (activeBlockIndex >= static_cast<int>(definition.blocks.size())) return;

    auto& block = definition.blocks[static_cast<size_t>(activeBlockIndex)];
    block.bypassed = !block.bypassed;

    if (effProcessor)
        effProcessor->loadDefinition(definition);

    updateInOutStrips();
    repaint();
    scheduleAutoSave();
}

// ─────────────────────────────────────────────────────────────────────────────
// Mouse — chain list block selection + IN/OUT/bypass clicks
// ─────────────────────────────────────────────────────────────────────────────

// We use paint() to draw the chain list, so override mouseDown on the component itself
// to handle clicks in the chain list area.
// The parent component's mouseDown is inherited from juce::Component; we don't override it
// because EffBuilderComponent doesn't declare mouseDown.  Instead we intercept via
// the chainListArea child. Let's handle all in a mouseDown override here.

// Actually, let's just use a lambda-based MouseListener. Add a mouse-down to the
// chainListArea transparent overlay in the constructor.

// ─────────────────────────────────────────────────────────────────────────────
// Auto-save / timer
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::scheduleAutoSave()
{
    pendingSave = true;
}

void EffBuilderComponent::timerCallback()
{
    if (pendingSave)
    {
        pendingSave = false;
        if (saveFile != juce::File{})
            saveToFile();
    }
}

void EffBuilderComponent::saveToFile()
{
    if (!saveFile.existsAsFile() && saveFile.getParentDirectory().createDirectory().failed())
        return;
    EffSerializer::saveToFile(definition, saveFile);
}

// ─────────────────────────────────────────────────────────────────────────────
// handleMacroRightClick — show popup to assign param
// ─────────────────────────────────────────────────────────────────────────────

void EffBuilderComponent::handleMacroRightClick(int macroIndex)
{
    juce::PopupMenu menu;
    menu.addSectionHeader("Assign Macro " + juce::String(macroIndex + 1));

    int itemId = 1;
    struct Item { juce::String blockId; juce::String paramId; juce::String label; };
    std::vector<Item> items;

    for (const auto& block : definition.blocks)
        for (const auto& param : block.params)
        {
            items.push_back({ block.blockId, param.paramId,
                              block.label + " / " + param.label });
            menu.addItem(itemId++, block.label + " / " + param.label);
        }

    menu.addSeparator();
    menu.addItem(itemId, "Clear");

    menu.showMenuAsync(juce::PopupMenu::Options{},
        [this, macroIndex, items, itemId](int result)
        {
            if (result <= 0) return;

            // Remove existing assignment for this macro
            definition.macros.erase(
                std::remove_if(definition.macros.begin(), definition.macros.end(),
                    [macroIndex](const EffDefinition::MacroAssignment& m)
                    { return m.macroIndex == macroIndex; }),
                definition.macros.end());

            if (result < itemId && result <= static_cast<int>(items.size()))
            {
                const auto& chosen = items[static_cast<size_t>(result - 1)];
                EffDefinition::MacroAssignment ma;
                ma.macroIndex  = macroIndex;
                ma.blockId     = chosen.blockId;
                ma.paramId     = chosen.paramId;
                ma.macroLabel  = chosen.label;
                definition.macros.push_back(ma);
            }

            if (effProcessor)
                effProcessor->loadDefinition(definition);

            updateMacroStrip();
            scheduleAutoSave();
        });
}

} // namespace setle::eff
