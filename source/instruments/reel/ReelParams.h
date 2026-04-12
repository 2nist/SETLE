#pragma once

//==============================================================================
/**
    ReelParams — authoritative parameter data for the REEL instrument.

    All param IDs used by ReelProcessor::setParam / getParam match the
    string keys defined as comments on each field.  Serialised to/from
    juce::ValueTree in ReelProcessor::getState / setState.
*/

enum class ReelMode { loop, sample, grain, slice };

struct ReelParams
{
    // ── Mode ──────────────────────────────────────────────────────────────
    ReelMode mode { ReelMode::loop };   // "mode.current"

    // ── Playback (shared) ─────────────────────────────────────────────────
    struct Play
    {
        float start   { 0.0f };     // "play.start"   0-1 normalised
        float end     { 1.0f };     // "play.end"     0-1 normalised
        float level   { 1.0f };     // "play.level"
        float pan     { 0.0f };     // "play.pan"     -1 to +1
        bool  reverse { false };    // "play.reverse"
    } play;

    // ── Loop mode ──────────────────────────────────────────────────────────
    struct Loop
    {
        float speed { 1.0f };       // "loop.speed"   0.25 - 4.0
        bool  sync  { false };      // "loop.sync"
        float pitch { 0.0f };       // "loop.pitch"   semitones -24..+24
    } loop;

    // ── Sample mode ────────────────────────────────────────────────────────
    struct Sample
    {
        int  root    { 60 };        // "sample.root"   MIDI note
        bool oneshot { false };     // "sample.oneshot"
        int  interp  { 0 };         // "sample.interp" 0=linear 1=cubic
    } sample;

    // ── Grain mode ─────────────────────────────────────────────────────────
    struct Grain
    {
        float position { 0.5f };    // "grain.position"  0-1
        float size     { 80.0f };   // "grain.size"      ms
        float density  { 20.0f };   // "grain.density"   grains/sec
        float pitch    { 0.0f };    // "grain.pitch"     semitones
        float spread   { 0.3f };    // "grain.spread"    0-1
        float scatter  { 0.2f };    // "grain.scatter"   0-1
        int   envelope { 0 };       // "grain.envelope"  0=hann 1=linear 2=gaussian
    } grain;

    // ── Slice mode ─────────────────────────────────────────────────────────
    struct Slice
    {
        int  count    { 8 };        // "slice.count"    2-64
        int  order    { 0 };        // "slice.order"    0=fwd 1=rev 2=rand 3=shuffle
        bool quantize { false };    // "slice.quantize"
    } slice;

    // ── Output ─────────────────────────────────────────────────────────────
    struct Out
    {
        float level { 1.0f };       // "out.level"
        float pan   { 0.0f };       // "out.pan"
    } out;

    //=========================================================================
    // Helpers

    float getFloat (const juce::String& id) const noexcept
    {
        if (id == "play.start")      return play.start;
        if (id == "play.end")        return play.end;
        if (id == "play.level")      return play.level;
        if (id == "play.pan")        return play.pan;
        if (id == "play.reverse")    return play.reverse ? 1.0f : 0.0f;
        if (id == "loop.speed")      return loop.speed;
        if (id == "loop.pitch")      return loop.pitch;
        if (id == "loop.sync")       return loop.sync ? 1.0f : 0.0f;
        if (id == "sample.root")     return static_cast<float> (sample.root);
        if (id == "sample.oneshot")  return sample.oneshot ? 1.0f : 0.0f;
        if (id == "grain.position")  return grain.position;
        if (id == "grain.size")      return grain.size;
        if (id == "grain.density")   return grain.density;
        if (id == "grain.pitch")     return grain.pitch;
        if (id == "grain.spread")    return grain.spread;
        if (id == "grain.scatter")   return grain.scatter;
        if (id == "grain.envelope")  return static_cast<float> (grain.envelope);
        if (id == "slice.count")     return static_cast<float> (slice.count);
        if (id == "slice.order")     return static_cast<float> (slice.order);
        if (id == "slice.quantize")  return slice.quantize ? 1.0f : 0.0f;
        if (id == "out.level")       return out.level;
        if (id == "out.pan")         return out.pan;
        return 0.0f;
    }

