#include "TrackLane.h"

#include <cmath>

namespace setle::timeline
{
namespace
{
constexpr const char* kTrackKindProperty = "setleTrackKind";
constexpr const char* kTrackKindMidi = "midi";
}

TrackLane::TrackLane(te::Track& track)
    : trackRef(track)
{
    nameLabel.setText(trackRef.getName(), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));

    muteButton.onClick = [this]
    {
        if (onMuteToggle)
            onMuteToggle(trackRef);
    };

    soloButton.onClick = [this]
    {
        if (onSoloToggle)
            onSoloToggle(trackRef);
    };

    addAndMakeVisible(nameLabel);
    addAndMakeVisible(muteButton);
    addAndMakeVisible(soloButton);
}

void TrackLane::setTimeRange(double startBeat, double endBeat)
{
    visibleStartBeat = startBeat;
    visibleEndBeat = juce::jmax(startBeat + 1.0, endBeat);
    repaint();
}

void TrackLane::setPlayheadFraction(double fraction)
{
    playheadFraction = juce::jlimit(0.0, 1.0, fraction);
    repaint();
}

void TrackLane::refreshClips()
{
    nameLabel.setText(trackRef.getName(), juce::dontSendNotification);
    repaint();
}

void TrackLane::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    auto headerArea = bounds.removeFromLeft(kHeaderWidth);
    auto clipArea = bounds;

    const auto isMidi = trackRef.state.getProperty(kTrackKindProperty).toString() == kTrackKindMidi;

    g.setColour(juce::Colour(0xff1b1f29));
    g.fillRect(headerArea);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRect(headerArea);

    g.setColour(juce::Colour(0xff11161e));
    g.fillRect(clipArea);

    const auto beatSpan = juce::jmax(1.0, visibleEndBeat - visibleStartBeat);
    const int startBeatInt = static_cast<int>(std::floor(visibleStartBeat));
    const int endBeatInt = static_cast<int>(std::ceil(visibleEndBeat));

    for (int beat = startBeatInt; beat <= endBeatInt; ++beat)
    {
        const auto frac = (static_cast<double>(beat) - visibleStartBeat) / beatSpan;
        const auto x = clipArea.getX() + static_cast<int>(std::round(frac * clipArea.getWidth()));
        const bool major = (beat % 4) == 0;
        g.setColour(juce::Colours::white.withAlpha(major ? 0.20f : 0.08f));
        g.drawVerticalLine(x, static_cast<float>(clipArea.getY()), static_cast<float>(clipArea.getBottom()));
    }

    rebuildPaintedClips(clipArea);

    for (const auto& painted : paintedClips)
    {
        if (painted.clip == nullptr)
            continue;

        g.setColour(isMidi ? juce::Colour(0xff2b8a6e) : juce::Colour(0xff6078c4));
        g.fillRoundedRectangle(painted.bounds.toFloat().reduced(1.0f), 4.0f);
        g.setColour(juce::Colours::white.withAlpha(0.40f));
        g.drawRoundedRectangle(painted.bounds.toFloat().reduced(1.0f), 4.0f, 1.0f);

        g.setColour(juce::Colours::white.withAlpha(0.90f));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(painted.clip->getName(), painted.bounds.reduced(6, 2), juce::Justification::centredLeft, true);
    }

    const auto playheadX = clipArea.getX() + static_cast<int>(std::round(playheadFraction * clipArea.getWidth()));
    g.setColour(juce::Colours::white.withAlpha(0.85f));
    g.drawVerticalLine(playheadX, static_cast<float>(clipArea.getY()), static_cast<float>(clipArea.getBottom()));
}

void TrackLane::resized()
{
    auto headerArea = getLocalBounds().removeFromLeft(kHeaderWidth).reduced(6, 4);

    nameLabel.setBounds(headerArea.removeFromTop(20));
    headerArea.removeFromTop(4);

    auto buttonRow = headerArea.removeFromBottom(20);
    muteButton.setBounds(buttonRow.removeFromLeft(24));
    buttonRow.removeFromLeft(4);
    soloButton.setBounds(buttonRow.removeFromLeft(24));

    muteButton.setToggleState(trackRef.isMuted(false), juce::dontSendNotification);
    soloButton.setToggleState(trackRef.isSolo(false), juce::dontSendNotification);
}

