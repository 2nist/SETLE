#include "WorkspaceShellComponent.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <set>

#include "../theory/ChordIdentifier.h"
#include "../theory/DiatonicHarmony.h"

namespace te = tracktion::engine;

namespace setle::ui
{

namespace
{
enum TheoryActionId
{
    sectionEditTheory = 101,
    sectionSetRepeatPattern = 102,
    sectionAddTransitionAnchor = 103,
    sectionConflictCheck = 104,

    chordEdit = 201,
    chordSubstitution = 202,
    chordSetFunction = 203,
    chordScopeOccurrence = 204,
    chordScopeRepeat = 205,
    chordScopeSelectedRepeats = 206,
    chordScopeAllRepeats = 207,
    chordSnapWholeBar = 208,
    chordSnapHalfBar = 209,
    chordSnapBeat = 210,
    chordSnapHalfBeat = 211,
    chordSnapEighth = 212,
    chordSnapSixteenth = 213,
    chordSnapThirtySecond = 214,

    noteEditTheory = 301,
    noteQuantizeScale = 302,
    noteConvertToChord = 303,
    noteDeriveProgression = 304,

    progressionGrabSampler = 401,
    progressionCreateCandidate = 402,
    progressionAnnotateKeyMode = 403,
    progressionTagTransition = 404
};

juce::String chordRootNameForMidi(int midiNote)
{
    static constexpr std::array<const char*, 12> names { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    const auto pitchClass = ((midiNote % 12) + 12) % 12;
    return names[static_cast<size_t>(pitchClass)];
}

int quantizeToCMajor(int midiNote)
{
    static constexpr std::array<int, 7> scale { 0, 2, 4, 5, 7, 9, 11 };

    const auto octave = midiNote / 12;
    const auto pitchClass = ((midiNote % 12) + 12) % 12;

    auto nearest = scale[0];
    auto bestDistance = std::abs(pitchClass - nearest);

    for (const auto note : scale)
    {
        const auto deltaFromScale = std::abs(pitchClass - note);
        if (deltaFromScale < bestDistance)
        {
            bestDistance = deltaFromScale;
            nearest = note;
        }
    }

    return juce::jlimit(0, 127, octave * 12 + nearest);
}

juce::String nextSectionName(const juce::String& current)
{
    static constexpr std::array<const char*, 5> names { "Verse", "Pre-Chorus", "Chorus", "Bridge", "Outro" };

    for (size_t i = 0; i < names.size(); ++i)
        if (current.equalsIgnoreCase(names[i]))
            return names[(i + 1) % names.size()];

    return names[0];
}

juce::String nextFunction(const juce::String& current)
{
    if (current.equalsIgnoreCase("T")) return "PD";
    if (current.equalsIgnoreCase("PD")) return "D";
    if (current.equalsIgnoreCase("D")) return "T";
    return "T";
}

juce::String cycleChordSymbol(const juce::String& current)
{
    static constexpr std::array<const char*, 6> symbols { "Cmaj7", "Am7", "Dm7", "G7", "Fmaj7", "Em7b5" };

    for (size_t i = 0; i < symbols.size(); ++i)
        if (current.equalsIgnoreCase(symbols[i]))
            return symbols[(i + 1) % symbols.size()];

    return symbols[0];
}

int parseIntOr(const juce::String& value, int fallback)
{
    const auto trimmed = value.trim();
    if (trimmed.isEmpty())
        return fallback;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtol(utf8, &end, 10);

    if (utf8 == end || *end != '\0' || errno == ERANGE)
        return fallback;

    return static_cast<int>(parsed);
}

double parseDoubleOr(const juce::String& value, double fallback)
{
    const auto trimmed = value.trim();
    if (trimmed.isEmpty())
        return fallback;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    const auto parsed = std::strtod(utf8, &end);

    if (utf8 == end || *end != '\0' || errno == ERANGE)
        return fallback;

    return parsed;
}

float parseFloatOr(const juce::String& value, float fallback)
{
    return static_cast<float>(parseDoubleOr(value, static_cast<double>(fallback)));
}

bool isStrictInt(const juce::String& value)
{
    const auto trimmed = value.trim();
    if (trimmed.isEmpty())
        return false;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    std::strtol(utf8, &end, 10);
    return (utf8 != end && *end == '\0' && errno != ERANGE);
}

bool isStrictDouble(const juce::String& value)
{
    const auto trimmed = value.trim();
    if (trimmed.isEmpty())
        return false;

    const auto utf8 = trimmed.toRawUTF8();
    char* end = nullptr;
    errno = 0;
    std::strtod(utf8, &end);
    return (utf8 != end && *end == '\0' && errno != ERANGE);
}

std::set<int> parseRepeatIndexSet(const juce::String& serialized)
{
    std::set<int> result;
    const auto tokens = juce::StringArray::fromTokens(serialized, ",", "");

    for (auto token : tokens)
    {
        token = token.trim();
        if (token.isEmpty())
            continue;

        const auto dashIndex = token.indexOfChar('-');
        if (dashIndex <= 0 || dashIndex >= token.length() - 1)
        {
            const auto value = token.getIntValue();
            if (value > 0)
                result.insert(value);
            continue;
        }

        const auto left = token.substring(0, dashIndex).trim().getIntValue();
        const auto right = token.substring(dashIndex + 1).trim().getIntValue();
        if (left <= 0 || right <= 0)
            continue;

        const auto start = juce::jmin(left, right);
        const auto end = juce::jmax(left, right);
        for (int value = start; value <= end; ++value)
            result.insert(value);
    }

    return result;
}

juce::String serializeRepeatIndexSet(const std::set<int>& indices)
{
    if (indices.empty())
        return {};

    juce::StringArray parts;

    auto it = indices.begin();
    while (it != indices.end())
    {
        const auto rangeStart = *it;
        auto rangeEnd = rangeStart;
        ++it;

        while (it != indices.end() && *it == rangeEnd + 1)
        {
            rangeEnd = *it;
            ++it;
        }

        if (rangeStart == rangeEnd)
            parts.add(juce::String(rangeStart));
        else
            parts.add(juce::String(rangeStart) + "-" + juce::String(rangeEnd));
    }

    return parts.joinIntoString(",");
}

juce::String normalizeRepeatIndices(const juce::String& serialized, const juce::String& fallback)
{
    auto parsed = parseRepeatIndexSet(serialized);
    if (parsed.empty())
        parsed = parseRepeatIndexSet(fallback);

    if (parsed.empty())
        parsed.insert(1);

    return serializeRepeatIndexSet(parsed);
}

std::vector<int> intervalsForQuality(const juce::String& quality)
{
    const auto q = quality.trim().toLowerCase();
    if (q == "major") return { 0, 4, 7 };
    if (q == "minor") return { 0, 3, 7 };
    if (q == "diminished") return { 0, 3, 6 };
    if (q == "augmented") return { 0, 4, 8 };
    if (q == "major7") return { 0, 4, 7, 11 };
    if (q == "minor7") return { 0, 3, 7, 10 };
    if (q == "dominant7") return { 0, 4, 7, 10 };
    if (q == "halfdiminished") return { 0, 3, 6, 10 };
    if (q == "sus2") return { 0, 2, 7 };
    if (q == "sus4") return { 0, 5, 7 };
    return { 0, 4, 7 };
}

void populateChordNotesFromQuality(model::Chord& chord, double startBeats, double durationBeats)
{
    auto chordTree = chord.valueTree();
    for (int i = chordTree.getNumChildren() - 1; i >= 0; --i)
        if (chordTree.getChild(i).hasType(model::Schema::noteType))
            chordTree.removeChild(i, nullptr);

    const auto intervals = intervalsForQuality(chord.getQuality());
    const auto root = chord.getRootMidi();
    for (const auto interval : intervals)
    {
        chord.addNote(model::Note::create(
            juce::jlimit(0, 127, root + interval),
            0.78f,
            startBeats,
            durationBeats,
            1));
    }
}
} // namespace

//==============================================================================
class WorkspaceShellComponent::InDevicePanel final : public juce::Component,
                                                     private juce::Timer,
                                                     private juce::MidiInputCallback
{
public:
    explicit InDevicePanel(te::Engine& e) : engine(e)
    {
        openMidiInputs();
        startTimerHz(4);
    }

    ~InDevicePanel() override
    {
        stopTimer();
        for (auto& m : midiInputs)
            m->stop();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds();
        g.setColour(juce::Colour(0xff22352a));
        g.fillRoundedRectangle(bounds.toFloat().reduced(1.0f), 5.0f);

        auto area = bounds.reduced(10, 8);

        // Title
        g.setFont(juce::FontOptions(16.0f));
        g.setColour(juce::Colours::white.withAlpha(0.88f));
        g.drawText("IN", area.removeFromTop(22), juce::Justification::centredLeft, false);

        area.removeFromTop(4);

        // ── Audio Output ──────────────────────────────────────────────────────
        drawSectionLabel(g, area, "Audio Output");
        {
            auto& dm = engine.getDeviceManager().deviceManager;
            auto* dev = dm.getCurrentAudioDevice();
            if (dev != nullptr)
            {
                const auto sr = static_cast<int>(dev->getCurrentSampleRate());
                const auto bs = dev->getCurrentBufferSizeSamples();
                const auto devName = dev->getName();
                g.setColour(juce::Colours::white.withAlpha(0.80f));
                g.setFont(juce::FontOptions(12.0f));
                g.drawText(devName, area.removeFromTop(16), juce::Justification::centredLeft, true);
                g.setColour(juce::Colours::white.withAlpha(0.50f));
                g.setFont(juce::FontOptions(11.0f));
                g.drawText(juce::String(sr) + " Hz  |  " + juce::String(bs) + " samples",
                           area.removeFromTop(15), juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour(juce::Colours::white.withAlpha(0.40f));
                g.setFont(juce::FontOptions(12.0f));
                g.drawText("(no audio device open)", area.removeFromTop(16), juce::Justification::centredLeft, true);
            }
        }
        area.removeFromTop(8);

        // ── MIDI Inputs ───────────────────────────────────────────────────────
        drawSectionLabel(g, area, "MIDI Inputs");
        {
            const auto midiDevices = juce::MidiInput::getAvailableDevices();
            if (midiDevices.isEmpty())
            {
                g.setColour(juce::Colours::white.withAlpha(0.40f));
                g.setFont(juce::FontOptions(12.0f));
                g.drawText("(no MIDI inputs found)", area.removeFromTop(15), juce::Justification::centredLeft, true);
            }
            else
            {
                for (const auto& info : midiDevices)
                {
                    if (area.getHeight() < 14)
                        break;
                    const bool isOpen = isDeviceOpen(info.identifier);
                    g.setColour(isOpen ? juce::Colour(0xff6ac87b) : juce::Colours::white.withAlpha(0.45f));
                    g.setFont(juce::FontOptions(12.0f));
                    const auto dot = isOpen ? juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f "))
                                            : juce::String(juce::CharPointer_UTF8("\xe2\x97\x8b "));
                    g.drawText(dot + info.name, area.removeFromTop(15), juce::Justification::centredLeft, true);
                }
            }
        }
        area.removeFromTop(8);

        // ── MIDI Monitor ──────────────────────────────────────────────────────
        if (area.getHeight() < 36)
            return;

        drawSectionLabel(g, area, "MIDI Monitor");
        {
            juce::Array<juce::MidiMessage> snapshot;
            {
                const juce::ScopedLock sl(logLock);
                for (const auto& m : midiLog)
                    snapshot.add(m);
            }

            g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
            // Draw newest at top
            for (int i = snapshot.size() - 1; i >= 0 && area.getHeight() >= 13; --i)
            {
                const auto& msg = snapshot.getReference(i);
                juce::String text;
                if (msg.isNoteOn())
                    text = "NoteOn  ch" + juce::String(msg.getChannel()) + "  n" + juce::String(msg.getNoteNumber()) + "  v" + juce::String(msg.getVelocity());
                else if (msg.isNoteOff())
                    text = "NoteOff ch" + juce::String(msg.getChannel()) + "  n" + juce::String(msg.getNoteNumber());
                else if (msg.isController())
                    text = "CC  ch" + juce::String(msg.getChannel()) + "  #" + juce::String(msg.getControllerNumber()) + "=" + juce::String(msg.getControllerValue());
                else if (msg.isProgramChange())
                    text = "PC  ch" + juce::String(msg.getChannel()) + "  p" + juce::String(msg.getProgramChangeNumber());
                else if (msg.isPitchWheel())
                    text = "PW  ch" + juce::String(msg.getChannel()) + "  " + juce::String(msg.getPitchWheelValue());
                else
                    text = "0x" + juce::String::toHexString(msg.getRawData(), juce::jmin(3, msg.getRawDataSize()));

                const float alpha = 0.35f + 0.65f * (static_cast<float>(i) / static_cast<float>(juce::jmax(1, snapshot.size() - 1)));
                g.setColour(juce::Colours::white.withAlpha(alpha));
                g.drawText(text, area.removeFromTop(13), juce::Justification::centredLeft, true);
            }
        }
    }

private:
    static void drawSectionLabel(juce::Graphics& g, juce::Rectangle<int>& area, const juce::String& label)
    {
        g.setColour(juce::Colour(0xff4a8a60).withAlpha(0.85f));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(label.toUpperCase(), area.removeFromTop(14), juce::Justification::centredLeft, false);
    }

    void openMidiInputs()
    {
        for (const auto& info : juce::MidiInput::getAvailableDevices())
        {
            if (auto dev = juce::MidiInput::openDevice(info.identifier, this))
            {
                dev->start();
                midiInputs.push_back(std::move(dev));
            }
        }
    }

    bool isDeviceOpen(const juce::String& identifier) const
    {
        for (const auto& m : midiInputs)
            if (m->getIdentifier() == identifier)
                return true;
        return false;
    }

    void timerCallback() override { repaint(); }

    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg) override
    {
        const juce::ScopedLock sl(logLock);
        midiLog.add(msg);
        if (midiLog.size() > 12)
            midiLog.remove(0);
    }

    te::Engine& engine;
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
    juce::CriticalSection logLock;
    juce::Array<juce::MidiMessage> midiLog;
};

//==============================================================================
class WorkspaceShellComponent::LabelPanel : public juce::Component
{
public:
    LabelPanel(juce::String panelName, juce::Colour panelColour, juce::String subtitleText)
        : title(std::move(panelName)),
          subtitle(std::move(subtitleText)),
          colour(panelColour)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.setColour(colour);
        g.fillRoundedRectangle(bounds.toFloat().reduced(1.0f), 5.0f);

