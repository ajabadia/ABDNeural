/*
  ==============================================================================

    AdditiveVoice.cpp
    Created: 29 Jan 2026

  ==============================================================================
*/

#include "AdditiveVoice.h"
#include "../DSPUtils.h"
#include <cmath>

namespace NEURONiK::DSP::Synthesis {

AdditiveVoice::AdditiveVoice()
{
    filter.setType(NEURONiK::DSP::Core::FilterBank::FilterType::LowPass);
    filter.setCutoff(2000.0f);
    filter.setResonance(0.1f);
}

void AdditiveVoice::prepare(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    
    resonator.setSampleRate(sampleRate);
    ampEnvelope.setSampleRate(sampleRate);
    filterEnvelope.setSampleRate(sampleRate);
    filter.setSampleRate(sampleRate);

    cutoffSmoother.reset(sampleRate, 0.02);
    resSmoother.reset(sampleRate, 0.02);
    morphXSmoother.reset(sampleRate, 0.02);
    morphYSmoother.reset(sampleRate, 0.02);
    inharmonicitySmoother.reset(sampleRate, 0.02);
    roughnessSmoother.reset(sampleRate, 0.02);
    paritySmoother.reset(sampleRate, 0.02);
    shiftSmoother.reset(sampleRate, 0.02);
    rollOffSmoother.reset(sampleRate, 0.02);
    unisonDetuneSmoother.reset(sampleRate, 0.02);
    unisonSpreadSmoother.reset(sampleRate, 0.02);
}

void AdditiveVoice::noteOn(int midiNoteNumber, float velocity)
{
    currentNote = midiNoteNumber;
    
    float curvedVelocity = velocity;
    if (pendingParams.velocityCurve == 1) // Soft
        curvedVelocity = velocity * velocity;
    else if (pendingParams.velocityCurve == 2) // Hard
        curvedVelocity = std::sqrt(velocity);

    currentVelocity = curvedVelocity;
    originalFrequency = static_cast<float>(juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
    
    resonator.setBaseFrequency(originalFrequency);
    
    // Immediate parameter update for start
    updateParameters();
    
    ampEnvelope.noteOn();
    filterEnvelope.noteOn();
}

void AdditiveVoice::noteOff(float /*velocity*/, bool allowTail)
{
    if (allowTail)
    {
        ampEnvelope.noteOff();
        filterEnvelope.noteOff();
    }
    else
    {
        reset();
    }
}

void AdditiveVoice::updateParameters()
{
    currentParams = pendingParams;

    ampEnvelope.setParameters(currentParams.attack, 
                              currentParams.decay, 
                              currentParams.sustain, 
                              currentParams.release);
                              
    filterEnvelope.setParameters(currentParams.fAttack,
                                 currentParams.fDecay,
                                 currentParams.fSustain,
                                 currentParams.fRelease);

    cutoffSmoother.setTargetValue(currentParams.filterCutoff);
    resSmoother.setTargetValue(currentParams.filterRes);
    morphXSmoother.setTargetValue(currentParams.morphX);
    morphYSmoother.setTargetValue(currentParams.morphY);
    inharmonicitySmoother.setTargetValue(currentParams.inharmonicity);
    roughnessSmoother.setTargetValue(currentParams.roughness);
    paritySmoother.setTargetValue(currentParams.resonatorParity);
    shiftSmoother.setTargetValue(currentParams.resonatorShift);
    rollOffSmoother.setTargetValue(currentParams.resonatorRollOff);
    unisonDetuneSmoother.setTargetValue(currentParams.unisonDetune);
    unisonSpreadSmoother.setTargetValue(currentParams.unisonSpread);
}

bool AdditiveVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (ampEnvelope.getCurrentState() == NEURONiK::DSP::Core::Envelope::State::Idle)
    {
        currentNote = -1;
        return false;
    }

    // Update DSP modules once per block
    float startMorphX = juce::jlimit(0.0f, 1.0f, morphXSmoother.getNextValue() + modMorphX);
    float startMorphY = juce::jlimit(0.0f, 1.0f, morphYSmoother.getNextValue() + modMorphY);
    float startInharmonicity = juce::jlimit(0.0f, 1.0f, inharmonicitySmoother.getNextValue() + modInharmonicity);
    float startRoughness = juce::jlimit(0.0f, 1.0f, roughnessSmoother.getNextValue() + modRoughness);
    float startParity = juce::jlimit(0.0f, 1.0f, paritySmoother.getNextValue() + modParity);
    float startShift = juce::jlimit(0.0f, 2.0f, shiftSmoother.getNextValue() + modShift);
    float startRollOff = juce::jlimit(0.0f, 1.0f, rollOffSmoother.getNextValue());
    float startDetune = juce::jlimit(0.0f, 0.1f, unisonDetuneSmoother.getNextValue() + modUnison);
    float startSpread = juce::jlimit(0.0f, 1.0f, unisonSpreadSmoother.getNextValue());

    resonator.setStretching(startInharmonicity);
    resonator.setEntropy(startRoughness * 0.5f);
    resonator.setParity(startParity);
    resonator.setShift(startShift);
    resonator.setRollOff(startRollOff);
    resonator.setUnison(startDetune, startSpread);
    resonator.updateHarmonicsFromModels(startMorphX, startMorphY);
    resonator.prepareEntropy(numSamples);

    for (int i = 1; i < numSamples; ++i) {
        morphXSmoother.getNextValue(); morphYSmoother.getNextValue();
        inharmonicitySmoother.getNextValue(); roughnessSmoother.getNextValue();
        paritySmoother.getNextValue(); shiftSmoother.getNextValue();
        rollOffSmoother.getNextValue();
        unisonDetuneSmoother.getNextValue(); unisonSpreadSmoother.getNextValue();
    }

    for (int i = 0; i < numSamples; ++i)
    {
        float currentCutoff = cutoffSmoother.getNextValue();
        float currentRes = resSmoother.getNextValue();

        float rawSample = resonator.processSample(i);
        float fEnv = filterEnvelope.processSample();
        
        float targetCutoff = currentCutoff + modCutoff + (fEnv * currentParams.fEnvAmount * 18000.0f);
        filter.setCutoff(juce::jlimit(20.0f, 20000.0f, targetCutoff));
        filter.setResonance(currentRes);
        
        float filteredSample = filter.processSample(rawSample);
        float envValue = ampEnvelope.processSample();
        float levelMod = juce::jlimit(0.0f, 2.0f, currentParams.oscLevel + modLevel);
        float finalSample = filteredSample * envValue * currentVelocity * levelMod;
        
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            outputBuffer.addSample(channel, startSample + i, finalSample);
        }
    }
    
