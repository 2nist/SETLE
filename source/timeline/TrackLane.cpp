#include "TrackLane.h"
#include "../theme/ThemeManager.h"
#include "../theme/ThemeStyleHelpers.h"

#include <cmath>

namespace setle::timeline
{
namespace
{
constexpr const char* kTrackKindProperty = "setleTrackKind";
constexpr const char* kTrackKindMidi = "midi";
constexpr const char* kTrackArmedProperty = "setleArmed";
constexpr const char* kTrackColorProperty = "setleTrackColor";

juce::Colour colourFromTrackState(const te::Track& track, const ThemeData& theme, bool isMidi)
{
    auto colorText = track.state.getProperty(kTrackColorProperty).toString().trim();
    if (colorText.startsWith("#"))
        colorText = colorText.substring(1);
    if (colorText.length() == 6)
        colorText = "ff" + colorText;
    if (colorText.length() == 8)
        return juce::Colour::fromString(colorText);

    return isMidi ? theme.signalMidi : theme.signalAudio;
}
}

TrackLane::TrackLane(te::Track& track)
    : trackRef(track)
{
    const auto& themeData = ThemeManager::get().theme();
    nameLabel.setText(trackRef.getName(), juce::dontSendNotification);
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId,
                        setle::theme::textForRole(themeData, setle::theme::TextRole::primary).withAlpha(0.9f));

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
    const bool isArmed = static_cast<bool>(trackRef.state.getProperty(kTrackArmedProperty, false));

    const auto& theme = ThemeManager::get().theme();
    const auto trackColour = colourFromTrackState(trackRef, theme, isMidi).withMultipliedSaturation(0.7f);

    g.setColour(isArmed
                    ? setle::theme::stateOverlay(theme, setle::theme::SurfaceState::warning).interpolatedWith(theme.surface2, 0.55f)
                    : setle::theme::panelBackground(theme, setle::theme::ZoneRole::neutral));
    g.fillRect(headerArea);
    g.setColour(setle::theme::timelineGridLine(theme, true));
    g.drawRect(headerArea);
    if (isArmed)
    {
        g.setColour(theme.accentWarm.withAlpha(0.95f));
        g.fillEllipse(static_cast<float>(headerArea.getRight() - 12),
                      static_cast<float>(headerArea.getY() + 6),
                      6.0f,
                      6.0f);
    }

    g.setColour(setle::theme::panelBackground(theme, setle::theme::ZoneRole::timeline));
    g.fillRect(clipArea);

    const auto beatSpan = juce::jmax(1.0, visibleEndBeat - visibleStartBeat);
    const int startBeatInt = static_cast<int>(std::floor(visibleStartBeat));
    const int endBeatInt = static_cast<int>(std::ceil(visibleEndBeat));

    for (int beat = startBeatInt; beat <= endBeatInt; ++beat)
    {
        const auto frac = (static_cast<double>(beat) - visibleStartBeat) / beatSpan;
        const auto x = clipArea.getX() + static_cast<int>(std::round(frac * clipArea.getWidth()));
        const bool barLine = (beat % 4) == 0;
        const bool fourBars = (beat % 16) == 0;
        g.setColour(setle::theme::timelineGridLine(theme, barLine, fourBars));
        g.drawVerticalLine(x, static_cast<float>(clipArea.getY()), static_cast<float>(clipArea.getBottom()));
    }

    rebuildPaintedClips(clipArea);