        g.setColour(juce::Colours::white.withAlpha(0.88f));
        g.setFont(juce::FontOptions(16.0f));
        g.drawText(title, bounds.removeFromTop(32).reduced(10, 4), juce::Justification::centredLeft, false);

        g.setColour(juce::Colours::white.withAlpha(0.60f));
        g.setFont(juce::FontOptions(13.0f));
        g.drawText(subtitle, bounds.reduced(10, 4), juce::Justification::topLeft, true);
    }

private:
    juce::String title;
    juce::String subtitle;
    juce::Colour colour;
};

class WorkspaceShellComponent::TimelineShell final : public juce::Component
{
public:
    using ActionCallback = std::function<void(TheoryMenuTarget, int, const juce::String&)>;

    struct ChordView
    {
        model::Chord chord;
        juce::String key;
        juce::String mode;
    };

    class ContextLane final : public LabelPanel
    {
    public:
        struct BlockData
        {
            juce::String id;
            juce::String label;
            juce::String secondaryLabel;
            float widthFraction { 1.0f };
            double startTimeSeconds { 0.0 };  // 7C: absolute start time for seek
        };

        using SelectionCallback = std::function<void(const juce::String&)>;
        using SeekCallback = std::function<void(double)>;

        ContextLane(juce::String panelName,
                    juce::Colour panelColour,
                    juce::String subtitleText,
                    TheoryMenuTarget targetType,
                    ActionCallback callback)
            : LabelPanel(panelName, panelColour, subtitleText),
              menuTarget(targetType),
              onAction(std::move(callback))
        {
        }

        void setBlocks(std::vector<BlockData> newBlocks,
                       juce::String currentSelectedId,
                       SelectionCallback cb)
        {
            blocks = std::move(newBlocks);
            selectedId = std::move(currentSelectedId);
            onSelect = std::move(cb);
            repaint();
        }

        void setPlayheadFraction(double fraction)
        {
            playheadFraction = juce::jlimit(0.0, 1.0, fraction);
            repaint();
        }

        void setSeekCallback(SeekCallback cb) { onSeekBlock = std::move(cb); }

        void paint(juce::Graphics& g) override
        {
            if (blocks.empty())
            {
                LabelPanel::paint(g);
                return;
            }

            const auto bounds = getLocalBounds();
            g.fillAll(juce::Colour(0xff1b2535));

            const float totalWidth = static_cast<float>(bounds.getWidth());
            const float h = static_cast<float>(bounds.getHeight());
            float x = 0.0f;

            for (const auto& block : blocks)
            {
                const float w = totalWidth * block.widthFraction;
                const juce::Rectangle<float> blockRect(x + 1.0f, 2.0f, w - 2.0f, h - 4.0f);
                const bool isSelected = (block.id == selectedId);

                g.setColour(isSelected ? juce::Colour(0xff4a7fa5) : juce::Colour(0xff2c3e50));
                g.fillRoundedRectangle(blockRect, 3.0f);

                g.setColour(isSelected ? juce::Colours::white : juce::Colours::white.withAlpha(0.70f));
                if (block.secondaryLabel.isNotEmpty())
                {
                    auto textArea = blockRect.reduced(4.0f, 2.0f).toNearestInt();
                    auto primary = textArea.removeFromTop(textArea.getHeight() / 2 + 1);
                    g.setFont(juce::FontOptions(12.0f));
                    g.drawText(block.label, primary, juce::Justification::centredLeft, true);

                    g.setColour(isSelected ? juce::Colours::white.withAlpha(0.82f)
                                           : juce::Colours::white.withAlpha(0.58f));
                    g.setFont(juce::FontOptions(10.0f));
                    g.drawText(block.secondaryLabel, textArea, juce::Justification::centredLeft, true);
                }
                else
                {
                    g.setFont(juce::FontOptions(12.0f));
                    g.drawText(block.label, blockRect.reduced(4.0f, 0.0f), juce::Justification::centredLeft, true);
                }

                x += w;
            }

            // Draw playhead if fraction is set
            if (playheadFraction > 0.0)
            {
                const float phX = totalWidth * static_cast<float>(playheadFraction);
                g.setColour(juce::Colours::white.withAlpha(0.85f));
                g.drawVerticalLine(static_cast<int>(phX), 0.0f, h);
            }
        }

        void mouseUp(const juce::MouseEvent& event) override
        {
            if (!event.mods.isRightButtonDown())
            {
                if (!blocks.empty() && onSelect != nullptr)
                {
                    const float totalWidth = static_cast<float>(getWidth());
                    float x = 0.0f;
                    const float clickX = static_cast<float>(event.x);
                    for (const auto& block : blocks)
                    {
                        const float w = totalWidth * block.widthFraction;
                        if (clickX >= x && clickX < x + w)
                        {
                            selectedId = block.id;
                            repaint();
                            onSelect(block.id);
                            // Call seek callback if available (for sections-specific seek)
                            if (onSeekBlock != nullptr)
                                onSeekBlock(block.startTimeSeconds);
                            return;
                        }
                        x += w;
                    }
                }
                return;
            }

            juce::PopupMenu menu;
            populateMenu(menu);

            auto options = juce::PopupMenu::Options().withTargetComponent(this);
            menu.showMenuAsync(
                options,
                [safeThis = juce::Component::SafePointer<ContextLane>(this)](int selectedId)
                {
                    if (safeThis == nullptr || selectedId <= 0 || safeThis->onAction == nullptr)
                        return;

                    safeThis->onAction(safeThis->menuTarget, selectedId, safeThis->actionNameFor(selectedId));
                });
        }

    private:
        void populateMenu(juce::PopupMenu& menu) const
        {
            switch (menuTarget)
            {
                case TheoryMenuTarget::section:
                {
                    menu.addItem(sectionEditTheory, "Edit Section Theory...");
                    menu.addItem(sectionSetRepeatPattern, "Set Repeat Pattern...");
                    menu.addItem(sectionAddTransitionAnchor, "Add Transition Anchor...");
                    menu.addSeparator();
                    menu.addItem(sectionConflictCheck, "Run Section Conflict Check");
                    break;
                }

                case TheoryMenuTarget::chord:
                {
                    menu.addItem(chordEdit, "Edit Chord...");
                    menu.addItem(chordSubstitution, "Chord Substitution...");
                    menu.addItem(chordSetFunction, "Set Harmonic Function...");
                    menu.addSeparator();
                    menu.addItem(chordScopeOccurrence, "Apply Scope: This Occurrence");
                    menu.addItem(chordScopeRepeat, "Apply Scope: This Repeat");
                    menu.addItem(chordScopeSelectedRepeats, "Apply Scope: Selected Repeats");
                    menu.addItem(chordScopeAllRepeats, "Apply Scope: All Repeats");
                    menu.addSeparator();
                    menu.addItem(chordSnapWholeBar, "Theory Snap: Whole Bar");
                    menu.addItem(chordSnapHalfBar, "Theory Snap: Half Bar");
                    menu.addItem(chordSnapBeat, "Theory Snap: Beat");
                    menu.addItem(chordSnapHalfBeat, "Theory Snap: Half Beat");
                    menu.addItem(chordSnapEighth, "Theory Snap: 1/8");
                    menu.addItem(chordSnapSixteenth, "Theory Snap: 1/16");
                    menu.addItem(chordSnapThirtySecond, "Theory Snap: 1/32");
                    break;
                }

                case TheoryMenuTarget::note:
                {
                    menu.addItem(noteEditTheory, "Edit Note Theory...");
                    menu.addItem(noteQuantizeScale, "Quantize Note to Scale");
                    menu.addItem(noteConvertToChord, "Convert Notes to Chord");
                    menu.addSeparator();
                    menu.addItem(noteDeriveProgression, "Derive Progression from Selection");
                    break;
                }

                case TheoryMenuTarget::progression:
                {
                    menu.addItem(progressionGrabSampler, "Grab to Sampler Queue");
                    menu.addItem(progressionCreateCandidate, "Create Progression Candidate");
                    menu.addItem(progressionAnnotateKeyMode, "Annotate Key / Mode");
                    menu.addSeparator();
                    menu.addItem(progressionTagTransition, "Tag as Transition Material");
                    break;
                }
            }
        }

        juce::String actionNameFor(int id) const
        {
            switch (menuTarget)
            {
                case TheoryMenuTarget::section:
                {
                    if (id == sectionEditTheory) return "Edit Section Theory";
                    if (id == sectionSetRepeatPattern) return "Set Repeat Pattern";
                    if (id == sectionAddTransitionAnchor) return "Add Transition Anchor";
                    if (id == sectionConflictCheck) return "Run Section Conflict Check";
                    break;
                }

                case TheoryMenuTarget::chord:
                {
                    if (id == chordEdit) return "Edit Chord";
                    if (id == chordSubstitution) return "Chord Substitution";
                    if (id == chordSetFunction) return "Set Harmonic Function";
                    if (id == chordScopeOccurrence) return "Apply Scope This Occurrence";
                    if (id == chordScopeRepeat) return "Apply Scope This Repeat";
                    if (id == chordScopeSelectedRepeats) return "Apply Scope Selected Repeats";
                    if (id == chordScopeAllRepeats) return "Apply Scope All Repeats";
                    if (id == chordSnapWholeBar) return "Set Theory Snap Whole Bar";
                    if (id == chordSnapHalfBar) return "Set Theory Snap Half Bar";
                    if (id == chordSnapBeat) return "Set Theory Snap Beat";
                    if (id == chordSnapHalfBeat) return "Set Theory Snap Half Beat";
                    if (id == chordSnapEighth) return "Set Theory Snap 1/8";
                    if (id == chordSnapSixteenth) return "Set Theory Snap 1/16";
                    if (id == chordSnapThirtySecond) return "Set Theory Snap 1/32";
                    break;
                }

                case TheoryMenuTarget::note:
                {
                    if (id == noteEditTheory) return "Edit Note Theory";
                    if (id == noteQuantizeScale) return "Quantize Note to Scale";
                    if (id == noteConvertToChord) return "Convert Notes to Chord";
                    if (id == noteDeriveProgression) return "Derive Progression from Selection";
                    break;
                }

                case TheoryMenuTarget::progression:
                {
                    if (id == progressionGrabSampler) return "Grab to Sampler Queue";
                    if (id == progressionCreateCandidate) return "Create Progression Candidate";
                    if (id == progressionAnnotateKeyMode) return "Annotate Key / Mode";
                    if (id == progressionTagTransition) return "Tag as Transition Material";
                    break;
                }
            }

            return "Unknown Action";
        }

        TheoryMenuTarget menuTarget;
        ActionCallback onAction;
        std::vector<BlockData> blocks;
        juce::String selectedId;
        SelectionCallback onSelect;
        SeekCallback onSeekBlock;
        double playheadFraction { 0.0 };
    };

    void updateSections(const std::vector<model::Section>& sections,
                        const std::vector<model::Progression>& progressions,
                        const juce::String& currentSectionId,
                        ContextLane::SelectionCallback cb)
    {
        if (sections.empty())
        {
            sectionLane.setBlocks({}, {}, nullptr);
            return;
        }

        // Compute beat length per section (repeatCount × progression length)
        struct SectionEntry { juce::String id; juce::String name; double beats; };
        std::vector<SectionEntry> entries;
        double totalBeats = 0.0;

        for (const auto& section : sections)
        {
            double sectionBeats = 0.0;
            for (const auto& ref : section.getProgressionRefs())
            {
                for (const auto& prog : progressions)
                {
                    if (prog.getId() == ref.getProgressionId())
                    {
                        double len = prog.getLengthBeats();
                        if (len <= 0.0) len = 8.0;
                        sectionBeats += len;
                        break;
                    }
                }
            }
            if (sectionBeats <= 0.0) sectionBeats = 8.0;
            const double withRepeats = sectionBeats * juce::jmax(1, section.getRepeatCount());
            totalBeats += withRepeats;
            entries.push_back({ section.getId(), section.getName(), withRepeats });
        }

        std::vector<ContextLane::BlockData> blockData;
        double cumulativeTime = 0.0;
        double bpm = 120.0;  // Default BPM - will be overridden by callback context
        double secPerBeat = 60.0 / bpm;

        for (const auto& e : entries)
        {
            blockData.push_back({
                e.id,
                e.name,
                {},
                static_cast<float>(e.beats / totalBeats),
                cumulativeTime  // 7C: absolute start time for seek
            });
            cumulativeTime += e.beats * secPerBeat;
        }

        sectionLane.setBlocks(std::move(blockData), currentSectionId, std::move(cb));
    }

    void updateChords(const std::vector<ChordView>& chords,
                      const juce::String& currentChordId,
                      ContextLane::SelectionCallback cb)
    {
        if (chords.empty())
        {
            theoryLane.setBlocks({}, {}, nullptr);
            return;
        }

        double totalDuration = 0.0;
        for (const auto& chordView : chords)
        {
            const auto& chord = chordView.chord;
            double d = chord.getDurationBeats();
            totalDuration += (d > 0.0 ? d : 1.0);
        }

        std::vector<ContextLane::BlockData> blockData;
        for (const auto& chordView : chords)
        {
            const auto& chord = chordView.chord;
            double d = chord.getDurationBeats();
            if (d <= 0.0) d = 1.0;

            juce::String roman = chord.getSymbol();
            const auto progressionKey = chordView.key;
            const auto progressionMode = chordView.mode;

            if (progressionKey.isNotEmpty() && progressionMode.isNotEmpty())
            {
                const int keyPc = DiatonicHarmony::pitchClassForRoot(progressionKey);
                const int chordPc = ((chord.getRootMidi() % 12) + 12) % 12;
                const int interval = (chordPc - keyPc + 12) % 12;

                const auto intervals = DiatonicHarmony::modeIntervals(progressionMode);
                const auto romans = DiatonicHarmony::modeRomanNumerals(progressionMode);
                for (int i = 0; i < 7; ++i)
                {
                    if (intervals[static_cast<size_t>(i)] != interval)
                        continue;

                    roman = romans[static_cast<size_t>(i)];
                    if (chord.getQuality().containsIgnoreCase("7"))
                        roman += "7";
                    break;
                }
            }

            juce::String primary = roman;
            if (chord.getFunction().isNotEmpty())
                primary += " [" + chord.getFunction() + "]";

            blockData.push_back({ chord.getId(), primary, chord.getSymbol(), static_cast<float>(d / totalDuration) });
        }

        theoryLane.setBlocks(std::move(blockData), currentChordId, std::move(cb));
    }

