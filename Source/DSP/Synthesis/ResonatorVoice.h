/*
  ==============================================================================

    ResonatorVoice.h
    Created: 21 Jan 2026
    Description: Polyphonic voice implementation for NEXUS.
                 Integrates Resonator, FilterBank, and Envelope modules.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../CoreModules/Resonator.h"
#include "../CoreModules/Envelope.h"
#include "../CoreModules/FilterBank.h"
#include "ResonatorSound.h"

namespace Nexus::DSP::Synthesis {

class ResonatorVoice : public juce::SynthesiserVoice
{
public:
    ResonatorVoice();
    ~ResonatorVoice() override = default;

    struct VoiceParams
    {
        float oscLevel;
        float attack, decay, sustain, release;
        float filterCutoff, filterRes;

        // Legacy parameters (to be phased out)
        float resonatorRollOff;
        float resonatorParity;
        float resonatorShift;

        // Neural Engine parameters
        float morphX;
        float morphY;
        float inharmonicity;
        float roughness;
    };

    // --- juce::SynthesiserVoice Overrides ---

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void setCurrentPlaybackSampleRate(double newRate) override;
    
    // --- Parameter Updates ---
    void updateParameters(const VoiceParams& params);

    // --- Real-time Processing ---
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        int startSample, int numSamples) override;

private:
    // --- DSP Chain ---
    Nexus::DSP::Core::Resonator resonator;
    Nexus::DSP::Core::Envelope ampEnvelope;
    Nexus::DSP::Core::FilterBank filter;

    float currentVelocity = 0.0f;
    VoiceParams currentParams; // Cached parameters

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonatorVoice)
};

} // namespace Nexus::DSP::Synthesis
