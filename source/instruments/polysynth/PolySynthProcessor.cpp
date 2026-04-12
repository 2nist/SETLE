#include "PolySynthProcessor.h"

namespace setle::instruments::polysynth
{

PolySynthProcessor::PolySynthProcessor()
    : juce::AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void PolySynthProcessor::setMonoMode(bool enabled) { monoMode.store(enabled); }
bool PolySynthProcessor::isMonoMode() const noexcept { return monoMode.load(); }

void PolySynthProcessor::playChord(const std::vector<int>& midiNotes, float velocity)
{
    const juce::ADSR::Parameters ampP { ampA.load(), ampD.load(), ampS.load(), ampR.load() };
    const juce::ADSR::Parameters filtP { filtA.load(), filtD.load(), filtS.load(), filtR.load() };

    if (monoMode.load())
    {
        if (!midiNotes.empty())
            noteOn(midiNotes.back(), velocity, ampP, filtP);
        return;
    }

    for (auto note : midiNotes)
        noteOn(note, velocity, ampP, filtP);
}

void PolySynthProcessor::playNote(int midiNote, float velocity)
{
    const juce::ADSR::Parameters ampP { ampA.load(), ampD.load(), ampS.load(), ampR.load() };
    const juce::ADSR::Parameters filtP { filtA.load(), filtD.load(), filtS.load(), filtR.load() };
    noteOn(midiNote, velocity, ampP, filtP);
}

void PolySynthProcessor::allNotesOff()
{
    for (auto& v : voices)
    {
        if (v.active)
            v.noteOff();
    }
}

void PolySynthProcessor::setAttackMs(float value) { ampA.store(juce::jlimit(0.001f, 5.0f, value * 0.001f)); }
void PolySynthProcessor::setDecayMs(float value) { ampD.store(juce::jlimit(0.001f, 5.0f, value * 0.001f)); }
void PolySynthProcessor::setSustain(float value) { ampS.store(juce::jlimit(0.0f, 1.0f, value)); }
void PolySynthProcessor::setReleaseMs(float value) { ampR.store(juce::jlimit(0.001f, 8.0f, value * 0.001f)); }
void PolySynthProcessor::setCutoffHz(float value) { filterCutoff.store(juce::jlimit(20.0f, 20000.0f, value)); }
void PolySynthProcessor::setResonance(float value) { filterRes.store(juce::jlimit(0.0f, 1.0f, value)); }
void PolySynthProcessor::setOutputGain(float value) { outLevel.store(juce::jlimit(0.0f, 1.5f, value)); }
void PolySynthProcessor::setPatchName(const juce::String& name) { patchName = name; }

float PolySynthProcessor::getAttackMs() const noexcept { return ampA.load() * 1000.0f; }
float PolySynthProcessor::getDecayMs() const noexcept { return ampD.load() * 1000.0f; }
float PolySynthProcessor::getSustain() const noexcept { return ampS.load(); }
float PolySynthProcessor::getReleaseMs() const noexcept { return ampR.load() * 1000.0f; }
float PolySynthProcessor::getCutoffHz() const noexcept { return filterCutoff.load(); }
float PolySynthProcessor::getResonance() const noexcept { return filterRes.load(); }
float PolySynthProcessor::getOutputGain() const noexcept { return outLevel.load(); }
juce::String PolySynthProcessor::getPatchName() const { return patchName; }

const juce::String PolySynthProcessor::getName() const { return "SETLE PolySynth"; }

void PolySynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::ScopedLock sl(stateLock);
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    lfoPhase = 0.0;

    for (auto& v : voices)
        v.prepare(currentSampleRate, samplesPerBlock);
}

void PolySynthProcessor::releaseResources()
{
    const juce::ScopedLock sl(stateLock);
    for (auto& v : voices)
        v.reset();
}