    void setPlayheadFraction(double fraction)
    {
        playheadFraction = juce::jlimit(0.0, 1.0, fraction);
        sectionLane.setPlayheadFraction(playheadFraction);
        theoryLane.setPlayheadFraction(playheadFraction);
    }

    void setSectionSeekCallback(ContextLane::SeekCallback cb)
    {
        sectionLane.setSeekCallback(std::move(cb));
    }

    std::function<void(double)> onSeek;

    explicit TimelineShell(ActionCallback callback)
        : sectionLane(
              "Section Lane",
              juce::Colour(0xff28323f),
              "Container and section timing alignment (right-click for section theory tools)",
              TheoryMenuTarget::section,
              callback),
          theoryLane(
              "Theory Strip",
              juce::Colour(0xff203043),
              "Chord/function overlays aligned to timeline (right-click for chord tools)",
              TheoryMenuTarget::chord,
              callback),
          trackArea(
              "Main Timeline / Tracks",
              juce::Colour(0xff1d2430),
              "Primary arrangement surface (right-click for note/progression tools)",
              TheoryMenuTarget::note,
              callback),
          historyLane(
              "History Buffer Lane",
              juce::Colour(0xff232338),
              "Always-on capture lane (right-click for progression capture actions)",
              TheoryMenuTarget::progression,
              callback)
    {
        addAndMakeVisible(sectionLane);
        addAndMakeVisible(theoryLane);
        addAndMakeVisible(trackArea);
        addAndMakeVisible(historyLane);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(0, 1);

        sectionLane.setBounds(bounds.removeFromTop(sectionHeight));
        theoryLane.setBounds(bounds.removeFromTop(theoryHeight));
        historyLane.setBounds(bounds.removeFromBottom(historyHeight));
        trackArea.setBounds(bounds);
    }

private:
    static constexpr int sectionHeight = 38;
    static constexpr int theoryHeight = 38;
    static constexpr int historyHeight = 60;

    double playheadFraction { 0.0 };

    ContextLane sectionLane;
    ContextLane theoryLane;
    ContextLane trackArea;
    ContextLane historyLane;
};

class WorkspaceShellComponent::DragBar final : public juce::Component
{
public:
    enum class Orientation
    {
        vertical,
        horizontal
    };

    DragBar(Orientation barOrientation,
            std::function<void(int)> dragDeltaHandler,
            std::function<void()> dragEndHandler)
        : orientation(barOrientation),
          onDragDelta(std::move(dragDeltaHandler)),
          onDragEnd(std::move(dragEndHandler))
    {
        setMouseCursor(orientation == Orientation::vertical
                           ? juce::MouseCursor::LeftRightResizeCursor
                           : juce::MouseCursor::UpDownResizeCursor);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff141414));
        g.setColour(juce::Colours::white.withAlpha(0.20f));

        if (orientation == Orientation::vertical)
            g.drawVerticalLine(getWidth() / 2, 2.0f, static_cast<float>(getHeight() - 2));
        else
            g.drawHorizontalLine(getHeight() / 2, 2.0f, static_cast<float>(getWidth() - 2));
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        lastMousePosition = orientation == Orientation::vertical ? event.getScreenX() : event.getScreenY();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        const auto currentPosition = orientation == Orientation::vertical ? event.getScreenX() : event.getScreenY();
        const auto delta = currentPosition - lastMousePosition;
        lastMousePosition = currentPosition;

        if (delta != 0 && onDragDelta != nullptr)
            onDragDelta(delta);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (onDragEnd != nullptr)
            onDragEnd();
    }

private:
    Orientation orientation;
    std::function<void(int)> onDragDelta;
    std::function<void()> onDragEnd;
    int lastMousePosition = 0;
};

WorkspaceShellComponent::WorkspaceShellComponent(te::Engine& engine)
    : engineRef(engine)
{
    setWantsKeyboardFocus(true);

    auto topStripLabelFont = juce::FontOptions(15.0f);
    topTitle.setText("SETLE - IN > WORK > OUT", juce::dontSendNotification);
    topTitle.setFont(topStripLabelFont);
    topTitle.setJustificationType(juce::Justification::centredLeft);
    topTitle.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.88f));

    interactionStatus.setText("Right-click timeline lanes to access theory actions", juce::dontSendNotification);
    interactionStatus.setFont(juce::FontOptions(13.0f));
    interactionStatus.setJustificationType(juce::Justification::centredLeft);
    interactionStatus.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.72f));

    topStrip.addAndMakeVisible(topTitle);
    topStrip.addAndMakeVisible(interactionStatus);
    topStrip.addAndMakeVisible(playButton);
    topStrip.addAndMakeVisible(stopButton);
    topStrip.addAndMakeVisible(recordButton);
    topStrip.addAndMakeVisible(bpmLabel);
    topStrip.addAndMakeVisible(bpmEditor);
    topStrip.addAndMakeVisible(focusInButton);
    topStrip.addAndMakeVisible(focusBalancedButton);
    topStrip.addAndMakeVisible(focusOutButton);
    topStrip.addAndMakeVisible(undoTheoryButton);
    topStrip.addAndMakeVisible(redoTheoryButton);
    addAndMakeVisible(topStrip);

    inPanel = new InDevicePanel(engineRef);
    addAndMakeVisible(inPanel);

    workPanel = new LabelPanel(
        "WORK",
        juce::Colour(0xff242835),
        "Active editor host:\npiano roll, sequencer, notation,\nor focused plugin UI");
    addAndMakeVisible(workPanel);
    configureTheoryEditorPanel();

    outPanel = new LabelPanel(
        "OUT",
        juce::Colour(0xff2d2431),
        "Mixer, FX chain, routing,\nexport and output diagnostics");
    addAndMakeVisible(outPanel);

    timelineShell = new TimelineShell(
        [this](TheoryMenuTarget target, int actionId, const juce::String& actionName)
        {
            handleTimelineTheoryAction(target, actionId, actionName);
        });
    addAndMakeVisible(timelineShell);

    // 7C: Wire seek callback for section clicks
    timelineShell->onSeek = [this](double seekSeconds)
    {
        if (edit != nullptr)
            edit->getTransport().setPosition(tracktion::TimePosition::fromSeconds(seekSeconds));
    };

    leftResizeBar = new DragBar(
        DragBar::Orientation::vertical,
        [this](int delta)
        {
            leftPanelWidth += delta;
            focusMode = FocusMode::balanced;
            resized();
        },
        [this] { saveLayoutState(); });
    addAndMakeVisible(leftResizeBar);

    rightResizeBar = new DragBar(
        DragBar::Orientation::vertical,
        [this](int delta)
        {
            rightPanelWidth -= delta;
            focusMode = FocusMode::balanced;
            resized();
        },
        [this] { saveLayoutState(); });
    addAndMakeVisible(rightResizeBar);

    timelineResizeBar = new DragBar(
        DragBar::Orientation::horizontal,
        [this](int delta)
        {
            timelineHeight -= delta;
            resized();
        },
        [this] { saveLayoutState(); });
    addAndMakeVisible(timelineResizeBar);

    focusInButton.onClick = [this] { applyFocusMode(FocusMode::inFocused); };
    focusBalancedButton.onClick = [this] { applyFocusMode(FocusMode::balanced); };
    focusOutButton.onClick = [this] { applyFocusMode(FocusMode::outFocused); };
    undoTheoryButton.onClick = [this] { performUndo(); };
    redoTheoryButton.onClick = [this] { performRedo(); };

    playButton.onClick = [this]
    {
        if (edit != nullptr)
        {
            loadProgressionToEdit();
            edit->getTransport().play(false);
        }
    };

    stopButton.onClick = [this]
    {
        if (edit != nullptr)
            edit->getTransport().stop(false, false);
    };

    recordButton.onClick = [this]
    {
        if (edit != nullptr)
        {
            loadProgressionToEdit();
            edit->getTransport().record(false);
        }
    };

    bpmLabel.setText("BPM", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.72f));
    bpmLabel.setFont(juce::FontOptions(13.0f));
    bpmEditor.setInputRestrictions(6, "0123456789.");
    bpmEditor.setJustification(juce::Justification::centred);
    bpmEditor.onReturnKey = [this]
    {
        const double bpm = juce::jlimit(20.0, 300.0, bpmEditor.getText().getDoubleValue());
        songState.setBpm(bpm);
        if (edit != nullptr)
        {
            const bool wasPlaying = edit->getTransport().isPlaying();
            if (wasPlaying)
                edit->getTransport().stop(false, false);

            if (auto* tempo = edit->tempoSequence.getTempo(0))
                tempo->setBpm(bpm);

            // Force audio graph rebuild so new tempo takes effect immediately
            edit->getTransport().ensureContextAllocated(true);

            // Reload MIDI clip so beat-to-time mapping reflects the new BPM
            loadProgressionToEdit();

            if (wasPlaying)
                edit->getTransport().play(false);
        }
        bpmEditor.setText(juce::String(bpm, 1), juce::dontSendNotification);
        saveSongState();
    };

    juce::PropertiesFile::Options options;
    options.applicationName = "SETLE";
    options.filenameSuffix = "settings";
    options.folderName = "SETLE";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    appProperties.setStorageParameters(options);

    loadLayoutState();
    initialiseSongState();

    // --- Phase 4/5/6/7: create Edit, init audio graph, enable MIDI out, setup transport UI ---
    edit = te::Edit::createSingleTrackEdit(engineRef);
    if (edit != nullptr)
    {
        if (auto* tempo = edit->tempoSequence.getTempo(0))
            tempo->setBpm(songState.getBpm());

        // Build the audio graph so transport commands produce sound
        edit->getTransport().ensureContextAllocated();

        // Enable the first available MIDI output device
        auto& dm = engineRef.getDeviceManager();
        for (int i = 0; i < dm.getNumMidiOutDevices(); ++i)
        {
            if (auto* midiOut = dm.getMidiOutDevice(i))
            {
                midiOut->setEnabled(true);
                break;
            }
        }
    }
    bpmEditor.setText(juce::String(songState.getBpm(), 1), juce::dontSendNotification);

    // Position display — updated at 30 Hz by timerCallback()
    positionLabel.setText("0:00.000", juce::dontSendNotification);
    positionLabel.setFont(juce::FontOptions(13.0f));
    positionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.80f));
    positionLabel.setJustificationType(juce::Justification::centred);
    topStrip.addAndMakeVisible(positionLabel);

    // Loop button
    loopButton.setClickingTogglesState(true);
    loopButton.setToggleState(appProperties.getUserSettings()->getBoolValue("transport.loop", false), juce::dontSendNotification);
    topStrip.addAndMakeVisible(loopButton);

    loopButton.onClick = [this]
    {
        if (edit != nullptr)
        {
            edit->getTransport().looping = loopButton.getToggleState();
            if (loopButton.getToggleState())
            {
                const double progLen = getProgressionLengthSeconds();
                edit->getTransport().setLoopRange(
                    tracktion::TimeRange(
                        tracktion::TimePosition::fromSeconds(0.0),
                        tracktion::TimePosition::fromSeconds(progLen)));
            }
        }
        if (auto* settings = appProperties.getUserSettings())
        {
            settings->setValue("transport.loop", loopButton.getToggleState());
            settings->saveIfNeeded();
        }
    };

    startTimerHz(30);

    openTheoryEditor(TheoryMenuTarget::section, sectionEditTheory, "Edit Section Theory");
    updateFocusButtonState();
    updateUndoRedoButtonState();
}

WorkspaceShellComponent::~WorkspaceShellComponent()
{
    saveLayoutState();
    saveSongState();
}

bool WorkspaceShellComponent::keyPressed(const juce::KeyPress& key)
{
    const auto modifiers = key.getModifiers();
    const auto commandLike = modifiers.isCommandDown() || modifiers.isCtrlDown();

    if (!commandLike)
        return false;

    if (key.getKeyCode() == 'z' || key.getKeyCode() == 'Z')
    {
        if (modifiers.isShiftDown())
            performRedo();
        else
            performUndo();
        return true;
    }

    if (key.getKeyCode() == 'y' || key.getKeyCode() == 'Y')
    {
        performRedo();
        return true;
    }

    return false;
}

void WorkspaceShellComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121417));
}

void WorkspaceShellComponent::applyFocusMode(FocusMode mode)
{
    focusMode = mode;

    const auto topWidth = juce::jmax(1, getWidth() - (2 * splitterThickness));

    if (focusMode == FocusMode::inFocused)
    {
        leftPanelWidth = static_cast<int>(topWidth * 0.34f);
        rightPanelWidth = static_cast<int>(topWidth * 0.08f);
    }
    else if (focusMode == FocusMode::outFocused)
    {
        leftPanelWidth = static_cast<int>(topWidth * 0.08f);
        rightPanelWidth = static_cast<int>(topWidth * 0.34f);
    }
    else
    {
        leftPanelWidth = static_cast<int>(topWidth * 0.22f);
        rightPanelWidth = static_cast<int>(topWidth * 0.22f);
    }

    updateFocusButtonState();
    resized();
    saveLayoutState();
}

void WorkspaceShellComponent::updateFocusButtonState()
{
    auto setButtonState = [](juce::TextButton& button, bool active)
    {
        button.setColour(juce::TextButton::buttonColourId, active ? juce::Colour(0xff2c4a67) : juce::Colour(0xff303338));
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.9f));
    };

    setButtonState(focusInButton, focusMode == FocusMode::inFocused);
    setButtonState(focusBalancedButton, focusMode == FocusMode::balanced);
    setButtonState(focusOutButton, focusMode == FocusMode::outFocused);
}

