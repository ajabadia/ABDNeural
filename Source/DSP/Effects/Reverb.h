/*
  ==============================================================================

    Reverb.h
    Created: 26 Jan 2026
    Description: Envoltorio de producto sobre el reverb del motor.

    Hasta la Fase 1 [5/6] esto envolvia juce::Reverb. Ahora envuelve dsp::Reverb
    (Effects/DspReverb.h), el port libre de JUCE: el motor ya no depende de
    juce_audio_basics para la reverb.

    Separacion de responsabilidades: DspReverb.h es el efecto puro (reutilizable
    tal cual); aqui vive lo especifico del producto (mapeo de mix a wet/dry y
    suavizado de parametros).

  ==============================================================================
 */

#pragma once

#include "DspCore.h"
#include "DspReverb.h"

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
        // Update parameters once per block (standard JUCE Reverb is block-based)
        // For smoother transitions, we could process in smaller sub-blocks if needed, 
        // but updating once per block is usually fine for Reverb unless the block is very large.
        params.roomSize = sizeSmoother.getNextValue();
        params.damping = dampingSmoother.getNextValue();
        params.width = widthSmoother.getNextValue();
        float mix = mixSmoother.getNextValue();
        params.wetLevel = mix * 0.5f;
        params.dryLevel = 1.0f - (mix * 0.2f);
        
        reverb.setParameters(params);

        if (params.wetLevel <= 0.001f) return;

        if (buffer.getNumChannels() == 1)
        {
            reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
        }
        else
        {
            reverb.processStereo(buffer.getWritePointer(0), buffer.getWritePointer(1), buffer.getNumSamples());
        }
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
    dsp::Reverb reverb;
    dsp::Reverb::Parameters params;

    dsp::LinearSmoothedValue<float> sizeSmoother { 0.5f };
    dsp::LinearSmoothedValue<float> dampingSmoother { 0.5f };
    dsp::LinearSmoothedValue<float> widthSmoother { 1.0f };
    dsp::LinearSmoothedValue<float> mixSmoother { 0.0f };

    dspDeclareNonCopyableWithLeakDetector(Reverb)
};

} // namespace NEURONiK::DSP::Effects
