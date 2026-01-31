/*
  ==============================================================================

    ResonatorBank.h
    Created: 30 Jan 2026
    Description: A bank of 64 resonant filters (BPF) for spectral modeling.
                 Inspired by physical modeling and the Neurotik concept.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <immintrin.h>
#include "../../Common/SpectralModel.h"

namespace NEURONiK::DSP::Core {

/**
 * Specialized lightweight biquad for the ResonatorBank.
 * Optimized for speed in banks of 64. No atomics in process.
 */
struct ResonatorBiquad {
    float z1 = 0.0f, z2 = 0.0f;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    inline float processSample(float input) noexcept {
        float output = b0 * input + z1;
        z1 = (b1 * input) - (a1 * output) + z2;
        z2 = (b2 * input) - (a2 * output);
        return output;
    }

    void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }
};

class ResonatorBank {
public:
    ResonatorBank() noexcept;
    ~ResonatorBank() = default;

    void setSampleRate(double sr) noexcept;
    void setBaseFrequency(float hz) noexcept;
    void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) noexcept;

    // --- Real-time safe processing ---
    /** 
     * Updates all 64 filters' coefficients. 
     * Resonance is normalized 0.0 to 1.0 (mapped inside).
     */
    void updateParameters(float morphX, float morphY, float resonance, float detune) noexcept;
    
    float processSample(float excitation) noexcept;
    void reset() noexcept;

    const std::array<float, 64>& getPartialAmplitudes() const noexcept { return partialAmplitudes; }

private:
    std::array<ResonatorBiquad, 128> resonators;
    std::array<float, 64> partialAmplitudes;
    std::array<NEURONiK::Common::SpectralModel, 4> models;

    float baseFrequency = 440.0f;
    double sampleRate = 48000.0;
    float currentResonance = 0.99f;
    
    // State for optimization
    float lastMorphX = -1.0f, lastMorphY = -1.0f;
    float lastRes = -1.0f, lastDetune = -1.0f;
    float lastBaseFreq = -1.0f;
    bool modelChanged = true;

    // SIMD Buffers (Aligned for SSE 128-bit)
    alignas(16) float b0_v[128] = {0}, b1_v[128] = {0}, b2_v[128] = {0};
    alignas(16) float a1_v[128] = {0}, a2_v[128] = {0};
    alignas(16) float z1_v[128] = {0}, z2_v[128] = {0};
    alignas(16) float partialAmplitudes_v[128] = {0};
};

} // namespace NEURONiK::DSP::Core
