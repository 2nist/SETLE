#include "TheoryEngine.h"

#include "DiatonicHarmony.h"

#include <cmath>

namespace
{
int nearestDegree(const std::vector<int>& intervals, int pitchClass)
{
    if (intervals.empty())
        return 0;

    int bestIndex = 0;
    int bestDistance = 127;
    for (int i = 0; i < static_cast<int>(intervals.size()); ++i)
    {
        const auto diff = std::abs(intervals[static_cast<size_t>(i)] - pitchClass);
        const auto wrapped = juce::jmin(diff, 12 - diff);
        if (wrapped < bestDistance)
        {
            bestDistance = wrapped;
            bestIndex = i;
        }
    }

    return bestIndex;
}
} // namespace

namespace setle::theory
{

int TheoryEngine::applyModalTransform(int midiPitch,
                                      const juce::String& sessionKey,
                                      const juce::String& sourceMode,
                                      const juce::String& targetMode)
{
    const auto src = DiatonicHarmony::modeIntervals(sourceMode);
    const auto dst = DiatonicHarmony::modeIntervals(targetMode);
    if (src.empty() || dst.empty())
        return juce::jlimit(0, 127, midiPitch);

    const auto tonicPc = DiatonicHarmony::pitchClassForRoot(sessionKey);
    const auto relativePc = ((midiPitch % 12) - tonicPc + 12) % 12;
    const auto degree = nearestDegree(src, relativePc);
    const auto dstPc = (tonicPc + dst[static_cast<size_t>(degree)]) % 12;

    const auto octave = juce::jmax(0, midiPitch / 12);
    return juce::jlimit(0, 127, octave * 12 + dstPc);
}

int TheoryEngine::transposeToMode(int midiPitch,
                                  const juce::String& sessionKey,
                                  const juce::String& sourceMode,
                                  const juce::String& targetMode)
{
    return applyModalTransform(midiPitch, sessionKey, sourceMode, targetMode);
}

} // namespace setle::theory
