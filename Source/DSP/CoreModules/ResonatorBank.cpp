/*
  ==============================================================================

    ResonatorBank.cpp
    Created: 30 Jan 2026

  ==============================================================================
*/

#include "ResonatorBank.h"
#include "../DSPUtils.h"
#include "../Utils/SIMDWrapper.h"
#include <cmath>
#include <numeric>

namespace NEURONiK::DSP::Core {

using namespace NEURONiK::DSP::Utils;

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

void ResonatorBank::updateFilterCoefficients(int i, float partialFreq, float q, float amp, float detuneVal) noexcept
{
    // Layer 1 (Main)
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
        partialAmplitudes_v[i] = amp;
        
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
                partialAmplitudes_v[i + 64] = (amp * 0.707f); 
            }
            else { b0_v[i+64]=0; b1_v[i+64]=0; b2_v[i+64]=0; partialAmplitudes_v[i+64]=0; }
        }
        else { b0_v[i+64]=0; b1_v[i+64]=0; b2_v[i+64]=0; partialAmplitudes_v[i+64]=0; }
    }
    else
    {
        b0_v[i] = 0; b1_v[i] = 0; b2_v[i] = 0; a1_v[i] = 0; a2_v[i] = 0;
        b0_v[i+64] = 0; b1_v[i+64] = 0; b2_v[i+64] = 0; a1_v[i+64] = 0; a2_v[i+64] = 0;
        partialAmplitudes_v[i] = 0; partialAmplitudes_v[i+64] = 0;
    }
}

void ResonatorBank::updateParameters(float morphX, float morphY, float resonance, float detune) noexcept
{
    float mx = juce::jlimit(0.0f, 1.0f, morphX);
    float my = juce::jlimit(0.0f, 1.0f, morphY);
    float res = juce::jlimit(0.0f, 1.0f, resonance);
    float det = juce::jlimit(-1.0f, 1.0f, detune);

    bool anythingChanged = modelChanged || 
                          (mx != lastMorphX) || (my != lastMorphY) ||
                          (res != lastRes) || (det != lastDetune) ||
                          (baseFrequency != lastBaseFreq);

    if (!anythingChanged) return;

    lastMorphX = mx; lastMorphY = my; lastRes = res; lastDetune = det;
    lastBaseFreq = baseFrequency;
    modelChanged = false;

    float q = 1.0f + (res * res * 199.0f);
    float totalAmplitude = 0.0f;
    float tempAmps[64];

    for (int i = 0; i < 64; ++i)
    {
        float harmonicNumber = static_cast<float>(i + 1);
        float ampTop = lerp(models[0].amplitudes[i], models[1].amplitudes[i], mx);
        float ampBottom = lerp(models[2].amplitudes[i], models[3].amplitudes[i], mx);
        tempAmps[i] = lerp(ampTop, ampBottom, my);
        totalAmplitude += tempAmps[i];

        float freqOffsetTop = lerp(models[0].frequencyOffsets[i], models[1].frequencyOffsets[i], mx);
        float freqOffsetBottom = lerp(models[2].frequencyOffsets[i], models[3].frequencyOffsets[i], mx);
        float freqOffset = lerp(freqOffsetTop, freqOffsetBottom, my);
        float partialFreq = (baseFrequency * harmonicNumber) + freqOffset;

        updateFilterCoefficients(i, partialFreq, q, tempAmps[i], det);
    }

    float invNorm = (totalAmplitude > 0.001f) ? (1.0f / totalAmplitude) : 0.0f;
    for (int i = 0; i < 128; ++i)
        partialAmplitudes_v[i] *= invNorm;
        
    // Copy main layer to partialAmplitudes for UI visualization
    for (int i = 0; i < 64; i++) {
        partialAmplitudes[i] = partialAmplitudes_v[i];
    }
}

float ResonatorBank::processSample(float excitation) noexcept
{
    juce::ScopedNoDenormals noDenormals;
    SIMDFloat inputV = setAll(excitation);
    SIMDFloat totalSumV = setZero();

    // 128 filters: 64 main + 64 unison
    for (int i = 0; i < 128; i += 4)
    {
        SIMDFloat b0V = loadUnaligned(&b0_v[i]);
        SIMDFloat b2V = loadUnaligned(&b2_v[i]);
        SIMDFloat a1V = loadUnaligned(&a1_v[i]);
        SIMDFloat a2V = loadUnaligned(&a2_v[i]);
        SIMDFloat z1V = loadUnaligned(&z1_v[i]);
        SIMDFloat z2V = loadUnaligned(&z2_v[i]);
        SIMDFloat ampV = loadUnaligned(&partialAmplitudes_v[i]);

        SIMDFloat outV = (b0V * inputV) + z1V;
        SIMDFloat nextZ1 = z2V - (a1V * outV);
        SIMDFloat nextZ2 = (b2V * inputV) - (a2V * outV);

        storeUnaligned(&z1_v[i], nextZ1);
        storeUnaligned(&z2_v[i], nextZ2);

        totalSumV += (outV * ampV);
    }

    return sumRegister(totalSumV);
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
