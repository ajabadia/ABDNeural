/*
  ==============================================================================

    NeurotikVoice.cpp
    Created: 30 Jan 2026
    Description: Implementation of the Neurotik resonator voice.

  ==============================================================================
*/

#include "NeurotikVoice.h"
#include "../DSPUtils.h"
#include <algorithm>
#include <array>

namespace NEURONiK::DSP::Synthesis {

NeurotikVoice::NeurotikVoice()
{
    lastNoiseSample = 0.0f;
}

void NeurotikVoice::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    resonatorBank.setSampleRate(sampleRate);
    ampEnvelope.setSampleRate(sampleRate);

    morphXSmoother.reset(sampleRate, 0.02);
    morphYSmoother.reset(sampleRate, 0.02);
    resonanceSmoother.reset(sampleRate, 0.02);
    unisonDetuneSmoother.reset(sampleRate, 0.02);
}

void NeurotikVoice::noteOn(int midiNoteNumber, float velocity)
{
    currentNote = midiNoteNumber;
    currentVelocity = velocity;
    baseFreq = (float)juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    
    resonatorBank.setBaseFrequency(baseFreq);
    resonatorBank.reset(); // Crucial: Clear ANY history/denormals from previous note
    impulseTrigger = 1.0f;

    // Refresh parameters immediately for the new note
    updateParameters(); 

    // Snap smoothers to target (using the just-updated currentParams)
    morphXSmoother.setCurrentAndTargetValue(currentParams.morphX);
    morphYSmoother.setCurrentAndTargetValue(currentParams.morphY);
    resonanceSmoother.setCurrentAndTargetValue(currentParams.resonatorResonance);
    unisonDetuneSmoother.setCurrentAndTargetValue(currentParams.unisonDetune);

    ampEnvelope.noteOn();
}

void NeurotikVoice::noteOff(float velocity, bool /*allowTail*/)
{
    juce::ignoreUnused(velocity);
    ampEnvelope.noteOff();
}

bool NeurotikVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isActive()) return false;

    // Process in sub-blocks for control rate smoothing (eliminate zipper noise)
    static constexpr int kSubBlockSize = 32;

    for (int start = 0; start < numSamples; start += kSubBlockSize)
    {
        int thisBlockSamples = std::min(kSubBlockSize, numSamples - start);
        
        // Zero-alloc stack buffer for this voice's output
        std::array<float, kSubBlockSize> tempBuffer; 

        // 1. Advance Smoothers & Update Params
        // Get START values
        float mX = morphXSmoother.getNextValue();
        float mY = morphYSmoother.getNextValue();
        float res = resonanceSmoother.getNextValue();
        float detune = unisonDetuneSmoother.getNextValue();

        // Apply Modulations from IVoice
        mX = juce::jlimit(0.0f, 1.0f, mX + modMorphX);
        mY = juce::jlimit(0.0f, 1.0f, mY + modMorphY);
        res = juce::jlimit(0.0f, 1.0f, res + modResonance);
        detune = juce::jlimit(0.0f, 0.1f, detune + modUnison);

        // FIX: Catch up the smoothers (fix for 32x lag)
        if (thisBlockSamples > 1) {
             for(int k=1; k<thisBlockSamples; ++k) {
                 morphXSmoother.getNextValue();
                 morphYSmoother.getNextValue();
                 resonanceSmoother.getNextValue();
                 unisonDetuneSmoother.getNextValue();
             }
        }

        resonatorBank.updateParameters(mX, mY, res, detune);

        // 2. Render Audio Logic (Inner Loop)
        juce::ScopedNoDenormals noDenormals; // Local safety for feedback loops
        
        for (int i = 0; i < thisBlockSamples; ++i)
        {
             // Noise Generation
             float rawNoise = (random.nextFloat() * 2.0f - 1.0f);
             float alpha = juce::jlimit(0.01f, 0.99f, currentParams.excitationColor + modExciteColor);
             
             // Simple One-Pole Color Filter
             float coloredNoise = alpha * rawNoise + (1.0f - alpha) * lastNoiseSample;
             lastNoiseSample = coloredNoise; // Update state

             // Excite Mix
             float exciteAmt = juce::jlimit(0.0f, 1.0f, currentParams.excitationNoise + modExciteNoise); 
             float iMix = juce::jlimit(0.0f, 1.0f, currentParams.impulseMix + modImpulseMix);
             
             // Mix Impulse and Noise
             float excitation = (coloredNoise * (1.0f - iMix)) + (impulseTrigger * iMix);
             excitation *= exciteAmt;
             
             // Clear impulse after use
             impulseTrigger = 0.0f;

             // Resonator Processing
             float voiceSample = resonatorBank.processSample(excitation);
             
             // Envelope
             float env = ampEnvelope.processSample();
             
             float levelMod = juce::jlimit(0.0f, 2.0f, currentParams.level + modLevel);
             float finalSample = voiceSample * env * currentVelocity * levelMod;
             
             // Write to temp buffer
             tempBuffer[i] = finalSample;
        }

        // 3. Sanitize (Per-Voice Safety)
        // Check ONLY this voice's output before mixing
        bool badBlock = false;
        for (int i=0; i<thisBlockSamples; ++i) {
            if (!std::isfinite(tempBuffer[i])) { 
                badBlock = true; 
                break; 
            }
        }

        if (badBlock) {
            #if JUCE_DEBUG
            DBG("NeurotikVoice NaN detected - resetting voice");
            #endif
            reset();
            return false; // Stop processing this voice
        }

        // 4. Mix to Output (Safe)
        // Only if valid, mix to the main buffer
        for (int ch=0; ch < outputBuffer.getNumChannels(); ++ch) {
            outputBuffer.addFrom(ch, startSample + start, tempBuffer.data(), thisBlockSamples);
        }
    }

    if (!ampEnvelope.isActive())
    {
        currentNote = -1;
        return false;
    }

    return true;
}

bool NeurotikVoice::isActive() const
{
    return currentNote != -1 || ampEnvelope.isActive();
}

void NeurotikVoice::updateParameters()
{
    currentParams = pendingParams;
    
    ampEnvelope.setParameters(currentParams.attack,
                              currentParams.decay,
                              currentParams.sustain,
                              currentParams.release);

    morphXSmoother.setTargetValue(currentParams.morphX);
    morphYSmoother.setTargetValue(currentParams.morphY);
    resonanceSmoother.setTargetValue(currentParams.resonatorResonance);
    unisonDetuneSmoother.setTargetValue(currentParams.unisonDetune);
}

void NeurotikVoice::reset()
{
    resonatorBank.reset();
    ampEnvelope.reset();
    currentNote = -1;
    mpePitchBend = 0.0f;
    mpePressure = 0.0f;
    mpeTimbre = 0.0f;
}

void NeurotikVoice::notePitchBend(float bendSemitones)
{
    mpePitchBend = bendSemitones;
    float bentFreq = baseFreq * std::pow(2.0f, mpePitchBend / 12.0f);
    resonatorBank.setBaseFrequency(bentFreq);
}

void NeurotikVoice::notePressure(float pressure)
{
    mpePressure = pressure;
}

void NeurotikVoice::noteTimbre(float timbre)
{
    mpeTimbre = timbre;
}

} // namespace NEURONiK::DSP::Synthesis
