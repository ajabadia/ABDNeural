/*
  ==============================================================================

    NeurotikVoice.cpp
    Created: 30 Jan 2026
    Description: Implementation of the Neurotik resonator voice.

  ==============================================================================
*/

#include "NeurotikVoice.h"
#include "../DSPUtils.h"

namespace NEURONiK::DSP::Synthesis {

NeurotikVoice::NeurotikVoice()
{
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
    impulseTrigger = 1.0f;
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

    // Apply per-block modulations to the resonator bank
    float mX = juce::jlimit(0.0f, 1.0f, morphXSmoother.getNextValue() + modMorphX);
    float mY = juce::jlimit(0.0f, 1.0f, morphYSmoother.getNextValue() + modMorphY);
    float res = juce::jlimit(0.0f, 1.0f, resonanceSmoother.getNextValue() + modResonance);
    float detune = juce::jlimit(0.0f, 0.1f, unisonDetuneSmoother.getNextValue() + modUnison);
    
    resonatorBank.updateParameters(mX, mY, res, detune);

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Generate Noise
        float rawNoise = (random.nextFloat() * 2.0f - 1.0f);
        
        // 2. Color Noise (Simple One-Pole Filter)
        // Map 0.0 -> 1.0 to a coefficient
        float alpha = juce::jlimit(0.01f, 0.99f, currentParams.excitationColor);
        float coloredNoise = alpha * rawNoise + (1.0f - alpha) * lastNoiseSample;
        lastNoiseSample = coloredNoise;

        // 3. Combine with Impulse
        float exciteAmt = juce::jlimit(0.0f, 1.0f, currentParams.excitationNoise + modInharmonicity);
        float excitation = (coloredNoise * (1.0f - currentParams.impulseMix)) + (impulseTrigger * currentParams.impulseMix);
        excitation *= exciteAmt;
        
        // Clear impulse after first use in block
        impulseTrigger = 0.0f;

        float voiceSample = resonatorBank.processSample(excitation);
        float env = ampEnvelope.processSample();
        
        float levelMod = juce::jlimit(0.0f, 2.0f, currentParams.level + modLevel);
        float finalSample = voiceSample * env * currentVelocity * levelMod;
        
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            outputBuffer.addSample(ch, startSample + i, finalSample);
    }
    
    // Sanitize output to prevent NaN propagation
    if (NEURONiK::DSP::sanitizeAudioBuffer(outputBuffer, startSample, numSamples))
    {
        #if JUCE_DEBUG
        DBG("WARNING: NaN/Inf detected in NeurotikVoice output - voice reset");
        #endif
        reset(); // Emergency reset
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