    for (const auto& painted : paintedClips)
    {
        if (painted.clip == nullptr)
            continue;

        const auto clipStyle = setle::theme::progressionBlockStyle(theme, false, false, trackColour);
        g.setColour(clipStyle.fill.withAlpha(0.95f));
        g.fillRoundedRectangle(painted.bounds.toFloat().reduced(1.0f), 4.0f);
        g.setColour(clipStyle.outline.withAlpha(0.78f));
        g.drawRoundedRectangle(painted.bounds.toFloat().reduced(1.0f), 4.0f,
                               setle::theme::stroke(theme, setle::theme::StrokeRole::subtle));

        auto clipRect = painted.bounds.reduced(4, 3);

        if (isMidi)
        {
            if (auto* midiClip = dynamic_cast<te::MidiClip*>(painted.clip))
            {
                auto notes = midiClip->getSequence().getNotes();
                if (!notes.isEmpty())
                {
                    int minNote = 127;
                    int maxNote = 0;
                    for (auto* note : notes)
                    {
                        if (note == nullptr) continue;
                        minNote = juce::jmin(minNote, note->getNoteNumber());
                        maxNote = juce::jmax(maxNote, note->getNoteNumber());
                    }

                    const int noteRange = juce::jmax(1, maxNote - minNote);
                    const auto* tempo = trackRef.edit.tempoSequence.getTempo(0);
                    const auto bpm = tempo != nullptr ? tempo->getBpm() : 120.0;
                    const auto secPerBeat = 60.0 / juce::jmax(1.0, bpm);
                    const auto pos = painted.clip->getPosition();
                    const auto clipDurBeats = juce::jmax(0.001, pos.getLength().inSeconds() / secPerBeat);

                    g.setColour(clipStyle.label.withAlpha(0.60f));
                    for (auto* note : notes)
                    {
                        if (note == nullptr) continue;
                        const double startFrac = note->getStartBeat().inBeats() / clipDurBeats;
                        const double endFrac = (note->getStartBeat().inBeats() + note->getLengthBeats().inBeats()) / clipDurBeats;
                        const int x1 = clipRect.getX() + static_cast<int>(startFrac * clipRect.getWidth());
                        const int x2 = clipRect.getX() + static_cast<int>(endFrac * clipRect.getWidth());
                        const float noteNorm = static_cast<float>(note->getNoteNumber() - minNote) / static_cast<float>(noteRange);
                        const int y = clipRect.getBottom() - 8 - static_cast<int>(noteNorm * juce::jmax(1, clipRect.getHeight() - 14));
                        g.drawLine(static_cast<float>(x1), static_cast<float>(y),
                                   static_cast<float>(juce::jmax(x1 + 1, x2)), static_cast<float>(y), 1.0f);
                    }
                }
            }
        }
        else
        {
            // Lightweight waveform-like thumbnail for audio clips.
            g.setColour(clipStyle.subtitle.withAlpha(0.70f));
            const int mid = clipRect.getCentreY();
            for (int x = clipRect.getX(); x < clipRect.getRight(); x += 3)
            {
                const float phase = static_cast<float>((x - clipRect.getX()) * 0.13);
                const float amp = 0.15f + 0.85f * std::abs(std::sin(phase));
                const int h = static_cast<int>(amp * (clipRect.getHeight() * 0.4f));
                g.drawVerticalLine(x, static_cast<float>(mid - h), static_cast<float>(mid + h));
            }
        }

        const auto progressionSymbols = painted.clip->state.getProperty("progressionSymbols").toString();
        if (progressionSymbols.isNotEmpty())
        {
            g.setColour(clipStyle.label.withAlpha(0.72f));
            g.setFont(juce::FontOptions(9.0f));
            g.drawText(progressionSymbols,
                       painted.bounds.reduced(5, 2).removeFromTop(10),
                       juce::Justification::centredLeft,
                       true);
        }

        g.setColour(clipStyle.subtitle.withAlpha(0.95f));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(painted.clip->getName(),
                   painted.bounds.reduced(6, 3).removeFromBottom(12),
                   juce::Justification::centredLeft,
                   true);
    }

