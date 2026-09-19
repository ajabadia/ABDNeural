/*
  ==============================================================================

    Delay.h
    Created: 22 Jan 2026
    Description: Envoltorio de producto del retardo estereo realimentado.

    Separacion de responsabilidades (migracion de efectos, igual que la reverb
    en la Fase 1 [5/6]): la maquina del retardo vive en el modulo compartido
    ABDSharedCode::DspEffects (DspEffects/DspDelay.h, namespace abd::dsp). Aqui
    queda lo especifico de este producto: la conversion segundos -> muestras, el
    suavizado del tiempo (50ms) y del feedback (20ms), el recorte del feedback a
    0.95, la puerta de denormales y la mezcla (wet 0.5 sumado al dry).

    La API publica no cambia: el motor (BaseEngine) sigue llamando a prepare(),
    setParameters(), processBlock() y reset() exactamente igual.

  ==============================================================================
*/

#pragma once

#include "DspCore.h"
#include "DspEffects/DspDelay.h"

namespace NEURONiK::DSP::Effects {

/**
 * A simple stereo feedback delay.
 *
 * Thread-safety: processBlock is real-time safe.
 */
class Delay
{
public:
    Delay() = default;

    void prepare(double sampleRate, int maxDelaySamples)
    {
        currentSampleRate = sampleRate;
        delay.prepare(sampleRate, maxDelaySamples);

        timeSmoother.reset(sampleRate, 0.05); // 50ms ramp for delay time to avoid pitch jumps
        feedbackSmoother.reset(sampleRate, 0.02); // 20ms ramp
    }

    void setParameters(float timeInSeconds, float feedback, float mix = 0.5f) noexcept
    {
        dsp::ignoreUnused(mix);
        timeSmoother.setTargetValue(timeInSeconds * static_cast<float>(currentSampleRate));
        feedbackSmoother.setTargetValue(dsp::jlimit(0.0f, 0.95f, feedback));
    }

    void processBlock(dsp::AudioBuffer<float>& buffer)
    {
        dsp::ScopedNoDenormals noDenormals;
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float currentDelay = timeSmoother.getNextValue();
            const float currentFB = feedbackSmoother.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float inputSample = buffer.getReadPointer(channel)[sample];

                // El motor lee la posicion interpolada y escribe input + delayed * feedback.
                const float delayedSample =
                    delay.processSample(channel, inputSample, currentDelay, currentFB);

                // Mix
                buffer.getWritePointer(channel)[sample] += delayedSample * 0.5f;
            }

            // El puntero de escritura es por muestra, no por canal.
            delay.advanceWritePosition();
        }
    }

    void reset()
    {
        delay.reset();
        timeSmoother.setCurrentAndTargetValue(timeSmoother.getTargetValue());
        feedbackSmoother.setCurrentAndTargetValue(feedbackSmoother.getTargetValue());
    }

private:
    dsp::Delay delay;
    double currentSampleRate = 44100.0;

    dsp::LinearSmoothedValue<float> timeSmoother;
    dsp::LinearSmoothedValue<float> feedbackSmoother;

    dspDeclareNonCopyableWithLeakDetector(Delay)
};

} // namespace NEURONiK::DSP::Effects
