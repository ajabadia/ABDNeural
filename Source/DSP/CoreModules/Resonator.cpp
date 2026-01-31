/*
  ==============================================================================

    Resonator.cpp
    Created: 21 Jan 2026
    Description: Implementation of the additive synthesis core.

  ==============================================================================
*/

#include "Resonator.h"
#include "../DSPUtils.h"
#include <cmath>
#include <numeric>

namespace NEURONiK::DSP::Core {

// Helper function for linear interpolation
template<typename T>
T lerp(T a, T b, T t) { return a + t * (b - a); }

// Fast Xorshift32 implementation for entropy
inline float fastFloatRand(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return ((state & 0xFFFF) / 32768.0f) - 1.0f; 
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

    for (int i = 0; i < 64; ++i)
        lnTable[i] = std::log(static_cast<float>(i + 1));
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
    baseFrequency = validateAudioParam(hz, 10.0f, 20000.0f, 440.0f, "Resonator baseFrequency");
}

void Resonator::loadModel(const SpectralModel& model, int slot) noexcept
{
    if (slot >= 0 && slot < 4)
    {
        models[slot] = model;
        modelChanged = true;
    }
}

void Resonator::setStretching(float amount) noexcept
{
    stretchingAmount = validateAudioParam(amount, 0.0f, 1.0f, 0.0f, "Resonator stretchingAmount");
}

void Resonator::setEntropy(float amount) noexcept
{
    entropyAmount = validateAudioParam(amount, 0.0f, 1.0f, 0.0f, "Resonator entropyAmount");
}

void Resonator::setParity(float amount) noexcept
{
    parityAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void Resonator::setShift(float amount) noexcept
{
    shiftAmount = juce::jlimit(0.1f, 4.0f, amount);
}

void Resonator::setRollOff(float amount) noexcept
{
    rollOffAmount = juce::jlimit(0.1f, 5.0f, amount);
}

void Resonator::setUnison(float detune, float spread) noexcept
{
    unisonDetune = juce::jlimit(0.0f, 0.1f, detune);
    unisonSpread = juce::jlimit(0.0f, 1.0f, spread);
}

void Resonator::updateHarmonicsFromModels(float morphX, float morphY) noexcept
{
    morphX = juce::jlimit(0.0f, 1.0f, morphX);
    morphY = juce::jlimit(0.0f, 1.0f, morphY);

    // Optimization: check if anything meaningful changed
    bool anythingChanged = modelChanged || 
                          (morphX != lastMorphX) || (morphY != lastMorphY) ||
                          (baseFrequency != lastBaseFreq) || (stretchingAmount != lastStrecth) ||
                          (parityAmount != lastParity) || (shiftAmount != lastShift) ||
                          (rollOffAmount != lastRollOff) || (unisonDetune != lastUnisonDetune);

    if (!anythingChanged) return;

    // Update latches
    lastMorphX = morphX; lastMorphY = morphY;
    lastBaseFreq = baseFrequency; lastStrecth = stretchingAmount;
    lastParity = parityAmount; lastShift = shiftAmount;
    lastRollOff = rollOffAmount; lastUnisonDetune = unisonDetune;
    modelChanged = false;

    float totalAmplitude = 0.0f;

    const SpectralModel* mA = &models[0];
    const SpectralModel* mB = &models[1];
    const SpectralModel* mC = &models[2];
    const SpectralModel* mD = &models[3];

    bool bActive = std::accumulate(models[1].amplitudes.begin(), models[1].amplitudes.end(), 0.0f) > 0.001f;
    bool cActive = std::accumulate(models[2].amplitudes.begin(), models[2].amplitudes.end(), 0.0f) > 0.001f;
    bool dActive = std::accumulate(models[3].amplitudes.begin(), models[3].amplitudes.end(), 0.0f) > 0.001f;

    if (!bActive) mB = mA;
    if (!cActive) mC = mA;
    if (!dActive) mD = (bActive ? mB : mA);
    
    if (bActive && !cActive && !dActive) {
        mC = mA;
        mD = mB;
    }

    float tempAmps[64];

    for (int i = 0; i < 64; ++i)
    {

        float ampTop = lerp(mA->amplitudes[i], mB->amplitudes[i], morphX);
        float ampBottom = lerp(mC->amplitudes[i], mD->amplitudes[i], morphX);
        float baseAmp = lerp(ampTop, ampBottom, morphY);
        
        bool isEven = ((i + 1) % 2 == 0);
        float parityScale = isEven ? juce::jlimit(0.0f, 1.0f, parityAmount * 2.0f) 
                                   : juce::jlimit(0.0f, 1.0f, (1.0f - parityAmount) * 2.0f);
        
        // Fast power approximation using exp(ln(n)*x)
        float rollOffScale = std::exp(-lnTable[i] * (rollOffAmount - 1.0f));

        tempAmps[i] = baseAmp * parityScale * rollOffScale;
        totalAmplitude += tempAmps[i];

        float offsetTop = lerp(mA->frequencyOffsets[i], mB->frequencyOffsets[i], morphX);
        float offsetBottom = lerp(mC->frequencyOffsets[i], mD->frequencyOffsets[i], morphX);
        float morphedOffset = lerp(offsetTop, offsetBottom, morphY);

        float stretchedHarmonic = std::exp(lnTable[i] * (1.0f + stretchingAmount * 0.5f));
        float partialFreq = (baseFrequency * stretchedHarmonic * shiftAmount) + morphedOffset;

        if (partialFreq < static_cast<float>(sampleRate * 0.45) && tempAmps[i] > 0.0001f)
            phaseIncrements[i] = partialFreq / static_cast<float>(sampleRate);
        else
            phaseIncrements[i] = 0.0f;
    }

    float invNorm = (totalAmplitude > 0.0001f) ? (1.0f / totalAmplitude) : 0.0f;
    
    // Unison calculation
    float detuneRatio = 1.0f + unisonDetune;
    
    for (int i = 0; i < 64; ++i)
    {
        partialAmplitudes[i] = tempAmps[i] * invNorm;
        
        // Slot 0-63: Main Partials
        amplitudes_v[i] = partialAmplitudes[i];
        
        // Slot 64-127: Unison Partials
        // If unisonDetune is approx 0, we can skip or keep them at 0 amp? 
        // Better to always calculate for branchless processing in processSample
        float unisonAmp = (unisonDetune > 0.0001f) ? (amplitudes_v[i] * 0.707f) : 0.0f; // Lower gain for unison layer
        amplitudes_v[i + 64] = unisonAmp;
        
        // Main frequency was already in phaseIncrements[i]
        // Unison frequency
        phaseIncrements[i + 64] = phaseIncrements[i] * detuneRatio;
        
        // Panning/Spread would require stereo output from processSample, 
        // for now we just sum them mono.
    }
}

void Resonator::prepareEntropy(int numSamples) noexcept
{
    if (entropyAmount < 0.001f) return;
    
    if (ampJitterBuffer.size() < (size_t)numSamples) ampJitterBuffer.resize(numSamples);
    if (phaseJitterBuffer.size() < (size_t)numSamples) phaseJitterBuffer.resize(numSamples);
    
    for (int s = 0; s < numSamples; ++s)
    {
        ampJitterBuffer[s] = 1.0f + fastFloatRand(randomSeed) * entropyAmount * 0.5f;
        phaseJitterBuffer[s] = fastFloatRand(randomSeed) * entropyAmount * 0.2f;
    }
}

float Resonator::processSample() noexcept
{
    // Note: To truly eliminate branching, we'd need a processBlock in Resonator
    // that takes a sample index. For now, this is a placeholder for future block-op.
    return processSample(0); // This won't work as is, keeping original for now but cleaner
}

float Resonator::processSample(int sampleIdx) noexcept
{
    // Entropy path (rarely used, kept separate to keep SIMD hot)
    if (entropyAmount > 0.001f)
    {
        float out = 0.0f;
        float ampJitter = ampJitterBuffer[sampleIdx];
        float phaseJitter = phaseJitterBuffer[sampleIdx];
        
        for (int i = 0; i < 128; ++i)
        {
            if (amplitudes_v[i] > 0.0001f)
            {
                currentPhases[i] += phaseIncrements[i] + phaseJitter;
                if (currentPhases[i] >= 1.0f) currentPhases[i] -= 1.0f;
                else if (currentPhases[i] < 0.0f) currentPhases[i] += 1.0f;
                
                float x = currentPhases[i] * 2.0f - 1.0f;
                float s = 4.0f * x * (1.0f - std::abs(x));
                out += s * (amplitudes_v[i] * ampJitter);
            }
        }
        return out;
    }

    // SIMD Optimized Path (Standard)
    __m128 totalSumV = _mm_setzero_ps();
    const __m128 oneV = _mm_set1_ps(1.0f);
    const __m128 twoV = _mm_set1_ps(2.0f);
    const __m128 fourV = _mm_set1_ps(4.0f);
    const __m128 absMaskV = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));

