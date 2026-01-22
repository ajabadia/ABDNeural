/*
  ==============================================================================

    FilterBank.cpp
    Created: 20 Jan 2026
    Description: Thread-safe FilterBank implementation.

  ==============================================================================
*/

#include "FilterBank.h"
#include <cmath>

namespace Nexus::DSP::Core {

FilterBank::FilterBank() noexcept
{
    reset();
}

void FilterBank::setSampleRate(double newSampleRate) noexcept
{
    if (newSampleRate > 0.0)
    {
        sampleRate_ = newSampleRate;
        coefficientsDirty_.store(true, std::memory_order_release);
    }
}

void FilterBank::setCutoff(float frequencyHz) noexcept
{
    cutoffHz_.store(juce::jlimit(20.0f, 20000.0f, frequencyHz), std::memory_order_release);
    coefficientsDirty_.store(true, std::memory_order_release);
}

void FilterBank::setResonance(float q) noexcept
{
    // Mapping 0.0 - 1.0 to a musical Q range (0.5 to 15.0)
    // Quadratic curve for better resolution at low resonance
    float mappedQ = 0.5f + (q * q * 14.5f); 
    resonance_.store(mappedQ, std::memory_order_release);
    coefficientsDirty_.store(true, std::memory_order_release);
}

void FilterBank::setType(FilterType newType) noexcept
{
    type_.store(newType, std::memory_order_release);
    coefficientsDirty_.store(true, std::memory_order_release);
}

void FilterBank::reset() noexcept
{
    z1_ = 0.0f;
    z2_ = 0.0f;
}

float FilterBank::processSample(float input) noexcept
{
    if (coefficientsDirty_.load(std::memory_order_acquire))
        updateCoefficients();

    // Direct Form II Transposed implementation:
    // y[n] = b0*x[n] + z1[n-1]
    // z1[n] = b1*x[n] - a1*y[n] + z2[n-1]
    // z2[n] = b2*x[n] - a2*y[n]

    float output = b0_ * input + z1_;
    z1_ = (b1_ * input) - (a1_ * output) + z2_;
    z2_ = (b2_ * input) - (a2_ * output);

    return output;
}

void FilterBank::updateCoefficients() noexcept
{
    float f = cutoffHz_.load(std::memory_order_acquire);
    float q = resonance_.load(std::memory_order_acquire);
    FilterType t = type_.load(std::memory_order_acquire);

    // RBJ Biquad Calculations
    float omega = juce::MathConstants<float>::twoPi * f / static_cast<float>(sampleRate_);
    float cosW = std::cos(omega);
    float sinW = std::sin(omega);
    float alpha = sinW / (2.0f * q);

    float a0 = 0.0f;

    switch (t)
    {
        case FilterType::LowPass:
            b0_ = (1.0f - cosW) / 2.0f;
            b1_ = 1.0f - cosW;
            b2_ = (1.0f - cosW) / 2.0f;
            a0 = 1.0f + alpha;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha;
            break;

        case FilterType::HighPass:
            b0_ = (1.0f + cosW) / 2.0f;
            b1_ = -(1.0f + cosW);
            b2_ = (1.0f + cosW) / 2.0f;
            a0 = 1.0f + alpha;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha;
            break;

        case FilterType::BandPass:
            b0_ = alpha;
            b1_ = 0.0f;
            b2_ = -alpha;
            a0 = 1.0f + alpha;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha;
            break;

        case FilterType::Notch:
            b0_ = 1.0f;
            b1_ = -2.0f * cosW;
            b2_ = 1.0f;
            a0 = 1.0f + alpha;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha;
            break;
    }

    // Normalize
    if (std::abs(a0) > 0.000001f)
    {
        float invA0 = 1.0f / a0;
        b0_ *= invA0;
        b1_ *= invA0;
        b2_ *= invA0;
        a1_ *= invA0;
        a2_ *= invA0;
    }

    coefficientsDirty_.store(false, std::memory_order_release);
}

} // namespace Nexus::DSP::Core
