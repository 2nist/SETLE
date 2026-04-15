#include "InstrumentSlot.h"

#include "drum/DrumMachineEditorComponent.h"
#include "polysynth/PolySynthEditorComponent.h"
#include "reel/ReelEditorComponent.h"

namespace setle::instruments
{
namespace
{
class SlotEditorPlaceholder final : public juce::Component
{
public:
    SlotEditorPlaceholder(juce::String titleText, juce::String detailText)
        : title(std::move(titleText)),
          detail(std::move(detailText))
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff141a1f));
        g.fillRoundedRectangle(r, 4.0f);

        g.setColour(juce::Colour(0xff4a5a63));
        g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);

        auto textArea = getLocalBounds().reduced(10, 8);
        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.setFont(juce::FontOptions(14.0f));
        g.drawText(title, textArea.removeFromTop(20), juce::Justification::centredLeft, true);

        g.setColour(juce::Colours::white.withAlpha(0.68f));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(detail, textArea, juce::Justification::topLeft, true);
    }

private:
    juce::String title;
    juce::String detail;
};
} // namespace

InstrumentSlot::InstrumentSlot(te::AudioTrack& t, te::Edit& e)
    : track(t), edit(e)
{
    info.trackId = track.itemID.toString();
}

void InstrumentSlot::loadPolySynth()
{
    clear();

    static bool registered = false;
    if (!registered)
    {
        edit.engine.getPluginManager().createBuiltInType<setle::instruments::polysynth::TracktionPolySynthPlugin>();
        registered = true;
    }

    auto plugin = edit.getPluginCache().createNewPlugin(setle::instruments::polysynth::TracktionPolySynthPlugin::xmlTypeName, {});
    if (plugin != nullptr)
    {
        track.pluginList.insertPlugin(plugin, 0, nullptr);
        loadedPlugin = plugin;
    }

    setSlot(SlotType::PolySynth,
            "PolySynth",
            loadedPlugin != nullptr
                ? "Using native SETLE PolySynth as the slot audio backend."
                : "PolySynth slot active, but synth backend could not be created.");

    if (auto* native = dynamic_cast<setle::instruments::polysynth::TracktionPolySynthPlugin*>(loadedPlugin.get()))
        editorComponent = std::make_unique<setle::instruments::polysynth::PolySynthEditorComponent>(native->getProcessor());

    info.persistentIdentifier = setle::instruments::polysynth::TracktionPolySynthPlugin::xmlTypeName;
    info.persistentName = "PolySynth";
}

void InstrumentSlot::loadDrumMachine()
{
    clear();

    static bool registered = false;
    if (!registered)
    {
        edit.engine.getPluginManager().createBuiltInType<setle::instruments::drum::TracktionDrumMachinePlugin>();
        registered = true;
    }

    auto plugin = edit.getPluginCache().createNewPlugin(setle::instruments::drum::TracktionDrumMachinePlugin::xmlTypeName, {});
    if (plugin != nullptr)
    {
        track.pluginList.insertPlugin(plugin, 0, nullptr);
        loadedPlugin = plugin;
    }

    setSlot(SlotType::DrumMachine,
            "DrumMachine",
            "Native DrumMachine backend active. GridRoll pattern bridge is wired in Phase 15D.");

    if (auto* drum = dynamic_cast<setle::instruments::drum::TracktionDrumMachinePlugin*>(loadedPlugin.get()))
        editorComponent = std::make_unique<setle::instruments::drum::DrumMachineEditorComponent>(drum->getProcessor());

    info.persistentIdentifier = setle::instruments::drum::TracktionDrumMachinePlugin::xmlTypeName;
    info.persistentName = "DrumMachine";
}

void InstrumentSlot::loadReelSampler()
{
    clear();

    static bool registered = false;
    if (!registered)
    {
        edit.engine.getPluginManager().createBuiltInType<setle::instruments::reel::TracktionReelPlugin>();
        registered = true;
    }

    auto plugin = edit.getPluginCache().createNewPlugin(setle::instruments::reel::TracktionReelPlugin::xmlTypeName, {});
    if (plugin != nullptr)
    {
        track.pluginList.insertPlugin(plugin, 0, nullptr);
        loadedPlugin = plugin;
    }

    setSlot(SlotType::ReelSampler,
            "ReelSampler",
            loadedPlugin != nullptr
                ? "Native ReelSampler backend active."
                : "ReelSampler slot active, backend not available.");

    if (auto* reel = dynamic_cast<setle::instruments::reel::TracktionReelPlugin*>(loadedPlugin.get()))
        editorComponent = std::make_unique<setle::instruments::reel::ReelEditorComponent>(reel->getProcessor());

    info.persistentIdentifier = setle::instruments::reel::TracktionReelPlugin::xmlTypeName;
    info.persistentName = "ReelSampler";
}

