#pragma once

// Compatibility shim for third-party JUCE modules that still include
// Projucer-era AppConfig.h in CMake builds.
#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#endif
