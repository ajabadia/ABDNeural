/*
  ==============================================================================

    MidiChannelFilterTest.cpp
    Created: 16 Sep 2026
    Description: Regression test for the midiChannel input filter, including the
                 cases that used to break silently: system messages being dropped
                 and Omni filtering anyway.

  ==============================================================================
*/

#include "../Source/Main/MidiChannelFilter.h"

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

    int messagesPassing (const juce::MidiBuffer& buffer)
    {
        return buffer.getNumEvents();
    }
}

int main()
{
    using namespace NEURONiK::Midi;

    std::cout << "NEURONiK MIDI channel filter\n";

    // --- Choice index mapping ----------------------------------------------------
    std::cout << "\nChoice mapping\n";

    check (channelFromChoiceIndex (0) == 0, "index 0 is Omni");
    check (channelFromChoiceIndex (1) == 1, "index 1 is channel 1");
    check (channelFromChoiceIndex (16) == 16, "index 16 is channel 16");
    check (channelFromChoiceIndex (-1) == 0, "negative index falls back to Omni");
    check (channelFromChoiceIndex (99) == 0, "out-of-range index falls back to Omni");
    check (numChannelChoices == 17, "there are 17 channel choices");

    // --- Per message decisions ---------------------------------------------------
    std::cout << "\nPer message decisions\n";

    const auto noteOnCh3 = juce::MidiMessage::noteOn (3, 60, 0.8f);
    const auto noteOffCh3 = juce::MidiMessage::noteOff (3, 60);
    const auto noteOnCh5 = juce::MidiMessage::noteOn (5, 60, 0.8f);
    const auto pitchBendCh3 = juce::MidiMessage::pitchWheel (3, 9000);
    const auto sysex = juce::MidiMessage::createSysExMessage ("test", 4);
    const auto clock = juce::MidiMessage::midiClock();

    check (passesChannelFilter (noteOnCh3, 0), "Omni passes a note on any channel");
    check (passesChannelFilter (noteOnCh3, 3), "matching channel passes");
    check (! passesChannelFilter (noteOnCh5, 3), "other channel is rejected");
    check (! passesChannelFilter (pitchBendCh3, 4), "bend from another channel is rejected");
    check (passesChannelFilter (sysex, 3), "sysex always passes");
    check (passesChannelFilter (clock, 3), "MIDI clock always passes");

    // --- Buffer filtering --------------------------------------------------------
    std::cout << "\nBuffer filtering\n";

    juce::MidiBuffer scratch;

    {
        juce::MidiBuffer buffer;
        buffer.addEvent (noteOnCh3, 0);
        buffer.addEvent (noteOnCh5, 10);
        buffer.addEvent (noteOffCh3, 20);
        buffer.addEvent (clock, 30);

        filterMidiBuffer (buffer, 3, scratch);

        check (messagesPassing (buffer) == 3,
               "channel 3 keeps its two messages plus the clock (got "
                   + juce::String (messagesPassing (buffer)) + ")");

        bool onlyChannel3 = true;
        for (const auto metadata : buffer)
        {
            const auto message = metadata.getMessage();
            if (message.getChannel() != 0 && message.getChannel() != 3)
                onlyChannel3 = false;
        }

        check (onlyChannel3, "no foreign channel survives the filter");
    }

    {
        juce::MidiBuffer buffer;
        buffer.addEvent (noteOnCh5, 0);
        buffer.addEvent (noteOnCh5, 5);

        filterMidiBuffer (buffer, 3, scratch);

        check (messagesPassing (buffer) == 0, "a foreign channel alone is emptied");
    }

    {
        juce::MidiBuffer buffer;
        buffer.addEvent (noteOnCh3, 0);
        buffer.addEvent (noteOnCh5, 5);

        filterMidiBuffer (buffer, 0, scratch);

        check (messagesPassing (buffer) == 2, "Omni leaves the buffer untouched");
    }

    {
        juce::MidiBuffer empty;
        filterMidiBuffer (empty, 3, scratch);
        check (messagesPassing (empty) == 0, "an empty buffer stays empty");
    }

    {
        // Sample positions must survive filtering so timing is preserved.
        juce::MidiBuffer buffer;
        buffer.addEvent (noteOnCh5, 0);
        buffer.addEvent (noteOnCh3, 128);
        buffer.addEvent (noteOnCh5, 200);

        filterMidiBuffer (buffer, 3, scratch);

        const auto first = *buffer.begin();
        check (buffer.getNumEvents() == 1 && first.samplePosition == 128,
               "the surviving event keeps its sample position");
    }

    {
        // Reusing the scratch buffer across blocks must not leak old events.
        juce::MidiBuffer buffer;
        buffer.addEvent (noteOnCh5, 0);
        filterMidiBuffer (buffer, 3, scratch);
        check (messagesPassing (buffer) == 0, "first filtered block is emptied");

        juce::MidiBuffer second;
        second.addEvent (noteOnCh3, 4);
        filterMidiBuffer (second, 3, scratch);
        check (messagesPassing (second) == 1, "scratch reuse does not resurrect old events");
    }

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