void InstrumentSlot::loadVST3(const juce::PluginDescription& desc)
{
    clear();

    const auto name = desc.name.isNotEmpty() ? desc.name : "VST3 Plugin";
    if (!desc.fileOrIdentifier.isEmpty())
    {
        auto plugin = edit.getPluginCache().createNewPlugin(desc.createIdentifierString(), {});
        if (plugin == nullptr)
            plugin = edit.getPluginCache().createNewPlugin(desc.fileOrIdentifier, {});
        if (plugin != nullptr)
        {
            track.pluginList.insertPlugin(plugin, 0, nullptr);
            loadedPlugin = plugin;
        }
    }

    setSlot(SlotType::VST3,
            name,
            loadedPlugin != nullptr
                ? "External plugin inserted on track."
                : "Select a scanned VST3 plugin to insert.");

    if (loadedPlugin != nullptr)
    {
        auto pluginEditor = loadedPlugin->createEditor();
        if (pluginEditor != nullptr)
            editorComponent.reset(pluginEditor.release());
    }

    info.persistentIdentifier = desc.fileOrIdentifier;
    info.persistentName = name;
}

void InstrumentSlot::loadMidiOut(const juce::String& deviceIdentifier)
{
    clear();

    juce::String resolvedDeviceName;
    juce::String resolvedIdentifier = deviceIdentifier;
    auto& deviceManager = edit.engine.getDeviceManager();

    if (deviceIdentifier.isEmpty() || deviceIdentifier == "default")
    {
        if (auto* defaultOut = deviceManager.getDefaultMidiOutDevice())
        {
            resolvedDeviceName = defaultOut->getName();
            resolvedIdentifier = defaultOut->getDeviceID();
        }
    }
    else
    {
        for (int i = 0; i < deviceManager.getNumOutputDevices(); ++i)
        {
            auto* out = deviceManager.getOutputDeviceAt(i);
            auto* midiOut = dynamic_cast<te::MidiOutputDevice*>(out);
            if (midiOut == nullptr)
                continue;

            if (midiOut->getDeviceID() == deviceIdentifier)
            {
                resolvedDeviceName = midiOut->getName();
                resolvedIdentifier = midiOut->getDeviceID();
                break;
            }
        }
    }

    if (resolvedDeviceName.isEmpty())
        resolvedDeviceName = "Default MIDI Output";

    auto plugin = edit.getPluginCache().createNewPlugin(te::InsertPlugin::xmlTypeName, {});
    if (auto* insert = dynamic_cast<te::InsertPlugin*>(plugin.get()))
    {
        insert->name = "MIDI Out";
        insert->outputDevice = resolvedDeviceName;
        insert->updateDeviceTypes();
    }

    if (plugin != nullptr)
    {
        track.pluginList.insertPlugin(plugin, 0, nullptr);
        loadedPlugin = plugin;
    }

    if (resolvedIdentifier.isNotEmpty() && resolvedIdentifier != "default")
        deviceManager.setDefaultMidiOutDevice(resolvedIdentifier);

    setSlot(SlotType::MidiOut,
            "MIDI Out",
            "Routing MIDI to: " + resolvedDeviceName);

    info.persistentIdentifier = resolvedIdentifier;
    info.persistentName = resolvedDeviceName;
}

void InstrumentSlot::clear()
{
    if (loadedPlugin != nullptr)
        loadedPlugin->removeFromParent();

    loadedPlugin = nullptr;
    info.type = SlotType::Empty;
    info.displayName = "Empty";
    info.persistentIdentifier.clear();
    info.persistentName.clear();
    info.isActive = false;
    editorComponent.reset();
}

InstrumentSlot::SlotInfo InstrumentSlot::getInfo() const
{
    return info;
}

juce::Component* InstrumentSlot::getEditorComponent()
{
    return editorComponent.get();
}

void InstrumentSlot::setDrumPattern(const std::vector<setle::gridroll::GridRollCell>& cells)
{
    if (auto* drum = dynamic_cast<setle::instruments::drum::TracktionDrumMachinePlugin*>(loadedPlugin.get()))
        drum->setPattern(cells);
}

void InstrumentSlot::setTransportBeat(double beat)
{
    if (auto* drum = dynamic_cast<setle::instruments::drum::TracktionDrumMachinePlugin*>(loadedPlugin.get()))
        drum->setTransportBeat(beat);
}

bool InstrumentSlot::getDrumSubSyncHint(float& frequencyHz, float& intensity) const
{
    if (auto* drum = dynamic_cast<setle::instruments::drum::TracktionDrumMachinePlugin*>(loadedPlugin.get()))
    {
        frequencyHz = drum->getSubSyncFrequencyHz();
        intensity = drum->getSubSyncIntensity();
        return frequencyHz > 0.0f && intensity > 0.0f;
    }

    frequencyHz = 0.0f;
    intensity = 0.0f;
    return false;
}

void InstrumentSlot::loadEffChain(const setle::eff::EffDefinition& def)
{
    if (effProcessor == nullptr)
        effProcessor = std::make_unique<setle::eff::EffProcessor>();
    effProcessor->loadDefinition(def);
    effProcessor->prepare(44100.0, 512);
}

void InstrumentSlot::clearEffChain()
{
    effProcessor.reset();
}

setle::eff::EffProcessor* InstrumentSlot::getEffProcessor()
{
    return effProcessor.get();
}

void InstrumentSlot::setSlot(SlotType type,
                             const juce::String& displayName,
                             const juce::String& detailText)
{
    info.type = type;
    info.displayName = displayName;
    info.trackId = track.itemID.toString();
    info.isActive = (type != SlotType::Empty);

    editorComponent = std::make_unique<SlotEditorPlaceholder>(displayName, detailText);

    (void) edit;
}

} // namespace setle::instruments
