/*
  ==============================================================================

    Resonator.cpp
    Created: 21 Jan 2026
    Description: Implementation of the additive synthesis core.

  ==============================================================================
*/

#include "Resonator.h"
#include <cmath>
#include <numeric>

namespace Nexus::DSP::Core {

// Helper function for linear interpolation
template<typename T>
T lerp(T a, T b, T t) {
    return a + t * (b - a);
}

Resonator::Resonator() noexcept
{
    partialAmplitudes.fill(0.0f);
    for (auto& model : models)
    {
        model.amplitudes.fill(0.0f);
        model.frequencyOffsets.fill(0.0f);
    }

    // Default to a single sine wave in Model A
    models[0].amplitudes[0] = 1.0f;
    partialAmplitudes[0] = 1.0f;

    for (int i = 0; i < 64; ++i)
    {
        partials[i].setWaveform(Oscillator::Waveform::Sine);
    }
}

void Resonator::setSampleRate(double sr) noexcept
{
    sampleRate = sr;
    for (auto& p : partials)
    {
        p.setSampleRate(sr);
    }
}

void Resonator::setBaseFrequency(float hz) noexcept
{
    baseFrequency = hz;
}

void Resonator::loadModel(const SpectralModel& model, int slot) noexcept
{
    if (slot >= 0 && slot < 4)
        models[slot] = model;
}

void Resonator::setStretching(float amount) noexcept
{
    stretchingAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void Resonator::setEntropy(float amount) noexcept
{
    entropyAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void Resonator::updateHarmonicsFromModels(float morphX, float morphY) noexcept
{
    morphX = juce::jlimit(0.0f, 1.0f, morphX);
    morphY = juce::jlimit(0.0f, 1.0f, morphY);
    normalizationFactor = 0.0f;

    const auto& modelA = models[0]; // Top-left
    const auto& modelB = models[1]; // Top-right
    const auto& modelC = models[2]; // Bottom-left
    const auto& modelD = models[3]; // Bottom-right

    for (int i = 0; i < 64; ++i)
    {
        // Bilinear interpolation for amplitudes
        float ampTop = lerp(modelA.amplitudes[i], modelB.amplitudes[i], morphX);
        float ampBottom = lerp(modelC.amplitudes[i], modelD.amplitudes[i], morphX);
        partialAmplitudes[i] = lerp(ampTop, ampBottom, morphY);
        
        normalizationFactor += partialAmplitudes[i];

        // Bilinear interpolation for frequency offsets
        float offsetTop = lerp(modelA.frequencyOffsets[i], modelB.frequencyOffsets[i], morphX);
        float offsetBottom = lerp(modelC.frequencyOffsets[i], modelD.frequencyOffsets[i], morphX);
        float morphedOffset = lerp(offsetTop, offsetBottom, morphY);

        float harmonicNumber = static_cast<float>(i + 1);
        
        float stretchedHarmonic = std::pow(harmonicNumber, 1.0f + stretchingAmount * 0.5f);
        float partialFreq = baseFrequency * stretchedHarmonic + morphedOffset;

        if (partialFreq < static_cast<float>(sampleRate * 0.45) && partialAmplitudes[i] > 0.0001f)
            partials[i].setFrequency(partialFreq);
        else
            partials[i].setFrequency(0.0f);
    }

    if (normalizationFactor > 0.0f)
        normalizationFactor = 1.0f / normalizationFactor;
}

float Resonator::processSample() noexcept
{
    float out = 0.0f;
    
    for (int i = 0; i < 64; ++i)
    {
        if (partialAmplitudes[i] > 0.0001f)
        {
            float jitter = 0.0f;
            if (entropyAmount > 0.01f) {
                jitter = (juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f) * entropyAmount * 0.01f;
            }
            
            out += partials[i].processSample() * (partialAmplitudes[i] * (1.0f + jitter));
        }
    }
    
    return out * normalizationFactor;
}

void Resonator::reset() noexcept
{
    for (auto& p : partials)
        p.reset();
}

} // namespace Nexus::DSP::Core
