cmake_minimum_required(VERSION 3.24)

project(SETLE VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Third-party module paths
set(SETLE_FONTAUDIO_MODULE_PATH modules/fontaudio/wrappers/juce/module/fontaudio)

option(SETLE_ENABLE_MELATONIN_INSPECTOR "Enable melatonin_inspector module" ON)
option(SETLE_ENABLE_PITCH_DETECTOR "Enable pitch_detector + audio_fft modules" ON)
option(SETLE_ENABLE_FONTAUDIO "Enable fontaudio JUCE module" ON)
option(SETLE_ENABLE_CELLO "Enable cello JUCE module" ON)
option(SETLE_BUILD_TESTS "Build SETLE model tests" ON)

# ---------------------------------------------------------------
# Dependency preflight checks
# ---------------------------------------------------------------
set(SETLE_REQUIRED_PATHS
    modules/JUCE
    modules/tracktion_engine
    source/Main.cpp
)

set(SETLE_MISSING_PATHS "")
foreach(path IN LISTS SETLE_REQUIRED_PATHS)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
        list(APPEND SETLE_MISSING_PATHS "${path}")
    endif()
endforeach()

if(SETLE_MISSING_PATHS)
    string(JOIN "\n  - " SETLE_MISSING_PATHS_LIST ${SETLE_MISSING_PATHS})
    message(FATAL_ERROR
        "SETLE is missing required files/directories:\n"
        "  - ${SETLE_MISSING_PATHS_LIST}\n\n"
        "If this is a git checkout, run:\n"
        "  git submodule update --init --recursive\n"
    )
endif()

# Bring in JUCE
add_subdirectory(modules/JUCE)

# Bring in Tracktion Engine
add_subdirectory(modules/tracktion_engine)

# Optional modules from awesome-juce
if(SETLE_ENABLE_MELATONIN_INSPECTOR)
    set(module_path modules/melatonin_inspector)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module_path}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module_path}/CMakeLists.txt")
        add_subdirectory(${module_path})
        message(STATUS "Including optional module via CMake: ${module_path}")
    else()
        message(FATAL_ERROR
            "SETLE_ENABLE_MELATONIN_INSPECTOR is ON, but required module is missing.\n"
            "Expected:\n"
            "  - modules/melatonin_inspector (with CMakeLists.txt)\n"
            "Disable with -DSETLE_ENABLE_MELATONIN_INSPECTOR=OFF if needed."
        )
    endif()
endif()

# Register optional JUCE modules
if(SETLE_ENABLE_FONTAUDIO)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SETLE_FONTAUDIO_MODULE_PATH}")
        juce_add_modules(${SETLE_FONTAUDIO_MODULE_PATH})
        message(STATUS "Including optional JUCE module: fontaudio")
    else()
        message(FATAL_ERROR
            "SETLE_ENABLE_FONTAUDIO is ON, but required module path is missing.\n"
            "Expected:\n"
            "  - ${SETLE_FONTAUDIO_MODULE_PATH}\n"
            "Disable with -DSETLE_ENABLE_FONTAUDIO=OFF if needed."
        )
    endif()
endif()

if(SETLE_ENABLE_CELLO)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/cello")
        juce_add_modules(modules/cello)
        message(STATUS "Including optional JUCE module: cello")
    else()
        message(FATAL_ERROR
            "SETLE_ENABLE_CELLO is ON, but required module path is missing.\n"
            "Expected:\n"
            "  - modules/cello\n"
            "Disable with -DSETLE_ENABLE_CELLO=OFF if needed."
        )
    endif()
endif()

# Optional JUCE modules
if(SETLE_ENABLE_PITCH_DETECTOR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_fft" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/pitch_detector")
        juce_add_modules(modules/audio_fft)
        juce_add_modules(modules/pitch_detector)
        message(STATUS "Including optional JUCE modules: audio_fft, pitch_detector")
    else()
        message(FATAL_ERROR
            "SETLE_ENABLE_PITCH_DETECTOR is ON, but required modules are missing.\n"
            "Expected:\n"
            "  - modules/audio_fft\n"
            "  - modules/pitch_detector\n"
            "Disable with -DSETLE_ENABLE_PITCH_DETECTOR=OFF if needed."
        )
    endif()
