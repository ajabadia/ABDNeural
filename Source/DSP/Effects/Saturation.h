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
     * Initializes the smoother with the sample rate.
     */
    void prepare(double sampleRate) noexcept
    {
        driveSmoother.reset(sampleRate, 0.02); // 20ms ramp
    }

    /**
     * Sets the amount of saturation.
     * @param amount Range [0.0, 1.0]
     */
    void setAmount(float amount) noexcept {
        driveSmoother.setTargetValue(1.0f + (amount * 4.0f)); // Scale drive for noticeable effect
    }

    void setDrive(float drive) noexcept {
        setAmount(drive);
    }

    /**
     * Processes a single sample.
     */
    inline float processSample(float input) noexcept
    {
        // Simple soft-clipping using atan or tanh approximation
        float x = input * driveSmoother.getNextValue();
        return std::atan(x) * 0.63661977236f; // 2/PI constant
    }

    /**
     * Processes an entire buffer of samples.
     */
    void processBlock(juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        for (int s = 0; s < numSamples; ++s)
        {
            float currentDrive = driveSmoother.getNextValue();
            
            // Optimization: if drive is approx 1.0, do nothing (1.0 is the baseline)
            if (currentDrive > 1.001f)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float x = buffer.getSample(ch, s) * currentDrive;
                    buffer.setSample(ch, s, std::atan(x) * 0.63661977236f);
                }
            }
        }
    }

    /**
     * Resets the effect state.
     */
    void resetState() noexcept
    {
        driveSmoother.setCurrentAndTargetValue(1.0f);
    }

private:
    juce::LinearSmoothedValue<float> driveSmoother { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Saturation)
};

} // namespace NEURONiK::DSP::Effects
