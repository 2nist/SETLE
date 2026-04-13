#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <vector>

#include "PolyVoice.h"

namespace setle::instruments::polysynth
{

class PolySynthProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kNumVoices = 8;

    PolySynthProcessor();
    ~PolySynthProcessor() override = default;

    void setMonoMode(bool enabled);
    bool isMonoMode() const noexcept;

    void playChord(const std::vector<int>& midiNotes, float velocity);
    void playNote(int midiNote, float velocity);
    void allNotesOff();

    void setAttackMs(float value);
    void setDecayMs(float value);
    void setSustain(float value);
    void setReleaseMs(float value);
    void setCutoffHz(float value);
    void setResonance(float value);
    void setOutputGain(float value);
    void setPatchName(const juce::String& name);

    float getAttackMs() const noexcept;
    float getDecayMs() const noexcept;
    float getSustain() const noexcept;
    float getReleaseMs() const noexcept;
    float getCutoffHz() const noexcept;
    float getResonance() const noexcept;
    float getOutputGain() const noexcept;
    juce::String getPatchName() const;

    const juce::String getName() const override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

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
    void handleMidi(const juce::MidiBuffer& midi);
    PolyVoice* findFreeVoice();
    PolyVoice* findVoiceForNote(int note);

    std::array<PolyVoice, kNumVoices> voices;
    juce::CriticalSection stateLock;

    std::atomic<float> attackSec { 0.01f };
    std::atomic<float> decaySec { 0.2f };
    std::atomic<float> sustainLevel { 0.7f };
    std::atomic<float> releaseSec { 0.4f };
    std::atomic<float> cutoffHz { 8000.0f };
    std::atomic<float> resonance { 0.2f };
    std::atomic<float> outputGain { 0.8f };
    std::atomic<bool> monoMode { false };

    juce::String patchName { "Init Patch" };
};

} // namespace setle::instruments::polysynth