    for (int i = 0; i < 128; i += 4)
    {
        __m128 phaseV = _mm_loadu_ps(&currentPhases[i]);
        __m128 incV = _mm_loadu_ps(&phaseIncrements[i]);
        __m128 ampV = _mm_loadu_ps(&amplitudes_v[i]);

        // Phase accumulation
        phaseV = _mm_add_ps(phaseV, incV);
        
        // Wrap phase [0, 1] using mask and subtraction
        __m128 wrapMask = _mm_cmpge_ps(phaseV, oneV);
        phaseV = _mm_sub_ps(phaseV, _mm_and_ps(wrapMask, oneV));
        _mm_storeu_ps(&currentPhases[i], phaseV);

        // Parabolic Sine: 4 * x * (1 - abs(x))
        __m128 xV = _mm_sub_ps(_mm_mul_ps(phaseV, twoV), oneV);
        __m128 absXV = _mm_and_ps(xV, absMaskV);
        __m128 sV = _mm_mul_ps(fourV, _mm_mul_ps(xV, _mm_sub_ps(oneV, absXV)));

        totalSumV = _mm_add_ps(totalSumV, _mm_mul_ps(sV, ampV));
    }

    alignas(16) float res[4];
    _mm_storeu_ps(res, totalSumV);
    return res[0] + res[1] + res[2] + res[3];
}

void Resonator::reset() noexcept
{
    for (int i = 0; i < 128; ++i)
    {
        currentPhases[i] = 0.0f;
    }
}

} // namespace NEURONiK::DSP::Core
