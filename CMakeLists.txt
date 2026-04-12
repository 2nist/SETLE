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
    source/model/SetleSongModel.cpp
    source/model/SetleSongModel.h
    source/ui/WorkspaceShellComponent.cpp
    source/ui/WorkspaceShellComponent.h
    source/ui/ProgressionLibraryBrowser.cpp
    source/ui/ProgressionLibraryBrowser.h
    source/ui/ProgressionChordPalette.cpp
    source/ui/ProgressionChordPalette.h
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
