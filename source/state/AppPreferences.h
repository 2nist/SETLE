#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace setle::state
{

class AppPreferences
{
public:
    static AppPreferences& get();

    juce::String getCaptureSource() const;
    void setCaptureSource(const juce::String& deviceId);

    juce::String getActiveThemeName() const;
    void setActiveThemeName(const juce::String& name);

    juce::String getLastProjectPath() const;
    void setLastProjectPath(const juce::String& path);

    bool getScaleLockEnabled() const;
    void setScaleLockEnabled(bool enabled);

    bool getChordLockEnabled() const;
    void setChordLockEnabled(bool enabled);

    juce::String getPatternLibraryPath() const;
    juce::String getEffectsLibraryPath() const;

    int getUiLeftWidth(int fallback) const;
    int getUiRightWidth(int fallback) const;
    int getUiTimelineHeight(int fallback) const;
    int getUiFocusMode(int fallback) const;
    void setUiLeftWidth(int value);
    void setUiRightWidth(int value);
    void setUiTimelineHeight(int value);
    void setUiFocusMode(int value);

private:
    AppPreferences();

    juce::PropertiesFile* settings() const;

    juce::ApplicationProperties appProps;
};

} // namespace setle::state