endif()

# ---------------------------------------------------------------
# SETLE standalone app target
# ---------------------------------------------------------------
juce_add_gui_app(SETLE
    PRODUCT_NAME    "SETLE"
    VERSION         "0.0.1"
    COMPANY_NAME    "2nist"
    BUNDLE_ID       "com.2nist.setle"
)

# Sources — just Main.cpp to start
target_sources(SETLE PRIVATE
    source/Main.cpp
    source/capture/GrabSamplerQueue.cpp
    source/capture/GrabSamplerQueue.h
    source/capture/GrabSlot.h
    source/capture/HistoryBuffer.cpp
    source/capture/HistoryBuffer.h
    source/instruments/drum/DrumMachineProcessor.cpp
    source/instruments/drum/DrumMachineProcessor.h
    source/instruments/drum/DrumMachineEditorComponent.cpp
    source/instruments/drum/DrumMachineEditorComponent.h
    source/instruments/drum/DrumVoiceDSP.cpp
    source/instruments/drum/DrumVoiceDSP.h
    source/instruments/drum/DrumVoiceParams.h
    source/instruments/drum/FmDrumVoice.cpp
    source/instruments/drum/FmDrumVoice.h
    source/instruments/drum/DrumKitPatch.h
    source/instruments/drum/DrumModuleState.h
    source/instruments/drum/TracktionDrumMachinePlugin.cpp
    source/instruments/drum/TracktionDrumMachinePlugin.h
    source/instruments/InstrumentSlot.cpp
    source/instruments/InstrumentSlot.h
    source/instruments/reel/ReelBuffer.cpp
    source/instruments/reel/ReelBuffer.h
    source/instruments/reel/ReelGranular.cpp
    source/instruments/reel/ReelGranular.h
    source/instruments/reel/ReelLoopPlayer.cpp
    source/instruments/reel/ReelLoopPlayer.h
    source/instruments/reel/ReelParams.h
    source/instruments/reel/ReelProcessor.cpp
    source/instruments/reel/ReelProcessor.h
    source/instruments/reel/ReelEditorComponent.cpp
    source/instruments/reel/ReelEditorComponent.h
    source/instruments/reel/ReelSampler.cpp
    source/instruments/reel/ReelSampler.h
    source/instruments/reel/ReelSlicer.cpp
    source/instruments/reel/ReelSlicer.h
    source/instruments/reel/TracktionReelPlugin.cpp
    source/instruments/reel/TracktionReelPlugin.h
    source/module/ModuleProcessor.h
    source/instruments/polysynth/PolySynthProcessor.cpp
    source/instruments/polysynth/PolySynthProcessor.h
    source/instruments/polysynth/PolySynthEditorComponent.cpp
    source/instruments/polysynth/PolySynthEditorComponent.h
    source/instruments/polysynth/PolyVoice.h
    source/instruments/polysynth/SineOscNode.h
    source/instruments/polysynth/TracktionPolySynthPlugin.cpp
    source/instruments/polysynth/TracktionPolySynthPlugin.h
    source/gridroll/GridRollCell.h
    source/gridroll/ChordGridView.cpp
    source/gridroll/ChordGridView.h
    source/gridroll/NoteDetailView.cpp
    source/gridroll/NoteDetailView.h
    source/gridroll/DrumGridView.cpp
    source/gridroll/DrumGridView.h
    source/gridroll/GroovePanel.cpp
    source/gridroll/GroovePanel.h
    source/gridroll/DrumPatternLibrary.cpp
    source/gridroll/DrumPatternLibrary.h
    source/gridroll/DrumPatternBrowser.cpp
    source/gridroll/DrumPatternBrowser.h
    source/gridroll/DrumPatternMidiReader.cpp
    source/gridroll/DrumPatternMidiReader.h
    source/gridroll/GridRollComponent.cpp
    source/gridroll/GridRollComponent.h
    source/midi/MidiConstraintEngine.cpp
    source/midi/MidiConstraintEngine.h
    source/theory/DiatonicHarmony.cpp
    source/theory/DiatonicHarmony.h
    source/theory/TheoryAdapter.cpp
    source/theory/TheoryAdapter.h
    source/theory/BachTheory.cpp
    source/theory/BachTheory.h
    source/state/AppPreferences.cpp
    source/state/AppPreferences.h
    source/theme/ThemeData.h
    source/theme/ThemeManager.cpp
    source/theme/ThemeManager.h
    source/theme/ThemePresets.cpp
    source/theme/ThemePresets.h
    source/theme/SetleLookAndFeel.h
    source/theme/ThemeEditorPanel.cpp
    source/theme/ThemeEditorPanel.h
    source/model/SetleSongModel.cpp
    source/model/SetleSongModel.h
    source/mixer/BusManager.cpp
    source/mixer/BusManager.h
    source/mixer/MasterStrip.cpp
    source/mixer/MasterStrip.h
    source/mixer/MixerComponent.cpp
    source/mixer/MixerComponent.h
    source/mixer/MixerStrip.cpp
    source/mixer/MixerStrip.h
    source/timeline/TrackManager.cpp
    source/timeline/TrackManager.h
    source/timeline/TrackLane.cpp
    source/timeline/TrackLane.h
    source/timeline/TimelineTracksComponent.cpp
    source/timeline/TimelineTracksComponent.h
    source/ui/WorkspaceShellComponent.cpp
    source/ui/WorkspaceShellComponent.h
    source/ui/ProgressionLibraryBrowser.cpp
    source/ui/ProgressionLibraryBrowser.h
    source/ui/ProgressionChordPalette.cpp
    source/ui/ProgressionChordPalette.h
    source/eff/EffModel.h
    source/eff/EffSerializer.h
    source/eff/EffSerializer.cpp
    source/eff/EffProcessor.h
    source/eff/EffProcessor.cpp
    source/eff/EffTemplates.h
    source/eff/EffTemplates.cpp
    source/eff/EffBlockPickerOverlay.h
    source/eff/EffBlockPickerOverlay.cpp
    source/eff/EffBuilderComponent.h
    source/eff/EffBuilderComponent.cpp
)