void WorkspaceShellComponent::configureTheoryEditorPanel()
{
    if (workPanel == nullptr)
        return;

    workPanel->addAndMakeVisible(theoryEditorPanel);
    theoryEditorPanel.addAndMakeVisible(theoryEditorTitle);
    theoryEditorPanel.addAndMakeVisible(theoryEditorHint);
    theoryEditorPanel.addAndMakeVisible(theoryObjectLabel);
    theoryEditorPanel.addAndMakeVisible(theoryObjectSelector);
    theoryEditorPanel.addAndMakeVisible(progressionTemplateLabel);
    theoryEditorPanel.addAndMakeVisible(progressionTemplateSelector);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel1);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel2);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel3);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel4);
    theoryEditorPanel.addAndMakeVisible(theoryFieldLabel5);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor1);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor2);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor3);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor4);
    theoryEditorPanel.addAndMakeVisible(theoryFieldEditor5);
    theoryEditorPanel.addAndMakeVisible(applyTheoryEditorButton);
    theoryEditorPanel.addAndMakeVisible(reloadTheoryEditorButton);

    theoryEditorTitle.setFont(juce::FontOptions(16.0f));
    theoryEditorTitle.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
    theoryEditorTitle.setJustificationType(juce::Justification::centredLeft);

    theoryEditorHint.setFont(juce::FontOptions(12.5f));
    theoryEditorHint.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.72f));
    theoryEditorHint.setJustificationType(juce::Justification::centredLeft);

    auto configureFieldLabel = [](juce::Label& label)
    {
        label.setFont(juce::FontOptions(12.5f));
        label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.84f));
        label.setJustificationType(juce::Justification::centredLeft);
    };

    configureFieldLabel(theoryObjectLabel);
    configureFieldLabel(progressionTemplateLabel);
    configureFieldLabel(theoryFieldLabel1);
    configureFieldLabel(theoryFieldLabel2);
    configureFieldLabel(theoryFieldLabel3);
    configureFieldLabel(theoryFieldLabel4);
    configureFieldLabel(theoryFieldLabel5);

    auto configureEditor = [](juce::TextEditor& editor)
    {
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1d2229));
        editor.setColour(juce::TextEditor::textColourId, juce::Colours::white.withAlpha(0.96f));
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a414a));
        editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff5b7894));
        editor.setTextToShowWhenEmpty("...", juce::Colours::white.withAlpha(0.35f));
    };

    configureEditor(theoryFieldEditor1);
    configureEditor(theoryFieldEditor2);
    configureEditor(theoryFieldEditor3);
    configureEditor(theoryFieldEditor4);
    configureEditor(theoryFieldEditor5);

    applyTheoryEditorButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3d4f62));
    applyTheoryEditorButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.94f));
    reloadTheoryEditorButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2f3945));
    reloadTheoryEditorButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.94f));

    theoryObjectSelector.onChange = [this]
    {
        updateSelectionFromSelector();
        populateTheoryFieldsForCurrentSelection();
    };

    progressionTemplateLabel.setVisible(false);
    progressionTemplateSelector.setVisible(false);
    progressionTemplateSelector.onChange = [this]
    {
        selectedProgressionTemplateIndex = progressionTemplateSelector.getSelectedId() - 1;

        if (activeEditorTarget != TheoryMenuTarget::progression || activeEditorActionId != progressionCreateCandidate)
            return;

        if (selectedProgressionTemplateIndex < 0
            || selectedProgressionTemplateIndex >= static_cast<int>(DiatonicHarmony::progressionPresets().size()))
            return;

        const auto& preset = DiatonicHarmony::progressionPresets()[static_cast<size_t>(selectedProgressionTemplateIndex)];
        auto progression = getSelectedProgression();
        if (!progression.has_value())
            return;

        auto progressionTree = progression->valueTree();
        for (int i = progressionTree.getNumChildren() - 1; i >= 0; --i)
            if (progressionTree.getChild(i).hasType(model::Schema::chordType))
                progressionTree.removeChild(i, nullptr);

        double beatCursor = 0.0;
        const auto generated = DiatonicHarmony::buildProgressionFromTemplate(progression->getKey(), progression->getMode(), preset);
        for (auto generatedChord : generated)
        {
            if (generatedChord.getDurationBeats() <= 0.0)
                generatedChord.setDurationBeats(4.0);

            generatedChord.setStartBeats(beatCursor);
            generatedChord.setSource("midi");
            populateChordNotesFromQuality(generatedChord, beatCursor, generatedChord.getDurationBeats());
            progression->addChord(generatedChord);

            if (selectedChordId.isEmpty())
                selectedChordId = generatedChord.getId();

            beatCursor += generatedChord.getDurationBeats();
        }

        if (beatCursor > 0.0)
            progression->setLengthBeats(beatCursor);

        saveSongState();
        refreshTimelineData();
        populateTheoryObjectSelector();
        interactionStatus.setText("Template applied: " + preset.name, juce::dontSendNotification);
    };

    applyTheoryEditorButton.onClick = [this] { commitTheoryEditorAction(); };
    reloadTheoryEditorButton.onClick = [this]
    {
        ensureSelectionDefaults();
        populateTheoryObjectSelector();
        populateTheoryFieldsForCurrentSelection();
        interactionStatus.setText("Theory editor reloaded from current selection", juce::dontSendNotification);
    };
}

void WorkspaceShellComponent::openTheoryEditor(TheoryMenuTarget target, int actionId, const juce::String& actionName)
{
    activeEditorTarget = target;
    activeEditorActionId = actionId;

    theoryEditorTitle.setText("Theory Editor - " + actionName, juce::dontSendNotification);
    theoryEditorHint.setText("Non-modal editing in WORK panel; right-click actions target selected objects.", juce::dontSendNotification);

    ensureSelectionDefaults();
    populateTheoryObjectSelector();
    populateTheoryFieldsForCurrentSelection();
}

void WorkspaceShellComponent::populateTheoryObjectSelector()
{
    ensureSelectionDefaults();

    theoryObjectSelector.clear(juce::dontSendNotification);
    theorySelectorObjectIds.clear();

    auto addObject = [this](const juce::String& objectId, const juce::String& label)
    {
        theorySelectorObjectIds.push_back(objectId);
        theoryObjectSelector.addItem(label, static_cast<int>(theorySelectorObjectIds.size()));
    };

    juce::String targetSelectedId;

    if (activeEditorTarget == TheoryMenuTarget::section)
    {
        theoryObjectLabel.setText("Section", juce::dontSendNotification);
        targetSelectedId = selectedSectionId;
        for (const auto& section : songState.getSections())
            addObject(section.getId(), section.getName() + "  [" + section.getId().substring(0, 8) + "]");
    }
    else if (activeEditorTarget == TheoryMenuTarget::chord)
    {
        theoryObjectLabel.setText("Chord", juce::dontSendNotification);
        targetSelectedId = selectedChordId;

        if (auto progression = getSelectedProgression())
            for (const auto& chord : progression->getChords())
                addObject(chord.getId(), chord.getSymbol() + " (" + chord.getFunction() + ")");
    }
    else if (activeEditorTarget == TheoryMenuTarget::note)
    {
        theoryObjectLabel.setText("Note", juce::dontSendNotification);
        targetSelectedId = selectedNoteId;

        if (auto chord = getSelectedChord())
        {
            for (const auto& note : chord->getNotes())
            {
                addObject(note.getId(),
                          "MIDI " + juce::String(note.getPitch())
                              + "  vel " + juce::String(note.getVelocity(), 2));
            }
        }
    }
    else
    {
        theoryObjectLabel.setText("Progression", juce::dontSendNotification);
        targetSelectedId = selectedProgressionId;
        for (const auto& progression : songState.getProgressions())
            addObject(progression.getId(), progression.getName() + " (" + progression.getKey() + " " + progression.getMode() + ")");
    }

    int selectedItemId = 0;
    for (int i = 0; i < static_cast<int>(theorySelectorObjectIds.size()); ++i)
        if (theorySelectorObjectIds[static_cast<size_t>(i)] == targetSelectedId)
            selectedItemId = i + 1;

    if (selectedItemId == 0 && !theorySelectorObjectIds.empty())
        selectedItemId = 1;

    if (selectedItemId > 0)
        theoryObjectSelector.setSelectedId(selectedItemId, juce::dontSendNotification);

    theoryObjectSelector.setEnabled(!theorySelectorObjectIds.empty());
}

void WorkspaceShellComponent::updateSelectionFromSelector()
{
    const auto selectedIndex = theoryObjectSelector.getSelectedId();
    if (selectedIndex <= 0 || selectedIndex > static_cast<int>(theorySelectorObjectIds.size()))
        return;

    const auto selectedId = theorySelectorObjectIds[static_cast<size_t>(selectedIndex - 1)];
    if (activeEditorTarget == TheoryMenuTarget::section)
        selectedSectionId = selectedId;
    else if (activeEditorTarget == TheoryMenuTarget::chord)
        selectedChordId = selectedId;
    else if (activeEditorTarget == TheoryMenuTarget::note)
        selectedNoteId = selectedId;
    else
        selectedProgressionId = selectedId;
}

std::optional<model::Section> WorkspaceShellComponent::getSelectedSection()
{
    ensureSelectionDefaults();
    for (const auto& section : songState.getSections())
        if (section.getId() == selectedSectionId)
            return section;

    return std::nullopt;
}

std::optional<model::Progression> WorkspaceShellComponent::getSelectedProgression()
{
    ensureSelectionDefaults();
    for (const auto& progression : songState.getProgressions())
        if (progression.getId() == selectedProgressionId)
            return progression;

    return std::nullopt;
}

std::optional<model::Chord> WorkspaceShellComponent::getSelectedChord()
{
    ensureSelectionDefaults();
    if (auto progression = getSelectedProgression())
        for (const auto& chord : progression->getChords())
            if (chord.getId() == selectedChordId)
                return chord;

    return std::nullopt;
}

std::optional<model::Note> WorkspaceShellComponent::getSelectedNote()
{
    ensureSelectionDefaults();
    if (auto chord = getSelectedChord())
        for (const auto& note : chord->getNotes())
            if (note.getId() == selectedNoteId)
                return note;

    return std::nullopt;
}

void WorkspaceShellComponent::ensureSelectionDefaults()
{
    auto sections = songState.getSections();
    auto hasSection = [sections](const juce::String& id)
    {
        for (const auto& section : sections)
            if (section.getId() == id)
                return true;
        return false;
    };

    if (sections.empty())
        selectedSectionId.clear();
    else if (!hasSection(selectedSectionId))
        selectedSectionId = sections.front().getId();

    auto progressions = songState.getProgressions();
    auto hasProgression = [progressions](const juce::String& id)
    {
        for (const auto& progression : progressions)
            if (progression.getId() == id)
                return true;
        return false;
    };

    if (progressions.empty())
        selectedProgressionId.clear();
    else if (!hasProgression(selectedProgressionId))
        selectedProgressionId = progressions.front().getId();

    for (const auto& progression : progressions)
    {
        if (progression.getId() != selectedProgressionId)
            continue;

        const auto chords = progression.getChords();
        auto hasChord = [chords](const juce::String& id)
        {
            for (const auto& chord : chords)
                if (chord.getId() == id)
                    return true;
            return false;
        };

        if (chords.empty())
            selectedChordId.clear();
        else if (!hasChord(selectedChordId))
            selectedChordId = chords.front().getId();

        for (const auto& chord : chords)
        {
            if (chord.getId() != selectedChordId)
                continue;

            const auto notes = chord.getNotes();
            auto hasNote = [notes](const juce::String& id)
            {
                for (const auto& note : notes)
                    if (note.getId() == id)
                        return true;
                return false;
            };

            if (notes.empty())
                selectedNoteId.clear();
            else if (!hasNote(selectedNoteId))
                selectedNoteId = notes.front().getId();
            break;
        }

        break;
    }
}

