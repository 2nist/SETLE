#include "WorkspaceShellComponent.h"

namespace setle::ui
{
namespace
{
// Navigation methods extracted from WorkspaceShellComponent
void WorkspaceShellComponent::switchWorkTab(int tabIndex)
{
    workPanelTabIndex = tabIndex;
    workPanelShowGridRoll = (tabIndex == WorkTabIds::gridRoll);
    resized();
}

void WorkspaceShellComponent::switchWorkTab(bool showGridRoll)
{
    switchWorkTab(showGridRoll ? WorkTabIds::gridRoll : WorkTabIds::theory);
}

void WorkspaceShellComponent::switchNavSection(NavSection section)
{
    activeNavSection = section;
    resized();
}

int WorkspaceShellComponent::getPrimaryWorkTabForSection(NavSection section) const
{
    switch (section)
    {
        case NavSection::edit: return WorkTabIds::theory;
        case NavSection::create: return WorkTabIds::create;
        case NavSection::arrange: return WorkTabIds::arrange;
        case NavSection::sound: return WorkTabIds::sound;
        case NavSection::mix: return WorkTabIds::mix;
        case NavSection::library: return WorkTabIds::library;
        case NavSection::sessionDashboard: return WorkTabIds::sessionDashboard;
        default: return WorkTabIds::theory;
    }
}

bool WorkspaceShellComponent::isWorkTabOwnedBySection(NavSection section, int tabIndex) const
{
    switch (section)
    {
        case NavSection::edit: return tabIndex == WorkTabIds::theory;
        case NavSection::create: return tabIndex == WorkTabIds::create;
        case NavSection::arrange: return tabIndex == WorkTabIds::arrange;
        case NavSection::sound: return tabIndex == WorkTabIds::sound;
        case NavSection::mix: return tabIndex == WorkTabIds::mix;
        case NavSection::library: return tabIndex == WorkTabIds::library;
        case NavSection::sessionDashboard: return tabIndex == WorkTabIds::sessionDashboard;
        default: return false;
    }
}
} // namespace
} // namespace setle::ui