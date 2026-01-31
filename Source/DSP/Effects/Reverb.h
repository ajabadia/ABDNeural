/*
  ==============================================================================

    Reverb.h
    Created: 26 Jan 2026
    Description: Wrapper for JUCE standard Reverb.

  ==============================================================================
 */

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

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

    void processBlock(juce::AudioBuffer<float>& buffer)
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
    juce::Reverb reverb;
    juce::Reverb::Parameters params;

    juce::LinearSmoothedValue<float> sizeSmoother { 0.5f };
    juce::LinearSmoothedValue<float> dampingSmoother { 0.5f };
    juce::LinearSmoothedValue<float> widthSmoother { 1.0f };
    juce::LinearSmoothedValue<float> mixSmoother { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Reverb)
};

} // namespace NEURONiK::DSP::Effects
