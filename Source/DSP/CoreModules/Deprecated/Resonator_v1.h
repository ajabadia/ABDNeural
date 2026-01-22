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

    // --- Synthesis Controls ---
    void setSampleRate(double sr) noexcept;
    void setBaseFrequency(float hz) noexcept;

    /**
     * Updates the harmonic profile of the 64 partials.
     * @param rollOff Brightness distribution (1/n^rollOff)
     * @param parity  Odd/Even balance (0.0 = Odd only, 0.5 = Both, 1.0 = Even only)
     * @param shift   Harmonic stretching (1.0 = pure harmonic)
     */
    void updateHarmonics(float rollOff, float parity, float shift) noexcept;

    float processSample() noexcept;
    void reset() noexcept;

    // --- State Access ---
    const std::array<float, 64>& getPartialAmplitudes() const noexcept { return partialAmplitudes; }

private:
    std::array<Oscillator, 64> partials;
    std::array<float, 64> partialAmplitudes;

    float baseFrequency = 440.0f;
    double sampleRate = 48000.0;

    // Current Harmonic Settings
    float currentRollOff = 1.0f;
    float currentParity = 0.5f;
    float currentShift = 1.0f;

    // Normalization factor to avoid clipping
    float normalizationFactor = 1.0f;
};

} // namespace Nexus::DSP::Core
