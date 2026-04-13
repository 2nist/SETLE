#pragma once

#include <array>
#include <functional>

#include <tracktion_engine/tracktion_engine.h>

#include "GrabSlot.h"
#include "../model/SetleSongModel.h"

namespace te = tracktion::engine;

namespace setle::capture
{

class GrabSamplerQueue
{
public:
    static constexpr int kSlotCount = 4;

    GrabSamplerQueue();

    // Slot management
    int loadProgression(const juce::String& progressionId,
                        const juce::String& displayName,
                        float confidence);
    bool loadProgression(int slotIndex,
                         const juce::String& progressionId,
                         const juce::String& displayName,
                         float confidence);
    bool setSlotAudioBuffer(int slotIndex,
                            const juce::AudioBuffer<float>& audio,
                            double sampleRate);
    void clearSlot(int slotIndex);
    const GrabSlot& getSlot(int slotIndex) const;
    GrabSlot& getSlotMutable(int slotIndex);

    // Playback
    void playSlot(int slotIndex, te::Edit& edit);
    void stopPlayback(te::Edit& edit);
    int getActiveSlotIndex() const noexcept;

    // Promotion
    juce::String promoteToLibrary(int slotIndex, model::Song& song);  // returns progressionId

    void setSong(model::Song* songState);
    void setOnQueueChanged(std::function<void()> callback);

private:
    bool isValidSlot(int slotIndex) const;
    te::AudioTrack* findOrCreateSamplerTrack(te::Edit& edit);
    void rebuildSamplerClipForSlot(te::Edit& edit, int slotIndex, te::AudioTrack& track);
    void rebuildSamplerClipForAudioSlot(te::AudioTrack& track, double durationSeconds);
    void notifyQueueChanged();

    std::array<GrabSlot, kSlotCount> slots;
    int activeSlot { -1 };
    double prePlayheadSeconds { 0.0 };
    model::Song* songRef { nullptr };
    std::function<void()> onQueueChanged;
};

} // namespace setle::capture