    void setFloat (const juce::String& id, float v) noexcept
    {
        if (id == "play.start")      { play.start    = juce::jlimit (0.0f, 1.0f, v); return; }
        if (id == "play.end")        { play.end      = juce::jlimit (0.0f, 1.0f, v); return; }
        if (id == "play.level")      { play.level    = juce::jlimit (0.0f, 1.0f, v); return; }
        if (id == "play.pan")        { play.pan      = juce::jlimit (-1.0f, 1.0f, v); return; }
        if (id == "play.reverse")    { play.reverse  = v >= 0.5f; return; }
        if (id == "loop.speed")      { loop.speed    = juce::jlimit (0.25f, 4.0f, v); return; }
        if (id == "loop.pitch")      { loop.pitch    = juce::jlimit (-24.0f, 24.0f, v); return; }
        if (id == "loop.sync")       { loop.sync     = v >= 0.5f; return; }
        if (id == "sample.root")     { sample.root   = juce::jlimit (0, 127, static_cast<int> (v)); return; }
        if (id == "sample.oneshot")  { sample.oneshot = v >= 0.5f; return; }
        if (id == "grain.position")  { grain.position = juce::jlimit (0.0f, 1.0f, v); return; }
        if (id == "grain.size")      { grain.size    = juce::jlimit (10.0f, 500.0f, v); return; }
        if (id == "grain.density")   { grain.density = juce::jlimit (1.0f, 100.0f, v); return; }
        if (id == "grain.pitch")     { grain.pitch   = juce::jlimit (-24.0f, 24.0f, v); return; }
        if (id == "grain.spread")    { grain.spread  = juce::jlimit (0.0f, 1.0f, v); return; }
        if (id == "grain.scatter")   { grain.scatter = juce::jlimit (0.0f, 1.0f, v); return; }
        if (id == "grain.envelope")  { grain.envelope = juce::jlimit (0, 2, static_cast<int> (v)); return; }
        if (id == "slice.count")     { slice.count   = juce::jlimit (2, 64, static_cast<int> (v)); return; }
        if (id == "slice.order")     { slice.order   = juce::jlimit (0, 3, static_cast<int> (v)); return; }
        if (id == "slice.quantize")  { slice.quantize = v >= 0.5f; return; }
        if (id == "out.level")       { out.level     = juce::jlimit (0.0f, 1.0f, v); return; }
        if (id == "out.pan")         { out.pan       = juce::jlimit (-1.0f, 1.0f, v); return; }
    }
};

//==============================================================================
/** Default Zone B parameter mappings per mode. */
struct ReelDefaultMapping
{
    struct Slot { const char* paramId; const char* label; float min; float max; float def; };

    static const Slot* forMode (ReelMode mode)
    {
        static const Slot kLoop[] = {
            { "play.start",   "START",   0.0f,  1.0f,  0.0f },
            { "play.end",     "END",     0.0f,  1.0f,  1.0f },
            { "loop.speed",   "SPEED",   0.25f, 4.0f,  1.0f },
            { "loop.pitch",   "PITCH",  -24.0f, 24.0f, 0.0f },
            { "out.level",    "LEVEL",   0.0f,  1.0f,  1.0f },
            { "out.pan",      "PAN",    -1.0f,  1.0f,  0.0f },
            { "play.start",   "STRT2",   0.0f,  1.0f,  0.0f },
            { "play.reverse", "REV",     0.0f,  1.0f,  0.0f },
        };
        static const Slot kGrain[] = {
            { "grain.position", "POSN",  0.0f,  1.0f,  0.5f },
            { "grain.size",     "SIZE",  10.0f, 500.0f, 80.0f },
            { "grain.density",  "DENS",  1.0f, 100.0f, 20.0f },
            { "grain.pitch",    "PITCH",-24.0f, 24.0f,  0.0f },
            { "grain.scatter",  "SCAT",  0.0f,  1.0f,  0.2f },
            { "grain.spread",   "SPRD",  0.0f,  1.0f,  0.3f },
            { "out.level",      "LEVEL", 0.0f,  1.0f,  1.0f },
            { "out.pan",        "PAN",  -1.0f,  1.0f,  0.0f },
        };
        static const Slot kSample[] = {
            { "play.start",    "START",   0.0f,  1.0f,  0.0f },
            { "play.end",      "END",     0.0f,  1.0f,  1.0f },
            { "grain.pitch",   "PITCH",  -24.0f, 24.0f, 0.0f },
            { "out.level",     "LEVEL",   0.0f,  1.0f,  1.0f },
            { "out.pan",       "PAN",    -1.0f,  1.0f,  0.0f },
            { "loop.speed",    "SPEED",   0.25f, 4.0f,  1.0f },
            { "sample.root",   "ROOT",    0.0f, 127.0f, 60.0f },
            { "play.reverse",  "REV",     0.0f,  1.0f,  0.0f },
        };
        static const Slot kSlice[] = {
            { "slice.count",   "SLICES",  2.0f, 64.0f,  8.0f },
            { "slice.order",   "ORDER",   0.0f,  3.0f,  0.0f },
            { "out.level",     "LEVEL",   0.0f,  1.0f,  1.0f },
            { "out.pan",       "PAN",    -1.0f,  1.0f,  0.0f },
            { "play.start",    "START",   0.0f,  1.0f,  0.0f },
            { "play.end",      "END",     0.0f,  1.0f,  1.0f },
            { "grain.scatter", "SCAT",    0.0f,  1.0f,  0.0f },
            { "grain.pitch",   "PITCH",  -24.0f, 24.0f, 0.0f },
        };

        switch (mode)
        {
            case ReelMode::loop:   return kLoop;
            case ReelMode::grain:  return kGrain;
            case ReelMode::sample: return kSample;
            case ReelMode::slice:  return kSlice;
        }
        return kLoop;
    }

    static constexpr int kNumSlots = 8;
};
