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
    enum Waveform { Saw = 0, Square, Sine, Triangle, Noise };
    enum LfoTarget { LfoToPitch = 0, LfoToFilter };

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
    void noteOn(int note, float velocity,
                const juce::ADSR::Parameters& ampP,
                const juce::ADSR::Parameters& filtP);
    void noteOff(int note);
    void handleMidi(const juce::MidiBuffer& midi);
    PolyVoice* findFreeVoice();
    PolyVoice* findVoiceForNote(int note);
    PolyVoice* stealVoice();
    void renderVoice(PolyVoice& v, int numSamples,
                     bool osc1Enabled, bool osc2Enabled,
                     int osc1Shape, int osc2Shape,
                     double osc1Freq, double osc2Freq,
                     float osc1PulseWidth, float osc2PulseWidth, float osc2Level,
                     float baseCutoff, float filterRes, float filterEnvAmt,
                     float lfoValue, int lfoTarget,
                     float charAsym, float charDrive,
                     float driveGain, float driveNorm,
                     float charDrift) noexcept;
    static float renderOsc(double& phase, double phaseInc, int shape, float pulseWidth) noexcept;

    std::array<PolyVoice, kNumVoices> voices;
    std::vector<int> heldNotes;
    double lfoPhase { 0.0 };
    double currentSampleRate { 44100.0 };

    juce::CriticalSection stateLock;

    std::atomic<bool> osc1Enabled { true };
    std::atomic<bool> osc2Enabled { true };
    std::atomic<int> osc1Shape { Saw };
    std::atomic<int> osc2Shape { Saw };
    std::atomic<float> osc1Detune { 0.0f };
    std::atomic<float> osc2Detune { 7.0f };
    std::atomic<int> osc1Octave { 0 };
    std::atomic<int> osc2Octave { 0 };
    std::atomic<float> osc1PulseWidth { 0.5f };
    std::atomic<float> osc2PulseWidth { 0.5f };
    std::atomic<float> osc2Level { 0.5f };

    std::atomic<float> filterCutoff { 8000.0f };
    std::atomic<float> filterRes { 0.2f };
    std::atomic<float> filterEnvAmt { 0.5f };

    std::atomic<float> ampA { 0.01f };
    std::atomic<float> ampD { 0.2f };
    std::atomic<float> ampS { 0.7f };
    std::atomic<float> ampR { 0.4f };

    std::atomic<float> filtA { 0.01f };
    std::atomic<float> filtD { 0.2f };
    std::atomic<float> filtS { 0.5f };
    std::atomic<float> filtR { 0.3f };

    std::atomic<bool> lfoEnabled { true };
    std::atomic<float> lfoRate { 5.0f };
    std::atomic<float> lfoDepth { 0.0f };
    std::atomic<int> lfoTarget { LfoToPitch };

    std::atomic<float> charAsym { 0.15f };
    std::atomic<float> charDrive { 0.2f };
    std::atomic<float> charDrift { 0.15f };

    std::atomic<float> outLevel { 0.8f };
    std::atomic<float> outPan { 0.0f };
    std::atomic<bool> monoMode { false };

    juce::String patchName { "Init Patch" };
};

} // namespace setle::instruments::polysynth