void WorkspaceShellComponent::populateTheoryFieldsForCurrentSelection()
{
    auto hideField = [](juce::Label& label, juce::TextEditor& editor)
    {
        label.setVisible(false);
        editor.setVisible(false);
    };

    auto setField = [](juce::Label& label, juce::TextEditor& editor, const juce::String& title, const juce::String& value)
    {
        label.setVisible(true);
        editor.setVisible(true);
        label.setText(title, juce::dontSendNotification);
        editor.setText(value, juce::dontSendNotification);
    };

    hideField(theoryFieldLabel1, theoryFieldEditor1);
    hideField(theoryFieldLabel2, theoryFieldEditor2);
    hideField(theoryFieldLabel3, theoryFieldEditor3);
    hideField(theoryFieldLabel4, theoryFieldEditor4);
    hideField(theoryFieldLabel5, theoryFieldEditor5);
    progressionTemplateLabel.setVisible(false);
    progressionTemplateSelector.setVisible(false);
    progressionTemplateSelector.clear(juce::dontSendNotification);
    selectedProgressionTemplateIndex = -1;

    applyTheoryEditorButton.setButtonText("Apply Edit");

    if (activeEditorTarget == TheoryMenuTarget::section)
    {
        if (auto section = getSelectedSection())
        {
            if (activeEditorActionId == sectionEditTheory)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Name", section->getName());
                setField(theoryFieldLabel2, theoryFieldEditor2, "Repeat count", juce::String(section->getRepeatCount()));
                setField(theoryFieldLabel3, theoryFieldEditor3, "Energy", "");
                auto progressionRefs = section->getProgressionRefs();
                setField(theoryFieldLabel4, theoryFieldEditor4, "Progression ID",
                         progressionRefs.empty() ? "" : progressionRefs.front().getProgressionId());
                applyTheoryEditorButton.setButtonText("Apply Section Edit");
            }
            else if (activeEditorActionId == sectionSetRepeatPattern)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Repeat Count", juce::String(section->getRepeatCount()));
                applyTheoryEditorButton.setButtonText("Set Repeat Pattern");
            }
            else if (activeEditorActionId == sectionAddTransitionAnchor)
            {
                setField(theoryFieldLabel1,
                         theoryFieldEditor1,
                         "Transition Name",
                         "Anchor " + juce::String(static_cast<int>(songState.getTransitions().size()) + 1));
                setField(theoryFieldLabel2, theoryFieldEditor2, "Strategy", "pushRight");
                setField(theoryFieldLabel3, theoryFieldEditor3, "To Section Name (optional)", "");
                applyTheoryEditorButton.setButtonText("Create Transition Anchor");
            }
        }
    }
    else if (activeEditorTarget == TheoryMenuTarget::chord)
    {
        if (auto chord = getSelectedChord())
        {
            if (activeEditorActionId == chordEdit)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Symbol", chord->getSymbol());
                setField(theoryFieldLabel2, theoryFieldEditor2, "Quality", chord->getQuality());
                setField(theoryFieldLabel3, theoryFieldEditor3, "Root MIDI", juce::String(chord->getRootMidi()));
                setField(theoryFieldLabel4, theoryFieldEditor4, "Duration (beats)", juce::String(chord->getDurationBeats()));
                setField(theoryFieldLabel5, theoryFieldEditor5, "Function", chord->getFunction());
                applyTheoryEditorButton.setButtonText("Apply Chord Edit");
            }
            else if (activeEditorActionId == chordSubstitution)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Profile", "qualityCycle");
                applyTheoryEditorButton.setButtonText("Apply Substitution");
            }
            else if (activeEditorActionId == chordSetFunction)
            {
                setField(theoryFieldLabel1,
                         theoryFieldEditor1,
                         "Function (T/PD/D/custom)",
                         chord->getFunction().isNotEmpty() ? chord->getFunction() : "T");
                applyTheoryEditorButton.setButtonText("Set Harmonic Function");
            }
        }
    }
    else if (activeEditorTarget == TheoryMenuTarget::note)
    {
        if (auto note = getSelectedNote())
        {
            setField(theoryFieldLabel1, theoryFieldEditor1, "Pitch (0-127)", juce::String(note->getPitch()));
            setField(theoryFieldLabel2, theoryFieldEditor2, "Velocity (0-1)", juce::String(note->getVelocity(), 3));
            setField(theoryFieldLabel3, theoryFieldEditor3, "Start Beats", juce::String(note->getStartBeats()));
            setField(theoryFieldLabel4, theoryFieldEditor4, "Duration Beats", juce::String(note->getDurationBeats()));
            applyTheoryEditorButton.setButtonText("Apply Note Edit");
        }
    }
    else
    {
        if (auto progression = getSelectedProgression())
        {
            if (activeEditorActionId == progressionCreateCandidate || activeEditorActionId == progressionGrabSampler)
            {
                setField(theoryFieldLabel1,
                         theoryFieldEditor1,
                         "Name",
                         (activeEditorActionId == progressionGrabSampler ? "Queue Grab " : "Candidate ")
                             + juce::String(static_cast<int>(songState.getProgressions().size()) + 1));
                setField(theoryFieldLabel2, theoryFieldEditor2, "Key", progression->getKey());
                setField(theoryFieldLabel3, theoryFieldEditor3, "Mode", progression->getMode());

                if (activeEditorActionId == progressionCreateCandidate)
                {
                    progressionTemplateLabel.setVisible(true);
                    progressionTemplateSelector.setVisible(true);
                    progressionTemplateLabel.setText("Template", juce::dontSendNotification);

                    int itemId = 1;
                    for (const auto& preset : DiatonicHarmony::progressionPresets())
                        progressionTemplateSelector.addItem(preset.name + " - " + preset.summary, itemId++);

                    if (progressionTemplateSelector.getNumItems() > 0)
                        progressionTemplateSelector.setSelectedId(1, juce::dontSendNotification);

                    selectedProgressionTemplateIndex = progressionTemplateSelector.getSelectedId() - 1;
                }

                applyTheoryEditorButton.setButtonText(activeEditorActionId == progressionGrabSampler
                                                          ? "Create Queue Candidate"
                                                          : "Create Candidate");
            }
            else if (activeEditorActionId == progressionAnnotateKeyMode)
            {
                setField(theoryFieldLabel1, theoryFieldEditor1, "Name", progression->getName());
                setField(theoryFieldLabel2, theoryFieldEditor2, "Key", progression->getKey());
                setField(theoryFieldLabel3, theoryFieldEditor3, "Mode", progression->getMode());
                setField(theoryFieldLabel4, theoryFieldEditor4, "Length (beats)", juce::String(progression->getLengthBeats()));
                setField(theoryFieldLabel5, theoryFieldEditor5, "Variant of", progression->getVariantOf());
                applyTheoryEditorButton.setButtonText("Apply Key/Mode");
            }
        }
    }
}

void WorkspaceShellComponent::commitTheoryEditorAction()
{
    seedSongStateIfNeeded();

    const auto beforeSnapshot = createSongSnapshot();
    const auto actionResult = applyTheoryEditorAction();
    runChordTheoryEngineForSelection();

    captureUndoStateIfChanged(beforeSnapshot);
    saveSongState();
    ensureSelectionDefaults();
    refreshTimelineData();
    populateTheoryObjectSelector();
    populateTheoryFieldsForCurrentSelection();
    updateUndoRedoButtonState();

    const auto message = actionResult + " | " + summarizeSongState();
    interactionStatus.setText(message, juce::dontSendNotification);
    DBG(message);
}

void WorkspaceShellComponent::runChordTheoryEngineForSelection()
{
    auto chord = getSelectedChord();
    auto progression = getSelectedProgression();
    if (!chord.has_value() || !progression.has_value())
        return;

    const bool isUserOwned = chord->getSource().equalsIgnoreCase("user");

    std::vector<int> pcs;
    for (const auto& note : chord->getNotes())
        pcs.push_back(((note.getPitch() % 12) + 12) % 12);

    if (pcs.size() >= 2 && !isUserOwned)
    {
        const auto id = setle::theory::identify(pcs);
        if (id.confidence >= 0.75f)
        {
            const auto currentOctave = chord->getRootMidi() / 12;
            const int identifiedRoot = DiatonicHarmony::pitchClassForRoot(id.root);

            chord->setRootMidi(juce::jlimit(0, 127, currentOctave * 12 + identifiedRoot));
            chord->setQuality(id.quality);
            chord->setSymbol(id.symbol);
            chord->setName(id.symbol);
            chord->setSource("midi");
        }
    }

    const int keyPc = DiatonicHarmony::pitchClassForRoot(progression->getKey());
    const int chordPc = ((chord->getRootMidi() % 12) + 12) % 12;
    const int interval = (chordPc - keyPc + 12) % 12;

    const auto intervals = DiatonicHarmony::modeIntervals(progression->getMode());
    for (int i = 0; i < 7; ++i)
    {
        if (intervals[static_cast<size_t>(i)] != interval)
            continue;

        const auto diatonic = DiatonicHarmony::buildDiatonicChord(progression->getKey(), progression->getMode(), i);
        if (!isUserOwned)
        {
            chord->setFunction(diatonic.function);
            chord->setSource("midi");
        }
        break;
    }
}

juce::String WorkspaceShellComponent::applyTheoryEditorAction()
{
    if (activeEditorTarget == TheoryMenuTarget::section)
    {
        auto section = getSelectedSection();
        if (!section.has_value())
            return "Editor apply failed: no section selected";

        if (activeEditorActionId == sectionEditTheory)
        {
            const auto name = theoryFieldEditor1.getText().trim();
            const auto repeatText = theoryFieldEditor2.getText().trim();
            if (repeatText.isNotEmpty() && !isStrictInt(repeatText))
                return "Section edit rejected: Repeat Count must be an integer";

            const auto repeats = juce::jlimit(1, 32, parseIntOr(theoryFieldEditor2.getText(), section->getRepeatCount()));
            section->setName(name.isNotEmpty() ? name : section->getName());
            section->setRepeatCount(repeats);
            return "Section updated from center editor";
        }

        if (activeEditorActionId == sectionSetRepeatPattern)
        {
            const auto repeatText = theoryFieldEditor1.getText().trim();
            if (repeatText.isNotEmpty() && !isStrictInt(repeatText))
                return "Repeat pattern rejected: Repeat Count must be an integer";

            const auto repeats = juce::jlimit(1, 32, parseIntOr(theoryFieldEditor1.getText(), section->getRepeatCount()));
            section->setRepeatCount(repeats);
            return "Section repeat pattern updated from center editor";
        }

        if (activeEditorActionId == sectionAddTransitionAnchor)
        {
            auto sections = songState.getSections();
            if (sections.size() < 2)
            {
                auto generated = model::Section::create("Generated Target", 2);
                if (auto progression = getSelectedProgression())
                    generated.addProgressionRef(model::SectionProgressionRef::create(progression->getId(), 0, ""));
                songState.addSection(generated);
                sections = songState.getSections();
            }

            juce::String toSectionId;
            const auto preferredToSectionName = theoryFieldEditor3.getText().trim();
            for (const auto& candidate : sections)
            {
                if (candidate.getId() == section->getId())
                    continue;

                if (preferredToSectionName.isNotEmpty() && candidate.getName().equalsIgnoreCase(preferredToSectionName))
                {
                    toSectionId = candidate.getId();
                    break;
                }

                if (toSectionId.isEmpty())
                    toSectionId = candidate.getId();
            }

            if (toSectionId.isEmpty())
                return "Transition anchor failed: no destination section";

            auto transition = model::Transition::create(
                section->getId(),
                toSectionId,
                theoryFieldEditor2.getText().trim().isNotEmpty() ? theoryFieldEditor2.getText().trim()
                                                                 : juce::String("pushRight"));
            transition.setName(theoryFieldEditor1.getText().trim().isNotEmpty()
                                   ? theoryFieldEditor1.getText().trim()
                                   : "Anchor " + juce::String(static_cast<int>(songState.getTransitions().size()) + 1));
            songState.addTransition(transition);
            return "Transition anchor created from center editor";
        }
    }
    else if (activeEditorTarget == TheoryMenuTarget::chord)
    {
        auto chord = getSelectedChord();
        if (!chord.has_value())
            return "Editor apply failed: no chord selected";

        if (activeEditorActionId == chordEdit)
        {
            const auto rootText = theoryFieldEditor3.getText().trim();
            const auto durationText = theoryFieldEditor4.getText().trim();
            if (rootText.isNotEmpty() && !isStrictInt(rootText))
                return "Chord edit rejected: Root MIDI must be an integer";
            if (durationText.isNotEmpty() && !isStrictDouble(durationText))
                return "Chord edit rejected: Duration Beats must be numeric";

            const auto beforeSymbol = chord->getSymbol();
            chord->setSymbol(theoryFieldEditor1.getText().trim().isNotEmpty()
                                 ? theoryFieldEditor1.getText().trim()
                                 : chord->getSymbol());
            chord->setName(chord->getSymbol());
            chord->setQuality(theoryFieldEditor2.getText().trim().isNotEmpty()
                                  ? theoryFieldEditor2.getText().trim()
                                  : chord->getQuality());
            chord->setRootMidi(juce::jlimit(0, 127, parseIntOr(theoryFieldEditor3.getText(), chord->getRootMidi())));
            chord->setDurationBeats(juce::jmax(0.125, parseDoubleOr(theoryFieldEditor4.getText(), chord->getDurationBeats())));
            chord->setFunction(theoryFieldEditor5.getText().trim().isNotEmpty()
                                   ? theoryFieldEditor5.getText().trim()
                                   : chord->getFunction());
            chord->setSource("user");
            const auto afterSymbol = chord->getSymbol();
            return "Applied chord edit: " + beforeSymbol + " → " + afterSymbol;
        }

        if (activeEditorActionId == chordSubstitution)
        {
            const auto profile = theoryFieldEditor1.getText().trim().toLowerCase();
            const auto root = chordRootNameForMidi(chord->getRootMidi());
            juce::String substitutionSymbol;
            juce::String quality;

            if (profile == "tritone")
            {
                const auto tritoneRoot = juce::jlimit(0, 127, chord->getRootMidi() + 6);
                substitutionSymbol = chordRootNameForMidi(tritoneRoot) + "7";
                quality = "dominant7";
                chord->setRootMidi(tritoneRoot);
            }
            else if (profile == "relative")
            {
                if (chord->getQuality().containsIgnoreCase("major"))
                {
                    const auto relativeRoot = juce::jlimit(0, 127, chord->getRootMidi() - 3);
                    substitutionSymbol = chordRootNameForMidi(relativeRoot) + "m7";
                    quality = "minor7";
                    chord->setRootMidi(relativeRoot);
                }
                else
                {
                    const auto relativeRoot = juce::jlimit(0, 127, chord->getRootMidi() + 3);
                    substitutionSymbol = chordRootNameForMidi(relativeRoot) + "maj7";
                    quality = "major7";
                    chord->setRootMidi(relativeRoot);
                }
            }
            else if (chord->getQuality().containsIgnoreCase("major"))
            {
                substitutionSymbol = root + "m7";
                quality = "minor7";
            }
            else if (chord->getQuality().containsIgnoreCase("minor"))
            {
                substitutionSymbol = root + "7";
                quality = "dominant7";
            }
            else
            {
                substitutionSymbol = root + "maj7";
                quality = "major7";
            }

            chord->setSymbol(substitutionSymbol);
            chord->setName(substitutionSymbol);
            chord->setQuality(quality);
            chord->setSource("user");
            return "Chord substitution applied from center editor";
        }

        if (activeEditorActionId == chordSetFunction)
        {
            const auto chordFunction = theoryFieldEditor1.getText().trim();
            chord->setFunction(chordFunction.isNotEmpty() ? chordFunction : nextFunction(chord->getFunction()));
            chord->setSource("user");
            return "Chord function updated from center editor";
        }
    }
    else if (activeEditorTarget == TheoryMenuTarget::note)
    {
        auto note = getSelectedNote();
        if (!note.has_value())
            return "Editor apply failed: no note selected";

        if (activeEditorActionId == noteEditTheory)
        {
            const auto pitchText = theoryFieldEditor1.getText().trim();
            const auto velocityText = theoryFieldEditor2.getText().trim();
            const auto startText = theoryFieldEditor3.getText().trim();
            const auto durationText = theoryFieldEditor4.getText().trim();

            if (pitchText.isNotEmpty() && !isStrictInt(pitchText))
                return "Note edit rejected: Pitch must be an integer";
            if (velocityText.isNotEmpty() && !isStrictDouble(velocityText))
                return "Note edit rejected: Velocity must be numeric";
            if (startText.isNotEmpty() && !isStrictDouble(startText))
                return "Note edit rejected: Start Beats must be numeric";
            if (durationText.isNotEmpty() && !isStrictDouble(durationText))
                return "Note edit rejected: Duration Beats must be numeric";

            note->setPitch(juce::jlimit(0, 127, parseIntOr(theoryFieldEditor1.getText(), note->getPitch())));
            note->setVelocity(juce::jlimit(0.0f, 1.0f, parseFloatOr(theoryFieldEditor2.getText(), note->getVelocity())));
            note->setStartBeats(juce::jmax(0.0, parseDoubleOr(theoryFieldEditor3.getText(), note->getStartBeats())));
            note->setDurationBeats(juce::jmax(0.03125, parseDoubleOr(theoryFieldEditor4.getText(), note->getDurationBeats())));
            return "Note updated from center editor";
        }
    }
    else
    {
        auto progression = getSelectedProgression();
        if (!progression.has_value())
            return "Editor apply failed: no progression selected";

        if (activeEditorActionId == progressionAnnotateKeyMode)
        {
            const auto name = theoryFieldEditor1.getText().trim();
            const auto key = theoryFieldEditor2.getText().trim();
            const auto mode = theoryFieldEditor3.getText().trim();
            const auto lengthText = theoryFieldEditor4.getText().trim();
            const auto variantOf = theoryFieldEditor5.getText().trim();

            if (lengthText.isNotEmpty() && !isStrictDouble(lengthText))
                return "Progression edit rejected: Length (beats) must be numeric";

            progression->setName(name.isNotEmpty() ? name : progression->getName());
            progression->setKey(key.isNotEmpty() ? key : progression->getKey());
            progression->setMode(mode.isNotEmpty() ? mode : progression->getMode());
            progression->setLengthBeats(juce::jmax(0.125, parseDoubleOr(theoryFieldEditor4.getText(), progression->getLengthBeats())));
            progression->setVariantOf(variantOf.isNotEmpty() ? variantOf : progression->getVariantOf());
            return "Progression key/mode updated from center editor";
        }

        if (activeEditorActionId == progressionCreateCandidate || activeEditorActionId == progressionGrabSampler)
        {
            auto candidate = model::Progression::create(
                theoryFieldEditor1.getText().trim().isNotEmpty()
                    ? theoryFieldEditor1.getText().trim()
                    : (activeEditorActionId == progressionGrabSampler ? "Queue Grab " : "Candidate ")
                          + juce::String(static_cast<int>(songState.getProgressions().size()) + 1),
                theoryFieldEditor2.getText().trim().isNotEmpty()
                    ? theoryFieldEditor2.getText().trim()
                    : progression->getKey(),
                theoryFieldEditor3.getText().trim().isNotEmpty()
                    ? theoryFieldEditor3.getText().trim()
                    : progression->getMode());
            candidate.setVariantOf(progression->getId());
            candidate.setLengthBeats(progression->getLengthBeats());

            bool usedTemplate = false;
            if (activeEditorActionId == progressionCreateCandidate
                && selectedProgressionTemplateIndex >= 0
                && selectedProgressionTemplateIndex < static_cast<int>(DiatonicHarmony::progressionPresets().size()))
            {
                const auto& preset = DiatonicHarmony::progressionPresets()[static_cast<size_t>(selectedProgressionTemplateIndex)];
                const auto generated = DiatonicHarmony::buildProgressionFromTemplate(candidate.getKey(), candidate.getMode(), preset);

                double beatCursor = 0.0;
                for (auto generatedChord : generated)
                {
                    if (generatedChord.getDurationBeats() <= 0.0)
                        generatedChord.setDurationBeats(4.0);

                    generatedChord.setStartBeats(beatCursor);
                    generatedChord.setSource("midi");
                    populateChordNotesFromQuality(generatedChord, beatCursor, generatedChord.getDurationBeats());
                    candidate.addChord(generatedChord);
                    beatCursor += generatedChord.getDurationBeats();
                }

                if (beatCursor > 0.0)
                    candidate.setLengthBeats(beatCursor);

                usedTemplate = true;
            }

            if (!usedTemplate)
            {
                for (const auto& sourceChord : progression->getChords())
                {
                    auto cloned = model::Chord::create(sourceChord.getSymbol(), sourceChord.getQuality(), sourceChord.getRootMidi());
                    cloned.setName(sourceChord.getName());
                    cloned.setFunction(sourceChord.getFunction());
                    cloned.setSource(sourceChord.getSource());
                    cloned.setStartBeats(sourceChord.getStartBeats());
                    cloned.setDurationBeats(sourceChord.getDurationBeats());
                    cloned.setTension(sourceChord.getTension());
                    for (const auto& sourceNote : sourceChord.getNotes())
                        cloned.addNote(model::Note::create(sourceNote.getPitch(),
                                                           sourceNote.getVelocity(),
                                                           sourceNote.getStartBeats(),
                                                           sourceNote.getDurationBeats(),
                                                           sourceNote.getChannel()));
                    candidate.addChord(cloned);
                }
            }

            songState.addProgression(candidate);
            selectedProgressionId = candidate.getId();
            return activeEditorActionId == progressionGrabSampler
                       ? "Queue candidate created from center editor"
                       : (usedTemplate
                              ? "Progression candidate created from selected template"
                              : "Progression candidate created from center editor");
        }
    }

    return "No center-editor action mapped for current context";
}

