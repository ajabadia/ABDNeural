/*
  ==============================================================================

    MidiChannelFilter.cpp

  ==============================================================================
*/

#include "MidiChannelFilter.h"

namespace NEURONiK::Midi
{

int channelFromChoiceIndex (int choiceIndex) noexcept
{
    if (choiceIndex < 0 || choiceIndex >= numChannelChoices)
        return 0;

    return choiceIndex;
}

bool passesChannelFilter (const juce::MidiMessage& message, int channelSetting) noexcept
{
    if (channelSetting <= 0)
        return true;

    const int messageChannel = message.getChannel();

    // System messages (sysex, clock, song position, ...) report channel 0.
    if (messageChannel <= 0)
        return true;

    return messageChannel == channelSetting;
}

void filterMidiBuffer (juce::MidiBuffer& midiMessages, int channelSetting, juce::MidiBuffer& scratch)
{
    if (channelSetting <= 0 || midiMessages.isEmpty())
        return;

    scratch.clear();

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (passesChannelFilter (message, channelSetting))
            scratch.addEvent (message, metadata.samplePosition);
    }

    // swapWith keeps both buffers' capacity, so no allocation happens next block.
    midiMessages.swapWith (scratch);
}

} // namespace NEURONiK::Midi
