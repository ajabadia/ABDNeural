/*
  ==============================================================================

    Envelope.cpp
    Created: 20 Jan 2026
    Description: Implementation of ADSR Envelope generator.

  ==============================================================================
*/

#include "Envelope.h"
#include <cmath>

namespace Nexus::DSP::Core {

Envelope::Envelope() noexcept
{
    updateMultipliers();
}


void Envelope::setSampleRate(double newSampleRate) noexcept
{
    if (newSampleRate > 0.0)
    {
        sampleRate_ = newSampleRate;
        updateMultipliers();
    }
}

void Envelope::setAttackTime(float ms) noexcept
{
    attackTimeMs_.store(juce::jmax(0.1f, ms), std::memory_order_release);
    parametersDirty_.store(true, std::memory_order_release);
}

void Envelope::setDecayTime(float ms) noexcept
{
    decayTimeMs_.store(juce::jmax(0.1f, ms), std::memory_order_release);
    parametersDirty_.store(true, std::memory_order_release);
}

void Envelope::setSustainLevel(float level) noexcept
{
    sustainLevel_.store(juce::jlimit(0.0f, 1.0f, level), std::memory_order_release);
}

void Envelope::setReleaseTime(float ms) noexcept
{
    releaseTimeMs_.store(juce::jmax(0.1f, ms), std::memory_order_release);
    parametersDirty_.store(true, std::memory_order_release);
}

void Envelope::setParameters(float attack, float decay, float sustain, float release) noexcept
{
    setAttackTime(attack);
    setDecayTime(decay);
    setSustainLevel(sustain);
    setReleaseTime(release);
}

void Envelope::noteOn() noexcept
{
    updateMultipliers(); // Ensure we have latest values
    currentState_ = State::Attack;
}

void Envelope::noteOff() noexcept
{
    updateMultipliers(); 
    currentState_ = State::Release;
}

void Envelope::reset() noexcept
{
    currentState_ = State::Idle;
    currentLevel_ = 0.0f;
}

float Envelope::processSample() noexcept
{
    // Check if parameters changed
    if (parametersDirty_.load(std::memory_order_acquire))
    {
        updateMultipliers();
        parametersDirty_.store(false, std::memory_order_release);
    }

    float sustain = sustainLevel_.load(std::memory_order_acquire);

    switch (currentState_)
    {
        case State::Idle:
            currentLevel_ = 0.0f;
            break;

        case State::Attack:
            // y = y * mult + (1 - mult) * target
            // Simplified: currentLevel = currentLevel_ * attackMult_ + (1.0 - attackMult_) * ATTACK_TARGET;
            currentLevel_ = ATTACK_TARGET + attackMult_ * (currentLevel_ - ATTACK_TARGET);
            
            if (currentLevel_ >= 1.0f)
            {
                currentLevel_ = 1.0f;
                currentState_ = State::Decay;
            }
            break;

        case State::Decay:
            currentLevel_ = sustain + decayMult_ * (currentLevel_ - sustain);
            
            if (std::abs(currentLevel_ - sustain) < 0.001f)
            {
                currentLevel_ = sustain;
                currentState_ = State::Sustain;
            }
            break;

        case State::Sustain:
            currentLevel_ = sustain;
            break;

        case State::Release:
            currentLevel_ = releaseMult_ * currentLevel_;
            
            if (currentLevel_ < RELEASE_TARGET)
            {
                currentLevel_ = 0.0f;
                currentState_ = State::Idle;
            }
            break;
    }

    return currentLevel_;
}

void Envelope::updateMultipliers() noexcept
{
    attackMult_ = calculateMultiplier(attackTimeMs_.load(std::memory_order_acquire));
    decayMult_ = calculateMultiplier(decayTimeMs_.load(std::memory_order_acquire));
    releaseMult_ = calculateMultiplier(releaseTimeMs_.load(std::memory_order_acquire));
}

float Envelope::calculateMultiplier(float ms) const noexcept
{
    // multiplier = exp(-1.0 / (time_in_samples))
    // This gives a curve that reaches ~63.2% of target in 'ms' time.
    // To match traditional ADSR timing, we might need a coefficient.
    // Standard approach: -exp(-log(9) / (ms * 0.001 * sampleRate)) to reach 90%
    
    float samples = static_cast<float>(ms * 0.001 * sampleRate_);
    if (samples <= 0.0f) return 0.0f;
    
    return std::exp(-1.0f / samples);
}

} // namespace Nexus::DSP::Core
