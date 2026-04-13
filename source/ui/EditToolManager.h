#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace setle::ui
{

enum class EditTool
{
    Select,
    Draw,
    Erase,
    Split,
    Stretch,
    Listen,
    Marquee,
    ScaleSnap
};

class EditToolManager
{
public:
    static EditToolManager& get();

    EditTool getActiveTool() const noexcept { return activeTool; }
    void     setActiveTool(EditTool tool);

    enum class Surface {
        SectionLane, TheoryStrip, TrackLane, ChordGridView,
        NoteDetailView, DrumGridView, MixerStrip, FXChain
    };

    juce::MouseCursor getCursorFor(Surface surface) const;

    struct Listener
    {
        virtual void activeToolChanged(EditTool newTool) = 0;
        virtual ~Listener() = default;
    };

    void addListener(Listener* l) { listeners.add(l); }
    void removeListener(Listener* l) { listeners.remove(l); }

private:
    EditTool activeTool { EditTool::Select };
    juce::ListenerList<Listener> listeners;
};

} // namespace setle::ui
