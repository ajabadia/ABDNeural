/*
  ==============================================================================

    AdditiveVoice.cpp
    Created: 29 Jan 2026

  ==============================================================================
*/

#include "DspCore.h"
#include "../DspDebug.h"
#include "../DspMidiMessage.h"
#include "AdditiveVoice.h"
#include "../DspSafety.h"
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
    // Reserva el jitter de entropia AQUI (fuera del hilo de audio):
    // Resonator::prepareEntropy ya no asigna memoria en el callback.
    resonator.prepareJitterBuffers(samplesPerBlock);

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

    // Initialize to avoid zero-start
    cutoffSmoother.setCurrentAndTargetValue(pendingParams.filterCutoff);
    resSmoother.setCurrentAndTargetValue(pendingParams.filterRes);
    morphXSmoother.setCurrentAndTargetValue(pendingParams.morphX);
    morphYSmoother.setCurrentAndTargetValue(pendingParams.morphY);
    inharmonicitySmoother.setCurrentAndTargetValue(pendingParams.inharmonicity);
    roughnessSmoother.setCurrentAndTargetValue(pendingParams.roughness);
    paritySmoother.setCurrentAndTargetValue(pendingParams.resonatorParity);
    shiftSmoother.setCurrentAndTargetValue(pendingParams.resonatorShift);
    rollOffSmoother.setCurrentAndTargetValue(pendingParams.resonatorRollOff);
    unisonDetuneSmoother.setCurrentAndTargetValue(pendingParams.unisonDetune);
    unisonSpreadSmoother.setCurrentAndTargetValue(pendingParams.unisonSpread);
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
    originalFrequency = static_cast<float>(dsp::MidiMessage::getMidiNoteInHertz(midiNoteNumber));
    
    resonator.setBaseFrequency(originalFrequency);
    
    // Immediate parameter update for start
    updateParameters(); // Sets targets

    // SNAP smoothers to targets immediately for NoteOn (prevent 0-start ramp)
    // This effectively bypasses smoothing for the initial attack of the note
    cutoffSmoother.setCurrentAndTargetValue(pendingParams.filterCutoff);
    resSmoother.setCurrentAndTargetValue(pendingParams.filterRes);
    morphXSmoother.setCurrentAndTargetValue(pendingParams.morphX);
    morphYSmoother.setCurrentAndTargetValue(pendingParams.morphY);
    inharmonicitySmoother.setCurrentAndTargetValue(pendingParams.inharmonicity);
    roughnessSmoother.setCurrentAndTargetValue(pendingParams.roughness);
    paritySmoother.setCurrentAndTargetValue(pendingParams.resonatorParity);
    shiftSmoother.setCurrentAndTargetValue(pendingParams.resonatorShift);
    rollOffSmoother.setCurrentAndTargetValue(pendingParams.resonatorRollOff);
    unisonDetuneSmoother.setCurrentAndTargetValue(pendingParams.unisonDetune);
    unisonSpreadSmoother.setCurrentAndTargetValue(pendingParams.unisonSpread);
    
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

