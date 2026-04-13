#include "EditToolManager.h"
#include "../theme/ThemeManager.h"

namespace setle::ui
{

EditToolManager& EditToolManager::get()
{
    static EditToolManager instance;
    return instance;
}

void EditToolManager::setActiveTool(EditTool tool)
{
    if (activeTool == tool)
        return;

    activeTool = tool;
    listeners.call([&](Listener& l) { l.activeToolChanged(tool); });
}

juce::MouseCursor EditToolManager::getCursorFor(Surface surface) const
{
    // Default cursor
    using S = Surface;
    using T = EditTool;

    switch (activeTool)
    {
        case T::Select:
            return juce::MouseCursor::NormalCursor;
        case T::Draw:
            // Editable surfaces get crosshair
            switch (surface)
            {
                case S::SectionLane: case S::TheoryStrip: case S::TrackLane:
                case S::ChordGridView: case S::NoteDetailView: case S::DrumGridView:
                    return juce::MouseCursor::CrosshairCursor;
                default:
                    return juce::MouseCursor::NormalCursor;
            }
        case T::Erase:
            // For now reuse crosshair; UI can tint when rendering
            return juce::MouseCursor::CrosshairCursor;
        case T::Split:
            return juce::MouseCursor::LeftRightResizeCursor;
        case T::Stretch:
            return juce::MouseCursor::LeftRightResizeCursor;
        case T::Listen:
            return juce::MouseCursor::NormalCursor;
        case T::Marquee:
            return juce::MouseCursor::CrosshairCursor;
        case T::ScaleSnap:
            return juce::MouseCursor::CrosshairCursor;
    }

    return juce::MouseCursor::NormalCursor;
}

} // namespace setle::ui