void WorkspaceShellComponent::refreshTimelineData()
{
    const auto sections = songState.getSections();
    const auto progressions = songState.getProgressions();

    timelineShell->updateSections(
        sections,
        progressions,
        selectedSectionId,
        [this](const juce::String& sectionId)
        {
            selectedSectionId = sectionId;
            ensureSelectionDefaults();
            populateTheoryObjectSelector();
            populateTheoryFieldsForCurrentSelection();
            refreshTimelineData(); // refresh chord strip for newly selected section
            interactionStatus.setText("Selected section: " + sectionId.substring(0, 8), juce::dontSendNotification);
        });

    // 7C: Wire section seek callback to main transport seek
    timelineShell->setSectionSeekCallback([this](double seekSeconds)
    {
        if (timelineShell != nullptr && timelineShell->onSeek != nullptr)
            timelineShell->onSeek(seekSeconds);
    });

    // Collect chords from active progressions referenced by the selected section
    std::vector<TimelineShell::ChordView> visibleChords;
    for (const auto& section : sections)
    {
        if (section.getId() != selectedSectionId)
            continue;
        for (const auto& ref : section.getProgressionRefs())
        {
            for (const auto& prog : progressions)
            {
                if (prog.getId() == ref.getProgressionId())
                {
                    for (const auto& chord : prog.getChords())
                        visibleChords.push_back({ chord, prog.getKey(), prog.getMode() });
                    break;
                }
            }
        }
        break;
    }

    timelineShell->updateChords(
        visibleChords,
        selectedChordId,
        [this](const juce::String& chordId)
        {
            selectedChordId = chordId;
            ensureSelectionDefaults();
            populateTheoryObjectSelector();
            populateTheoryFieldsForCurrentSelection();
            interactionStatus.setText("Selected chord: " + chordId.substring(0, 8), juce::dontSendNotification);
        });
}

void WorkspaceShellComponent::initialiseSongState()
{
    loadSongState();
    seedSongStateIfNeeded();
    ensureSelectionDefaults();
    saveSongState();
    refreshTimelineData();

    interactionStatus.setText("Theory state ready - " + summarizeSongState(), juce::dontSendNotification);
}

juce::File WorkspaceShellComponent::getSongStateFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("SETLE")
        .getChildFile("song_state.xml");
}

void WorkspaceShellComponent::loadSongState()
{
    if (auto loaded = model::Song::loadFromFile(getSongStateFile(), model::StorageFormat::xml))
    {
        songState = *loaded;
        return;
    }

    songState = model::Song::create("SETLE Session", 120.0);
}

void WorkspaceShellComponent::seedSongStateIfNeeded()
{
    if (!songState.isValid())
        songState = model::Song::create("SETLE Session", 120.0);

    if (songState.getProgressions().empty())
    {
        auto progression = model::Progression::create("Core Loop", "C", "ionian");

        auto cmaj7 = model::Chord::create("Cmaj7", "major7", 60);
        cmaj7.setFunction("T");
        cmaj7.addNote(model::Note::create(60, 0.85f, 0.0, 1.0, 1));
        cmaj7.addNote(model::Note::create(64, 0.78f, 0.0, 1.0, 1));
        cmaj7.addNote(model::Note::create(67, 0.74f, 0.0, 1.0, 1));
        cmaj7.addNote(model::Note::create(71, 0.70f, 0.0, 1.0, 1));

        auto dm7 = model::Chord::create("Dm7", "minor7", 62);
        dm7.setFunction("PD");
        dm7.setStartBeats(4.0);
        dm7.addNote(model::Note::create(62, 0.80f, 4.0, 1.0, 1));
        dm7.addNote(model::Note::create(65, 0.75f, 4.0, 1.0, 1));
        dm7.addNote(model::Note::create(69, 0.72f, 4.0, 1.0, 1));
        dm7.addNote(model::Note::create(72, 0.68f, 4.0, 1.0, 1));

        auto g7 = model::Chord::create("G7", "dominant7", 67);
        g7.setFunction("D");
        g7.setStartBeats(8.0);
        g7.addNote(model::Note::create(67, 0.82f, 8.0, 1.0, 1));
        g7.addNote(model::Note::create(71, 0.76f, 8.0, 1.0, 1));
        g7.addNote(model::Note::create(74, 0.73f, 8.0, 1.0, 1));
        g7.addNote(model::Note::create(77, 0.69f, 8.0, 1.0, 1));

        auto cResolve = model::Chord::create("Cmaj7", "major7", 60);
        cResolve.setFunction("T");
        cResolve.setStartBeats(12.0);
        cResolve.addNote(model::Note::create(60, 0.85f, 12.0, 1.0, 1));
        cResolve.addNote(model::Note::create(64, 0.78f, 12.0, 1.0, 1));
        cResolve.addNote(model::Note::create(67, 0.74f, 12.0, 1.0, 1));
        cResolve.addNote(model::Note::create(71, 0.70f, 12.0, 1.0, 1));

        progression.addChord(cmaj7);
        progression.addChord(dm7);
        progression.addChord(g7);
        progression.addChord(cResolve);
        songState.addProgression(progression);
    }

    auto progressions = songState.getProgressions();
    if (progressions.empty())
        return;

    if (songState.getSections().empty())
    {
        auto section = model::Section::create("Verse", 4);
        section.addProgressionRef(model::SectionProgressionRef::create(progressions.front().getId(), 0, ""));
        songState.addSection(section);

        auto chorus = model::Section::create("Chorus", 2);
        chorus.addProgressionRef(model::SectionProgressionRef::create(progressions.front().getId(), 0, ""));
        songState.addSection(chorus);
    }

    auto sections = songState.getSections();
    if (songState.getTransitions().empty() && sections.size() >= 2)
        songState.addTransition(model::Transition::create(sections.front().getId(), sections[1].getId(), "pushRight"));
}

void WorkspaceShellComponent::saveSongState()
{
    if (!songState.isValid())
        return;

    const auto result = songState.saveToFile(getSongStateFile(), model::StorageFormat::xml);
    if (result.failed())
        DBG("Failed to save song state: " + result.getErrorMessage());
}

juce::String WorkspaceShellComponent::createSongSnapshot() const
{
    if (!songState.isValid())
        return {};

    return songState.valueTree().toXmlString();
}

bool WorkspaceShellComponent::restoreSongFromSnapshot(const juce::String& xmlSnapshot)
{
    if (xmlSnapshot.isEmpty())
        return false;

    auto restored = juce::ValueTree::fromXml(xmlSnapshot);
    if (!restored.isValid() || !restored.hasType(model::Schema::songType))
        return false;

    songState = model::Song(restored);
    seedSongStateIfNeeded();
    ensureSelectionDefaults();
    saveSongState();
    return true;
}

void WorkspaceShellComponent::captureUndoStateIfChanged(const juce::String& beforeSnapshot)
{
    const auto afterSnapshot = createSongSnapshot();
    if (beforeSnapshot.isEmpty() || afterSnapshot.isEmpty() || beforeSnapshot == afterSnapshot)
        return;

    undoSnapshots.push_back(beforeSnapshot);
    if (undoSnapshots.size() > static_cast<size_t>(maxUndoDepth))
        undoSnapshots.erase(undoSnapshots.begin());

    redoSnapshots.clear();
    updateUndoRedoButtonState();
}

void WorkspaceShellComponent::performUndo()
{
    if (undoSnapshots.empty())
    {
        interactionStatus.setText("Undo not available", juce::dontSendNotification);
        updateUndoRedoButtonState();
        return;
    }

    const auto currentSnapshot = createSongSnapshot();
    const auto targetSnapshot = undoSnapshots.back();
    undoSnapshots.pop_back();

    if (restoreSongFromSnapshot(targetSnapshot))
    {
        if (currentSnapshot.isNotEmpty())
        {
            redoSnapshots.push_back(currentSnapshot);
            if (redoSnapshots.size() > static_cast<size_t>(maxUndoDepth))
                redoSnapshots.erase(redoSnapshots.begin());
        }
        refreshTimelineData();
        populateTheoryObjectSelector();
        populateTheoryFieldsForCurrentSelection();
        interactionStatus.setText("Undid theory action | " + summarizeSongState(), juce::dontSendNotification);
    }
    else
    {
        interactionStatus.setText("Undo failed: invalid snapshot", juce::dontSendNotification);
    }

    updateUndoRedoButtonState();
}

