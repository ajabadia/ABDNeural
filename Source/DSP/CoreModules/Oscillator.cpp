/*
  ==============================================================================

    Oscillator.cpp
    Created: 20 Jan 2026
    Description: Thread-safe Oscillator core module implementation.

  ==============================================================================
*/

#include "Oscillator.h"

namespace Nexus::DSP::Core {

Oscillator::Oscillator() noexcept
{
    updatePhaseIncrement();
}

void Oscillator::setSampleRate(double newSampleRate) noexcept
{
    // Protect against division by zero or invalid rates
    if (newSampleRate <= 0.0) 
    {
        jassertfalse; // Should not happen in valid host
        sampleRate_ = 48000.0;
    }
    else
    {
        sampleRate_ = newSampleRate;
    }
    
    updatePhaseIncrement();
}

void Oscillator::reset() noexcept
{
    currentPhase_ = 0.0f;
}

void Oscillator::setFrequency(float newFreqHz) noexcept
{
    // Clamp to reasonable range to prevent explosions
    newFreqHz = juce::jlimit(0.1f, 22000.0f, newFreqHz);
    frequencyHz_.store(newFreqHz, std::memory_order_release);
    
    // We update the increment in the process loop or here? 
    // Calculating increment here is unsafe if sampleRate changes, 
    // but sampleRate change is rare. Better to recalc increment in process 
    // OR have an atomic increment. 
    // For now, to ensure SoC and speed, we will recalc increment inside process
    // or use a helper that checks dirty flags? 
    // Simplest robust way: Recalculate increment in processSample using the atomic freq,
    // OR keep an atomic phaseIncrement.
    // Let's stick to calculating in process for perfect modulation response,
    // or calculate it here if modulation is not per-sample frequency. 
    // BUT since we want FM, we should probably take freq as parameter too or
    // just recalc.
}

void Oscillator::setWaveform(Waveform newWaveform) noexcept
{
    currentWaveform_.store(newWaveform, std::memory_order_release);
}

float Oscillator::processSample(float phaseMod) noexcept
{
    // 1. Load atomic parameters
    float freq = frequencyHz_.load(std::memory_order_acquire);
    
    // 2. Calculate Increment
    // optimization: pre-calculate this only when freq/SR changes? 
    // For an oscillator that might be modulated heavily, doing this div per sample is cheap on modern CPU.
    float inc = freq / static_cast<float>(sampleRate_);
    
    // 3. Update Phase
    currentPhase_ += inc;
    if (currentPhase_ >= 1.0f) currentPhase_ -= 1.0f;
    
    // 4. Calculate Final Phase with Modulation
    float modPhase = currentPhase_ + phaseMod;
    
    // Wrap modulated phase
    modPhase -= std::floor(modPhase); // Keeps it in [0, 1) safely
    
    // 5. Generate Output
    Waveform wf = currentWaveform_.load(std::memory_order_acquire);
    float output = 0.0f;

    switch (wf)
    {
        case Waveform::Sine:
            output = std::sin(modPhase * juce::MathConstants<float>::twoPi);
            break;

        case Waveform::Triangle:
            // 2 * abs(2 * (t - floor(t + 0.5))) - 1
            output = 2.0f * std::abs(2.0f * (modPhase - std::floor(modPhase + 0.5f))) - 1.0f;
            break;

        case Waveform::Saw:
            // 2 * (t - floor(t + 0.5))
            output = 2.0f * (modPhase - std::floor(modPhase + 0.5f));
            // Inverted saw is common, this is rising saw.
            break;

        case Waveform::Square:
            output = (modPhase < 0.5f) ? 1.0f : -1.0f;
            break;

        case Waveform::Noise:
            // Deterministic LCG for thread safety per instance and zero allocation
            noiseSeed_ = 1664525 * noiseSeed_ + 1013904223;
            output = (static_cast<float>(noiseSeed_) / static_cast<float>(0xFFFFFFFF)) * 2.0f - 1.0f;
            break;
    }

    return output;
}

void Oscillator::updatePhaseIncrement() noexcept
{
    // Used if we want to cache it, but currently dynamic in processSample
}

} // namespace Nexus::DSP::Core
