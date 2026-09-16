/*
  ==============================================================================

    VelocityCurve.h
    Created: 16 Sep 2026
    Description: Shapes incoming note-on velocities.

                 `velocityCurve` lived in the APVTS without being read anywhere.
                 Its default is Linear (the identity), so applying the curve does
                 not alter the response of any existing preset.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace NEURONiK::Midi
{

/** Number of choices in the velocityCurve parameter: Linear, Soft, Hard. */
constexpr int numVelocityCurves = 3;

/** @brief True when the index names a real curve. */
bool isVelocityCurveIndexValid (int curveIndex) noexcept;

/**
 * @brief Maps a normalised velocity through the selected curve.
 * @details Linear is the identity, Soft lifts low velocities (easier to play
 *          loud) with an exponent below 1, Hard requires more force with an
 *          exponent above 1. Results are clamped to 0..1 and never rounded down
 *          to zero for a sounding note, because a note-on with velocity 0 is a
 *          note-off and would leave the note stuck.
 */
float applyVelocityCurve (float velocity, int curveIndex) noexcept;

/**
 * @brief Rewrites note-on velocities in place.
 * @returns true when the buffer was modified.
 * @details Linear returns false and touches nothing, so the common case costs
 *          nothing on the audio thread. `scratch` is caller owned and reused to
 *          avoid allocations.
 */
bool shapeMidiVelocities (juce::MidiBuffer& midiMessages, int curveIndex, juce::MidiBuffer& scratch);

} // namespace NEURONiK::Midi