    // Sanitize output to prevent NaN propagation
    if (NEURONiK::DSP::sanitizeAudioBuffer(outputBuffer, startSample, numSamples))
    {
        #if JUCE_DEBUG
        DBG("WARNING: NaN/Inf detected in AdditiveVoice output - voice reset");
        #endif
        reset(); // Emergency reset
    }
    
    return ampEnvelope.getCurrentState() != NEURONiK::DSP::Core::Envelope::State::Idle;
}

bool AdditiveVoice::isActive() const
{
    return ampEnvelope.getCurrentState() != NEURONiK::DSP::Core::Envelope::State::Idle;
}

void AdditiveVoice::reset()
{
    ampEnvelope.reset();
    filterEnvelope.reset();
    resonator.reset();
    filter.reset();
    currentNote = -1;
    mpePitchBend = 0.0f;
    mpePressure = 0.0f;
    mpeTimbre = 0.0f;
}

void AdditiveVoice::notePitchBend(float bendSemitones)
{
    mpePitchBend = bendSemitones;
    float bentFreq = originalFrequency * std::pow(2.0f, mpePitchBend / 12.0f);
    resonator.setBaseFrequency(bentFreq);
}

void AdditiveVoice::notePressure(float pressure)
{
    mpePressure = pressure;
}

void AdditiveVoice::noteTimbre(float timbre)
{
    mpeTimbre = timbre;
}

} // namespace NEURONiK::DSP::Synthesis
