#include "DrumPatternMidiReader.h"

#include <cmath>

namespace setle::gridroll
{

DrumPatternMidiReader::Result DrumPatternMidiReader::readFile(const juce::File& midiFile, int stepCount)
{
    Result result;
    result.stepCount = stepCount;
    result.velocities.insertMultiple(0, 0, stepCount);

    if (!midiFile.existsAsFile())
        return result;

    juce::FileInputStream stream(midiFile);
    if (!stream.openedOk())
        return result;

    juce::MidiFile midi;
    if (!midi.readFrom(stream))
        return result;

    const auto ppq = midi.getTimeFormat();
    if (ppq <= 0)
        return result;

    int maxTick = 1;
    for (int ti = 0; ti < midi.getNumTracks(); ++ti)
    {
        if (const auto* track = midi.getTrack(ti))
        {
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto& msg = track->getEventPointer(i)->message;
                if (!msg.isNoteOn(false) || msg.getVelocity() == 0)
                    continue;

                const int tick = static_cast<int>(msg.getTimeStamp());
                maxTick = juce::jmax(maxTick, tick + 1);
                const int step = juce::jlimit(0, stepCount - 1,
                    static_cast<int>(std::floor((double)tick / (double)maxTick * stepCount)));
                const auto velocityInt = juce::jlimit(1, 127, static_cast<int>(msg.getVelocity()));
                const auto velocity = static_cast<juce::uint8>(velocityInt);
                if (velocity > result.velocities[step])
                    result.velocities.set(step, velocity);
                result.stepEvents.emplace_back(step, velocity);
            }
        }
    }

    result.success = !result.stepEvents.empty();
    return result;
}

} // namespace setle::gridroll
