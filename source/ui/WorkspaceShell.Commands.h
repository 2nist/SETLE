#pragma once

namespace TheoryActionIds {
    // Section actions (100-199)
    static constexpr int sectionEditTheory = 101;
    static constexpr int sectionSetRepeatPattern = 102;
    static constexpr int sectionAddTransitionAnchor = 103;
    static constexpr int sectionConflictCheck = 104;
    static constexpr int sectionRename = 105;
    static constexpr int sectionDelete = 106;
    static constexpr int sectionDuplicate = 107;
    static constexpr int sectionSetColor = 108;
    static constexpr int sectionSetTimeSig = 109;
    static constexpr int sectionExportMidi = 110;
    static constexpr int sectionJumpTo = 111;

    // Chord actions (200-299)
    static constexpr int chordEdit = 201;
    static constexpr int chordSubstitution = 202;
    static constexpr int chordSetFunction = 203;
    static constexpr int chordScopeOccurrence = 204;
    static constexpr int chordScopeRepeat = 205;
    static constexpr int chordScopeSelectedRepeats = 206;
    static constexpr int chordScopeAllRepeats = 207;
    static constexpr int chordSnapWholeBar = 208;
    static constexpr int chordSnapHalfBar = 209;
    static constexpr int chordSnapBeat = 210;
    static constexpr int chordSnapHalfBeat = 211;
    static constexpr int chordSnapEighth = 212;
    static constexpr int chordSnapSixteenth = 213;
    static constexpr int chordSnapThirtySecond = 214;
    static constexpr int chordDelete = 215;
    static constexpr int chordDuplicate = 216;
    static constexpr int chordInsertBefore = 217;
    static constexpr int chordInsertAfter = 218;
    static constexpr int chordNudgeLeft = 219;
    static constexpr int chordNudgeRight = 220;
    static constexpr int chordSetDuration = 221;
    static constexpr int chordInvertVoicing = 222;
    static constexpr int chordSendToGridRoll = 223;

    // Note actions (300-399)
    static constexpr int noteEditTheory = 301;
    static constexpr int noteQuantizeScale = 302;
    static constexpr int noteConvertToChord = 303;
    static constexpr int noteDeriveProgression = 304;

    // Progression actions (400-499)
    static constexpr int progressionGrabSampler = 401;
    static constexpr int progressionCreateCandidate = 402;
    static constexpr int progressionAnnotateKeyMode = 403;
    static constexpr int progressionTagTransition = 404;
    static constexpr int progressionEditIdentity = 405;
    static constexpr int progressionForkVariant = 406;
    static constexpr int progressionAddChord = 407;
    static constexpr int progressionClearChords = 408;
    static constexpr int progressionAssignToSection = 409;
    static constexpr int progressionSubAllDominants = 410;
    static constexpr int progressionTransposeToKey = 411;
    static constexpr int progressionSaveAsTemplate = 412;
    static constexpr int progressionBrowseLibrary = 420;

    // History buffer actions (500-599)
    static constexpr int historyGrab4 = 501;
    static constexpr int historyGrab8 = 502;
    static constexpr int historyGrab16 = 503;
    static constexpr int historyGrabCustom = 504;
    static constexpr int historySetLength = 505;
    static constexpr int historyChangeSource = 506;
    static constexpr int historyClear = 507;
    static constexpr int historyAutoGrabToggle = 508;
    static constexpr int historySetBpm = 510;
    static constexpr int historyCaptureSingle = 513;
}

namespace InstrumentSlotProps {
    static const juce::Identifier containerId { "instrumentSlots" };
    static const juce::Identifier entryId { "instrumentSlot" };
    static const juce::Identifier trackIdProp { "trackId" };
    static const juce::Identifier slotTypeProp { "slotType" };
    static const juce::Identifier persistentIdProp { "persistentId" };
    static const juce::Identifier persistentNameProp { "persistentName" };
}