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
    }

    void setParameters(float size, float damping, float width, float mix) noexcept
    {
        params.roomSize = size;
        params.damping = damping;
        params.width = width;
        params.wetLevel = mix * 0.5f; // Scale for better control
        params.dryLevel = 1.0f - (mix * 0.2f);
        
        reverb.setParameters(params);
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
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
    }

private:
    juce::Reverb reverb;
    juce::Reverb::Parameters params;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Reverb)
};

} // namespace NEURONiK::DSP::Effects
