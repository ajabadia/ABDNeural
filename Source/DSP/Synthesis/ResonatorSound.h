/*
  ==============================================================================

    ResonatorSound.h
    Created: 21 Jan 2026
    Description: JUCE SynthesiserSound definition for NEXUS.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace Nexus::DSP::Synthesis {

class ResonatorSound : public juce::SynthesiserSound
{
public:
    ResonatorSound() = default;

    bool appliesToNote(int /*midiNoteNumber*/) override    { return true; }
    bool appliesToChannel(int /*midiChannel*/) override    { return true; }
};

} // namespace Nexus::DSP::Synthesis
