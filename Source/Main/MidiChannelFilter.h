/*
  ==============================================================================

    MidiChannelFilter.h
    Created: 16 Sep 2026
    Description: Channel filtering for incoming MIDI.

                 `midiChannel` used to be read only by the editor menu, so every
                 instance responded to every channel. The filtering lives here,
                 outside the processor, so it can be exercised without building
                 the plugin.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace NEURONiK::Midi
{

/** Number of channel choices in the *midiChannel parameter: "Omni" plus 1..16. */
constexpr int numChannelChoices = 17;

/**
 * @brief Converts the choice index of the midiChannel parameter into a setting.
 * @returns 0 for "Omni" (no filtering) or 1..16 for a specific channel.
 *          Out-of-range indexes fall back to 0.
 */
int channelFromChoiceIndex (int choiceIndex) noexcept;

/**
 * @brief Decides whether a message may reach the engine.
 * @details Channel messages are matched against the setting. System messages
 *          (sysex, clock, transport, ...) report channel 0 and always pass:
 *          filtering them out would break sync and patch dumps.
 */
bool passesChannelFilter (const juce::MidiMessage& message, int channelSetting) noexcept;

/**
 * @brief Applies the filter to a MIDI buffer in place.
 * @details A setting of 0 (Omni) leaves the buffer untouched. `scratch` is a
 *          caller-owned buffer that must outlive the call; reusing it keeps the
 *          operation free of allocations after the first blocks, so this stays
 *          safe on the audio thread.
 */
void filterMidiBuffer (juce::MidiBuffer& midiMessages, int channelSetting, juce::MidiBuffer& scratch);

} // namespace NEURONiK::Midi
