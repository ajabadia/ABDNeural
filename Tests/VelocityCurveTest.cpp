/*
  ==============================================================================

    VelocityCurveTest.cpp
    Created: 16 Sep 2026
    Description: Regression test for the velocityCurve parameter. Covers the
                 shapes themselves and the buffer rewrite, including the edge
                 case that would leave a note stuck: a very low velocity rounded
                 down to 0, which readers interpret as a note-off.

  ==============================================================================
*/

#include "../Source/Main/VelocityCurve.h"

#include <cmath>
#include <iostream>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  [ok]   " << description << '\n';
            return;
        }

        std::cout << "  [FAIL] " << description << '\n';
        ++failures;
    }

    void checkClose (double actual, double expected, const juce::String& description)
    {
        check (std::abs (actual - expected) < 1.0e-4,
               description + " (got " + juce::String (actual, 5) + ")");
    }
}

int main()
{
    using namespace NEURONiK::Midi;

    std::cout << "NEURONiK velocity curve\n";

    // --- Shapes ------------------------------------------------------------------
    std::cout << "\nCurve shapes\n";

    check (numVelocityCurves == 3, "there are three curves: Linear, Soft, Hard");
    check (isVelocityCurveIndexValid (0), "Linear is a valid index");
    check (isVelocityCurveIndexValid (2), "Hard is a valid index");
    check (! isVelocityCurveIndexValid (3), "index past Hard is invalid");
    check (! isVelocityCurveIndexValid (-1), "a negative index is invalid");

    // Linear is the identity, which is why connecting the parameter is safe.
    for (float velocity = 0.0f; velocity <= 1.0f; velocity += 0.25f)
        checkClose (applyVelocityCurve (velocity, 0), velocity, "Linear is the identity");

    // Soft lifts low velocities, Hard lowers them, and both pass through the ends.
    check (applyVelocityCurve (0.25f, 1) > 0.25f, "Soft lifts a low velocity");
    check (applyVelocityCurve (0.25f, 2) < 0.25f, "Hard lowers a low velocity");
    checkClose (applyVelocityCurve (1.0f, 1), 1.0f, "Soft leaves full velocity alone");
    checkClose (applyVelocityCurve (1.0f, 2), 1.0f, "Hard leaves full velocity alone");
    checkClose (applyVelocityCurve (0.5f, 0), 0.5f, "Linear leaves the midpoint alone");

    // Monotonic: a harder hit is never quieter than a softer one.
    bool monotonic = true;
    for (float velocity = 0.01f; velocity < 1.0f; velocity += 0.01f)
        for (int curve = 0; curve < numVelocityCurves; ++curve)
            if (applyVelocityCurve (velocity, curve) > applyVelocityCurve (velocity + 0.01f, curve))
                monotonic = false;

    check (monotonic, "every curve is monotonic");

    check (applyVelocityCurve (-3.0f, 1) >= 0.0f, "a negative velocity is clamped");
    check (applyVelocityCurve (9.0f, 2) <= 1.0f, "an out-of-range velocity is clamped");
    check (applyVelocityCurve (0.01f, 2) > 0.0f, "Hard never returns exactly zero for a sounding note");

    // --- Buffer rewrite ----------------------------------------------------------
    std::cout << "\nBuffer rewrite\n";

    juce::MidiBuffer scratch;
    const auto noteOn = juce::MidiMessage::noteOn (2, 60, 0.25f);
    const auto noteOff = juce::MidiMessage::noteOff (2, 60);

    {
        juce::MidiBuffer buffer;
        buffer.addEvent (noteOn, 0);
        check (! shapeMidiVelocities (buffer, 0, scratch), "Linear reports no change");

        // JUCE quantises the velocity to a byte, so the reference value is what the
        // message actually stores (32/127), not the float that was asked for.
        checkClose ((*buffer.begin()).getMessage().getFloatVelocity(),
                    noteOn.getFloatVelocity(),
                    "Linear leaves velocities untouched");
    }

    {
        juce::MidiBuffer buffer;
        buffer.addEvent (noteOn, 0);
        buffer.addEvent (noteOff, 40);

        check (shapeMidiVelocities (buffer, 1, scratch), "Soft reports a change");
        check (buffer.getNumEvents() == 2, "both events survive the rewrite");

        auto iterator = buffer.begin();
        const auto shaped = (*iterator).getMessage();
        check (shaped.isNoteOn() && shaped.getNoteNumber() == 60 && shaped.getChannel() == 2,
               "the note identity is preserved");
        check (shaped.getFloatVelocity() > noteOn.getFloatVelocity(),
               "the note-on velocity was lifted");
        checkClose ((*iterator).samplePosition, 0.0f, "the first event keeps its sample position");

        ++iterator;
        const auto untouched = (*iterator).getMessage();
        check (untouched.isNoteOff(), "the note-off is still a note-off");
        checkClose ((*iterator).samplePosition, 40.0f, "the note-off keeps its sample position");
    }

    {
        // The trap: a note-on shaped down to velocity 0 reads as a note-off.
        juce::MidiBuffer buffer;
        buffer.addEvent (juce::MidiMessage::noteOn (1, 48, 0.01f), 0);

        shapeMidiVelocities (buffer, 2, scratch);

        const auto message = (*buffer.begin()).getMessage();
        check (message.isNoteOn(), "a hard-shaped light note still reads as a note-on");
        check (message.getVelocity() > 0, "its velocity is at least 1/127");
    }

    {
        juce::MidiBuffer buffer;
        buffer.addEvent (juce::MidiMessage::midiClock(), 0);
        buffer.addEvent (juce::MidiMessage::controllerEvent (2, 74, 90), 5);

        check (! shapeMidiVelocities (buffer, 1, scratch), "non-note messages report no change");
        check (buffer.getNumEvents() == 2, "non-note messages survive untouched");
    }

    {
        juce::MidiBuffer buffer;
        buffer.addEvent (noteOn, 0);

        check (! shapeMidiVelocities (buffer, 7, scratch), "an unknown index is refused");
        checkClose ((*buffer.begin()).getMessage().getFloatVelocity(),
                    noteOn.getFloatVelocity(),
                    "an unknown index leaves velocities untouched");
    }

    {
        juce::MidiBuffer empty;
        check (! shapeMidiVelocities (empty, 1, scratch), "an empty buffer reports no change");
    }

    {
        // Reusing the scratch buffer across blocks must not leak old events.
        juce::MidiBuffer first;
        first.addEvent (noteOn, 0);
        shapeMidiVelocities (first, 1, scratch);

        juce::MidiBuffer second;
        second.addEvent (juce::MidiMessage::noteOn (2, 61, 0.5f), 0);
        shapeMidiVelocities (second, 1, scratch);

        check (second.getNumEvents() == 1, "scratch reuse does not resurrect old events");
    }

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
