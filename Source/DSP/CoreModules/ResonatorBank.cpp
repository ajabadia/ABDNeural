/*
  ==============================================================================

    ResonatorBank.cpp
    Created: 30 Jan 2026

  ==============================================================================
*/

#include "ResonatorBank.h"
#include "../DSPUtils.h"
#include <cmath>
#include <numeric>

namespace NEURONiK::DSP::Core {

// Helper for linear interpolation
template<typename T>
T lerp(T a, T b, T t) { return a + t * (b - a); }

ResonatorBank::ResonatorBank() noexcept
{
    for (auto& model : models)
    {
        model.amplitudes.fill(0.0f);
        model.frequencyOffsets.fill(0.0f);
    }

    // Default: Sine (Fundamental only) for Slot 0
    models[0].amplitudes[0] = 1.0f;

    reset();
}

void ResonatorBank::setSampleRate(double sr) noexcept
{
    sampleRate = sr;
}

void ResonatorBank::setBaseFrequency(float hz) noexcept
{
    baseFrequency = validateAudioParam(hz, 20.0f, 20000.0f, 440.0f, "ResonatorBank baseFrequency");
}

void ResonatorBank::loadModel(const NEURONiK::Common::SpectralModel& model, int slot) noexcept
{
    if (slot >= 0 && slot < 4)
    {
        models[slot] = model;
        modelChanged = true;
    }
}

void ResonatorBank::updateParameters(float morphX, float morphY, float resonance, float detune) noexcept
{
    morphX = juce::jlimit(0.0f, 1.0f, morphX);
    morphY = juce::jlimit(0.0f, 1.0f, morphY);
    float resVal = juce::jlimit(0.0f, 1.0f, resonance);
    float detuneVal = juce::jlimit(-1.0f, 1.0f, detune);
    
    // Optimization: check if anything meaningful changed
    bool anythingChanged = modelChanged || 
                          (morphX != lastMorphX) || (morphY != lastMorphY) ||
                          (resVal != lastRes) || (detuneVal != lastDetune) ||
                          (baseFrequency != lastBaseFreq);

    if (!anythingChanged) return;

    // Update latches
    lastMorphX = morphX; lastMorphY = morphY;
    lastRes = resVal; lastDetune = detuneVal;
    lastBaseFreq = baseFrequency;
    modelChanged = false;

    float q = 1.0f + (resVal * resVal * 199.0f);

    const NEURONiK::Common::SpectralModel* mA = &models[0];
    const NEURONiK::Common::SpectralModel* mB = &models[1];
    const NEURONiK::Common::SpectralModel* mC = &models[2];
    const NEURONiK::Common::SpectralModel* mD = &models[3];

    float totalAmplitude = 0.0f;
    float tempAmps[64];

    for (int i = 0; i < 64; ++i)
    {
        float harmonicNumber = static_cast<float>(i + 1);

        // 1. Morph Amplitude
        float ampTop = lerp(mA->amplitudes[i], mB->amplitudes[i], morphX);
        float ampBottom = lerp(mC->amplitudes[i], mD->amplitudes[i], morphX);
        tempAmps[i] = lerp(ampTop, ampBottom, morphY);
        totalAmplitude += tempAmps[i];

        // 2. Morph Frequency Offset
        float freqOffsetTop = lerp(mA->frequencyOffsets[i], mB->frequencyOffsets[i], morphX);
        float freqOffsetBottom = lerp(mC->frequencyOffsets[i], mD->frequencyOffsets[i], morphX);
        float freqOffset = lerp(freqOffsetTop, freqOffsetBottom, morphY);

        float partialFreq = (baseFrequency * harmonicNumber) + freqOffset;

        // 3. Update SIMD coefficients
        if (partialFreq < static_cast<float>(sampleRate * 0.48) && partialFreq > 10.0f)
        {
            float omega = juce::MathConstants<float>::twoPi * partialFreq / static_cast<float>(sampleRate);
            float cosW = std::cos(omega);
            float alpha = std::sin(omega) / (2.0f * q);
            float a0 = 1.0f + alpha;
            float invA0 = 1.0f / a0;

            b0_v[i] = alpha * invA0;
            b1_v[i] = 0.0f;
            b2_v[i] = -alpha * invA0;
            a1_v[i] = (-2.0f * cosW) * invA0;
            a2_v[i] = (1.0f - alpha) * invA0;
            
            // Unison Layer (detune)
            if (std::abs(detuneVal) > 0.0001f)
            {
                float freqUnison = partialFreq * (1.0f + detuneVal);
                if (freqUnison < static_cast<float>(sampleRate * 0.48))
                {
                    float omegaU = juce::MathConstants<float>::twoPi * freqUnison / static_cast<float>(sampleRate);
                    float cosWU = std::cos(omegaU);
                    float alphaU = std::sin(omegaU) / (2.0f * q);
                    float a0U = 1.0f + alphaU;
                    float invA0U = 1.0f / a0U;
                    
                    b0_v[i + 64] = alphaU * invA0U;
                    b1_v[i + 64] = 0.0f;
                    b2_v[i + 64] = -alphaU * invA0U;
                    a1_v[i + 64] = (-2.0f * cosWU) * invA0U;
                    a2_v[i + 64] = (1.0f - alphaU) * invA0U;
                    
                    partialAmplitudes_v[i + 64] = (tempAmps[i] * 0.707f); // Lower gain for unison
                }
                else
                {
                    b0_v[i + 64] = 0; b1_v[i + 64] = 0; b2_v[i + 64] = 0;
                    a1_v[i + 64] = 0; a2_v[i + 64] = 0;
                    partialAmplitudes_v[i + 64] = 0;
                }
            }
            else
            {
                b0_v[i + 64] = 0; b1_v[i + 64] = 0; b2_v[i + 64] = 0;
                a1_v[i + 64] = 0; a2_v[i + 64] = 0;
                partialAmplitudes_v[i + 64] = 0;
            }
        }
        else
        {
            b0_v[i] = 0; b1_v[i] = 0; b2_v[i] = 0;
            a1_v[i] = 0; a2_v[i] = 0;
            tempAmps[i] = 0;
            
            b0_v[i + 64] = 0; b1_v[i + 64] = 0; b2_v[i + 64] = 0;
            a1_v[i + 64] = 0; a2_v[i + 64] = 0;
            partialAmplitudes_v[i + 64] = 0;
        }
    }

    // Normalize partial amplitudes (main layer) and store in SIMD buffer
    float invNorm = (totalAmplitude > 0.001f) ? (1.0f / totalAmplitude) : 0.0f;
    for (int i = 0; i < 64; ++i)
        partialAmplitudes_v[i] = tempAmps[i] * invNorm;
}

float ResonatorBank::processSample(float excitation) noexcept
{
    __m128 inputV = _mm_set1_ps(excitation);
    __m128 totalSumV = _mm_setzero_ps();

    for (int i = 0; i < 128; i += 4)
    {
        __m128 b0V = _mm_loadu_ps(&b0_v[i]);
        __m128 b2V = _mm_loadu_ps(&b2_v[i]);
        __m128 a1V = _mm_loadu_ps(&a1_v[i]);
        __m128 a2V = _mm_loadu_ps(&a2_v[i]);
        
        __m128 z1V = _mm_loadu_ps(&z1_v[i]);
        __m128 z2V = _mm_loadu_ps(&z2_v[i]);
        __m128 ampV = _mm_loadu_ps(&partialAmplitudes_v[i]);

        // out = b0 * in + z1
        __m128 outV = _mm_add_ps(_mm_mul_ps(b0V, inputV), z1V);
        
        // z1 = -a1 * out + z2 (since b1 is 0)
        __m128 nextZ1 = _mm_sub_ps(z2V, _mm_mul_ps(a1V, outV));
        
        // z2 = b2 * in - a2 * out
        __m128 nextZ2 = _mm_sub_ps(_mm_mul_ps(b2V, inputV), _mm_mul_ps(a2V, outV));

        _mm_storeu_ps(&z1_v[i], nextZ1);
        _mm_storeu_ps(&z2_v[i], nextZ2);

        totalSumV = _mm_add_ps(totalSumV, _mm_mul_ps(outV, ampV));
    }

    alignas(16) float res[4];
    _mm_storeu_ps(res, totalSumV);
    return res[0] + res[1] + res[2] + res[3];
}

void ResonatorBank::reset() noexcept
{
    for (int i = 0; i < 128; ++i)
    {
        z1_v[i] = 0.0f;
        z2_v[i] = 0.0f;
    }
}

} // namespace NEURONiK::DSP::Core