void TrackLane::mouseUp(const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown())
        return;

    const auto headerArea = getLocalBounds().removeFromLeft(kHeaderWidth);
    if (headerArea.contains(e.getPosition()))
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Rename Track");
        menu.addItem(2, "Remove Track");
        menu.addSeparator();
        menu.addItem(3, "Add MIDI Track Below");
        menu.addItem(4, "Add Audio Track Below");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                           [this](int selected)
                           {
                               if (selected == 1 && onRename)
                                   onRename(trackRef);
                               else if (selected == 2 && onRemove)
                                   onRemove(trackRef);
                               else if (selected == 3 && onAddMidiTrackBelow)
                                   onAddMidiTrackBelow(trackRef);
                               else if (selected == 4 && onAddAudioTrackBelow)
                                   onAddAudioTrackBelow(trackRef);
                           });
        return;
    }

    const auto* painted = findPaintedClipAt(e.getPosition());
    if (painted == nullptr || painted->clip == nullptr)
        return;

    if (onClipClicked)
        onClipClicked(*painted->clip);

    auto* clip = painted->clip;
    juce::PopupMenu menu;
    menu.addItem(101, "Open in Piano Roll");
    menu.addItem(102, "Rename Clip");
    menu.addItem(103, "Delete Clip");
    menu.addSeparator();
    menu.addItem(104, "Copy to Sampler Queue");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                       [this, clip](int selected)
                       {
                           if (clip == nullptr)
                               return;

                           if (selected == 101)
                           {
                               if (onClipClicked)
                                   onClipClicked(*clip);
                               return;
                           }

                           if (selected == 102)
                           {
                               juce::AlertWindow renameWindow("Rename Clip", "Enter clip name", juce::AlertWindow::NoIcon);
                               renameWindow.addTextEditor("name", clip->getName(), "Name");
                               renameWindow.addButton("Rename", 1);
                               renameWindow.addButton("Cancel", 0);

                               if (renameWindow.runModalLoop() == 1)
                               {
                                   const auto newName = renameWindow.getTextEditorContents("name").trim();
                                   if (newName.isNotEmpty())
                                       clip->state.setProperty("name", newName, nullptr);
                                   refreshClips();
                               }
                               return;
                           }

                           if (selected == 103)
                           {
                               clip->removeFromParent();
                               refreshClips();
                               return;
                           }

                           if (selected == 104 && onClipClicked)
                               onClipClicked(*clip);
                       });
}

void TrackLane::rebuildPaintedClips(const juce::Rectangle<int>& clipArea)
{
    paintedClips.clear();

    const auto* tempo = trackRef.edit.tempoSequence.getTempo(0);
    const auto bpm = tempo != nullptr ? tempo->getBpm() : 120.0;
    const auto secPerBeat = 60.0 / juce::jmax(1.0, bpm);
    const auto beatSpan = juce::jmax(1.0, visibleEndBeat - visibleStartBeat);

    for (auto* clip : trackRef.getClips())
    {
        if (clip == nullptr)
            continue;

        const auto pos = clip->getPosition();
        const auto clipStartBeat = pos.getStart().inSeconds() / secPerBeat;
        const auto clipEndBeat = pos.getEnd().inSeconds() / secPerBeat;

        if (clipEndBeat < visibleStartBeat || clipStartBeat > visibleEndBeat)
            continue;

        const auto startFrac = (clipStartBeat - visibleStartBeat) / beatSpan;
        const auto endFrac = (clipEndBeat - visibleStartBeat) / beatSpan;

        const auto x1 = clipArea.getX() + static_cast<int>(std::round(startFrac * clipArea.getWidth()));
        const auto x2 = clipArea.getX() + static_cast<int>(std::round(endFrac * clipArea.getWidth()));

        juce::Rectangle<int> rect(x1, clipArea.getY() + 6, juce::jmax(2, x2 - x1), clipArea.getHeight() - 12);
        rect = rect.getIntersection(clipArea.reduced(1, 1));

        if (!rect.isEmpty())
            paintedClips.push_back({ clip, rect });
    }
}

const TrackLane::PaintedClip* TrackLane::findPaintedClipAt(juce::Point<int> point) const
{
    for (const auto& painted : paintedClips)
    {
        if (painted.bounds.contains(point))
            return &painted;
    }

    return nullptr;
}

} // namespace setle::timeline
