#include "ProgressionLibraryBrowser.h"

namespace setle::ui
{

ProgressionLibraryBrowser::ProgressionLibraryBrowser(const juce::String& sessionKey, const juce::String& sessionMode)
    : currentSessionKey(sessionKey), currentSessionMode(sessionMode)
{
    filterLabel.setText("Mode Filter:", juce::dontSendNotification);
    filterLabel.setFont(juce::FontOptions(12.0f));
    filterLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.88f));
    addAndMakeVisible(filterLabel);

    modeFilter.addItem("All", 1);
    modeFilter.addItem("Major", 2);
    modeFilter.addItem("Minor", 3);
    modeFilter.addItem("Dorian", 4);
    modeFilter.addItem("Mixolydian", 5);
    modeFilter.setSelectedId(1, juce::dontSendNotification);
    modeFilter.onChange = [this] { rebuildBrowserRows(); };
    addAndMakeVisible(modeFilter);

    searchLabel.setText("Search:", juce::dontSendNotification);
    searchLabel.setFont(juce::FontOptions(12.0f));
    searchLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.88f));
    addAndMakeVisible(searchLabel);

    searchEditor.setFont(juce::FontOptions(12.0f));
    searchEditor.onTextChange = [this] { rebuildBrowserRows(); };
    addAndMakeVisible(searchEditor);

    addAndMakeVisible(scrollableContainer);

    rebuildBrowserRows();
}

void ProgressionLibraryBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a1a));
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
    scrollableContainer.setBounds(bounds);

    int y = 0;
    for (auto& row : browserRows)
    {
        row->setBounds(0, y, scrollableContainer.getWidth(), 40);
        y += 42;
    }
}

void ProgressionLibraryBrowser::setOnRowClicked(std::function<void(const juce::String&)> callback)
{
    onRowClicked = callback;
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
        scrollableContainer.removeChildComponent(row.get());
    browserRows.clear();

    auto templates = getProgressionTemplates();
    auto modeFilterId = modeFilter.getSelectedId();
    auto searchText = searchEditor.getText().toLowerCase();

    for (auto& tmpl : templates)
    {
        // Filter by mode
        if (modeFilterId != 1)
        {
            auto modeNames = juce::StringArray { "", "Major", "Minor", "Dorian", "Mixolydian" };
            if (modeFilterId < modeNames.size() && modeNames[modeFilterId] != tmpl.mode)
                continue;
        }

        // Filter by search text
        if (!searchText.isEmpty() && !tmpl.name.toLowerCase().contains(searchText))
            continue;

        auto row = std::make_unique<BrowserRow>(tmpl);
        row->setOnClicked([this, id = tmpl.templateId]
        {
            if (onRowClicked)
                onRowClicked(id);
        });
        scrollableContainer.addAndMakeVisible(*row);
        browserRows.push_back(std::move(row));
    }

    resized();
}

std::vector<ProgressionLibraryBrowser::ProgressionTemplate> ProgressionLibraryBrowser::getProgressionTemplates() const
{
    // Return a simplified set of template progressions
    // In a full implementation, these would be loaded from a library file
    return {
        { "tmpl_1", "I-IV-V", "I–IV–V", "C", "F", "G", "", "major" },
        { "tmpl_2", "I-V-vi-IV", "I–V–vi–IV", "C", "G", "A", "F", "major" },
        { "tmpl_3", "i-VII-VI-VII", "i–VII–VI–VII", "A", "G", "F", "G", "minor" },
        { "tmpl_4", "I-ii-iii-IV", "I–ii–iii–IV", "C", "D", "E", "F", "major" },
        { "tmpl_5", "vi-IV-I-V", "vi–IV–I–V", "A", "F", "C", "G", "major" },
        { "tmpl_6", "ii-V-I", "ii–V–I", "D", "G", "C", "", "major" },
    };
}

juce::String ProgressionLibraryBrowser::transposeChordToKey(const juce::String& chordSymbol, int degreesFromC) const
{
    // Simplified transposition - in full implementation, would use DiatonicHarmony
    auto keys = juce::StringArray { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    auto findKey = [&keys](const juce::String& k)
    {
        for (int i = 0; i < keys.size(); ++i)
            if (keys[i] == k) return i;
        return 0;
    };

    int currentKeyIndex = findKey(currentSessionKey);
    // Stub: return the chord symbol as-is for now
    return chordSymbol;
}

// BrowserRow implementation
ProgressionLibraryBrowser::BrowserRow::BrowserRow(const ProgressionTemplate& tmpl)
    : template_(tmpl)
{
}

void ProgressionLibraryBrowser::BrowserRow::paint(juce::Graphics& g)
{
    auto bgCol = isHovering ? juce::Colour(0xff2a2a3a) : juce::Colour(0xff1a1a2a);
    g.fillAll(bgCol);

    g.setColour(juce::Colours::white.withAlpha(0.88f));
    g.setFont(juce::FontOptions(13.0f));
    
    auto bounds = getLocalBounds().reduced(6, 4);
    g.drawText(template_.name, bounds.removeFromLeft(100), juce::Justification::centredLeft, true);
    
    g.setOpacity(0.7f);
    g.drawText(template_.degreeSummary, bounds.removeFromLeft(120), juce::Justification::centredLeft, true);
    
    g.setFont(juce::FontOptions(11.0f));
    juce::String chordText = template_.chord1;
    if (!template_.chord2.isEmpty()) chordText += " " + template_.chord2;
    if (!template_.chord3.isEmpty()) chordText += " " + template_.chord3;
    if (!template_.chord4.isEmpty()) chordText += " " + template_.chord4;
    g.drawText(chordText, bounds, juce::Justification::centredLeft, true);
}

void ProgressionLibraryBrowser::BrowserRow::mouseDown(const juce::MouseEvent&)
{
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
