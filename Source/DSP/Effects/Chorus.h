/*
  ==============================================================================

    Chorus.h
    Created: 26 Jan 2026
    Description: Envoltorio de producto del chorus estereo.

    Separacion de responsabilidades (migracion de efectos, igual que la reverb
    en la Fase 1 [5/6]): la maquina del efecto vive en el modulo compartido
    ABDSharedCode::DspEffects (DspEffects/DspChorus.h, namespace abd::dsp) y es
    reutilizable tal cual por cualquier sintoma. Aqui queda lo especifico de este
    producto: el suavizado de rate/depth/mix (20ms) alimentando al motor muestra
    a muestra.

    La API publica no cambia: el motor (BaseEngine) sigue llamando a prepare(),
    setParameters(), setMix(), processBlock() y reset() exactamente igual.

  ==============================================================================
 */

#pragma once

#include "DspCore.h"
#include "DspEffects/DspChorus.h"

namespace NEURONiK::DSP::Effects {

class Chorus
{
public:
    Chorus() = default;

    void prepare(double sampleRate)
    {
        chorus.prepare(sampleRate);

        rateSmoother.reset(sampleRate, 0.02);
        depthSmoother.reset(sampleRate, 0.02);
        mixSmoother.reset(sampleRate, 0.02);
    }

    void setParameters(float rateHz, float depth, float mix) noexcept
    {
        rateSmoother.setTargetValue(rateHz);
        depthSmoother.setTargetValue(depth);
        mixSmoother.setTargetValue(mix);
    }

    void setMix(float mix) noexcept
    {
        mixSmoother.setTargetValue(mix);
    }

    void processBlock(dsp::AudioBuffer<float>& buffer)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float currentRate = rateSmoother.getNextValue();
            const float currentDepth = depthSmoother.getNextValue();
            const float currentMix = mixSmoother.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float inputSample = buffer.getReadPointer(channel)[sample];

                buffer.getWritePointer(channel)[sample] =
                    chorus.processSample(channel, inputSample, currentDepth, currentMix);
            }

            // La fase del LFO y el puntero de escritura son por muestra, no por canal.
            chorus.advance(currentRate);
        }
    }

    void reset()
    {
        chorus.reset();
        rateSmoother.setCurrentAndTargetValue(rateSmoother.getTargetValue());
        depthSmoother.setCurrentAndTargetValue(depthSmoother.getTargetValue());
        mixSmoother.setCurrentAndTargetValue(mixSmoother.getTargetValue());
    }

private:
    dsp::Chorus chorus;

    dsp::LinearSmoothedValue<float> rateSmoother { 1.0f };
    dsp::LinearSmoothedValue<float> depthSmoother { 0.2f };
    dsp::LinearSmoothedValue<float> mixSmoother { 0.0f };

    dspDeclareNonCopyableWithLeakDetector(Chorus)
};

} // namespace NEURONiK::DSP::Effects
