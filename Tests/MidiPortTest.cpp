/*
  ==============================================================================

    MidiPortTest.cpp
    Description: Anti-drift test of the JUCE-free MIDI ports (Fase 1, paso 4/6).

    dsp::MidiMessage / dsp::MidiBuffer must answer EXACTLY like their JUCE
    originals for the subset the engine consumes, and Runtime::JuceMidiAdapter
    must copy channel/voice events without changing a single byte, order or
    sample position. Anything else would silently change the audio, because the
    engine routes notes with these predicates and quantises velocity with
    floatValueToMidiByte.

    The byte-space sweep is the important one: it compares every predicate and
    (where the message is well formed) every getter against juce::MidiMessage
    over all channel/voice statuses and the system range.

  ==============================================================================
*/

#include "../Source/DSP/DspMidiBuffer.h"
#include "../Source/DSP/Runtime/JuceMidiAdapter.h"

#include <iostream>
#include <vector>

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

    // --- Comparadores ------------------------------------------------------------

    /** Predicados y getters que JUCE define para CUALQUIER mensaje; getNoteNumber
        solo se compara cuando hay al menos dos bytes (en JUCE leer mas alla del
        tamano es memoria sin inicializar). */
    bool sameFlags (const dsp::MidiMessage& d, const juce::MidiMessage& j, int size)
    {
        const bool base = d.getChannel() == j.getChannel()
                       && d.isNoteOn() == j.isNoteOn()
                       && d.isNoteOff() == j.isNoteOff()
                       && d.isNoteOnOrOff() == j.isNoteOnOrOff()
                       && d.getVelocity() == j.getVelocity()
                       && d.getFloatVelocity() == j.getFloatVelocity()
                       && d.isPitchWheel() == j.isPitchWheel()
                       && d.isAftertouch() == j.isAftertouch()
                       && d.isChannelPressure() == j.isChannelPressure()
                       && d.isController() == j.isController();

        if (! base)
            return false;

        if (size >= 2 && d.getNoteNumber() != j.getNoteNumber())
            return false;

        if (d.isPitchWheel() && d.getPitchWheelValue() != j.getPitchWheelValue())
            return false;

        if (d.isAftertouch() && d.getAfterTouchValue() != j.getAfterTouchValue())
            return false;

        if (d.isChannelPressure() && d.getChannelPressureValue() != j.getChannelPressureValue())
            return false;

        if (d.isController() && (d.getControllerNumber() != j.getControllerNumber()
                                  || d.getControllerValue() != j.getControllerValue()))
            return false;

        return true;
    }

    bool sameBytes (const dsp::MidiMessage& d, const juce::MidiMessage& j)
    {
        if (d.getRawDataSize() != j.getRawDataSize())
            return false;

        for (int i = 0; i < d.getRawDataSize(); ++i)
            if (d.getRawData()[i] != j.getRawData()[i])
                return false;

        return true;
    }

    /** Longitud de un mensaje de canal/voz segun su status (tabla de JUCE). */
    int channelMessageLength (int status)
    {
        return (status & 0xf0) == 0xc0 || (status & 0xf0) == 0xd0 ? 2 : 3;
    }
}

