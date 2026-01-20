/*
  ==============================================================================

    FilterBank.h
    Created: 20 Jan 2026
    Description: Thread-safe FilterBank implementation with multiple biquad types.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <vector>

namespace Nexus::DSP::Core {

/**
 * @class FilterBank
 * @brief core filter module supporting LowPass, HighPass, BandPass, and Notch.
 * 
 * Uses a Direct Form II Transposed biquad implementation.
 *
 * Thread-Safety:
 * - setCutoff/setResonance/setType: Thread-safe (atomic).
 * - processSample: Real-time safe. Must be called from Audio Thread.
 */
class FilterBank
{
public:
    enum class FilterType
    {
        LowPass,
        HighPass,
        BandPass,
        Notch
    };

    FilterBank() noexcept;
    ~FilterBank() = default;

    // --- Configuration ---
    void setSampleRate(double newSampleRate) noexcept;

    // --- Parameters (Thread-safe) ---
    void setCutoff(float frequencyHz) noexcept;
    void setResonance(float q) noexcept;
    void setType(FilterType newType) noexcept;

    // --- Processing ---
    /** Resets the internal state (buffers) of the filter. */
    void reset() noexcept;
    
    /** Processes a single sample through the filter. */
    float processSample(float input) noexcept;

private:
    void updateCoefficients() noexcept;

    // --- State ---
    double sampleRate_ = 48000.0;
    
    // Biquad state variables (Direct Form II Transposed)
    float z1_ = 0.0f;
    float z2_ = 0.0f;

    // Coefficients
    float a1_ = 0.0f, a2_ = 0.0f;
    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f;

    // --- Parameters (Atomics) ---
    std::atomic<float> cutoffHz_{ 1000.0f };
    std::atomic<float> resonance_{ 0.707f };
    std::atomic<FilterType> type_{ FilterType::LowPass };

    // Dirty flag to avoid recalc coefficients every sample unless params changed
    std::atomic<bool> coefficientsDirty_{ true };
};

} // namespace Nexus::DSP::Core
