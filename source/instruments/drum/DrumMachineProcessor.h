#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <vector>

#include "DrumModuleState.h"
#include "FmDrumVoice.h"
#include "../../gridroll/GridRollCell.h"

namespace setle::instruments::drum
{

class DrumMachineProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kVoiceCount = static_cast<int>(DrumVoiceId::Count);

    DrumMachineProcessor();
    ~DrumMachineProcessor() override = default;

    void setPattern(const std::vector<setle::gridroll::GridRollCell>& cells);
    void setTransportBeat(double beat);

    void setVoiceTune(int voiceIndex, float valueHz);
    void setVoiceDecay(int voiceIndex, float valueSeconds);
    void setVoiceLevel(int voiceIndex, float value);
    float getVoiceTune(int voiceIndex) const;
    float getVoiceDecay(int voiceIndex) const;
    float getVoiceLevel(int voiceIndex) const;
    void setMasterVolume(float value);
    float getMasterVolume() const;
    float getVoiceMeter(int voiceIndex) const;

    const juce::String getName() const override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    void triggerVoice(int midiNote, float velocity, int ratchetCount);
    int voiceIndexForMidi(int midiNote) const;

    juce::CriticalSection lock;
    std::vector<setle::gridroll::GridRollCell> pattern;
    std::vector<FmDrumVoice> activeHits;
    DrumModuleState moduleState;
    std::array<float, kVoiceCount> voiceMeters { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    double sampleRate { 44100.0 };
    double transportBeat { 0.0 };
    double previousBeat { 0.0 };
    int loopCounter { 0 };
    uint32_t loopSeed { 0x12345678u };
};

} // namespace setle::instruments::drum
