/*
  ==============================================================================

    Resonator.h
    Created: 21 Jan 2026
    Description: Additive synthesis core with 64 partials.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include "Oscillator.h"

namespace Nexus::DSP::Core {

/**
 * Resonator handles up to 64 harmonically related partials.
 */
class Resonator {
public:
    Resonator() noexcept;
    ~Resonator() = default;

    void setSampleRate(double sr) noexcept;
    void setBaseFrequency(float hz) noexcept;
    
    // Set the overall "brightness" or distribution of partials
    void setHarmonicDistribution(float rollOff) noexcept;
    
    float processSample() noexcept;
    void reset() noexcept;

    // --- State Access ---
    const std::array<float, 64>& getPartialAmplitudes() const noexcept { return partialAmplitudes; }

private:
    std::array<Oscillator, 64> partials;
    std::array<float, 64> partialAmplitudes;
    float baseFrequency = 440.0f;
    double sampleRate = 48000.0;
    
    // Normalization factor to avoid clipping
    float normalizationFactor = 1.0f;
};

} // namespace Nexus::DSP::Core