void WorkspaceShellComponent::performRedo()
{
    if (redoSnapshots.empty())
    {
        interactionStatus.setText("Redo not available", juce::dontSendNotification);
        updateUndoRedoButtonState();
        return;
    }

    const auto currentSnapshot = createSongSnapshot();
    const auto targetSnapshot = redoSnapshots.back();
    redoSnapshots.pop_back();

    if (restoreSongFromSnapshot(targetSnapshot))
    {
        if (currentSnapshot.isNotEmpty())
        {
            undoSnapshots.push_back(currentSnapshot);
            if (undoSnapshots.size() > static_cast<size_t>(maxUndoDepth))
                undoSnapshots.erase(undoSnapshots.begin());
        }
        refreshTimelineData();
        populateTheoryObjectSelector();
        populateTheoryFieldsForCurrentSelection();
        interactionStatus.setText("Redid theory action | " + summarizeSongState(), juce::dontSendNotification);
    }
    else
    {
        interactionStatus.setText("Redo failed: invalid snapshot", juce::dontSendNotification);
    }

    updateUndoRedoButtonState();
}

void WorkspaceShellComponent::updateUndoRedoButtonState()
{
    const auto canUndo = !undoSnapshots.empty();
    const auto canRedo = !redoSnapshots.empty();

    undoTheoryButton.setEnabled(canUndo);
    redoTheoryButton.setEnabled(canRedo);

    undoTheoryButton.setColour(
        juce::TextButton::buttonColourId,
        canUndo ? juce::Colour(0xff3d4654) : juce::Colour(0xff2a2d32));
    redoTheoryButton.setColour(
        juce::TextButton::buttonColourId,
        canRedo ? juce::Colour(0xff3d4654) : juce::Colour(0xff2a2d32));

    undoTheoryButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.9f));
    redoTheoryButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.9f));
}

juce::String WorkspaceShellComponent::summarizeSongState() const
{
    if (!songState.isValid())
        return "No song state";

    const auto progressionCount = static_cast<int>(songState.getProgressions().size());
    const auto sectionCount = static_cast<int>(songState.getSections().size());
    const auto transitionCount = static_cast<int>(songState.getTransitions().size());

    return "P:" + juce::String(progressionCount)
         + " S:" + juce::String(sectionCount)
         + " T:" + juce::String(transitionCount);
}

void WorkspaceShellComponent::handleTimelineTheoryAction(TheoryMenuTarget target, int actionId, const juce::String& actionName)
{
    seedSongStateIfNeeded();
    ensureSelectionDefaults();
    const auto beforeSnapshot = createSongSnapshot();
    const auto actionResult = runTheoryAction(target, actionId, actionName);
    captureUndoStateIfChanged(beforeSnapshot);
    saveSongState();
    ensureSelectionDefaults();
    refreshTimelineData();
    populateTheoryObjectSelector();
    populateTheoryFieldsForCurrentSelection();
    updateUndoRedoButtonState();

    const auto message = actionResult + " | " + summarizeSongState();
    interactionStatus.setText(message, juce::dontSendNotification);
    DBG(message);
}

juce::String WorkspaceShellComponent::runTheoryAction(TheoryMenuTarget target, int actionId, const juce::String& actionName)
{
    switch (target)
    {
        case TheoryMenuTarget::section:
            return runSectionAction(actionId);
        case TheoryMenuTarget::chord:
            return runChordAction(actionId);
        case TheoryMenuTarget::note:
            return runNoteAction(actionId);
        case TheoryMenuTarget::progression:
            return runProgressionAction(actionId);
    }

    return "Unknown action: " + actionName;
}

juce::String WorkspaceShellComponent::runSectionAction(int actionId)
{
    if (actionId == sectionEditTheory || actionId == sectionSetRepeatPattern || actionId == sectionAddTransitionAnchor)
    {
        openTheoryEditor(TheoryMenuTarget::section, actionId, "Section Action");
        return "Section editor opened in WORK panel";
    }

    auto sections = songState.getSections();
    if (sections.empty())
        return "Section action skipped: no sections available";

    ensureSelectionDefaults();
    if (auto selectedSection = getSelectedSection())
        selectedSectionId = selectedSection->getId();

    if (actionId == sectionConflictCheck)
    {
        int missingProgressionRefs = 0;
        for (const auto& candidateSection : sections)
        {
            for (const auto& progressionRef : candidateSection.getProgressionRefs())
            {
                if (!songState.findProgressionById(progressionRef.getProgressionId()).has_value())
                    ++missingProgressionRefs;
            }
        }

        if (missingProgressionRefs == 0)
            return "Section conflict check passed";

        return "Section conflict check found " + juce::String(missingProgressionRefs) + " missing progression reference(s)";
    }

    return "Section action not implemented";
}

juce::String WorkspaceShellComponent::runChordAction(int actionId)
{
    if (actionId == chordEdit || actionId == chordSubstitution || actionId == chordSetFunction)
    {
        openTheoryEditor(TheoryMenuTarget::chord, actionId, "Chord Action");
        return "Chord editor opened in WORK panel";
    }

    auto progression = getSelectedProgression();
    if (!progression.has_value())
        return "Chord action skipped: no selected progression available";

    auto chords = progression->getChords();

    if (chords.empty())
    {
        auto bootstrapChord = model::Chord::create("Cmaj7", "major7", 60);
        bootstrapChord.setFunction("T");
        bootstrapChord.addNote(model::Note::create(60, 0.8f, 0.0, 1.0, 1));
        bootstrapChord.addNote(model::Note::create(64, 0.75f, 0.0, 1.0, 1));
        bootstrapChord.addNote(model::Note::create(67, 0.72f, 0.0, 1.0, 1));
        progression->addChord(bootstrapChord);
        chords = progression->getChords();
        selectedChordId = bootstrapChord.getId();
    }

    auto chord = getSelectedChord();
    if (!chord.has_value() && !chords.empty())
    {
        selectedChordId = chords.front().getId();
        chord = getSelectedChord();
    }

    if (!chord.has_value())
        return "Chord action skipped: no selected chord available";

    if (actionId == chordScopeOccurrence || actionId == chordScopeRepeat
        || actionId == chordScopeSelectedRepeats || actionId == chordScopeAllRepeats)
    {
        auto section = getSelectedSection();
        if (!section.has_value())
            return "Scope change skipped: no sections available";

        auto refs = section->getProgressionRefs();
        if (refs.empty())
        {
            section->addProgressionRef(model::SectionProgressionRef::create(progression->getId(), 0, ""));
            refs = section->getProgressionRefs();
        }

        if (refs.empty())
            return "Scope change skipped: progression mapping unavailable";

        const auto selectedProgressionIdLocal = progression->getId();
        const auto selectedBaseProgressionId = progression->getVariantOf().isNotEmpty()
                                                   ? progression->getVariantOf()
                                                   : selectedProgressionIdLocal;
        const auto currentRepeatIndex = juce::jmax(1, theoryCurrentRepeatIndex);
        const auto currentRepeatIndices = juce::String(currentRepeatIndex);
        const auto selectedRepeatIndices = normalizeRepeatIndices(
            theorySelectedRepeatIndices.isNotEmpty() ? theorySelectedRepeatIndices : currentRepeatIndices,
            currentRepeatIndices);

        auto refMatchesSelection = [this, &selectedProgressionIdLocal, &selectedBaseProgressionId](const model::SectionProgressionRef& candidate)
        {
            const auto candidateProgressionId = candidate.getProgressionId();
            if (candidateProgressionId == selectedProgressionIdLocal || candidateProgressionId == selectedBaseProgressionId)
                return true;

            if (auto candidateProgression = songState.findProgressionById(candidateProgressionId))
                return candidateProgression->getVariantOf() == selectedBaseProgressionId;

            return false;
        };

        int targetRefIndex = -1;
        for (size_t i = 0; i < refs.size(); ++i)
        {
            if (refMatchesSelection(refs[i]))
            {
                targetRefIndex = static_cast<int>(i);
                break;
            }
        }

        if (targetRefIndex < 0)
        {
            // New ref created with default scope="all"; the actionId blocks below
            // immediately override it with the requested scope.
            const auto nextOrderIndex = static_cast<int>(refs.size());
            section->addProgressionRef(model::SectionProgressionRef::create(selectedProgressionIdLocal, nextOrderIndex, ""));
            refs = section->getProgressionRefs();
            targetRefIndex = static_cast<int>(refs.size()) - 1;
        }

        if (actionId == chordScopeAllRepeats)
        {
            for (auto& sectionRef : refs)
            {
                if (!refMatchesSelection(sectionRef))
                    continue;

                if (auto currentProgression = songState.findProgressionById(sectionRef.getProgressionId()))
                {
                    if (currentProgression->getVariantOf().isNotEmpty())
                        sectionRef.setProgressionId(currentProgression->getVariantOf());
                }

                sectionRef.setVariantName("");
                sectionRef.setRepeatScope("all");
                sectionRef.setRepeatSelection("all");
                sectionRef.setRepeatIndices("");
            }

            theoryScope = "allRepeats";
            return "Theory scope set to all repeats (base progression)";
        }

        auto variant = model::Progression::create(
            progression->getName() + " Variant " + juce::String(static_cast<int>(songState.getProgressions().size()) + 1),
            progression->getKey(),
            progression->getMode());
        variant.setVariantOf(selectedBaseProgressionId);
        variant.setLengthBeats(progression->getLengthBeats());

        for (const auto& sourceChord : progression->getChords())
        {
            auto copiedChord = model::Chord::create(sourceChord.getSymbol(), sourceChord.getQuality(), sourceChord.getRootMidi());
            copiedChord.setName(sourceChord.getName());
            copiedChord.setFunction(sourceChord.getFunction());
            copiedChord.setSource(sourceChord.getSource());
            copiedChord.setStartBeats(sourceChord.getStartBeats());
            copiedChord.setDurationBeats(sourceChord.getDurationBeats());
            copiedChord.setTension(sourceChord.getTension());

            for (const auto& sourceNote : sourceChord.getNotes())
                copiedChord.addNote(model::Note::create(sourceNote.getPitch(),
                                                        sourceNote.getVelocity(),
                                                        sourceNote.getStartBeats(),
                                                        sourceNote.getDurationBeats(),
                                                        sourceNote.getChannel()));

            variant.addChord(copiedChord);
        }

        songState.addProgression(variant);
        refs[static_cast<size_t>(targetRefIndex)].setProgressionId(variant.getId());

        if (actionId == chordScopeOccurrence)
        {
            refs[static_cast<size_t>(targetRefIndex)].setVariantName("occurrenceOnly");
            refs[static_cast<size_t>(targetRefIndex)].setRepeatScope("occurrence");
            refs[static_cast<size_t>(targetRefIndex)].setRepeatSelection("single");
            refs[static_cast<size_t>(targetRefIndex)].setRepeatIndices(currentRepeatIndices);
            theoryScope = "thisOccurrence";
            return "Theory scope set to this occurrence (variant generated)";
        }

        // Guard: selectedRepeats requires an explicit selection — never fall silently through to repeat 1.
        if (actionId == chordScopeSelectedRepeats && theorySelectedRepeatIndices.trim().isEmpty())
            return "Scope change skipped: no repeat indices selected. Set repeat indices in the theory editor first.";

        if (actionId == chordScopeSelectedRepeats)
        {
            for (auto& sectionRef : refs)
            {
                if (!refMatchesSelection(sectionRef))
                    continue;

                sectionRef.setProgressionId(variant.getId());
                sectionRef.setVariantName("selectedRepeats");
                sectionRef.setRepeatScope("selectedRepeats");
                sectionRef.setRepeatSelection("selected");
                sectionRef.setRepeatIndices(selectedRepeatIndices);
            }

            theorySelectedRepeatIndices = selectedRepeatIndices;
            theoryScope = "selectedRepeats";
            return "Theory scope set to selected repeats (variant generated)";
        }

        refs[static_cast<size_t>(targetRefIndex)].setVariantName("thisRepeat");
        refs[static_cast<size_t>(targetRefIndex)].setRepeatScope("repeat");
        refs[static_cast<size_t>(targetRefIndex)].setRepeatSelection("current");
        refs[static_cast<size_t>(targetRefIndex)].setRepeatIndices(currentRepeatIndices);
        theoryScope = "thisRepeat";
        return "Theory scope set to this repeat (variant generated)";
    }

    if (actionId >= chordSnapWholeBar && actionId <= chordSnapThirtySecond)
    {
        juce::String snapValue = "1/16";

        if (actionId == chordSnapWholeBar) snapValue = "bar";
        else if (actionId == chordSnapHalfBar) snapValue = "1/2";
        else if (actionId == chordSnapBeat) snapValue = "1/4";
        else if (actionId == chordSnapHalfBeat) snapValue = "halfBeat";
        else if (actionId == chordSnapEighth) snapValue = "1/8";
        else if (actionId == chordSnapSixteenth) snapValue = "1/16";
        else if (actionId == chordSnapThirtySecond) snapValue = "1/32";

        theorySnap = snapValue;
        return "Theory snap set to " + snapValue;
    }

    return "Chord action not implemented";
}

