#include "ProgressionLibraryBrowser.h"

#include <juce_core/juce_core.h>

#include "../theme/ThemeManager.h"
#include "../theme/ThemeStyleHelpers.h"

namespace setle::ui
{

ProgressionLibraryBrowser::ProgressionLibraryBrowser(const juce::String& sessionKey, const juce::String& sessionMode)
    : currentSessionKey(sessionKey), currentSessionMode(sessionMode)
{
    filterLabel.setText("Mode Filter:", juce::dontSendNotification);
    filterLabel.setFont(juce::FontOptions(12.0f));
    filterLabel.setColour(juce::Label::textColourId,
                          setle::theme::textForRole(ThemeManager::get().theme(),
                                                     setle::theme::TextRole::muted)
                              .withAlpha(0.94f));
    addAndMakeVisible(filterLabel);

    modeFilter.addItem("All", 1);
    modeFilter.addItem("Major", 2);
    modeFilter.addItem("Minor", 3);
    modeFilter.addItem("Dorian", 4);
    modeFilter.addItem("Mixolydian", 5);
    modeFilter.addItem("Bundled", 6);
    modeFilter.setSelectedId(1, juce::dontSendNotification);
    modeFilter.onChange = [this] { rebuildBrowserRows(); };
    addAndMakeVisible(modeFilter);

    searchLabel.setText("Search:", juce::dontSendNotification);
    searchLabel.setFont(juce::FontOptions(12.0f));
    searchLabel.setColour(juce::Label::textColourId,
                          setle::theme::textForRole(ThemeManager::get().theme(),
                                                     setle::theme::TextRole::muted)
                              .withAlpha(0.94f));
    addAndMakeVisible(searchLabel);

    searchEditor.setFont(juce::FontOptions(12.0f));
    searchEditor.onTextChange = [this] { rebuildBrowserRows(); };
    searchEditor.setColour(juce::TextEditor::backgroundColourId,
                           setle::theme::rowStyle(ThemeManager::get().theme(),
                                                  setle::theme::SurfaceState::normal)
                               .fill);
    searchEditor.setColour(juce::TextEditor::outlineColourId,
                           setle::theme::rowStyle(ThemeManager::get().theme(),
                                                  setle::theme::SurfaceState::normal)
                               .outline);
    searchEditor.setColour(juce::TextEditor::textColourId,
                           setle::theme::textForRole(ThemeManager::get().theme(),
                                                     setle::theme::TextRole::primary));
    addAndMakeVisible(searchEditor);

    addAndMakeVisible(rowsViewport);
    rowsViewport.setViewedComponent(&rowsContent, false);
    rowsViewport.setScrollBarsShown(true, true);
    rebuildBrowserRows();
}

void ProgressionLibraryBrowser::paint(juce::Graphics& g)
{
    const auto& theme = ThemeManager::get().theme();
    g.fillAll(setle::theme::panelBackground(theme, setle::theme::ZoneRole::workPanel));

    g.setColour(setle::theme::timelineGridLine(theme, true).withAlpha(0.8f));
    g.drawRect(getLocalBounds());
}

void ProgressionLibraryBrowser::resized()
{
    auto bounds = getLocalBounds().reduced(8);

    auto filterRow = bounds.removeFromTop(28);
    filterLabel.setBounds(filterRow.removeFromLeft(90));
    modeFilter.setBounds(filterRow.removeFromLeft(120));

    bounds.removeFromTop(8);

    auto searchRow = bounds.removeFromTop(28);
    searchLabel.setBounds(searchRow.removeFromLeft(70));
    searchEditor.setBounds(searchRow);

    bounds.removeFromTop(8);
    rowsViewport.setBounds(bounds);

    int y = 0;
    for (auto& row : browserRows)
    {
        row->setBounds(0, y, juce::jmax(120, rowsViewport.getMaximumVisibleWidth() - 8), 40);
        y += 42;
    }
    rowsContent.setSize(juce::jmax(120, rowsViewport.getMaximumVisibleWidth() - 8), juce::jmax(y, rowsViewport.getMaximumVisibleHeight()));
}

void ProgressionLibraryBrowser::setOnRowClicked(std::function<void(const juce::String&)> callback)
{
    onRowClicked = callback;
}

void ProgressionLibraryBrowser::setOnRowContextRequested(std::function<void(const juce::String&, juce::Component&)> callback)
{
    onRowContextRequested = std::move(callback);
}

void ProgressionLibraryBrowser::updateSessionKey(const juce::String& newKey)
{
    if (currentSessionKey != newKey)
    {
        currentSessionKey = newKey;
        rebuildBrowserRows();
    }
}

void ProgressionLibraryBrowser::updateSessionMode(const juce::String& newMode)
{
    if (currentSessionMode != newMode)
    {
        currentSessionMode = newMode;
        rebuildBrowserRows();
    }
}

void ProgressionLibraryBrowser::rebuildBrowserRows()
{
    for (auto& row : browserRows)
        rowsContent.removeChildComponent(row.get());
    browserRows.clear();

    auto templates = getProgressionTemplates();
    const auto bundled = getBundledProgressions();
    templates.insert(templates.end(), bundled.begin(), bundled.end());

    auto modeFilterId = modeFilter.getSelectedId();
    auto searchText = searchEditor.getText().toLowerCase();

    for (auto& tmpl : templates)
    {
        if (modeFilterId == 6 && !tmpl.isBundled)
            continue;

        if (modeFilterId != 1 && modeFilterId != 6)
        {
            auto modeNames = juce::StringArray { "", "Major", "Minor", "Dorian", "Mixolydian" };
            if (modeFilterId < modeNames.size() && modeNames[modeFilterId] != tmpl.mode)
                continue;
        }

        if (!searchText.isEmpty() && !tmpl.name.toLowerCase().contains(searchText))
            continue;

        auto row = std::make_unique<BrowserRow>(tmpl);
        row->setOnClicked([this, id = tmpl.templateId]
        {
            if (onRowClicked)
                onRowClicked(id);
        });
        row->setOnContextRequested([this, id = tmpl.templateId](juce::Component& target)
        {
            if (onRowContextRequested)
                onRowContextRequested(id, target);
        });

        rowsContent.addAndMakeVisible(*row);
        browserRows.push_back(std::move(row));
    }

    resized();
}

std::vector<ProgressionLibraryBrowser::ProgressionTemplate> ProgressionLibraryBrowser::getProgressionTemplates() const
{
    return {
        { "tmpl_1", "I-IV-V", "Templates", "I-IV-V", "C", "F", "G", "", "Major", false },
        { "tmpl_2", "I-V-vi-IV", "Templates", "I-V-vi-IV", "C", "G", "A", "F", "Major", false },
        { "tmpl_3", "i-VII-VI-VII", "Templates", "i-VII-VI-VII", "A", "G", "F", "G", "Minor", false },
        { "tmpl_4", "I-ii-iii-IV", "Templates", "I-ii-iii-IV", "C", "D", "E", "F", "Major", false },
        { "tmpl_5", "vi-IV-I-V", "Templates", "vi-IV-I-V", "A", "F", "C", "G", "Major", false },
        { "tmpl_6", "ii-V-I", "Templates", "ii-V-I", "D", "G", "C", "", "Major", false },
    };
}

std::vector<ProgressionLibraryBrowser::ProgressionTemplate> ProgressionLibraryBrowser::getBundledProgressions() const
{
    std::vector<ProgressionTemplate> out;

    const auto appDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
    const auto root = appDir.getChildFile("assets").getChildFile("progressions");
    if (!root.isDirectory())
        return out;

    for (const auto& dataset : juce::RangedDirectoryIterator(root, false, "*", juce::File::findDirectories))
    {
        for (const auto& file : juce::RangedDirectoryIterator(dataset.getFile(), false, "*.setle-prg"))
        {
            const auto parsed = juce::JSON::parse(file.getFile().loadFileAsString());
            auto* obj = parsed.getDynamicObject();
            if (obj == nullptr)
                continue;

            ProgressionTemplate tmpl;
            tmpl.templateId = file.getFile().getFileNameWithoutExtension();
            tmpl.name = obj->getProperty("name").toString();
            tmpl.category = obj->getProperty("category").toString();
            tmpl.degreeSummary = tmpl.category;
            tmpl.mode = "Bundled";
            tmpl.isBundled = true;

            if (auto chords = obj->getProperty("chords"); chords.isArray())
            {
                auto* arr = chords.getArray();
                juce::String joined;
                for (int i = 0; i < juce::jmin(4, arr->size()); ++i)
                {
                    auto* chordObj = arr->getReference(i).getDynamicObject();
                    if (chordObj == nullptr)
                        continue;
                    if (i > 0)
                        joined << " ";
                    joined << chordObj->getProperty("symbol").toString();
                }
                tmpl.chord1 = joined;
            }

            out.push_back(std::move(tmpl));
        }
    }

    return out;
}

juce::String ProgressionLibraryBrowser::transposeChordToKey(const juce::String& chordSymbol, int degreesFromC) const
{
    (void)degreesFromC;
    return chordSymbol;
}

ProgressionLibraryBrowser::BrowserRow::BrowserRow(const ProgressionTemplate& tmpl)
    : template_(tmpl)
{
}

void ProgressionLibraryBrowser::BrowserRow::paint(juce::Graphics& g)
{
    const auto& themeData = ThemeManager::get().theme();
    const auto rowStyle = setle::theme::rowStyle(themeData,
                                                  isHovering ? setle::theme::SurfaceState::hovered
                                                             : setle::theme::SurfaceState::normal);
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(rowStyle.fill);
    g.fillRoundedRectangle(bounds, rowStyle.radius);
    g.setColour(rowStyle.outline);
    g.drawRoundedRectangle(bounds, rowStyle.radius, rowStyle.stroke);

    g.setColour(rowStyle.text.withAlpha(0.95f));
    g.setFont(juce::FontOptions(13.0f));

    auto content = getLocalBounds().reduced(8, 4);
    g.drawText(template_.name, content.removeFromLeft(180), juce::Justification::centredLeft, true);

    g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::muted).withAlpha(0.92f));
    g.drawText(template_.category.isNotEmpty() ? template_.category : template_.degreeSummary,
               content.removeFromLeft(100),
               juce::Justification::centredLeft,
               true);

    if (template_.isBundled)
    {
        auto badge = content.removeFromRight(70).reduced(4, 7);
        const auto chip = setle::theme::chipStyle(themeData,
                                                   setle::theme::SurfaceState::success,
                                                   themeData.zoneC);
        g.setColour(chip.fill);
        g.fillRoundedRectangle(badge.toFloat(), setle::theme::radius(themeData, setle::theme::RadiusRole::chip));
        g.setColour(chip.text);
        g.setFont(juce::FontOptions(9.0f).withStyle("Bold"));
        g.drawText("BUNDLED", badge, juce::Justification::centred, false);
    }

    g.setFont(juce::FontOptions(11.0f));
    juce::String chordText = template_.chord1;
    if (!template_.chord2.isEmpty()) chordText += " " + template_.chord2;
    if (!template_.chord3.isEmpty()) chordText += " " + template_.chord3;
    if (!template_.chord4.isEmpty()) chordText += " " + template_.chord4;
    g.setColour(setle::theme::textForRole(themeData, setle::theme::TextRole::ghost).withAlpha(0.95f));
    g.drawText(chordText, content, juce::Justification::centredLeft, true);
}

void ProgressionLibraryBrowser::BrowserRow::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        if (onContextRequested)
            onContextRequested(*this);
        return;
    }

    if (onClicked)
        onClicked();
}

void ProgressionLibraryBrowser::BrowserRow::mouseEnter(const juce::MouseEvent&)
{
    isHovering = true;
    repaint();
}

void ProgressionLibraryBrowser::BrowserRow::mouseExit(const juce::MouseEvent&)
{
    isHovering = false;
    repaint();
}

} // namespace setle::ui
