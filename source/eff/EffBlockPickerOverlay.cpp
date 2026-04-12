#include "EffBlockPickerOverlay.h"

namespace setle::eff
{

EffBlockPickerOverlay::EffBlockPickerOverlay()
{
    midiEntries = {
        { BlockType::MidiProbability,  "Probability"  },
        { BlockType::MidiVelocityCurve,"Velocity Curve"},
        { BlockType::MidiHumanize,     "Humanize"     },
        { BlockType::MidiQuantize,     "Quantize"     },
        { BlockType::MidiTranspose,    "Transpose"    },
        { BlockType::MidiArpeggiate,   "Arpeggiate"   },
        { BlockType::MidiBeatRepeat,   "Beat Repeat"  },
        { BlockType::MidiStrumSpread,  "Strum Spread" },
        { BlockType::MidiNoteFilter,   "Note Filter"  },
        { BlockType::MidiChordVoicing, "Chord Voicing"},
    };

    audioEntries = {
        { BlockType::AudioGain,        "Gain"         },
        { BlockType::AudioFilter,      "Filter"       },
        { BlockType::AudioSaturator,   "Saturator"    },
        { BlockType::AudioBitCrusher,  "Bit Crusher"  },
        { BlockType::AudioDelay,       "Delay"        },
        { BlockType::AudioReverb,      "Reverb"       },
        { BlockType::AudioChorus,      "Chorus"       },
        { BlockType::AudioCompressor,  "Compressor"   },
        { BlockType::AudioTremolo,     "Tremolo"      },
        { BlockType::AudioStereoWidth, "Stereo Width" },
        { BlockType::AudioDryWet,      "Dry/Wet"      },
    };

    setInterceptsMouseClicks(true, true);
}

void EffBlockPickerOverlay::resized()
{
    // Panel centred, wide enough for two columns
    const int panelW = 360;
    const int rowH   = 26;
    const int maxRows= static_cast<int>(std::max(midiEntries.size(), audioEntries.size())) + 2; // +header +padding
    const int panelH = rowH * maxRows + 24;

    panelBounds = getLocalBounds()
                      .withSizeKeepingCentre(panelW, panelH);
}

void EffBlockPickerOverlay::paint(juce::Graphics& g)
{
    // Dim background
    g.fillAll(juce::Colours::black.withAlpha(0.55f));

    // Panel background
    g.setColour(juce::Colour(0xff1e2838));
    g.fillRoundedRectangle(panelBounds.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff4a6080));
    g.drawRoundedRectangle(panelBounds.toFloat().reduced(0.5f), 6.0f, 1.0f);

    const int colW = panelBounds.getWidth() / 2;
    const int rowH = 26;
    const int headerH = 22;

    auto drawColumn = [&](int colX, const juce::String& title,
                          const std::vector<Entry>& entries, bool isMidi)
    {
        const auto colColour  = isMidi ? juce::Colour(0xff3a7bc8) : juce::Colour(0xff38b890);
        int y = panelBounds.getY() + 8;

        // Header
        g.setColour(colColour);
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(title, colX + 8, y, colW - 10, headerH,
                   juce::Justification::centredLeft, true);
        y += headerH;

        g.setFont(juce::FontOptions(13.0f));

        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        {
            const auto rowRect = juce::Rectangle<int>(colX + 4, y, colW - 8, rowH - 2);

            // Hover highlight handled via mouse position
            const auto mousePos = getMouseXYRelative();
            const bool hovered  = rowRect.contains(mousePos);

            if (hovered)
            {
                g.setColour(colColour.withAlpha(0.25f));
                g.fillRoundedRectangle(rowRect.toFloat(), 3.0f);
            }

            g.setColour(hovered ? colColour.brighter(0.4f)
                                : juce::Colours::white.withAlpha(0.85f));
            g.drawText(entries[static_cast<size_t>(i)].label,
                       rowRect.withLeft(rowRect.getX() + 6),
                       juce::Justification::centredLeft, true);
            y += rowH;
        }
    };

    drawColumn(panelBounds.getX(),            "MIDI Effects",  midiEntries,  true);
    drawColumn(panelBounds.getX() + colW,     "Audio Effects", audioEntries, false);

    // Divider
    g.setColour(juce::Colour(0xff4a6080));
    g.drawLine(static_cast<float>(panelBounds.getX() + colW),
               static_cast<float>(panelBounds.getY() + 4),
               static_cast<float>(panelBounds.getX() + colW),
               static_cast<float>(panelBounds.getBottom() - 4),
               1.0f);
}

void EffBlockPickerOverlay::mouseDown(const juce::MouseEvent& e)
{
    if (!panelBounds.contains(e.getPosition()))
    {
        // Click outside panel → dismiss
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
        return;
    }

    const int colW   = panelBounds.getWidth() / 2;
    const int rowH   = 26;
    const int headerH= 22;

    auto hitTest = [&](int colX, const std::vector<Entry>& entries) -> int
    {
        int y = panelBounds.getY() + 8 + headerH;
        for (int i = 0; i < static_cast<int>(entries.size()); ++i)
        {
            const auto rowRect = juce::Rectangle<int>(colX + 4, y, colW - 8, rowH - 2);
            if (rowRect.contains(e.getPosition()))
                return i;
            y += rowH;
        }
        return -1;
    };

    const int midiHit  = hitTest(panelBounds.getX(),        midiEntries);
    const int audioHit = hitTest(panelBounds.getX() + colW, audioEntries);

    if (midiHit >= 0)
    {
        const auto type = midiEntries[static_cast<size_t>(midiHit)].type;
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
        if (onBlockChosen)
            onBlockChosen(type);
    }
    else if (audioHit >= 0)
    {
        const auto type = audioEntries[static_cast<size_t>(audioHit)].type;
        if (auto* parent = getParentComponent())
            parent->removeChildComponent(this);
        if (onBlockChosen)
            onBlockChosen(type);
    }
}

} // namespace setle::eff
