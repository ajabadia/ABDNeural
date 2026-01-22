/*
  ==============================================================================

    ResonatorVoice.h
    Created: 21 Jan 2026
    Description: Polyphonic voice implementation for NEXUS.
                 Integrates Oscillator, FilterBank, and Envelope modules.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../CoreModules/Oscillator.h"
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

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    
    void startNote(int midiNoteNumber, float velocity, 
                   juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
                   
    void stopNote(float velocity, bool allowTailOff) override;
    
    struct VoiceParams
    {
        float oscLevel;
        float attack, decay, sustain, release;
        float filterCutoff, filterRes;
        float resonatorRollOff;
    };

    // --- Overrides for abstract base class ---
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void setCurrentPlaybackSampleRate(double newRate) override;
    
    // Existing methods
    void updateParameters(const VoiceParams& params);

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, 
                        int startSample, int numSamples) override;

private:
    // --- DSP Chain ---
    // Architecture specifies 64 harmonically related oscillators.
    Nexus::DSP::Core::Resonator resonator;
    Nexus::DSP::Core::Envelope ampEnvelope;
    Nexus::DSP::Core::FilterBank filter;

    float currentVelocity = 0.0f;
    float harmonicRollOff = 1.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonatorVoice)
};

} // namespace Nexus::DSP::Synthesis
