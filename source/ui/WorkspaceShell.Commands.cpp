#include "WorkspaceShellComponent.h"

namespace setle::ui
{
namespace
{
// Command handling extracted from WorkspaceShellComponent
juce::ApplicationCommandTarget* WorkspaceShellComponent::getNextCommandTarget()
{
    return nullptr;
}

void WorkspaceShellComponent::getAllCommands(juce::Array<juce::CommandID>& commands)
{
    const auto& manager = commandManager;
    if (manager != nullptr)
    {
        manager->getAllCommands(commands);
    }
}

void WorkspaceShellComponent::getCommandInfo(juce::CommandID commandID,
                                             juce::ApplicationCommandInfo& result)
{
    const auto& manager = commandManager;
    if (manager != nullptr)
    {
        manager->getCommandInfo(commandID, result);
    }
}

bool WorkspaceShellComponent::perform(const juce::ApplicationCommandTarget::InvocationInfo& info)
{
    const auto& manager = commandManager;
    if (manager != nullptr)
    {
        return manager->invoke(info, true);
    }
    return false;
}

// Theory action handling extracted from WorkspaceShellComponent
juce::String WorkspaceShellComponent::runTheoryAction(TheoryMenuTarget target, int actionId, const juce::String& actionName)
{
    switch (target)
    {
        case TheoryMenuTarget::section: return runSectionAction(actionId);
        case TheoryMenuTarget::chord: return runChordAction(actionId);
        case TheoryMenuTarget::note: return runNoteAction(actionId);
        case TheoryMenuTarget::progression: return runProgressionAction(actionId, selectedProgressionId);
        case TheoryMenuTarget::historyBuffer: return runHistoryBufferAction(actionId);
    }
    return {};
}

juce::String WorkspaceShellComponent::runSectionAction(int actionId)
{
    switch (actionId)
    {
        case TheoryActionIds::sectionEditTheory: return "Edit section theory";
        case TheoryActionIds::sectionSetRepeatPattern: return "Set repeat pattern";
        case TheoryActionIds::sectionAddTransitionAnchor: return "Add transition anchor";
        case TheoryActionIds::sectionConflictCheck: return "Check conflicts";
        case TheoryActionIds::sectionRename: return "Rename section";
        case TheoryActionIds::sectionDelete: return "Delete section";
        case TheoryActionIds::sectionDuplicate: return "Duplicate section";
        case TheoryActionIds::sectionSetColor: return "Set section color";
        case TheoryActionIds::sectionSetTimeSig: return "Set time signature";
        case TheoryActionIds::sectionExportMidi: return "Export MIDI";
        case TheoryActionIds::sectionJumpTo: return "Jump to section";
        default: return {};
    }
}

juce::String WorkspaceShellComponent::runChordAction(int actionId)
{
    switch (actionId)
    {
        case TheoryActionIds::chordEdit: return "Edit chord";
        case TheoryActionIds::chordSubstitution: return "Chord substitution";
        case TheoryActionIds::chordSetFunction: return "Set chord function";
        case TheoryActionIds::chordScopeOccurrence: return "Scope occurrence";
        case TheoryActionIds::chordScopeRepeat: return "Scope repeat";
        case TheoryActionIds::chordScopeSelectedRepeats: return "Scope selected repeats";
        case TheoryActionIds::chordScopeAllRepeats: return "Scope all repeats";
        case TheoryActionIds::chordSnapWholeBar: return "Snap to whole bar";
        case TheoryActionIds::chordSnapHalfBar: return "Snap to half bar";
        case TheoryActionIds::chordSnapBeat: return "Snap to beat";
        case TheoryActionIds::chordSnapHalfBeat: return "Snap to half beat";
        case TheoryActionIds::chordSnapEighth: return "Snap to eighth";
        case TheoryActionIds::chordSnapSixteenth: return "Snap to sixteenth";
        case TheoryActionIds::chordSnapThirtySecond: return "Snap to thirty-second";
        case TheoryActionIds::chordDelete: return "Delete chord";
        case TheoryActionIds::chordDuplicate: return "Duplicate chord";
        case TheoryActionIds::chordInsertBefore: return "Insert chord before";
        case TheoryActionIds::chordInsertAfter: return "Insert chord after";
        case TheoryActionIds::chordNudgeLeft: return "Nudge chord left";
        case TheoryActionIds::chordNudgeRight: return "Nudge chord right";
        case TheoryActionIds::chordSetDuration: return "Set chord duration";
        case TheoryActionIds::chordInvertVoicing: return "Invert voicing";
        case TheoryActionIds::chordSendToGridRoll: return "Send to GridRoll";
        default: return {};
    }
}

juce::String WorkspaceShellComponent::runNoteAction(int actionId)
{
    switch (actionId)
    {
        case TheoryActionIds::noteEditTheory: return "Edit note theory";
        case TheoryActionIds::noteQuantizeScale: return "Quantize to scale";
        case TheoryActionIds::noteConvertToChord: return "Convert to chord";
        case TheoryActionIds::noteDeriveProgression: return "Derive progression";
        default: return {};
    }
}

juce::String WorkspaceShellComponent::runProgressionAction(int actionId, const juce::String& targetProgressionId)
{
    switch (actionId)
    {
        case TheoryActionIds::progressionGrabSampler: return "Grab from sampler";
        case TheoryActionIds::progressionCreateCandidate: return "Create candidate";
        case TheoryActionIds::progressionAnnotateKeyMode: return "Annotate key/mode";
        case TheoryActionIds::progressionTagTransition: return "Tag transition";
        case TheoryActionIds::progressionEditIdentity: return "Edit identity";
        case TheoryActionIds::progressionForkVariant: return "Fork variant";
        case TheoryActionIds::progressionAddChord: return "Add chord";
        case TheoryActionIds::progressionClearChords: return "Clear chords";
        case TheoryActionIds::progressionAssignToSection: return "Assign to section";
        case TheoryActionIds::progressionSubAllDominants: return "Substitute all dominants";
        case TheoryActionIds::progressionTransposeToKey: return "Transpose to key";
        case TheoryActionIds::progressionSaveAsTemplate: return "Save as template";
        case TheoryActionIds::progressionBrowseLibrary: return "Browse library";
        default: return {};
    }
}

juce::String WorkspaceShellComponent::runHistoryBufferAction(int actionId)
{
    switch (actionId)
    {
        case TheoryActionIds::historyGrab4: return "Grab 4 beats";
        case TheoryActionIds::historyGrab8: return "Grab 8 beats";
        case TheoryActionIds::historyGrab16: return "Grab 16 beats";
        case TheoryActionIds::historyGrabCustom: return "Grab custom length";
        case TheoryActionIds::historySetLength: return "Set buffer length";
        case TheoryActionIds::historyChangeSource: return "Change source";
        case TheoryActionIds::historyClear: return "Clear buffer";
        case TheoryActionIds::historyAutoGrabToggle: return "Toggle auto-grab";
        case TheoryActionIds::historySetBpm: return "Set BPM";
        case TheoryActionIds::historyCaptureSingle: return "Capture single";
        default: return {};
    }
}
} // namespace
} // namespace setle::ui