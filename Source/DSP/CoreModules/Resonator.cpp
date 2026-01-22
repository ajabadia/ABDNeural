/*
  ==============================================================================

    Resonator.cpp
    Created: 21 Jan 2026

  ==============================================================================
*/

#include "Resonator.h"
#include <cmath>

namespace Nexus::DSP::Core {

Resonator::Resonator() noexcept
{
    partialAmplitudes.fill(0.0f);
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
    for (int i = 0; i < 64; ++i)
    {
        // Simple harmonic series: f, 2f, 3f, ...
        float partialFreq = baseFrequency * static_cast<float>(i + 1);
        
        // Anti-aliasing: skip partials above Nyquist
        if (partialFreq < static_cast<float>(sampleRate * 0.45))
        {
            partials[i].setFrequency(partialFreq);
        }
        else
        {
            partials[i].setFrequency(0.0f); // Disable
        }
    }
}

void Resonator::setHarmonicDistribution(float rollOff) noexcept
{
    normalizationFactor = 0.0f;
    for (int i = 0; i < 64; ++i)
    {
        float amplitude = 1.0f / std::pow(static_cast<float>(i + 1), rollOff);
        partialAmplitudes[i] = amplitude;
        normalizationFactor += amplitude;
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
            out += partials[i].processSample() * partialAmplitudes[i];
    }
    
    return out * normalizationFactor;
}

void Resonator::reset() noexcept
{
    for (auto& p : partials)
        p.reset();
}

} // namespace Nexus::DSP::Core
