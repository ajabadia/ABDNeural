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
    float processSample() noexcept;
    void reset() noexcept;

    const std::array<float, 64>& getPartialAmplitudes() const noexcept { return partialAmplitudes; }
    const std::array<SpectralModel, 4>& getModels() const noexcept { return models; }

private:
    std::array<Oscillator, 64> partials;
    std::array<float, 64> partialAmplitudes;
    
    // Four models for 2D morphing (A, B, C, D)
    std::array<SpectralModel, 4> models;

    float baseFrequency = 440.0f;
    double sampleRate = 48000.0;
    
    float stretchingAmount = 0.0f;
    float entropyAmount = 0.0f;
    float parityAmount = 0.5f;
    float shiftAmount = 1.0f;
    float rollOffAmount = 1.0f;

    float normalizationFactor = 1.0f;

    // Fast random seed
    uint32_t randomSeed = 1234567;
};

} // namespace NEURONiK::DSP::Core