bool AdditiveVoice::renderNextBlock(dsp::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (ampEnvelope.getCurrentState() == NEURONiK::DSP::Core::Envelope::State::Idle)
    {
        currentNote = -1;
        return false;
    }

    // Update DSP modules once per block
    float startMorphX = dsp::jlimit(0.0f, 1.0f, morphXSmoother.getNextValue() + modMorphX);
    float startMorphY = dsp::jlimit(0.0f, 1.0f, morphYSmoother.getNextValue() + modMorphY);
    float startInharmonicity = dsp::jlimit(0.0f, 1.0f, inharmonicitySmoother.getNextValue() + modInharmonicity);
    float startRoughness = dsp::jlimit(0.0f, 1.0f, roughnessSmoother.getNextValue() + modRoughness);
    float startParity = dsp::jlimit(0.0f, 1.0f, paritySmoother.getNextValue() + modParity);
    float startShift = dsp::jlimit(0.0f, 2.0f, shiftSmoother.getNextValue() + modShift);
    float startRollOff = dsp::jlimit(0.0f, 1.0f, rollOffSmoother.getNextValue());
    float startDetune = dsp::jlimit(0.0f, 0.1f, unisonDetuneSmoother.getNextValue() + modUnison);
    float startSpread = dsp::jlimit(0.0f, 1.0f, unisonSpreadSmoother.getNextValue());

    resonator.setStretching(startInharmonicity);
    resonator.setEntropy(startRoughness * 0.5f);
    resonator.setParity(startParity);
    resonator.setShift(startShift);
    resonator.setRollOff(startRollOff);
    resonator.setUnison(startDetune, startSpread);
    resonator.updateHarmonicsFromModels(startMorphX, startMorphY);
    resonator.prepareEntropy(numSamples);

    // 2. Render Audio Logic (Inner Loop)
    dsp::ScopedNoDenormals noDenormals; // Local safety for feedback loops

    // Process in sub-blocks for control rate smoothing and buffer safety
    static constexpr int kSubBlockSize = 32;

    for (int start = 0; start < numSamples; start += kSubBlockSize)
    {
        int thisBlockSamples = std::min(kSubBlockSize, numSamples - start);
        float tempBuffer[kSubBlockSize];

        // Smoothers de espectro: se avanzan UNA vez por sub-bloque (valor de
        // inicio) y se saltan los restantes con skip() — O(1) en vez del bucle
        // de getNextValue() que habia (hasta 31x9 llamadas por sub-bloque). Las
        // llamadas del bloque anterior ya avanzaron el smoother: aqui toca
        // avanzar thisBlockSamples-1, no thisBlockSamples.
        morphXSmoother.skip(thisBlockSamples - 1);
        morphYSmoother.skip(thisBlockSamples - 1);
        inharmonicitySmoother.skip(thisBlockSamples - 1);
        roughnessSmoother.skip(thisBlockSamples - 1);
        paritySmoother.skip(thisBlockSamples - 1);
        shiftSmoother.skip(thisBlockSamples - 1);
        rollOffSmoother.skip(thisBlockSamples - 1);
        unisonDetuneSmoother.skip(thisBlockSamples - 1);
        unisonSpreadSmoother.skip(thisBlockSamples - 1);

        for (int i = 0; i < thisBlockSamples; ++i)
        {
            float currentCutoff = cutoffSmoother.getNextValue();
            float currentRes = resSmoother.getNextValue();

            float rawSample = resonator.processSample(i + start);
            float fEnv = filterEnvelope.processSample();
            
            float targetCutoff = currentCutoff + modCutoff + (fEnv * currentParams.fEnvAmount * 18000.0f);
            filter.setCutoff(dsp::jlimit(20.0f, 20000.0f, targetCutoff));
            filter.setResonance(currentRes);
            
            float filteredSample = filter.processSample(rawSample);
            float envValue = ampEnvelope.processSample();
            float levelMod = dsp::jlimit(0.0f, 2.0f, currentParams.oscLevel + modLevel);
            
            tempBuffer[i] = filteredSample * envValue * currentVelocity * levelMod;
        }
        
        // Sanitize
        bool badBlock = false;
        for (int i = 0; i < thisBlockSamples; ++i)
        {
            if (!std::isfinite(tempBuffer[i]))
            {
                badBlock = true;
                break;
            }
        }

        if (badBlock)
        {
            dspDbg ("WARNING: NaN/Inf detected in AdditiveVoice output - voice reset");
            reset(); 
            return false;
        }

        // Safe Mix
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            outputBuffer.addFrom(channel, startSample + start, tempBuffer, thisBlockSamples);
        }
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

    // Reset smoothers to current pending values to avoid 0-start ramps
    cutoffSmoother.setCurrentAndTargetValue(pendingParams.filterCutoff);
    resSmoother.setCurrentAndTargetValue(pendingParams.filterRes);
    morphXSmoother.setCurrentAndTargetValue(pendingParams.morphX);
    morphYSmoother.setCurrentAndTargetValue(pendingParams.morphY);
    inharmonicitySmoother.setCurrentAndTargetValue(pendingParams.inharmonicity);
    roughnessSmoother.setCurrentAndTargetValue(pendingParams.roughness);
    paritySmoother.setCurrentAndTargetValue(pendingParams.resonatorParity);
    shiftSmoother.setCurrentAndTargetValue(pendingParams.resonatorShift);
    rollOffSmoother.setCurrentAndTargetValue(pendingParams.resonatorRollOff);
    unisonDetuneSmoother.setCurrentAndTargetValue(pendingParams.unisonDetune);
    unisonSpreadSmoother.setCurrentAndTargetValue(pendingParams.unisonSpread);
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
