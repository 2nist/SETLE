#include "AppPreferences.h"

namespace setle::state
{

AppPreferences& AppPreferences::get()
{
    static AppPreferences instance;
    return instance;
}

AppPreferences::AppPreferences()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "SETLE";
    options.filenameSuffix = "settings";
    options.folderName = "SETLE";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    appProps.setStorageParameters(options);
}

juce::PropertiesFile* AppPreferences::settings() const
{
    return appProps.getUserSettings();
}

juce::String AppPreferences::getCaptureSource() const
{
    if (auto* s = settings())
        return s->getValue("setle.capture.source", "");

    return {};
}

void AppPreferences::setCaptureSource(const juce::String& deviceId)
{
    if (auto* s = settings())
    {
        s->setValue("setle.capture.source", deviceId);
        s->saveIfNeeded();
    }
}

juce::String AppPreferences::getActiveThemeName() const
{
    if (auto* s = settings())
        return s->getValue("setle.theme.active", "Slate Dark");

    return "Slate Dark";
}

void AppPreferences::setActiveThemeName(const juce::String& name)
{
    if (auto* s = settings())
    {
        s->setValue("setle.theme.active", name);
        s->saveIfNeeded();
    }
}

juce::String AppPreferences::getLastProjectPath() const
{
    if (auto* s = settings())
        return s->getValue("setle.session.lastProject", "");

    return {};
}

void AppPreferences::setLastProjectPath(const juce::String& path)
{
    if (auto* s = settings())
    {
        s->setValue("setle.session.lastProject", path);
        s->saveIfNeeded();
    }
}

bool AppPreferences::getScaleLockEnabled() const
{
    if (auto* s = settings())
        return s->getBoolValue("setle.gridroll.scaleLock", false);

    return false;
}

void AppPreferences::setScaleLockEnabled(bool enabled)
{
    if (auto* s = settings())
    {
        s->setValue("setle.gridroll.scaleLock", enabled);
        s->saveIfNeeded();
    }
}

bool AppPreferences::getChordLockEnabled() const
{
    if (auto* s = settings())
        return s->getBoolValue("setle.gridroll.chordLock", false);

    return false;
}

void AppPreferences::setChordLockEnabled(bool enabled)
{
    if (auto* s = settings())
    {
        s->setValue("setle.gridroll.chordLock", enabled);
        s->saveIfNeeded();
    }
}

juce::String AppPreferences::getPatternLibraryPath() const
{
    return juce::File::getSpecialLocation(juce::File::currentApplicationFile)
        .getSiblingFile("assets")
        .getChildFile("patterns")
        .getFullPathName();
}

juce::String AppPreferences::getEffectsLibraryPath() const
{
    return juce::File::getSpecialLocation(juce::File::currentApplicationFile)
        .getSiblingFile("assets")
        .getChildFile("effects")
        .getFullPathName();
}

int AppPreferences::getHistoryBufferMaxBeats(int fallback) const
{
    if (auto* s = settings())
        return s->getIntValue("setle.capture.historyMaxBeats", fallback);

    return fallback;
}

void AppPreferences::setHistoryBufferMaxBeats(int beats)
{
    if (auto* s = settings())
    {
        s->setValue("setle.capture.historyMaxBeats", juce::jlimit(1, 128, beats));
        s->saveIfNeeded();
    }
}

int AppPreferences::getUiLeftWidth(int fallback) const
{
    if (auto* s = settings())
        return s->getIntValue("setle.ui.leftWidth", fallback);

    return fallback;
}

int AppPreferences::getUiRightWidth(int fallback) const
{
    if (auto* s = settings())
        return s->getIntValue("setle.ui.rightWidth", fallback);

    return fallback;
}

int AppPreferences::getUiTimelineHeight(int fallback) const
{
    if (auto* s = settings())
        return s->getIntValue("setle.ui.timelineHeight", fallback);

    return fallback;
}

int AppPreferences::getUiFocusMode(int fallback) const
{
    if (auto* s = settings())
        return s->getIntValue("setle.ui.focusMode", fallback);

    return fallback;
}

void AppPreferences::setUiLeftWidth(int value)
{
    if (auto* s = settings())
        s->setValue("setle.ui.leftWidth", value);
}

void AppPreferences::setUiRightWidth(int value)
{
    if (auto* s = settings())
        s->setValue("setle.ui.rightWidth", value);
}

void AppPreferences::setUiTimelineHeight(int value)
{
    if (auto* s = settings())
        s->setValue("setle.ui.timelineHeight", value);
}

void AppPreferences::setUiFocusMode(int value)
{
    if (auto* s = settings())
    {
        s->setValue("setle.ui.focusMode", value);
        s->saveIfNeeded();
    }
}

int AppPreferences::getMidiLearnCC() const
{
    if (auto* s = settings())
        return s->getIntValue("setle.midiLearn.cc", -1);
    return -1;
}

int AppPreferences::getMidiLearnChannel() const
{
    if (auto* s = settings())
        return s->getIntValue("setle.midiLearn.channel", 0);
    return 0;
}

void AppPreferences::setMidiLearnCC(int cc)
{
    if (auto* s = settings())
    {
        s->setValue("setle.midiLearn.cc", cc);
        s->saveIfNeeded();
    }
}

void AppPreferences::setMidiLearnChannel(int channel)
{
    if (auto* s = settings())
    {
        s->setValue("setle.midiLearn.channel", channel);
        s->saveIfNeeded();
    }
}

bool AppPreferences::getMidiLearnActive() const
{
    if (auto* s = settings())
        return s->getBoolValue("setle.midiLearn.active", false);
    return false;
}

void AppPreferences::setMidiLearnActive(bool active)
{
    if (auto* s = settings())
    {
        s->setValue("setle.midiLearn.active", active);
        s->saveIfNeeded();
    }
}

bool AppPreferences::getAutoGrabEnabled() const
{
    if (auto* s = settings())
        return s->getBoolValue("setle.capture.autoGrab", false);
    return false;
}

void AppPreferences::setAutoGrabEnabled(bool enabled)
{
    if (auto* s = settings())
    {
        s->setValue("setle.capture.autoGrab", enabled);
        s->saveIfNeeded();
    }
}

} // namespace setle::state
