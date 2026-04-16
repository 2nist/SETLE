#include "WorkspaceShellComponent.h"

namespace setle::ui
{
namespace
{
// Layout handling extracted from WorkspaceShellComponent
void WorkspaceShellComponent::clampLayoutValues(int totalTopWidth, int totalBodyHeight)
{
    leftPanelWidth = juce::jlimit(LayoutConstants::minSideWidth, LayoutConstants::maxSideWidth, leftPanelWidth);
    rightPanelWidth = juce::jlimit(LayoutConstants::minSideWidth, LayoutConstants::maxSideWidth, rightPanelWidth);
    timelineHeight = juce::jlimit(LayoutConstants::minTimelineHeight, totalBodyHeight - LayoutConstants::minTopWorkHeight, timelineHeight);
}

void WorkspaceShellComponent::loadLayoutState()
{
    // Load layout state from preferences
}

void WorkspaceShellComponent::saveLayoutState()
{
    // Save layout state to preferences
}
} // namespace
} // namespace setle::ui