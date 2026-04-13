#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/SetleSongModel.h"

namespace setle::ui
{

class ProgressionLibraryBrowser final : public juce::Component
{
public:
    explicit ProgressionLibraryBrowser(const juce::String& sessionKey, const juce::String& sessionMode);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setOnRowClicked(std::function<void(const juce::String&)> callback);
    void setOnRowContextRequested(std::function<void(const juce::String&, juce::Component&)> callback);
    void updateSessionKey(const juce::String& newKey);
    void updateSessionMode(const juce::String& newMode);

private:
    struct ProgressionTemplate
    {
        juce::String templateId;
        juce::String name;
        juce::String category;
        juce::String degreeSummary;
        juce::String chord1, chord2, chord3, chord4;
        juce::String mode; // "major", "minor", "dorian", "mixolydian"
        bool isBundled { false };
    };

    class BrowserRow final : public juce::Component
    {
    public:
        explicit BrowserRow(const ProgressionTemplate& tmpl);
        
        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseEnter(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        
        void setOnClicked(std::function<void()> callback) { onClicked = callback; }
        void setOnContextRequested(std::function<void(juce::Component&)> callback) { onContextRequested = callback; }
        const juce::String& getTemplateId() const { return template_.templateId; }
        
    private:
        ProgressionTemplate template_;
        std::function<void()> onClicked;
        std::function<void(juce::Component&)> onContextRequested;
        bool isHovering { false };
    };

    void rebuildBrowserRows();
    std::vector<ProgressionTemplate> getProgressionTemplates() const;
    std::vector<ProgressionTemplate> getBundledProgressions() const;
    juce::String transposeChordToKey(const juce::String& chordSymbol, int degreesFromC) const;
    
    juce::Label filterLabel;
    juce::ComboBox modeFilter;
    juce::Label searchLabel;
    juce::TextEditor searchEditor;
    juce::Viewport rowsViewport;
    juce::Component rowsContent;
    std::vector<std::unique_ptr<BrowserRow>> browserRows;
    std::function<void(const juce::String&)> onRowClicked;
    std::function<void(const juce::String&, juce::Component&)> onRowContextRequested;
    
    juce::String currentSessionKey { "C" };
    juce::String currentSessionMode { "ionian" };
};

} // namespace setle::ui
