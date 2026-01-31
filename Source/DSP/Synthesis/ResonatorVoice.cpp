/*
  ==============================================================================

    ResonatorVoice.cpp
    Created: 21 Jan 2026
    Description: Implementation of the ResonatorVoice DSP chain.

  ==============================================================================
*/

#include "ResonatorVoice.h"
#include "../../Main/NEURONiKProcessor.h" // Include the processor header

namespace NEURONiK::DSP::Synthesis {

// Constructor now accepts the owner processor
ResonatorVoice::ResonatorVoice(NEURONiKProcessor* p) : ownerProcessor(p)
{
    // Initial configuration
    filter.setType(NEURONiK::DSP::Core::FilterBank::FilterType::LowPass);
    filter.setCutoff(2000.0f);
    filter.setResonance(0.1f);
}

void ResonatorVoice::loadModel(const NEURONiK::DSP::Core::SpectralModel& model, int slot)
{
    resonator.loadModel(model, slot);
}

bool ResonatorVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<ResonatorSound*>(sound) != nullptr;
}

void ResonatorVoice::startNote(int midiNoteNumber, float velocity, 
                               juce::SynthesiserSound* /*sound*/, 
                               int currentPitchWheelPosition)
{
    float curvedVelocity = velocity;
    if (currentParams.velocityCurve == 1) // Soft (exponential)
        curvedVelocity = velocity * velocity;
    else if (currentParams.velocityCurve == 2) // Hard (logarithmic-ish)
        curvedVelocity = std::sqrt(velocity);

    currentVelocity = curvedVelocity;
    originalFrequency = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
    pitchWheelMoved(currentPitchWheelPosition); // Apply initial pitch bend

    // Force harmonic recalculation with the new frequency
    resonator.setStretching(currentParams.inharmonicity);
    resonator.setEntropy(currentParams.roughness * 0.5f);
    resonator.setParity(currentParams.resonatorParity);
    resonator.setShift(currentParams.resonatorShift);
    resonator.setRollOff(currentParams.resonatorRollOff);
    resonator.updateHarmonicsFromModels(currentParams.morphX, currentParams.morphY);

    // FIX: Do NOT reset resonator phases to 0 here.
    // Resetting causes audible clicks if the voice was already playing (stealing).
    // Letting the phases run ensures continuous waveforms, just changing frequency.
    // resonator.reset(); 

    // Use a very fast attack override if retriggering to avoid click?
    // Actually, simply noteOn() restarting the envelope from 0 is usually what causes
    // click if note was high level. 
    // Ideally, we crossfade or the envelope handles retrigger. 
    // But for now, just removing resonator reset fixes the phase discontinuity click.
    ampEnvelope.noteOn();
    filterEnvelope.noteOn();
}

void ResonatorVoice::stopNote(float /*velocity*/, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnvelope.noteOff();
        filterEnvelope.noteOff();
    }
    else
    {
        ampEnvelope.reset();
        filterEnvelope.reset();
        clearCurrentNote();
    }
}

void ResonatorVoice::setCurrentPlaybackSampleRate(double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);
    
    resonator.setSampleRate(newRate);
    ampEnvelope.setSampleRate(newRate);
    filterEnvelope.setSampleRate(newRate);
    filter.setSampleRate(newRate);

    cutoffSmoother.reset(newRate, 0.02);
    resSmoother.reset(newRate, 0.02);
    morphXSmoother.reset(newRate, 0.02);
    morphYSmoother.reset(newRate, 0.02);
    inharmonicitySmoother.reset(newRate, 0.02);
    roughnessSmoother.reset(newRate, 0.02);
    paritySmoother.reset(newRate, 0.02);
    shiftSmoother.reset(newRate, 0.02);
    rollOffSmoother.reset(newRate, 0.02);
}