bool PolySynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void PolySynthProcessor::processBlock(juce::AudioBuffer<float>& output, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const juce::ScopedLock sl(stateLock);

    const int numSamples = output.getNumSamples();
    const int numCh = juce::jmin(output.getNumChannels(), 2);
    output.clear();

    if (numSamples <= 0 || numCh == 0)
        return;

    handleMidi(midi);

    const bool bOsc1 = osc1Enabled.load();
    const bool bOsc2 = osc2Enabled.load();
    const int s1 = osc1Shape.load();
    const int s2 = osc2Shape.load();
    const float d1 = osc1Detune.load();
    const float d2 = osc2Detune.load();
    const int o1 = osc1Octave.load();
    const int o2 = osc2Octave.load();
    const float pw1 = osc1PulseWidth.load();
    const float pw2 = osc2PulseWidth.load();
    const float mix2 = osc2Level.load();
    const float cutoff = filterCutoff.load();
    const float res = filterRes.load();
    const float fenv = filterEnvAmt.load();
    const float cAsym = charAsym.load();
    const float cDrive = charDrive.load();
    const float cDrift = charDrift.load();

    const float driveGain = 1.f + cDrive * 3.f;
    const float dg2 = driveGain * driveGain;
    const float driveNorm = driveGain * (27.f + dg2) / (27.f + 9.f * dg2);

    const float lfoR = lfoRate.load();
    const float lfoD = lfoDepth.load();
    const bool lfoOn = lfoEnabled.load();
    const int lfoTgt = lfoTarget.load();
    const double lfoInc = juce::MathConstants<double>::twoPi * static_cast<double>(lfoR) / currentSampleRate;
    const float lfoSample = static_cast<float>(std::sin(lfoPhase));
    lfoPhase += lfoInc * numSamples;
    while (lfoPhase >= juce::MathConstants<double>::twoPi)
        lfoPhase -= juce::MathConstants<double>::twoPi;
    const float lfoMod = lfoOn ? (lfoSample * lfoD) : 0.0f;

    for (auto& v : voices)
    {
        if (!v.active)
            continue;

        const double baseFreq = juce::MidiMessage::getMidiNoteInHertz(v.note);
        const double f1 = baseFreq * std::pow(2.0, static_cast<double>(d1 + static_cast<float>(o1) * 1200.0f) / 1200.0);
        const double f2 = baseFreq * std::pow(2.0, static_cast<double>(d2 + static_cast<float>(o2) * 1200.0f) / 1200.0);
        const double pitchMult = (lfoTgt == LfoToPitch) ? std::pow(2.0, static_cast<double>(lfoMod) / 12.0) : 1.0;

        renderVoice(v, numSamples,
                    bOsc1, bOsc2,
                    s1, s2,
                    f1 * pitchMult, f2 * pitchMult,
                    pw1, pw2, mix2,
                    cutoff, res, fenv,
                    lfoMod, lfoTgt,
                    cAsym, cDrive,
                    driveGain, driveNorm,
                    cDrift);

        const float* src = v.scratch.getReadPointer(0);
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* dst = output.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                dst[i] += src[i];
        }

        if (v.isFinished())
            v.reset();
    }

    const float level = juce::jlimit(0.0f, 1.5f, outLevel.load());
    const float pan = juce::jlimit(-1.0f, 1.0f, outPan.load());
    const float leftGain = level * (pan <= 0.0f ? 1.0f : 1.0f - pan);
    const float rightGain = level * (pan >= 0.0f ? 1.0f : 1.0f + pan);

    if (numCh >= 1) output.applyGain(0, 0, numSamples, leftGain);
    if (numCh >= 2) output.applyGain(1, 0, numSamples, rightGain);
}

