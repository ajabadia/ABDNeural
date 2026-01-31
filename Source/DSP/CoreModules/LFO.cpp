/*
  ==============================================================================

    LFO.cpp
    Created: 27 Jan 2026
    Description: Implementation of the Low Frequency Oscillator (LFO) core module.

  ==============================================================================
*/

#include "LFO.h"
#include "../DSPUtils.h"

namespace NEURONiK::DSP::Core {

LFO::LFO() noexcept
{
    // Seed the random generator for Sample & Hold
    random_.setSeed(juce::Time::getMillisecondCounter());
    lastRandomValue_ = random_.nextFloat() * 2.0f - 1.0f; // -1 to 1
    nextRandomValue_ = random_.nextFloat() * 2.0f - 1.0f; // -1 to 1
    randomInterpolationPhase_ = 0.0f;
}

void LFO::setSampleRate(double newSampleRate) noexcept
{
    sampleRate_ = newSampleRate;
    updatePhaseIncrement();
}

void LFO::reset() noexcept
{
    phase_ = 0.0f;
    randomInterpolationPhase_ = 0.0f;
    lastRandomValue_ = random_.nextFloat() * 2.0f - 1.0f;
    nextRandomValue_ = random_.nextFloat() * 2.0f - 1.0f;
}

void LFO::setWaveform(Waveform newWaveform) noexcept
{
    currentWaveform_.store(newWaveform, std::memory_order_relaxed);
}

void LFO::setRate(float newRateHz) noexcept
{
    float validatedRate = validateAudioParam(newRateHz, 0.01f, 100.0f, 1.0f, "LFO rateHz");
    rateHz_.store(validatedRate, std::memory_order_relaxed);
    if (currentSyncMode_.load(std::memory_order_relaxed) == SyncMode::Free)
        updatePhaseIncrement();
}

void LFO::setSyncMode(SyncMode newSyncMode) noexcept
{
    currentSyncMode_.store(newSyncMode, std::memory_order_relaxed);
    updatePhaseIncrement();
}

void LFO::setTempoBPM(double newTempoBPM) noexcept
{
    double validatedBPM = validateAudioParam(static_cast<float>(newTempoBPM), 10.0f, 300.0f, 120.0f, "LFO tempoBPM");
    tempoBPM_.store(validatedBPM, std::memory_order_relaxed);
    if (currentSyncMode_.load(std::memory_order_relaxed) == SyncMode::TempoSync)
        updatePhaseIncrement();
}

void LFO::setRhythmicDivision(float newDivision) noexcept
{
    float validatedDiv = validateAudioParam(newDivision, 0.0625f, 32.0f, 1.0f, "LFO rhythmicDivision");
    rhythmicDivision_.store(validatedDiv, std::memory_order_relaxed);
    if (currentSyncMode_.load(std::memory_order_relaxed) == SyncMode::TempoSync)
        updatePhaseIncrement();
}

void LFO::setDepth(float newDepth) noexcept
{
    float validatedDepth = validateAudioParam(newDepth, 0.0f, 1.0f, 1.0f, "LFO depth");
    depth_.store(validatedDepth, std::memory_order_relaxed);
}

float LFO::processBlock(int numSamples) noexcept
{
    if (numSamples <= 0) return 0.0f;

    float out = processSample(); // This advances by 1 and returns value at current phase

    if (numSamples > 1)
    {
        float increment = phaseIncrement_;
        const Waveform waveform = currentWaveform_.load(std::memory_order_relaxed);

        if (waveform == Waveform::RandomSampleAndHold)
        {
            // For S&H, we need to check for ticks
            for (int i = 1; i < numSamples; ++i)
            {
                phase_ += increment;
                if (phase_ >= 1.0f)
                {
                    phase_ -= 1.0f;
                    lastRandomValue_ = nextRandomValue_;
                    nextRandomValue_ = random_.nextFloat() * 2.0f - 1.0f;
                    randomInterpolationPhase_ = 0.0f;
                }
                
                if (randomInterpolationPhase_ < 1.0f)
                    randomInterpolationPhase_ += randomInterpolationSpeed_ * increment / 0.01f;
            }
        }
        else
        {
            // For regular waveforms, just advance phase
            phase_ += increment * (numSamples - 1);
            while (phase_ >= 1.0f) phase_ -= 1.0f;
            while (phase_ < 0.0f) phase_ += 1.0f;
        }
    }

    return out;
}

float LFO::processSample() noexcept
{
    float out = 0.0f;
    const Waveform waveform = currentWaveform_.load(std::memory_order_relaxed);

    switch (waveform)
    {
        case Waveform::Sine:
            out = generateSine();
            break;
        case Waveform::Triangle:
            out = generateTriangle();
            break;
        case Waveform::SawUp:
            out = generateSawUp();
            break;
        case Waveform::SawDown:
            out = generateSawDown();
            break;
        case Waveform::Square:
            out = generateSquare();
            break;
        case Waveform::RandomSampleAndHold:
            out = generateRandomSampleAndHold();
            break;
    }

    // Update phase for next sample
    phase_ += phaseIncrement_;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;
    else if (phase_ < 0.0f)
        phase_ += 1.0f;

    return out * depth_.load(std::memory_order_relaxed);
}

void LFO::updatePhaseIncrement() noexcept
{
    float currentRateHz;
    if (currentSyncMode_.load(std::memory_order_relaxed) == SyncMode::Free)
    {
        currentRateHz = rateHz_.load(std::memory_order_relaxed);
    }
    else // TempoSync
    {
        currentRateHz = getSyncedRateHz();
    }
    phaseIncrement_ = currentRateHz / static_cast<float>(sampleRate_);
}

float LFO::getSyncedRateHz() const noexcept
{
    double bpm = tempoBPM_.load(std::memory_order_relaxed);
    if (bpm < 1.0) bpm = 120.0; // Safe fallback if no sync/invalid
    
    // A quarter note (1 beat) duration in seconds = 60.0 / BPM
    // Frequency of a quarter note = BPM / 60.0
    const double quarterNoteFreq = bpm / 60.0;
    return static_cast<float>(quarterNoteFreq * rhythmicDivision_.load(std::memory_order_relaxed));
}

float LFO::generateSine() const noexcept
{
    // Fast Parabolic Sine Approximation (Bhaskara I variant)
    // t is phase in [0, 1]
    // x is shifted to [-PI, PI] for the approximation
    float x = (phase_ - 0.5f) * juce::MathConstants<float>::twoPi * -1.0f; // Shifted and flipped to match sine starting at 0
    
    constexpr float B = 4.0f / juce::MathConstants<float>::pi;
    constexpr float C = -4.0f / (juce::MathConstants<float>::pi * juce::MathConstants<float>::pi);
    
    float y = B * x + C * x * std::abs(x);
    
    // Error correction
    return 0.225f * (y * std::abs(y) - y) + y;
}

float LFO::generateTriangle() const noexcept
{
    float t = phase_;
    if (t < 0.25f)
        return t * 4.0f; // 0 to 1
    else if (t < 0.75f)
        return 1.0f - (t - 0.25f) * 4.0f; // 1 to -1
    else
        return (t - 0.75f) * 4.0f - 1.0f; // -1 to 0
}

float LFO::generateSawUp() const noexcept
{
    return phase_ * 2.0f - 1.0f; // -1 to 1
}

float LFO::generateSawDown() const noexcept
{
    return (1.0f - phase_) * 2.0f - 1.0f; // 1 to -1
}

float LFO::generateSquare() const noexcept
{
    return phase_ < 0.5f ? 1.0f : -1.0f;
}

float LFO::generateRandomSampleAndHold() noexcept
{
    // Only update random values when phase resets to 0.0 or near it
    // and interpolate smoothly between last and next random value
    if (phase_ < phaseIncrement_ && randomInterpolationPhase_ >= 1.0f)
    {
        lastRandomValue_ = nextRandomValue_;
        nextRandomValue_ = random_.nextFloat() * 2.0f - 1.0f;
        randomInterpolationPhase_ = 0.0f;
    }

    if (randomInterpolationPhase_ < 1.0f)
    {
        // Smooth interpolation using a simple linear approach for now
        float interpolatedValue = juce::jmap(randomInterpolationPhase_,
                                           0.0f, 1.0f, lastRandomValue_, nextRandomValue_);
        randomInterpolationPhase_ += randomInterpolationSpeed_ * phaseIncrement_ / 0.01f; // Adjust speed
        return interpolatedValue;
    }

    return nextRandomValue_;
}

} // namespace NEURONiK::DSP::Core
