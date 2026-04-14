#include "AppMenuBarModel.h"

namespace setle::ui
{

AppMenuBarModel::AppMenuBarModel(juce::ApplicationCommandManager& commandManagerRef)
    : manager(commandManagerRef)
{
    setApplicationCommandManagerToWatch(&manager);
}

AppMenuBarModel::~AppMenuBarModel()
{
    setApplicationCommandManagerToWatch(nullptr);
}

juce::StringArray AppMenuBarModel::getMenuBarNames()
{
    return { "File", "Edit", "View", "Transport", "Insert", "Tools", "Help" };
}

juce::PopupMenu AppMenuBarModel::getMenuForIndex(int topLevelMenuIndex,
                                                  const juce::String& /*menuName*/)
{
    switch (topLevelMenuIndex)
    {
        case 0: return buildFileMenu();
        case 1: return buildEditMenu();
        case 2: return buildViewMenu();
        case 3: return buildTransportMenu();
        case 4: return buildInsertMenu();
        case 5: return buildToolsMenu();
        case 6: return buildHelpMenu();
        default: break;
    }
    return {};
}

void AppMenuBarModel::menuItemSelected(int /*menuItemID*/, int /*topLevelMenuIndex*/)
{
    // All items are added via addCommandItem(); the ApplicationCommandManager
    // handles dispatch automatically.  Nothing to do here.
}

// -----------------------------------------------------------------------
juce::PopupMenu AppMenuBarModel::buildFileMenu()
{
    juce::PopupMenu m;
    m.addCommandItem(&manager, AppCommandIDs::fileNew);
    m.addSeparator();
    m.addCommandItem(&manager, AppCommandIDs::fileSave);
    m.addSeparator();
    m.addCommandItem(&manager, AppCommandIDs::fileExportMidi);
    m.addSeparator();
    m.addCommandItem(&manager, AppCommandIDs::fileExit);
    return m;
}

juce::PopupMenu AppMenuBarModel::buildEditMenu()
{
    juce::PopupMenu m;
    m.addCommandItem(&manager, AppCommandIDs::editUndo);
    m.addCommandItem(&manager, AppCommandIDs::editRedo);
    return m;
}

juce::PopupMenu AppMenuBarModel::buildViewMenu()
{
    juce::PopupMenu m;

    // Focus mode sub-group
    m.addCommandItem(&manager, AppCommandIDs::viewFocusIn);
    m.addCommandItem(&manager, AppCommandIDs::viewFocusBalanced);
    m.addCommandItem(&manager, AppCommandIDs::viewFocusOut);
    m.addSeparator();

    // Nav sections sub-group
    m.addSectionHeader("Go to");
    m.addCommandItem(&manager, AppCommandIDs::navCreate);
    m.addCommandItem(&manager, AppCommandIDs::navEdit);
    m.addCommandItem(&manager, AppCommandIDs::navArrange);
    m.addCommandItem(&manager, AppCommandIDs::navSound);
    m.addCommandItem(&manager, AppCommandIDs::navMix);
    m.addCommandItem(&manager, AppCommandIDs::navLibrary);
    m.addCommandItem(&manager, AppCommandIDs::navSession);
    m.addSeparator();

    m.addCommandItem(&manager, AppCommandIDs::viewThemeEditor);
    return m;
}

juce::PopupMenu AppMenuBarModel::buildTransportMenu()
{
    juce::PopupMenu m;
    m.addCommandItem(&manager, AppCommandIDs::transportPlay);
    m.addCommandItem(&manager, AppCommandIDs::transportStop);
    m.addSeparator();
    m.addCommandItem(&manager, AppCommandIDs::transportRecord);
    return m;
}

juce::PopupMenu AppMenuBarModel::buildInsertMenu()
{
    juce::PopupMenu m;
    m.addCommandItem(&manager, AppCommandIDs::insertMidiTrack);
    m.addCommandItem(&manager, AppCommandIDs::insertAudioTrack);
    return m;
}

juce::PopupMenu AppMenuBarModel::buildToolsMenu()
{
    juce::PopupMenu m;
    m.addCommandItem(&manager, AppCommandIDs::toolSelect);
    m.addCommandItem(&manager, AppCommandIDs::toolDraw);
    m.addCommandItem(&manager, AppCommandIDs::toolErase);
    m.addCommandItem(&manager, AppCommandIDs::toolSplit);
    m.addCommandItem(&manager, AppCommandIDs::toolStretch);
    m.addCommandItem(&manager, AppCommandIDs::toolListen);
    m.addCommandItem(&manager, AppCommandIDs::toolMarquee);
    m.addCommandItem(&manager, AppCommandIDs::toolScaleSnap);
    return m;
}

juce::PopupMenu AppMenuBarModel::buildHelpMenu()
{
    juce::PopupMenu m;
    m.addCommandItem(&manager, AppCommandIDs::helpAbout);
    return m;
}

} // namespace setle::ui