juce::String WorkspaceShellComponent::runNoteAction(int actionId)
{
    if (actionId == noteEditTheory)
    {
        openTheoryEditor(TheoryMenuTarget::note, actionId, "Note Action");
        return "Note editor opened in WORK panel";
    }

    auto progression = getSelectedProgression();
    if (!progression.has_value())
        return "Note action skipped: no selected progression available";

    auto chord = getSelectedChord();
    if (!chord.has_value())
        return "Note action skipped: no selected chord available";

    auto notes = chord->getNotes();

    if (notes.empty())
    {
        chord->addNote(model::Note::create(chord->getRootMidi(), 0.8f, 0.0, 1.0, 1));
        notes = chord->getNotes();
    }

    if (notes.empty())
        return "Note action skipped: unable to build note context";

    auto note = getSelectedNote();
    if (!note.has_value())
    {
        selectedNoteId = notes.front().getId();
        note = getSelectedNote();
    }

    if (!note.has_value())
        return "Note action skipped: no selected note available";

    if (actionId == noteQuantizeScale)
    {
        const auto quantizedPitch = quantizeToCMajor(note->getPitch());
        note->setPitch(quantizedPitch);
        return "Lead note quantized to C-major tone " + chordRootNameForMidi(quantizedPitch);
    }

    if (actionId == noteConvertToChord)
    {
        if (notes.size() < 3)
        {
            chord->addNote(model::Note::create(chord->getRootMidi() + 4, 0.74f, 0.0, 1.0, 1));
            chord->addNote(model::Note::create(chord->getRootMidi() + 7, 0.70f, 0.0, 1.0, 1));
            notes = chord->getNotes();
        }

        std::vector<int> pitches;
        for (const auto& sourceNote : notes)
            pitches.push_back(sourceNote.getPitch());

        std::sort(pitches.begin(), pitches.end());
        const auto root = pitches.front();
        const auto third = pitches[juce::jmin(1, static_cast<int>(pitches.size() - 1))];
        const auto interval = (third - root + 12) % 12;
        const bool minorTriad = (interval == 3);

        const auto rootName = chordRootNameForMidi(root);
        const auto symbol = rootName + (minorTriad ? "m" : "maj");

        chord->setRootMidi(root);
        chord->setSymbol(symbol);
        chord->setName(symbol);
        chord->setQuality(minorTriad ? "minorTriad" : "majorTriad");
        chord->setSource("user");

        return "Notes converted to chord symbol " + symbol;
    }

    if (actionId == noteDeriveProgression)
    {
        auto derived = model::Progression::create(
            "Derived " + juce::String(static_cast<int>(songState.getProgressions().size()) + 1),
            progression->getKey(),
            progression->getMode());
        derived.setLengthBeats(progression->getLengthBeats());

        auto derivedChord = model::Chord::create(chord->getSymbol(), chord->getQuality(), chord->getRootMidi());
        derivedChord.setName(chord->getName());
        derivedChord.setFunction(chord->getFunction());
        derivedChord.setSource(chord->getSource());
        derivedChord.setDurationBeats(4.0);
        for (const auto& sourceNote : chord->getNotes())
            derivedChord.addNote(model::Note::create(sourceNote.getPitch(),
                                                     sourceNote.getVelocity(),
                                                     sourceNote.getStartBeats(),
                                                     sourceNote.getDurationBeats(),
                                                     sourceNote.getChannel()));

        derived.addChord(derivedChord);
        songState.addProgression(derived);

        auto sections = songState.getSections();
        if (!sections.empty())
        {
            auto section = getSelectedSection().value_or(sections.front());
            section.addProgressionRef(model::SectionProgressionRef::create(
                derived.getId(),
                static_cast<int>(section.getProgressionRefs().size()),
                "derived"));
        }

        return "Derived progression created from note selection";
    }

    return "Note action not implemented";
}

juce::String WorkspaceShellComponent::runProgressionAction(int actionId)
{
    if (actionId == progressionGrabSampler || actionId == progressionCreateCandidate || actionId == progressionAnnotateKeyMode)
    {
        openTheoryEditor(TheoryMenuTarget::progression, actionId, "Progression Action");
        return "Progression editor opened in WORK panel";
    }

    auto progression = getSelectedProgression();
    if (!progression.has_value())
        return "Progression action skipped: no selected progression available";

    if (actionId == progressionTagTransition)
    {
        auto sections = songState.getSections();
        if (sections.size() < 2)
            return "Transition tagging skipped: requires at least two sections";

        auto transition = model::Transition::create(sections.front().getId(), sections[1].getId(), "historyTagged");
        transition.setName("Tagged Transition " + juce::String(static_cast<int>(songState.getTransitions().size()) + 1));
        transition.setTargetTension(6);
        songState.addTransition(transition);
        return "Progression tagged as transition material";
    }

    return "Progression action not implemented";
}

void WorkspaceShellComponent::clampLayoutValues(int totalTopWidth, int totalBodyHeight)
{
    timelineHeight = juce::jlimit(minTimelineHeight, juce::jmax(minTimelineHeight, totalBodyHeight - minTopWorkHeight), timelineHeight);

    leftPanelWidth = juce::jlimit(minSideWidth, maxSideWidth, leftPanelWidth);
    rightPanelWidth = juce::jlimit(minSideWidth, maxSideWidth, rightPanelWidth);

    auto availableForPanels = totalTopWidth - (2 * splitterThickness);
    auto maxSidesSum = juce::jmax(0, availableForPanels - minCenterWidth);
    auto sidesSum = leftPanelWidth + rightPanelWidth;

    if (sidesSum > maxSidesSum && sidesSum > 0)
    {
        auto leftRatio = static_cast<float>(leftPanelWidth) / static_cast<float>(sidesSum);
        leftPanelWidth = static_cast<int>(maxSidesSum * leftRatio);
        rightPanelWidth = maxSidesSum - leftPanelWidth;
    }
}

void WorkspaceShellComponent::loadLayoutState()
{
    if (auto* settings = appProperties.getUserSettings())
    {
        leftPanelWidth = settings->getIntValue("ui.leftWidth", leftPanelWidth);
        rightPanelWidth = settings->getIntValue("ui.rightWidth", rightPanelWidth);
        timelineHeight = settings->getIntValue("ui.timelineHeight", timelineHeight);

        const auto modeRaw = settings->getIntValue("ui.focusMode", static_cast<int>(FocusMode::balanced));
        if (modeRaw == static_cast<int>(FocusMode::inFocused))
            focusMode = FocusMode::inFocused;
        else if (modeRaw == static_cast<int>(FocusMode::outFocused))
            focusMode = FocusMode::outFocused;
        else
            focusMode = FocusMode::balanced;
    }
}

void WorkspaceShellComponent::saveLayoutState()
{
    if (auto* settings = appProperties.getUserSettings())
    {
        settings->setValue("ui.leftWidth", leftPanelWidth);
        settings->setValue("ui.rightWidth", rightPanelWidth);
        settings->setValue("ui.timelineHeight", timelineHeight);
        settings->setValue("ui.focusMode", static_cast<int>(focusMode));
        settings->saveIfNeeded();
    }
}

void WorkspaceShellComponent::timerCallback()
{
    if (edit == nullptr)
        return;

    const auto& transport = edit->getTransport();
    const double totalSecs = transport.getPosition().inSeconds();
    const int mins = static_cast<int>(totalSecs) / 60;
    const double secs = std::fmod(totalSecs, 60.0);

    positionLabel.setText(
        juce::String(mins) + ":" + juce::String(secs, 3).paddedLeft('0', 6),
        juce::dontSendNotification);

    // 7A: Update playhead fraction on timeline
    const double progLen = getProgressionLengthSeconds();
    if (progLen > 0.0 && timelineShell != nullptr)
    {
        const double playheadFrac = totalSecs / progLen;
        timelineShell->setPlayheadFraction(playheadFrac);
    }

    // 6B: Color position label based on transport state
    if (transport.isRecording())
        positionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffdd4444));
    else if (transport.isPlaying())
        positionLabel.setColour(juce::Label::textColourId, juce::Colour(0xff6ac87b));
    else
        positionLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.80f));
}

double WorkspaceShellComponent::getProgressionLengthSeconds()
{
    auto progressionOpt = getSelectedProgression();
    if (!progressionOpt.has_value())
        return 0.0;

    const auto chords = progressionOpt->getChords();
    if (chords.empty())
        return 0.0;

    const double bpm = (edit != nullptr && edit->tempoSequence.getTempo(0) != nullptr)
                           ? edit->tempoSequence.getTempo(0)->getBpm()
                           : songState.getBpm();
    const double secPerBeat = 60.0 / bpm;

    double totalBeats = 0.0;
    for (const auto& chord : chords)
    {
        double d = chord.getDurationBeats();
        totalBeats += (d > 0.0 ? d : 1.0);
    }

    return totalBeats * secPerBeat;
}

void WorkspaceShellComponent::loadProgressionToEdit()
{
    if (edit == nullptr)
        return;

    auto tracks = te::getAudioTracks(*edit);
    if (tracks.isEmpty())
        return;

    auto* track = tracks.getFirst();

    // Remove all existing clips
    {
        juce::Array<te::Clip*> oldClips;
        for (auto* clip : track->getClips())
            oldClips.add(clip);
        for (auto* clip : oldClips)
            clip->removeFromParent();
    }

    auto progressionOpt = getSelectedProgression();
    if (!progressionOpt.has_value())
        return;

    const auto chords = progressionOpt->getChords();
    if (chords.empty())
        return;

    const double bpm     = (edit->tempoSequence.getTempo(0) != nullptr)
                               ? edit->tempoSequence.getTempo(0)->getBpm()
                               : 120.0;
    const double secPerBeat = 60.0 / bpm;

    // Calculate total duration in seconds
    double totalBeats = 0.0;
    for (const auto& chord : chords)
    {
        double d = chord.getDurationBeats();
        totalBeats += (d > 0.0 ? d : 1.0);
    }
    const double totalSecs = totalBeats * secPerBeat;

    // Insert one MIDI clip spanning the whole progression
    const auto clipRange = tracktion::TimeRange (
        tracktion::TimePosition::fromSeconds(0.0),
        tracktion::TimePosition::fromSeconds(totalSecs));

    auto clip = track->insertMIDIClip(clipRange, nullptr);
    if (clip == nullptr)
        return;

    auto& seq = clip->getSequence();
    double beatPos = 0.0;

    for (const auto& chord : chords)
    {
        double d = chord.getDurationBeats();
        if (d <= 0.0)
            d = 1.0;

        const auto startBeat = tracktion::BeatPosition::fromBeats(beatPos);
        const auto beatLen   = tracktion::BeatDuration::fromBeats(d * 0.95);

        const auto notes = chord.getNotes();
        if (notes.empty())
        {
            // Fall back to root-only note
            seq.addNote(chord.getRootMidi(), startBeat, beatLen, 80, 0, nullptr);
        }
        else
        {
            for (const auto& note : notes)
                seq.addNote(note.getPitch(),
                            startBeat,
                            beatLen,
                            static_cast<int>(note.getVelocity() * 127.0f),
                            0,
                            nullptr);
        }

        beatPos += d;
    }
}

void WorkspaceShellComponent::resized()
{
    auto bounds = getLocalBounds();
    clampLayoutValues(bounds.getWidth(), bounds.getHeight() - topStripHeight);

    auto topBounds = bounds.removeFromTop(topStripHeight);
    topStrip.setBounds(topBounds);

    auto buttonArea = topBounds.removeFromRight(970);
    redoTheoryButton.setBounds(buttonArea.removeFromRight(112).reduced(4, 6));
    undoTheoryButton.setBounds(buttonArea.removeFromRight(122).reduced(4, 6));
    focusOutButton.setBounds(buttonArea.removeFromRight(118).reduced(4, 6));
    focusBalancedButton.setBounds(buttonArea.removeFromRight(148).reduced(4, 6));
    focusInButton.setBounds(buttonArea.removeFromRight(104).reduced(4, 6));

    // Transport controls (left of focus buttons): BPM, Play, Stop, Rec, Loop, Position
    auto bpmArea = buttonArea.removeFromRight(100).reduced(4, 6);
    bpmLabel.setBounds(bpmArea.removeFromLeft(32));
    bpmEditor.setBounds(bpmArea);
    recordButton.setBounds(buttonArea.removeFromRight(46).reduced(4, 6));
    stopButton.setBounds(buttonArea.removeFromRight(46).reduced(4, 6));
    playButton.setBounds(buttonArea.removeFromRight(52).reduced(4, 6));
    loopButton.setBounds(buttonArea.removeFromRight(44).reduced(4, 6));
    positionLabel.setBounds(buttonArea.removeFromRight(80).reduced(4, 6));

    topTitle.setBounds(topBounds.removeFromLeft(250).reduced(8, 6));
    interactionStatus.setBounds(topBounds.reduced(8, 8));

    auto timelineArea = bounds.removeFromBottom(timelineHeight);
    auto timelineSplitterArea = bounds.removeFromBottom(splitterThickness);
    timelineResizeBar->setBounds(timelineSplitterArea);
    timelineShell->setBounds(timelineArea);

    clampLayoutValues(bounds.getWidth(), bounds.getHeight() + timelineHeight);

    auto inArea = bounds.removeFromLeft(leftPanelWidth);
    inPanel->setBounds(inArea);

    auto leftSplitterArea = bounds.removeFromLeft(splitterThickness);
    leftResizeBar->setBounds(leftSplitterArea);

    auto outArea = bounds.removeFromRight(rightPanelWidth);
    outPanel->setBounds(outArea);

    auto rightSplitterArea = bounds.removeFromRight(splitterThickness);
    rightResizeBar->setBounds(rightSplitterArea);

    workPanel->setBounds(bounds);

    auto editorBounds = workPanel->getLocalBounds().reduced(12);
    editorBounds.removeFromTop(42);
    theoryEditorPanel.setBounds(editorBounds);

    auto panelBounds = theoryEditorPanel.getLocalBounds().reduced(10);

    theoryEditorTitle.setBounds(panelBounds.removeFromTop(24));
    theoryEditorHint.setBounds(panelBounds.removeFromTop(20));
    panelBounds.removeFromTop(6);

    auto objectRow = panelBounds.removeFromTop(28);
    theoryObjectLabel.setBounds(objectRow.removeFromLeft(120));
    theoryObjectSelector.setBounds(objectRow);

    auto templateRow = panelBounds.removeFromTop(28);
    progressionTemplateLabel.setBounds(templateRow.removeFromLeft(120));
    progressionTemplateSelector.setBounds(templateRow);

    auto placeFieldRow = [&panelBounds](juce::Label& label, juce::TextEditor& editor)
    {
        auto row = panelBounds.removeFromTop(28);
        label.setBounds(row.removeFromLeft(220));
        editor.setBounds(row);
    };

    panelBounds.removeFromTop(6);
    placeFieldRow(theoryFieldLabel1, theoryFieldEditor1);
    placeFieldRow(theoryFieldLabel2, theoryFieldEditor2);
    placeFieldRow(theoryFieldLabel3, theoryFieldEditor3);
    placeFieldRow(theoryFieldLabel4, theoryFieldEditor4);
    placeFieldRow(theoryFieldLabel5, theoryFieldEditor5);

    panelBounds.removeFromTop(10);
    auto buttonRow = panelBounds.removeFromTop(30);
    reloadTheoryEditorButton.setBounds(buttonRow.removeFromRight(96));
    buttonRow.removeFromRight(6);
    applyTheoryEditorButton.setBounds(buttonRow.removeFromRight(140));
}

} // namespace setle::ui
