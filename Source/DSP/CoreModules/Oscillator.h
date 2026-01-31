/*
  ==============================================================================

    Oscillator.h
    Created: 20 Jan 2026
    Description: Thread-safe Oscillator core module (Phase 2).

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace NEURONiK::DSP::Core {

/**
 * @class Oscillator
 * @brief core oscillator engine supporting multiple waveforms and phase modulation.
 *
 * Thread-Safety:
 * - setFrequency/setWaveform: Thread-safe (atomic). Can be called from UI/Message thread.
 * - processSample: Real-time safe (no logs, no allocations, no locks). 
 *   Must be called from Audio Thread.
 */
class Oscillator
{
public:
    enum class Waveform
    {
        Sine,
        Triangle,
        Saw,
        Square,
        Noise
    };

    Oscillator() noexcept;
    ~Oscillator() = default;

    // Non-copyable to prevent accidental atomics copying
    Oscillator(const Oscillator&) = delete;
    Oscillator& operator=(const Oscillator&) = delete;

    // --- Configuration (Non-Realtime) ---
    /** Sets the sample rate. Not thread-safe, call before playback. */
    void setSampleRate(double newSampleRate) noexcept;

    /** Resets phase to 0. Not strictly RT-safe due to potential discontinuity, handle with care. */
    void reset() noexcept;

    // --- Parameters (Realtime Safe) ---
    /** Sets the frequency in Hz. Thread-safe. */
    void setFrequency(float newFreqHz) noexcept;

    /** Sets the waveform type. Thread-safe. */
    void setWaveform(Waveform newWaveform) noexcept;

    // --- Processing (Realtime Safe) ---
    /**
     * Renders the next sample.
     * @param phaseMod Normalized phase modulation (0 to 1) added to internal phase.
     */
    float processSample(float phaseMod = 0.0f) noexcept;

private:
    static constexpr int WAVETABLE_SIZE = 2048;
    static float sineTable[WAVETABLE_SIZE + 1];
    static bool tableInitialized;
    static void initializeTable() noexcept;

    // --- State ---
    std::atomic<float> frequencyHz_{ 440.0f };
    std::atomic<Waveform> currentWaveform_{ Waveform::Sine };
    
    double sampleRate_ = 48000.0;
    float currentPhase_ = 0.0f;
    float phaseIncrement_ = 0.0f;
    uint32_t noiseSeed_ = 123456789;

    // --- Internal Helpers ---
    void updatePhaseIncrement() noexcept;

    // PolyBLEP residuals for anti-aliasing (Placeholder for advanced implementation)
    // float polyBlep(float t) const noexcept;
};

} // namespace NEURONiK::DSP::Core