void PolySynthProcessor::renderVoice(PolyVoice& v, int numSamples,
                                     bool bOsc1, bool bOsc2,
                                     int s1, int s2,
                                     double f1, double f2,
                                     float pw1, float pw2, float mix2,
                                     float baseCutoff, float fRes, float fEnvAmt,
                                     float lfoValue, int lfoTgt,
                                     float cAsym, float cDrive,
                                     float driveGain, float driveNorm,
                                     float cDrift) noexcept
{
    float* buf = v.scratch.getWritePointer(0);
    const double inc1 = juce::MathConstants<double>::twoPi * f1 / currentSampleRate;
    const double inc2 = juce::MathConstants<double>::twoPi * f2 / currentSampleRate;

    const bool useDrive = (cDrive > 0.002f);
    const bool useDrift = (cDrift > 0.002f && v.driftOscInc > 0.0);

    for (int i = 0; i < numSamples; ++i)
    {
        double driftMod = 1.0;
        if (useDrift)
        {
            driftMod = 1.0 + static_cast<double>(cDrift) * std::sin(v.driftOscPhase) * 0.003;
            v.driftOscPhase += v.driftOscInc;
            if (v.driftOscPhase >= juce::MathConstants<double>::twoPi)
                v.driftOscPhase -= juce::MathConstants<double>::twoPi;
        }

        float o1 = bOsc1 ? renderOsc(v.osc1Phase, inc1 * driftMod, s1, pw1) : 0.0f;
        float o2 = bOsc2 ? renderOsc(v.osc2Phase, inc2 * driftMod, s2, pw2) : 0.0f;

        const float envF = v.filterEnv.getNextSample();
        const float envA = v.ampEnv.getNextSample();

        if (cAsym > 0.002f)
        {
            o1 += cAsym * o1 * std::abs(o1) * 0.35f;
            o2 += cAsym * o2 * std::abs(o2) * 0.35f;
        }

        const float mix = juce::jlimit(0.0f, 1.0f, mix2);
        float sig = (o1 * (1.0f - mix) + o2 * mix) * envA * v.velocity;

        if (useDrive)
        {
            const float sg = sig * driveGain;
            const float sg2 = sg * sg;
            const float sat = sg * (27.f + sg2) / (27.f + 9.f * sg2) / driveNorm;
            sig = sig + cDrive * (sat - sig);
        }

        buf[i] = sig;

        float cutoff = baseCutoff + envF * fEnvAmt * (20000.0f - baseCutoff);
        if (lfoTgt == LfoToFilter)
            cutoff *= std::pow(2.0f, lfoValue * 2.0f);
        cutoff = juce::jlimit(20.0f, 20000.0f, cutoff);

        v.filter.setCutoffFrequencyHz(cutoff);
        v.filter.setResonance(fRes);

        float* chanPtr = buf + i;
        juce::dsp::AudioBlock<float> block(&chanPtr, 1, 1);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        v.filter.process(ctx);
    }
}

float PolySynthProcessor::renderOsc(double& phase, double phaseInc, int shape, float pulseWidth) noexcept
{
    float sample = 0.0f;
    const double wrappedPW = juce::jlimit(0.05, 0.95, static_cast<double>(pulseWidth));

    switch (shape)
    {
        case Saw: sample = static_cast<float>(phase / juce::MathConstants<double>::pi) - 1.0f; break;
        case Square: sample = (phase < juce::MathConstants<double>::twoPi * wrappedPW) ? 1.0f : -1.0f; break;
        case Sine: sample = static_cast<float>(std::sin(phase)); break;
        case Triangle:
            if (phase < juce::MathConstants<double>::pi)
                sample = static_cast<float>(phase / juce::MathConstants<double>::pi * 2.0 - 1.0);
            else
                sample = static_cast<float>(3.0 - phase / juce::MathConstants<double>::pi * 2.0);
            break;
        case Noise: sample = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f; break;
        default: break;
    }

    phase += phaseInc;
    if (phase >= juce::MathConstants<double>::twoPi)
        phase -= juce::MathConstants<double>::twoPi;

    return sample;
}

void PolySynthProcessor::handleMidi(const juce::MidiBuffer& midi)
{
    const juce::ADSR::Parameters ampP { ampA.load(), ampD.load(), ampS.load(), ampR.load() };
    const juce::ADSR::Parameters filtP { filtA.load(), filtD.load(), filtS.load(), filtR.load() };

    for (const auto& event : midi)
    {
        const auto msg = event.getMessage();
        if (msg.isNoteOn() && msg.getVelocity() > 0)
            noteOn(msg.getNoteNumber(), msg.getFloatVelocity(), ampP, filtP);
        else if (msg.isNoteOff() || (msg.isNoteOn() && msg.getVelocity() == 0))
            noteOff(msg.getNoteNumber());
    }
}

