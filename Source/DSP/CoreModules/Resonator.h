/*
  ==============================================================================

    Resonator.h
    Created: 21 Jan 2026
    Description: Additive synthesis core with 64 partials.
                 Now supports 2D spectral model morphing.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <immintrin.h>
#include "Oscillator.h"

#include "SpectralModel.h"

namespace NEURONiK::DSP::Core {

using NEURONiK::Common::SpectralModel;

class Resonator {
public:
    Resonator() noexcept;
    ~Resonator() = default;

    void setSampleRate(double sr) noexcept;
    void setBaseFrequency(float hz) noexcept;

    void loadModel(const SpectralModel& model, int slot) noexcept;

    // --- Real-time safe processing ---
    void updateHarmonicsFromModels(float morphX, float morphY) noexcept;
    void setStretching(float amount) noexcept;
    void setEntropy(float amount) noexcept;
    void setParity(float amount) noexcept;
    void setShift(float amount) noexcept;
    void setRollOff(float amount) noexcept;
    void setUnison(float detune, float spread) noexcept;
    float processSample() noexcept;
    float processSample(int sampleIdx) noexcept;
    void reset() noexcept;
    
    // Call once per block to pre-calculate jitter if entropy is active
    void prepareEntropy(int numSamples) noexcept;

    const std::array<float, 64>& getPartialAmplitudes() const noexcept { return partialAmplitudes; }
    const std::array<SpectralModel, 4>& getModels() const noexcept { return models; }

private:
    std::array<Oscillator, 128> partials;
    std::array<float, 64> partialAmplitudes; // 64 for visualization (main engine)
    
    // Four models for 2D morphing (A, B, C, D)
    std::array<SpectralModel, 4> models;

    float baseFrequency = 440.0f;
    double sampleRate = 48000.0;
    
    float stretchingAmount = 0.0f;
    float entropyAmount = 0.0f;
    float parityAmount = 0.5f;
    float shiftAmount = 1.0f;
    float rollOffAmount = 1.0f;
    float unisonDetune = 0.01f;
    float unisonSpread = 0.5f;


    // State for optimization
    float lastMorphX = -1.0f;
    float lastMorphY = -1.0f;
    float lastBaseFreq = -1.0f;
    float lastStrecth = -1.0f;
    float lastParity = -1.0f;
    float lastShift = -1.0f;
    float lastRollOff = -1.0f;
    float lastUnisonDetune = -1.0f;
    bool modelChanged = true;

    // Fast random seed
    uint32_t randomSeed = 1234567;

    // SIMD Buffers
    alignas(16) float currentPhases[128] = {0};
    alignas(16) float phaseIncrements[128] = {0};
    alignas(16) float amplitudes_v[128] = {0};

    std::array<float, 64> lnTable;

    // Entropy Buffers (for block processing)
    std::vector<float> ampJitterBuffer;
    std::vector<float> phaseJitterBuffer;
};

} // namespace NEURONiK::DSP::Core
