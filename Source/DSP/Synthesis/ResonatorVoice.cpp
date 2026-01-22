/*
  ==============================================================================

    ResonatorVoice.cpp
    Created: 21 Jan 2026
    Description: Implementation of the ResonatorVoice DSP chain.

  ==============================================================================
*/

#include "ResonatorVoice.h"

namespace Nexus::DSP::Synthesis {

ResonatorVoice::ResonatorVoice()
{
    // Initial configuration
    filter.setType(Nexus::DSP::Core::FilterBank::FilterType::LowPass);
    filter.setCutoff(2000.0f);
    filter.setResonance(0.1f);
}

bool ResonatorVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<ResonatorSound*>(sound) != nullptr;
}

void ResonatorVoice::startNote(int midiNoteNumber, float velocity, 
                               juce::SynthesiserSound* /*sound*/, 
                               int /*currentPitchWheelPosition*/)
{
    currentVelocity = velocity;
    
    // Convert MIDI to Frequency
    float freq = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
    resonator.setBaseFrequency(freq);
    
    // Force harmonic recalculation with the new frequency
    resonator.setStretching(currentParams.inharmonicity);
    resonator.setEntropy(currentParams.roughness);
    resonator.updateHarmonicsFromModels(currentParams.morphX, currentParams.morphY);

    // Reset DSP state for clean start
    resonator.reset();
    ampEnvelope.noteOn();
}

void ResonatorVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnvelope.noteOff();
    }
    else
    {
        ampEnvelope.reset();
        clearCurrentNote();
    }
}

void ResonatorVoice::setCurrentPlaybackSampleRate(double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);
    
    resonator.setSampleRate(newRate);
    ampEnvelope.setSampleRate(newRate);
    filter.setSampleRate(newRate);
}

void ResonatorVoice::pitchWheelMoved(int /*newPitchWheelValue*/)
{
    // Pitch wheel implementation can be added here
}

void ResonatorVoice::controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/)
{
    // MIDI CC implementation can be added here
}

void ResonatorVoice::updateParameters(const VoiceParams& params)
{
    // Cache the parameters for use in startNote
    currentParams = params;

    // APVTS parameters are in seconds, Envelope expects milliseconds
    ampEnvelope.setParameters(params.attack * 1000.0f, 
                              params.decay * 1000.0f, 
                              params.sustain, 
                              params.release * 1000.0f);
                              
    filter.setCutoff(params.filterCutoff);
    filter.setResonance(params.filterRes);

    // Use the new Neural Model Engine for harmonic generation
    resonator.setStretching(params.inharmonicity);
    resonator.setEntropy(params.roughness);
    resonator.updateHarmonicsFromModels(params.morphX, params.morphY);
}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, 
                                    int startSample, int numSamples)
{
    // Optimization: if envelope is idle, skip processing
    if (ampEnvelope.getCurrentState() == Nexus::DSP::Core::Envelope::State::Idle)
    {
        clearCurrentNote();
        return;
    }

    // Processing loop
    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Generate Resonator Output (64 partials)
        float rawSample = resonator.processSample();
        
        // 2. Apply it to the filter
        float filteredSample = filter.processSample(rawSample);
        
        // 3. Apply Amplitude Envelope
        float envValue = ampEnvelope.processSample();
        float finalSample = filteredSample * envValue * currentVelocity;
        
        // 4. Mix into output buffer (Additive)
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            outputBuffer.addSample(channel, startSample + i, finalSample);
        }
    }
    
    // Check if envelope finished after the block
    if (ampEnvelope.getCurrentState() == Nexus::DSP::Core::Envelope::State::Idle)
    {
        clearCurrentNote();
    }
}

} // namespace Nexus::DSP::Synthesis