int main()
{
    std::cout << "NEURONiK MIDI ports (dsp::MidiMessage / dsp::MidiBuffer)\n";

    // --- 1. Anti-drift sobre todo el espacio de bytes ----------------------------
    std::cout << "\nByte-space anti-drift\n";

    {
        const int dataBytes[] = { 0, 1, 63, 64, 127 };
        int cases = 0;
        int mismatches = 0;
        juce::String firstMismatch;

        for (int status = 0x80; status <= 0xef; ++status)
        {
            const int size = channelMessageLength (status);

            for (int d1 : dataBytes)
            {
                for (int d2 : dataBytes)
                {
                    const uint8_t bytes[3] = { (uint8_t) status, (uint8_t) d1, (uint8_t) d2 };
                    const dsp::MidiMessage d (bytes, size);
                    const juce::MidiMessage j (bytes, size);
                    ++cases;

                    if (! sameFlags (d, j, size))
                    {
                        ++mismatches;

                        if (firstMismatch.isEmpty())
                            firstMismatch = "status 0x" + juce::String::toHexString (status)
                                          + " data " + juce::String (d1) + "/" + juce::String (d2);
                    }
                }
            }
        }

        // System messages (0xF0..0xFF) are never engine events, but the predicates
        // must still agree; JUCE allows 1..3 bytes for them.
        for (int status = 0xf0; status <= 0xff; ++status)
        {
            for (int size = 1; size <= 3; ++size)
            {
                const uint8_t bytes[3] = { (uint8_t) status, 60, 100 };
                const dsp::MidiMessage d (bytes, size);
                const juce::MidiMessage j (bytes, size);
                ++cases;

                if (! sameFlags (d, j, size))
                {
                    ++mismatches;

                    if (firstMismatch.isEmpty())
                        firstMismatch = "system status 0x" + juce::String::toHexString (status)
                                      + " size " + juce::String (size);
                }
            }
        }

        // Default message: JUCE initialises it as an empty sysex {0xF0, 0xF7}.
        {
            const dsp::MidiMessage d;
            const juce::MidiMessage j;
            ++cases;

            if (! (sameFlags (d, j, 2) && sameBytes (d, j)))
                ++mismatches;
        }

        check (mismatches == 0,
               "predicates and getters coincide with juce::MidiMessage over "
                 + juce::String (cases) + " byte patterns"
                 + (mismatches == 0 ? juce::String() : " (first: " + firstMismatch + ")"));
    }

    // --- 2. Semantica de los predicados que usa el motor -------------------------
    std::cout << "\nRouting predicates\n";

    {
        const auto noteOnVel0 = dsp::MidiMessage::noteOn (1, 60, 0.0f);
        check (noteOnVel0.isNoteOn() == false, "note-on with velocity 0 is not a note-on");
        check (noteOnVel0.isNoteOff() == true, "note-on with velocity 0 IS a note-off (JUCE default)");
        check (dsp::MidiMessage::noteOn (1, 60, 1.0f).isNoteOn(), "note-on with velocity is a note-on");
        check (dsp::MidiMessage::noteOn (1, 60, 1.0f).isNoteOff() == false, "velocity > 0 is not a note-off");

        const auto cc74 = dsp::MidiMessage::controllerEvent (3, 74, 100);
        check (cc74.isController() && cc74.getControllerNumber() == 74, "CC 74 is the timbre controller");
        check (cc74.getControllerValue() == 100, "CC value survives");

        const auto bend = dsp::MidiMessage::pitchWheel (3, 9000);
        check (bend.isPitchWheel() && bend.getPitchWheelValue() == 9000, "14-bit pitch wheel survives");
        check (dsp::MidiMessage().getChannel() == 0, "the default (sysex) message has no channel");
    }

    // --- 3. Factorias byte a byte ------------------------------------------------
    std::cout << "\nFactory byte-level anti-drift\n";

    {
        int cases = 0;
        int mismatches = 0;

        for (int channel = 1; channel <= 16; ++channel)
        {
            for (int note = 0; note < 128; note += 7)
            {
                struct Pair { dsp::MidiMessage d; juce::MidiMessage j; };

                const std::vector<Pair> pairs {
                    { dsp::MidiMessage::noteOn (channel, note, (uint8_t) 100),
                      juce::MidiMessage::noteOn (channel, note, (uint8_t) 100) },
                    { dsp::MidiMessage::noteOff (channel, note, (uint8_t) 64),
                      juce::MidiMessage::noteOff (channel, note, (uint8_t) 64) },
                    { dsp::MidiMessage::pitchWheel (channel, note * 128),
                      juce::MidiMessage::pitchWheel (channel, note * 128) },
                    { dsp::MidiMessage::channelPressureChange (channel, note),
                      juce::MidiMessage::channelPressureChange (channel, note) },
                    { dsp::MidiMessage::aftertouchChange (channel, note, 42),
                      juce::MidiMessage::aftertouchChange (channel, note, 42) },
                    { dsp::MidiMessage::controllerEvent (channel, 74, note),
                      juce::MidiMessage::controllerEvent (channel, 74, note) }
                };

                for (const auto& pair : pairs)
                {
                    ++cases;

                    if (! sameBytes (pair.d, pair.j))
                        ++mismatches;
                }
            }
        }

        check (mismatches == 0,
               "the six factories produce the same raw bytes as JUCE over "
                 + juce::String (cases) + " cases");
    }

    // --- 4. Cuantizacion de velocity (float -> byte) -----------------------------
    std::cout << "\nVelocity quantisation\n";

    {
        int cases = 0;
        int mismatches = 0;

        for (int i = 0; i <= 2540; ++i)   // 0..1 en pasos de 1/2540 (incluye empates)
        {
            const float v = (float) i / 2540.0f;
            ++cases;

            if (dsp::MidiMessage::floatValueToMidiByte (v) != juce::MidiMessage::floatValueToMidiByte (v))
                ++mismatches;
        }

        check (mismatches == 0,
               "floatValueToMidiByte matches JUCE over " + juce::String (cases) + " values");

        // Los empates son justo donde std::lround se desviaria: 0.5 * 127 = 63.5.
        check (dsp::MidiMessage::floatValueToMidiByte (0.5f)
                 == juce::MidiMessage::floatValueToMidiByte (0.5f),
               "the 0.5 tie (63.5) rounds like JUCE, not like std::lround");

        int roundMismatches = 0;

        for (int i = -1000; i <= 1000; ++i)
            if (dsp::roundToInt ((float) i * 0.5f) != juce::roundToInt ((float) i * 0.5f))
                ++roundMismatches;

        check (roundMismatches == 0, "dsp::roundToInt matches juce::roundToInt on half-integers");
    }

    // --- 5. Frecuencia de nota ---------------------------------------------------
    std::cout << "\nNote frequency\n";

    {
        int mismatches = 0;

        for (int note = 0; note < 128; ++note)
            if (dsp::MidiMessage::getMidiNoteInHertz (note) != juce::MidiMessage::getMidiNoteInHertz (note))
                ++mismatches;

        check (mismatches == 0, "getMidiNoteInHertz matches JUCE for the 128 notes");
        check (dsp::MidiMessage::getMidiNoteInHertz (69) == 440.0, "A4 is still 440 Hz");
    }

    // --- 6. dsp::MidiBuffer ------------------------------------------------------
    std::cout << "\ndsp::MidiBuffer\n";

    {
        dsp::MidiBuffer d;
        juce::MidiBuffer j;

        const int positions[] = { 10, 0, 6, 6, 3 };

        for (int position : positions)
        {
            d.addEvent (dsp::MidiMessage::noteOn (1, 60, 0.5f), position);
            j.addEvent (juce::MidiMessage::noteOn (1, 60, 0.5f), position);
        }

        check (d.getNumEvents() == j.getNumEvents(), "same number of events");

        auto di = d.begin();
        int compared = 0;
        bool identical = true;

        for (const auto metadata : j)
        {
            if (di == d.end())
            {
                identical = false;
                break;
            }

            const auto dm = *di;

            if (dm.samplePosition != metadata.samplePosition
                 || dm.numBytes != metadata.numBytes
                 || ! sameBytes (dm.getMessage(), metadata.getMessage()))
                identical = false;

            ++di;
            ++compared;
        }

        check (identical && compared == j.getNumEvents(),
               "events keep JUCE's sorted order, positions and bytes");

        check (d.getFirstEventTime() == j.getFirstEventTime()
                 && d.getFirstEventTime() == 0 && d.getNumEvents() == 5,
               "out-of-order insertion is sorted by sample position, ties stay FIFO");

        d.clear();
        check (d.isEmpty() && d.getNumEvents() == 0, "clear() empties the buffer");
        check (d.getFirstEventTime() == 0, "an empty buffer reports time 0");
    }

    // --- 7. Adapter JUCE -> dsp --------------------------------------------------
    std::cout << "\nJuceMidiAdapter\n";

    {
        using namespace NEURONiK::DSP::Runtime;

        check (isEngineMidiEvent (nullptr, 0) == false, "a null event is rejected");
        check (isEngineMidiEvent ((const uint8_t*) "\x90\x3c\x64", 3), "note-on is an engine event");
        check (isEngineMidiEvent ((const uint8_t*) "\xd0\x40", 2), "channel pressure is an engine event");
        check (isEngineMidiEvent ((const uint8_t*) "\x90\x3c\x64\x00", 4) == false, "more than 3 bytes is rejected");
        check (isEngineMidiEvent ((const uint8_t*) "\x40\x00\x00", 3) == false, "a missing status byte is rejected");
        check (isEngineMidiEvent ((const uint8_t*) "\xf0\x01\x02", 3) == false, "sysex is not an engine event");
        check (isEngineMidiEvent ((const uint8_t*) "\xf8\x00\x00", 1) == false, "realtime is not an engine event");

        juce::MidiBuffer source;
        source.addEvent (juce::MidiMessage::noteOn (3, 60, 0.8f), 4);
        source.addEvent (juce::MidiMessage::midiClock(), 0);
        source.addEvent (juce::MidiMessage::controllerEvent (3, 74, 100), 2);
        source.addEvent (juce::MidiMessage::createSysExMessage ("x", 1), 1);
        source.addEvent (juce::MidiMessage::pitchWheel (3, 9000), 8);

        dsp::MidiBuffer destination;
        const int copied = copyToDspMidiBuffer (source, destination);

        check (copied == 3, "only the three channel/voice events are copied");
        check (destination.getNumEvents() == 3, "the destination holds exactly those events");

        const int expectedPositions[] = { 2, 4, 8 };
        int index = 0;
        bool positionsOk = true;

        for (const auto metadata : destination)
        {
            if (index >= 3 || metadata.samplePosition != expectedPositions[index])
                positionsOk = false;

            ++index;
        }

        check (positionsOk && index == 3, "block positions and order are preserved");

        // Reusing the destination must not resurrect the previous block's events.
        juce::MidiBuffer second;
        second.addEvent (juce::MidiMessage::noteOff (3, 60, 0.5f), 1);
        copyToDspMidiBuffer (second, destination);

        check (destination.getNumEvents() == 1, "reuse clears the previous block's events");
        check (destination.getFirstEventTime() == 1, "the new block's position survives");
    }

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
