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
        // Harmonic series with stretching parameter
        // f_n = f_1 * n^shift (simplified stretching)
        // or more standard: f_n = f_1 * n * sqrt(1 + B*n^2) - but we stick to power for 'shift'
        float n = static_cast<float>(i + 1);
        float partialFreq = baseFrequency * std::pow(n, currentShift);

        // Anti-aliasing: skip partials above Nyquist
        if (partialFreq < static_cast<float>(sampleRate * 0.45))
            partials[i].setFrequency(partialFreq);
        else
            partials[i].setFrequency(0.0f);
    }
}

void Resonator::updateHarmonics(float rollOff, float parity, float shift) noexcept
{
    bool shiftChanged = (std::abs(shift - currentShift) > 0.001f);

    currentRollOff = rollOff;
    currentParity = parity;
    currentShift = shift;

    if (shiftChanged)
        setBaseFrequency(baseFrequency); // Recalculate partial frequencies

    normalizationFactor = 0.0f;
    for (int i = 0; i < 64; ++i)
    {
        int harmonicNumber = i + 1;
        bool isEven = (harmonicNumber % 2 == 0);

        // 1. Basic Roll-off
        float amp = 1.0f / std::pow(static_cast<float>(harmonicNumber), currentRollOff);

        // 2. Parity weighting
        // parity 0.0 -> Only Odd
        // parity 0.5 -> Both
        // parity 1.0 -> Only Even
        float parityWeight = 1.0f;
        if (isEven)
            parityWeight = currentParity * 2.0f; // 0.0 at 0.0, 1.0 at 0.5, 2.0 at 1.0
        else
            parityWeight = (1.0f - currentParity) * 2.0f; // 2.0 at 0.0, 1.0 at 0.5, 0.0 at 1.0

        amp *= juce::jlimit(0.0f, 1.0f, parityWeight);

        partialAmplitudes[i] = amp;
        normalizationFactor += amp;
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
