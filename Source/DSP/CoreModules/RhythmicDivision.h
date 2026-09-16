/*
  ==============================================================================

    RhythmicDivision.h
    Created: 16 Sep 2026
    Description: Single source of truth for the rhythmic division used by
                 tempo-synced modulations.

                 The choice lists of the *RhythmicDivision parameters (LFO 1/2
                 today, the delay next) must stay in the same order as the table
                 below. Indexes are validated, so a mismatched preset cannot push
                 an out-of-range value into the DSP.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace NEURONiK::DSP::Core
{

/** @brief Number of entries in the *RhythmicDivision choice parameters. */
constexpr int numRhythmicDivisions = 9;

/**
 * @brief Division lengths in quarter notes (1.0 = 1/4 note).
 * @details Order matches { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
 *          "1/4t", "1/8t", "1/16t" }. Triplets are two thirds of their plain
 *          counterpart, so "1/4t" is 2/3 of a quarter note.
 */
inline constexpr float rhythmicDivisionInQuarterNotes[numRhythmicDivisions] = {
    4.0f,              // 1/1
    2.0f,              // 1/2
    1.0f,              // 1/4
    0.5f,              // 1/8
    0.25f,             // 1/16
    0.125f,            // 1/32
    2.0f / 3.0f,       // 1/4t
    1.0f / 3.0f,       // 1/8t
    1.0f / 6.0f        // 1/16t
};

/** @brief Length of a division in quarter notes. Invalid indexes fall back to 1/4. */
inline float quarterNotesForDivision (int divisionIndex) noexcept
{
    if (divisionIndex < 0 || divisionIndex >= numRhythmicDivisions)
        return 1.0f;

    return rhythmicDivisionInQuarterNotes[divisionIndex];
}

/** @brief Length of a division in seconds at the given tempo. */
inline double secondsForDivision (int divisionIndex, double bpm) noexcept
{
    const double safeBpm = bpm > 1.0 ? bpm : 120.0;
    return (60.0 / safeBpm) * static_cast<double> (quarterNotesForDivision (divisionIndex));
}

} // namespace NEURONiK::DSP::Core