void PolySynthProcessor::noteOn(int note, float velocity,
                                const juce::ADSR::Parameters& ampP,
                                const juce::ADSR::Parameters& filtP)
{
    PolyVoice* v = nullptr;
    if (monoMode.load())
    {
        v = &voices[0];
        for (size_t i = 1; i < voices.size(); ++i)
            voices[i].reset();
        if (v->active)
            v->reset();
    }
    else
    {
        v = findFreeVoice();
        if (v == nullptr)
            v = stealVoice();
    }

    if (v == nullptr)
        return;

    v->ampEnv.setParameters(ampP);
    v->filterEnv.setParameters(filtP);
    v->noteOn(note, velocity);

    const float drift = charDrift.load();
    if (drift > 0.002f)
    {
        const uint32_t seed = static_cast<uint32_t>(note + 37) * 2654435761u;
        const float r = static_cast<float>(seed & 0xFFFFu) / 65535.0f;
        v->osc1Phase = static_cast<double>(drift) * 0.3 * juce::MathConstants<double>::twoPi * static_cast<double>(r);
        v->osc2Phase = static_cast<double>(drift) * 0.3 * juce::MathConstants<double>::twoPi * static_cast<double>(1.0f - r);
        const float driftHz = 0.4f + r * 2.0f;
        v->driftOscPhase = 0.0;
        v->driftOscInc = juce::MathConstants<double>::twoPi * static_cast<double>(driftHz) / currentSampleRate;
    }
    else
    {
        v->driftOscPhase = 0.0;
        v->driftOscInc = 0.0;
    }
}

void PolySynthProcessor::noteOff(int note)
{
    for (auto& v : voices)
    {
        if (v.active && !v.releasing && (note < 0 || v.note == note))
        {
            v.noteOff();
            return;
        }
    }
}

PolyVoice* PolySynthProcessor::findFreeVoice()
{
    for (auto& v : voices)
        if (!v.active)
            return &v;
    return nullptr;
}

PolyVoice* PolySynthProcessor::findVoiceForNote(int note)
{
    for (auto& v : voices)
        if (v.active && (note < 0 || v.note == note))
            return &v;
    return nullptr;
}

PolyVoice* PolySynthProcessor::stealVoice()
{
    for (auto& v : voices)
        if (v.releasing)
            return &v;
    for (auto& v : voices)
        if (v.active)
            return &v;
    return nullptr;
}

juce::AudioProcessorEditor* PolySynthProcessor::createEditor() { return nullptr; }
bool PolySynthProcessor::hasEditor() const { return false; }
double PolySynthProcessor::getTailLengthSeconds() const { return 0.0; }
int PolySynthProcessor::getNumPrograms() { return 1; }
int PolySynthProcessor::getCurrentProgram() { return 0; }
void PolySynthProcessor::setCurrentProgram(int) {}
const juce::String PolySynthProcessor::getProgramName(int) { return {}; }
void PolySynthProcessor::changeProgramName(int, const juce::String&) {}

void PolySynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ValueTree state("setlePolySynth");
    state.setProperty("attackMs", getAttackMs(), nullptr);
    state.setProperty("decayMs", getDecayMs(), nullptr);
    state.setProperty("sustain", getSustain(), nullptr);
    state.setProperty("releaseMs", getReleaseMs(), nullptr);
    state.setProperty("cutoff", getCutoffHz(), nullptr);
    state.setProperty("res", getResonance(), nullptr);
    state.setProperty("gain", getOutputGain(), nullptr);
    state.setProperty("mono", monoMode.load(), nullptr);
    state.setProperty("patch", patchName, nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PolySynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml(*xmlState);
    if (!state.isValid())
        return;

    setAttackMs(static_cast<float>(state.getProperty("attackMs", 10.0f)));
    setDecayMs(static_cast<float>(state.getProperty("decayMs", 200.0f)));
    setSustain(static_cast<float>(state.getProperty("sustain", 0.7f)));
    setReleaseMs(static_cast<float>(state.getProperty("releaseMs", 400.0f)));
    setCutoffHz(static_cast<float>(state.getProperty("cutoff", 8000.0f)));
    setResonance(static_cast<float>(state.getProperty("res", 0.2f)));
    setOutputGain(static_cast<float>(state.getProperty("gain", 0.8f)));
    setMonoMode(static_cast<bool>(state.getProperty("mono", false)));
    setPatchName(state.getProperty("patch", "Init Patch").toString());
}

} // namespace setle::instruments::polysynth
