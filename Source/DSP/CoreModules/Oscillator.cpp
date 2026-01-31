/*
  ==============================================================================

    Oscillator.cpp
    Created: 20 Jan 2026
    Description: Thread-safe Oscillator core module implementation.

  ==============================================================================
*/

#include "Oscillator.h"

namespace NEURONiK::DSP::Core {

float Oscillator::sineTable[WAVETABLE_SIZE + 1];
bool Oscillator::tableInitialized = false;

void Oscillator::initializeTable() noexcept
{
    if (tableInitialized) return;
    for (int i = 0; i <= WAVETABLE_SIZE; ++i)
    {
        float phase = (static_cast<float>(i) / static_cast<float>(WAVETABLE_SIZE)) * juce::MathConstants<float>::twoPi;
        sineTable[i] = std::sin(phase);
    }
    tableInitialized = true;
}

Oscillator::Oscillator() noexcept
{
    initializeTable();
    updatePhaseIncrement();
}

void Oscillator::setSampleRate(double newSampleRate) noexcept
{
    if (newSampleRate <= 0.0) 
    {
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
    newFreqHz = juce::jlimit(0.1f, 22000.0f, newFreqHz);
    frequencyHz_.store(newFreqHz, std::memory_order_release);
}

void Oscillator::setWaveform(Waveform newWaveform) noexcept
{
    currentWaveform_.store(newWaveform, std::memory_order_release);
}

float Oscillator::processSample(float phaseMod) noexcept
{
    float freq = frequencyHz_.load(std::memory_order_relaxed);
    float inc = freq / static_cast<float>(sampleRate_);
    
    currentPhase_ += inc;
    if (currentPhase_ >= 1.0f) currentPhase_ -= 1.0f;
    
    float modPhase = currentPhase_ + phaseMod;
    if (modPhase >= 1.0f) modPhase -= 1.0f;
    if (modPhase < 0.0f) modPhase += 1.0f;
    
    Waveform wf = currentWaveform_.load(std::memory_order_relaxed);
    float output = 0.0f;

    switch (wf)
    {
        case Waveform::Sine:
        {
            float fIndex = modPhase * static_cast<float>(WAVETABLE_SIZE);
            int index = static_cast<int>(fIndex);
            float frac = fIndex - static_cast<float>(index);
            output = sineTable[index] + frac * (sineTable[index + 1] - sineTable[index]);
            break;
        }

        case Waveform::Triangle:
            output = 2.0f * std::abs(2.0f * (modPhase - std::floor(modPhase + 0.5f))) - 1.0f;
            break;

        case Waveform::Saw:
            output = 2.0f * (modPhase - 0.5f);
            break;

        case Waveform::Square:
            output = (modPhase < 0.5f) ? 1.0f : -1.0f;
            break;

        case Waveform::Noise:
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

} // namespace NEURONiK::DSP::Core
