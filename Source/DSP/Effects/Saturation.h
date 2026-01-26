/*
  ==============================================================================

    Saturation.h
    Created: 22 Jan 2026
    Description: Soft-clipping saturation module for adding harmonic character.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace NEURONiK::DSP::Effects {

/**
 * Saturation effect using a soft-clipping sigmoid function.
 *
 * Thread-safety: processSample is real-time safe.
 */
class Saturation
{
public:
    Saturation() noexcept = default;
    ~Saturation() = default;

    /**
     * Sets the amount of saturation.
     * @param amount Range [0.0, 1.0]
     */
    void setAmount(float amount) noexcept {
        drive = 1.0f + (amount * 4.0f); // Scale drive for noticeable effect
    }

    /**
     * Processes a single sample.
     */
    inline float processSample(float input) const noexcept
    {
        // Simple soft-clipping using atan or tanh approximation
        float x = input * drive;
        return std::atan(x) * 0.63661977236f; // 2/PI constant
    }

    /**
     * Processes an entire buffer of samples.
     */
    void processBlock(juce::AudioBuffer<float>& buffer) const noexcept
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int s = 0; s < buffer.getNumSamples(); ++s)
                data[s] = processSample(data[s]);
        }
    }

private:
    float drive = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Saturation)
};

} // namespace NEURONiK::DSP::Effects
