#include "MixerComponent.h"

namespace setle::mixer
{

MixerComponent::MixerComponent(te::Edit& e,
                               setle::timeline::TrackManager& manager,
                               std::map<juce::String, std::unique_ptr<setle::instruments::InstrumentSlot>>& slots)
    : edit(e),
      trackManager(manager),
      instrumentSlots(slots)
{
    busManager = std::make_unique<BusManager>(edit);
    masterStrip = std::make_unique<MasterStrip>(edit);

    addAndMakeVisible(stripsViewport);
    stripsViewport.setViewedComponent(&stripsContent, false);
    stripsViewport.setScrollBarsShown(true, false);

    addAndMakeVisible(*masterStrip);
    refreshStrips();
}

void MixerComponent::refreshStrips()
{
    strips.clear();
    stripsContent.removeAllChildren();

    const auto tracks = trackManager.getAllUserTracks();
    for (auto* track : tracks)
    {
        auto* audioTrack = dynamic_cast<te::AudioTrack*>(track);
        if (audioTrack == nullptr || setle::timeline::TrackManager::isSystemTrack(*audioTrack))
            continue;

        juce::String instrumentType = "Empty";
        const auto trackId = audioTrack->itemID.toString();
        const auto it = instrumentSlots.find(trackId);
        if (it != instrumentSlots.end() && it->second != nullptr)
            instrumentType = it->second->getInfo().displayName;

        auto strip = std::make_unique<MixerStrip>(*audioTrack, edit, instrumentType, *busManager);
        stripsContent.addAndMakeVisible(*strip);
        strips.push_back(std::move(strip));
    }

    resized();
    repaint();
}

void MixerComponent::resized()
{
    auto area = getLocalBounds();

    auto masterArea = area.removeFromRight(MasterStrip::kStripWidth + 8).reduced(4, 4);
    masterStrip->setBounds(masterArea);

    stripsViewport.setBounds(area.reduced(4, 4));

    int x = 0;
    for (auto& strip : strips)
    {
        strip->setBounds(x, 0, MixerStrip::kStripWidth, juce::jmax(220, stripsViewport.getHeight() - 2));
        x += MixerStrip::kStripWidth + 6;
    }

    stripsContent.setSize(juce::jmax(x, stripsViewport.getWidth()), juce::jmax(220, stripsViewport.getHeight()));
}

void MixerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff12161b));

    const int dividerX = juce::jmax(0, getWidth() - MasterStrip::kStripWidth - 12);
    g.setColour(juce::Colour(0xff3f4852));
    g.drawVerticalLine(dividerX, 2.0f, static_cast<float>(getHeight() - 2));
}

} // namespace setle::mixer