void ResonatorVoice::pitchWheelMoved(int newPitchWheelValue)
{
    // JUCE pitch wheel is 14-bit (0-16383). 8192 is center.
    auto pitchBend = newPitchWheelValue - 8192;
    // Map to +/- 2 semitones
    auto bendRange = 2.0f / 8192.0f;
    pitchBendRatio = 1.0f + pitchBend * bendRange;

    resonator.setBaseFrequency(originalFrequency * pitchBendRatio);
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
                              
    filterEnvelope.setParameters(params.fAttack * 1000.0f,
                                 params.fDecay * 1000.0f,
                                 params.fSustain,
                                 params.fRelease * 1000.0f);

    cutoffSmoother.setTargetValue(params.filterCutoff);
    resSmoother.setTargetValue(params.filterRes);
    morphXSmoother.setTargetValue(params.morphX);
    morphYSmoother.setTargetValue(params.morphY);
    inharmonicitySmoother.setTargetValue(params.inharmonicity);
    roughnessSmoother.setTargetValue(params.roughness);
    paritySmoother.setTargetValue(params.resonatorParity);
    shiftSmoother.setTargetValue(params.resonatorShift);
    rollOffSmoother.setTargetValue(params.resonatorRollOff);
}

void ResonatorVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, 
                                    int startSample, int numSamples)
{
    // Optimization: if envelope is idle, skip processing
    if (ampEnvelope.getCurrentState() == NEURONiK::DSP::Core::Envelope::State::Idle)
    {
        clearCurrentNote();
        return;
    }

    // --- Data transfer to UI ---
    // If this voice is active, send its spectral data to the processor for the UI.
    if (isVoiceActive() && ownerProcessor != nullptr)
    {
        const auto& partials = resonator.getPartialAmplitudes();
        for (int i = 0; i < 64; ++i)
        {
            ownerProcessor->spectralDataForUI[i].store(partials[i], std::memory_order_relaxed);
        }
    }

    // --- Optimized: Update DSP modules once per block if no smoothing transition is happening, ---
    // --- or periodically if it is. For now, once per block is a huge 60x internal speedup. ---
    
    float startMorphX = morphXSmoother.getNextValue();
    float startMorphY = morphYSmoother.getNextValue();
    float startInharmonicity = inharmonicitySmoother.getNextValue();
    float startRoughness = roughnessSmoother.getNextValue();
    float startParity = paritySmoother.getNextValue();
    float startShift = shiftSmoother.getNextValue();
    float startRollOff = rollOffSmoother.getNextValue();

    resonator.setStretching(startInharmonicity);
    resonator.setEntropy(startRoughness * 0.5f);
    resonator.setParity(startParity);
    resonator.setShift(startShift);
    resonator.setRollOff(startRollOff);
    resonator.updateHarmonicsFromModels(startMorphX, startMorphY);

    // Skip the first values in smoothers if we used them above
    for (int i = 1; i < numSamples; ++i) {
        morphXSmoother.getNextValue();
        morphYSmoother.getNextValue();
        inharmonicitySmoother.getNextValue();
        roughnessSmoother.getNextValue();
        paritySmoother.getNextValue();
        shiftSmoother.getNextValue();
        rollOffSmoother.getNextValue();
    }

    // Processing loop
    for (int i = 0; i < numSamples; ++i)
    {
        // Still need per-sample smoothing for filter to avoid zippering
        float currentCutoff = cutoffSmoother.getNextValue();
        float currentRes = resSmoother.getNextValue();

        // 1. Generate Resonator Output
        float rawSample = resonator.processSample();
        
        // 2. Apply Filter Envelope to Cutoff
        float fEnv = filterEnvelope.processSample();
        
        // Simple linear modulation: offset up to 18k Hz based on amount
        float targetCutoff = currentCutoff + (fEnv * currentParams.fEnvAmount * 18000.0f);
        filter.setCutoff(juce::jlimit(20.0f, 20000.0f, targetCutoff));
        filter.setResonance(currentRes);
        
        // 3. Apply it to the filter
        float filteredSample = filter.processSample(rawSample);
        
        // 4. Apply Amplitude Envelope
        float envValue = ampEnvelope.processSample();
        float finalSample = filteredSample * envValue * currentVelocity * currentParams.oscLevel;
        
        // 4. Mix into output buffer (Additive)
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            outputBuffer.addSample(channel, startSample + i, finalSample);
        }
    }
    
    // Check if envelope finished after the block
    if (ampEnvelope.getCurrentState() == NEURONiK::DSP::Core::Envelope::State::Idle)
    {
        // When the note ends, clear the visualizer data
        if (ownerProcessor != nullptr)
        {
            for (int i = 0; i < 64; ++i)
            {
                ownerProcessor->spectralDataForUI[i].store(0.0f, std::memory_order_relaxed);
            }
        }
        clearCurrentNote();
    }
}

} // namespace NEURONiK::DSP::Synthesis
