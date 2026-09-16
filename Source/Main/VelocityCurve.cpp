/*
  ==============================================================================

    VelocityCurve.cpp

  ==============================================================================
*/

#include "VelocityCurve.h"

#include <cmath>

namespace NEURONiK::Midi
{
namespace
{
    /** Soft makes low velocities louder, Hard makes them quieter. */
    constexpr float softExponent = 0.6f;
    constexpr float hardExponent = 1.7f;

    /** A note-on must never be rounded down to 0: that would be read as a note-off. */
    constexpr float minimumSoundingVelocity = 1.0f / 127.0f;
}

bool isVelocityCurveIndexValid (int curveIndex) noexcept
{
    return curveIndex >= 0 && curveIndex < numVelocityCurves;
}

float applyVelocityCurve (float velocity, int curveIndex) noexcept
{
    const float clamped = juce::jlimit (0.0f, 1.0f, velocity);

    switch (curveIndex)
    {
        case 1:  return juce::jlimit (minimumSoundingVelocity, 1.0f, std::pow (clamped, softExponent));
        case 2:  return juce::jlimit (minimumSoundingVelocity, 1.0f, std::pow (clamped, hardExponent));
        case 0:
        default: return clamped;
    }
}

bool shapeMidiVelocities (juce::MidiBuffer& midiMessages, int curveIndex, juce::MidiBuffer& scratch)
{
    // Linear, an unknown index or an empty block: nothing to do.
    if (curveIndex <= 0 || ! isVelocityCurveIndexValid (curveIndex) || midiMessages.isEmpty())
        return false;

    bool changed = false;
    scratch.clear();

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        // Only sounding note-ons are shaped. isNoteOn() already excludes
        // velocity-0 messages, which are note-offs in disguise.
        if (! message.isNoteOn())
        {
            scratch.addEvent (message, metadata.samplePosition);
            continue;
        }

        const float shaped = applyVelocityCurve (message.getFloatVelocity(), curveIndex);

        if (shaped == message.getFloatVelocity())
        {
            scratch.addEvent (message, metadata.samplePosition);
            continue;
        }

        scratch.addEvent (juce::MidiMessage::noteOn (message.getChannel(),
                                                     message.getNoteNumber(),
                                                     shaped),
                          metadata.samplePosition);
        changed = true;
    }

    if (changed)
        midiMessages.swapWith (scratch);

    return changed;
}

} // namespace NEURONiK::Midi