# Compile definitions
target_compile_definitions(SETLE PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SETLE,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SETLE,JUCE_VERSION>"
)

set(SETLE_OPTIONAL_LIBS "")
if(TARGET melatonin_inspector)
    list(APPEND SETLE_OPTIONAL_LIBS melatonin_inspector)
endif()
if(TARGET pitch_detector)
    list(APPEND SETLE_OPTIONAL_LIBS pitch_detector)
endif()
if(TARGET fontaudio)
    list(APPEND SETLE_OPTIONAL_LIBS fontaudio)
endif()
if(TARGET cello)
    list(APPEND SETLE_OPTIONAL_LIBS cello)
endif()

# Link libraries
target_link_libraries(SETLE PRIVATE
    # JUCE modules
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    # Tracktion Engine
    tracktion::tracktion_engine
    tracktion::tracktion_graph
    # Optional modules
    ${SETLE_OPTIONAL_LIBS}
    # Recommended JUCE flags
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(SETLE_BUILD_TESTS)
    enable_testing()

    add_executable(setle_model_tests
        source/tests/SetleSongModelTests.cpp
        source/model/SetleSongModel.cpp
        source/model/SetleSongModel.h
    )

    target_link_libraries(setle_model_tests PRIVATE
        juce::juce_core
        juce::juce_data_structures
    )

    target_compile_definitions(setle_model_tests PRIVATE
        JUCE_USE_CURL=0
    )

    add_test(NAME setle_model_tests COMMAND setle_model_tests)
endif()
