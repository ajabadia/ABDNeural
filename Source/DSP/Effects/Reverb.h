/*
  ==============================================================================

    Reverb.h
    Created: 26 Jan 2026
    Description: Envoltorio de producto sobre el reverb del motor.

    Hasta la Fase 1 [5/6] esto envolvia juce::Reverb. Ahora envuelve dsp::Reverb,
    el port libre de JUCE: el motor ya no depende de juce_audio_basics para la
    reverb.

    Separacion de responsabilidades: el efecto puro vive en el modulo compartido
    ABDSharedCode::DspEffects (DspEffects/DspReverb.h, namespace abd::dsp) y es
    reutilizable tal cual por cualquier sintoma; aqui queda lo especifico de este
    producto (mapeo de mix a wet/dry y suavizado de parametros).

  ==============================================================================
 */

#pragma once

#include "DspCore.h"
#include "DspEffects/DspReverb.h"

namespace NEURONiK::DSP::Effects {

class Reverb
{
public:
    Reverb() = default;

    void prepare(double sampleRate)
    {
        reverb.setSampleRate(sampleRate);
        sizeSmoother.reset(sampleRate, 0.02);
        dampingSmoother.reset(sampleRate, 0.02);
        widthSmoother.reset(sampleRate, 0.02);
        mixSmoother.reset(sampleRate, 0.02);
    }

    void setParameters(float size, float damping, float width, float mix) noexcept
    {
        sizeSmoother.setTargetValue(size);
        dampingSmoother.setTargetValue(damping);
        widthSmoother.setTargetValue(width);
        mixSmoother.setTargetValue(mix);
    }

    void setMix(float mix) noexcept
    {
        mixSmoother.setTargetValue(mix);
    }

    void processBlock(dsp::AudioBuffer<float>& buffer)
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        if (numSamples <= 0 || numChannels <= 0)
            return;

        // Reverb apagada y ya asentada en silencio: con wet = 0 el efecto no
        // toca la senal (dsp::Reverb escala el dry x2 por dentro, de ahi que el
        // envoltorio tenga que saltarse el bloque en vez de procesar), asi que
        // procesar no cambiaria ni una muestra. Los smoothers SI avanzan: al
        // encenderla, la rampa arranca donde le toca por tiempo y no donde se
        // quedo el bloque anterior.
        if (mixSmoother.getTargetValue() <= 0.002f && mixSmoother.getCurrentValue() <= 0.002f)
        {
            sizeSmoother.skip (numSamples);
            dampingSmoother.skip (numSamples);
            widthSmoother.skip (numSamples);
            mixSmoother.skip (numSamples);
            return;
        }

        float* const left  = buffer.getWritePointer (0);
        float* const right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        // La rampa de parametros dura 20 ms (882 pasos a 44,1 kHz). Consumiendo
        // UN paso por bloque duraba 882 BLOQUES: ~2,5 s con bloque 128 y ~10 s
        // con bloque 512, es decir el tiempo de subida lo imponia el tamano de
        // bloque del host. Aplicando un valor por bloque el resultado seguia
        // siendo funcion del troceado. Aqui el parametro se lee y se aplica por
        // MUESTRA, asi que el efecto depende del tiempo y no del buffer (era el
        // escenario C de la matriz de paridad de Tests/WasmParityTest.cpp).
        // dsp::Reverb solo expone API por bloque: de ahi el bucle de una muestra,
        // y de ahi tambien que el parametro se aplique una vez por muestra solo
        // mientras la rampa esta viva.
        if (sizeSmoother.isSmoothing() || dampingSmoother.isSmoothing()
            || widthSmoother.isSmoothing() || mixSmoother.isSmoothing())
        {
            for (int i = 0; i < numSamples; ++i)
            {
                applyParameters (sizeSmoother.getNextValue(),
                                 dampingSmoother.getNextValue(),
                                 widthSmoother.getNextValue(),
                                 mixSmoother.getNextValue());

                if (right != nullptr)
                    reverb.processStereo (left + i, right + i, 1);
                else
                    reverb.processMono (left + i, 1);
            }

            return;
        }

        // Camino habitual: ningun parametro se mueve dentro del bloque, asi que
        // la reverb procesa el bloque entero de una vez. Es el mismo recorrido
        // que el bucle de arriba (dsp::Reverb ya itera por muestra por dentro),
        // con una sola llamada.
        applyParameters (sizeSmoother.getCurrentValue(), dampingSmoother.getCurrentValue(),
                         widthSmoother.getCurrentValue(), mixSmoother.getCurrentValue());

        if (right != nullptr)
            reverb.processStereo (left, right, numSamples);
        else
            reverb.processMono (left, numSamples);
    }

    void reset()
    {
        reverb.reset();
        sizeSmoother.setCurrentAndTargetValue(sizeSmoother.getTargetValue());
        dampingSmoother.setCurrentAndTargetValue(dampingSmoother.getTargetValue());
        widthSmoother.setCurrentAndTargetValue(widthSmoother.getTargetValue());
        mixSmoother.setCurrentAndTargetValue(mixSmoother.getTargetValue());
    }

private:
    // Aplica los valores de la rampa solo si han cambiado respecto al ultimo
    // aplicado: setParameters rearma los smoothers internos de dsp::Reverb, y en
    // el camino por muestra no queremos pagar eso en cada una.
    void applyParameters(float size, float damping, float width, float mix) noexcept
    {
        if (paramsApplied
            && size == params.roomSize && damping == params.damping
            && width == params.width && mix == appliedMix)
            return;

        params.roomSize = size;
        params.damping = damping;
        params.width = width;
        params.wetLevel = mix * 0.5f;
        params.dryLevel = 1.0f - (mix * 0.2f);

        reverb.setParameters(params);
        appliedMix = mix;
        paramsApplied = true;
    }

    dsp::Reverb reverb;
    dsp::Reverb::Parameters params;   // ultimo valor aplicado a `reverb`
    float appliedMix = -1.0f;
    bool paramsApplied = false;

    dsp::LinearSmoothedValue<float> sizeSmoother { 0.5f };
    dsp::LinearSmoothedValue<float> dampingSmoother { 0.5f };
    dsp::LinearSmoothedValue<float> widthSmoother { 1.0f };
    dsp::LinearSmoothedValue<float> mixSmoother { 0.0f };

    dspDeclareNonCopyableWithLeakDetector(Reverb)
};

} // namespace NEURONiK::DSP::Effects
