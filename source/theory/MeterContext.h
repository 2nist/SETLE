#pragma once

#include <juce_core/juce_core.h>

namespace setle::model
{
class Song;
} // namespace setle::model

namespace setle::theory
{

struct MeterContext
{
    int numerator   { 4 };
    int denominator { 4 };

    // Beats per bar in quarter notes
    // 4/4: 4.0   5/4: 5.0   7/8: 3.5   11/8: 5.5   6/8: 3.0
    double beatsPerBar() const noexcept
    {
        return (numerator * 4.0) / denominator;
    }

    // Duration of one denominator unit in quarter note beats
    // 4/4: 1.0 (quarter note)
    // 7/8: 0.5 (eighth note)
    // 11/8: 0.5 (eighth note)
    // 5/4: 1.0 (quarter note)
    // 6/8: 0.5 (eighth note)
    double beatUnit() const noexcept
    {
        return 4.0 / denominator;
    }

// Steps per bar at eighth-note resolution
    // 4/4: 8 (4 quarter * 2)
    // 11/8: 11 (11 denominator-unit eighths)
    // 7/8: 7
    // 5/4: 10 (5 quarter * 2)
    // 6/8: 6
    int stepsPerBarEighths() const noexcept
    {
        // Formula: how many eighth notes fit in numerator denominator-units?
        // denominator=8 (eighth note): numerator eighths = numerator
        // denominator=4 (quarter note): numerator quarters = numerator*2 eighths
        // denominator=2 (half note): numerator halves = numerator*4 eighths
        // denominator=1 (whole note): numerator wholes = numerator*8 eighths
        return (numerator * 4) / denominator;
    }

    // Steps per bar at sixteenth-note resolution
    int stepsPerBarSixteenths() const noexcept
    {
        return stepsPerBarEighths() * 2;
    }

    bool isCompound() const noexcept
    {
        // 6/8, 9/8, 12/8 are compound — grouped in threes
        return denominator == 8 && numerator % 3 == 0;
    }

    bool isSimple() const noexcept
    {
        return denominator == 4 || denominator == 2 || denominator == 1;
    }

    // Display string: "11/8", "4/4", "7/8"
    juce::String toString() const
    {
        return juce::String(numerator) + "/" + juce::String(denominator);
    }

    // Get the meter context for the section containing a given beat position
    static MeterContext forBeat(double beat, const setle::model::Song& song);
};

} // namespace setle::theory
