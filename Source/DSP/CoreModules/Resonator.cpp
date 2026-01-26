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

namespace NEURONiK::DSP::Core {

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

    // --- Define 4 distinct spectral models for testing the XY Pad ---

    // Model A (Top-Left): Sawtooth-like (1/n falloff)
    for (int i = 0; i < 64; ++i) models[0].amplitudes[i] = 1.0f / (i + 1.0f);

    // Model B (Top-Right): Square-like (1/n falloff, odd harmonics)
    for (int i = 0; i < 64; i += 2) models[1].amplitudes[i] = 1.0f / (i + 1.0f);

    // Model C (Bottom-Left): Triangle-like (1/n^2 falloff, odd harmonics)
    for (int i = 0; i < 64; i += 2) models[2].amplitudes[i] = 1.0f / ((i + 1.0f) * (i + 1.0f));

    // Model D (Bottom-Right): Sine wave (fundamental only)
    models[3].amplitudes[0] = 1.0f;

    // --- Normalize all models to have a total amplitude of 1.0 ---
    for (auto& model : models) {
        float sum = std::accumulate(model.amplitudes.begin(), model.amplitudes.end(), 0.0f);
        if (sum > 0.0f) {
            for (float& amp : model.amplitudes) {
                amp /= sum;
            }
        }
    }

    // Set initial state from model A
    partialAmplitudes = models[0].amplitudes;

    for (int i = 0; i < 64; ++i)
    {
        partials[i].setWaveform(Oscillator::Waveform::Sine);
    }

    // Seed the fast random generator
    randomSeed = (uint32_t)juce::Time::getMillisecondCounter();
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

// Fast Xorshift32 implementation
inline float fastFloatRand(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    // Normalize to -1.0 to 1.0 roughly
    return ((state & 0xFFFF) / 32768.0f) - 1.0f; 
}

float Resonator::processSample() noexcept
{
    float out = 0.0f;

    // Optimization: Pre-calculate randomness only if needed
    // And do it per sample, not per partial, unless strictly necessary for "independent" jitter.
    // For "Roughness", independent jitter is better, but expensive.
    // Let's use the fast generator.
    
    // Check entropy once
    const bool hasEntropy = entropyAmount > 0.001f;

    for (int i = 0; i < 64; ++i)
    {
        // Only process audible partials
        if (partialAmplitudes[i] > 0.0001f)
        {
            float ampJitter = 1.0f;
            float phaseJitter = 0.0f;

            if (hasEntropy) 
            {
                // Use fast RNG
                float r1 = fastFloatRand(randomSeed);
                float r2 = fastFloatRand(randomSeed);

                ampJitter = 1.0f + r1 * entropyAmount * 0.5f;
                phaseJitter = r2 * entropyAmount * 0.2f;
            }
            
            out += partials[i].processSample(phaseJitter) * (partialAmplitudes[i] * ampJitter);
        }
    }
    
    return out * normalizationFactor;
}

void Resonator::reset() noexcept
{
    for (auto& p : partials)
        p.reset();
}

} // namespace NEURONiK::DSP::Core