    const auto playheadX = clipArea.getX() + static_cast<int>(std::round(playheadFraction * clipArea.getWidth()));
    g.setColour(setle::theme::textForRole(theme, setle::theme::TextRole::primary).withAlpha(0.95f));
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

void TrackLane::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isLeftButtonDown())
    {
        const auto* painted = findPaintedClipAt(e.getPosition());
        if (painted == nullptr || painted->clip == nullptr)
            return;

        // Check if click is in bottom 8px of the clip (pip strip area)
        const auto clipRect = painted->bounds;
        const auto pipStripBottom = clipRect.getBottom() - 4; // pip strip is 4px at bottom
        const auto pipStripTop = pipStripBottom - 8; // 8px height for pip strip click area

        if (e.y >= pipStripTop && e.y <= pipStripBottom)
        {
            // Compute which pip was clicked
            const auto blockLeft = clipRect.getX();
            const auto blockWidth = clipRect.getWidth();
            const auto repeatCells = painted->clip->state.getProperty("repeatCells").toString();
            const auto repeatCellsArray = juce::JSON::parse(repeatCells);

            if (repeatCellsArray.isArray())
            {
                const int pipIndex = juce::jlimit(0, repeatCellsArray.size() - 1,
                    static_cast<int>((e.x - blockLeft) * repeatCellsArray.size() / blockWidth));

                if (pipIndex >= 0 && pipIndex < repeatCellsArray.size())
                {
                    const auto progressionId = repeatCellsArray[pipIndex]["progressionId"].toString();

                    if (progressionId.isNotEmpty())
                    {
                        // Call GridRoll tab switch
                        if (onClipClicked)
                            onClipClicked(*painted->clip);

                        // Call gridRollComponent->setTargetProgression(progressionId)
                        // This would need to be implemented in the calling context

                        // Call loadProgressionToEdit(progressionId, 0.0, true, nullptr)
                        // This would need to be implemented in the calling context

#ifdef SETLE_DEV_TOOLS
                        juce::Logger::writeToLog("section-pip, pipClick, " + progressionId);
#endif
                    }
                }
            }
        }
    }
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
        menu.addSeparator();
        menu.addItem(5, "Duplicate Track");
        menu.addItem(6, "Move Up");
        menu.addItem(7, "Move Down");
        menu.addSeparator();
        menu.addItem(8, "Open FX Chain");
        menu.addItem(9, "Arm for Recording");
        menu.addItem(10, "Bounce to Audio");

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
                               else if (selected == 5 && onDuplicateTrack)
                                   onDuplicateTrack(trackRef);
                               else if (selected == 6 && onMoveTrackUp)
                                   onMoveTrackUp(trackRef);
                               else if (selected == 7 && onMoveTrackDown)
                                   onMoveTrackDown(trackRef);
                               else if (selected == 8 && onOpenFx)
                                   onOpenFx(trackRef);
                               else if (selected == 9 && onArmToggle)
                                   onArmToggle(trackRef);
                               else if (selected == 10 && onBounceToAudio)
                                   onBounceToAudio(trackRef);
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
    menu.addItem(105, "Duplicate Clip");
    menu.addSeparator();
    menu.addItem(106, "Toggle Loop");
    menu.addItem(107, "Quantize Start to Bar");
    menu.addItem(108, "Transpose...");
    menu.addItem(109, "Detach from Progression");

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
                           {
                               onClipClicked(*clip);
                               return;
                           }

                           if (selected == 105)
                           {
                               // Duplicate: clone the clip state and append
                               auto newClipState = clip->state.createCopy();
                               newClipState.setProperty("id", juce::Uuid().toString(), nullptr);
                               auto* track = clip->getTrack();
                               if (track != nullptr)
                               {
                                   track->state.appendChild(newClipState, nullptr);
                                   refreshClips();
                               }
                               return;
                           }

                           if (selected == 106)
                           {
                               const bool looping = static_cast<bool>(clip->state.getProperty("looping", false));
                               clip->state.setProperty("looping", !looping, nullptr);
                               refreshClips();
                               return;
                           }

                           if (selected == 107)
                           {
                               // Quantize start to nearest bar (4 beats)
                               auto pos = clip->getPosition();
                               const auto startSec = pos.getStart().inSeconds();
                               const auto* tempo = clip->edit.tempoSequence.getTempo(0);
                               const double bpm = tempo != nullptr ? tempo->getBpm() : 120.0;
                               const double secPerBeat = 60.0 / juce::jmax(1.0, bpm);
                               const double beatPos = startSec / secPerBeat;
                               const double snapped = std::round(beatPos / 4.0) * 4.0;
                                                             clip->setPosition(te::ClipPosition { { tracktion::TimePosition::fromSeconds(snapped * secPerBeat),
                                                                                                                                             pos.getLength() },
                                                                                                                                         pos.getOffset() });
                               refreshClips();
                               return;
                           }

                           if (selected == 108)
                           {
                               juce::AlertWindow transposeWindow("Transpose Clip", "Semitones (+/-):", juce::AlertWindow::NoIcon);
                               transposeWindow.addTextEditor("semi", "0", "Semitones");
                               transposeWindow.addButton("Apply", 1);
                               transposeWindow.addButton("Cancel", 0);
                               if (transposeWindow.runModalLoop() == 1)
                               {
                                   const int semi = juce::jlimit(-48, 48, transposeWindow.getTextEditorContents("semi").trim().getIntValue());
                                   if (auto* midiClip = dynamic_cast<te::MidiClip*>(clip))
                                   {
                                       for (auto* note : midiClip->getSequence().getNotes())
                                           note->setNoteNumber(note->getNoteNumber() + semi, nullptr);
                                       refreshClips();
                                   }
                               }
                               return;
                           }

                           if (selected == 109)
                           {
                               clip->state.removeProperty(juce::Identifier("progressionId"), nullptr);
                               refreshClips();
                           }
                       });
}

void TrackLane::rebuildPaintedClips(const juce::Rectangle<int>& clipArea)
{
    paintedClips.clear();

    const auto* tempo = trackRef.edit.tempoSequence.getTempo(0);
    const auto bpm = tempo != nullptr ? tempo->getBpm() : 120.0;
    const auto secPerBeat = 60.0 / juce::jmax(1.0, bpm);
    const auto beatSpan = juce::jmax(1.0, visibleEndBeat - visibleStartBeat);

    auto* clipTrack = dynamic_cast<te::ClipTrack*>(&trackRef);
    if (clipTrack == nullptr)
        return;

    for (auto* clip : clipTrack->getClips())
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
