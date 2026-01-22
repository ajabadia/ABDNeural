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

namespace Nexus::DSP::Effects {

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
        // f(x) = (2 / PI) * atan(x * drive)
        float x = input * drive;
        return std::atan(x) * 0.63661977236f; // 2/PI constant
    }

private:
    float drive = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Saturation)
};

} // namespace Nexus::DSP::Effects
