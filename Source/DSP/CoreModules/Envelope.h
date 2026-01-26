/*
  ==============================================================================

    Envelope.h
    Created: 20 Jan 2026
    Description: Thread-safe ADSR Envelope generator with exponential curves.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

namespace NEURONiK::DSP::Core {

/**
 * @class Envelope
 * @brief Multi-stage ADSR envelope generator.
 *
 * Uses a recursive exponential formula: y = y * multiplier + target
 * This provides natural-sounding curves with very low CPU cost and zero allocations.
 *
 * Thread-Safety:
 * - setParameters: Thread-safe (atomic).
 * - processSample: Real-time safe. Must be called from Audio Thread.
 */
class Envelope
{
public:
    enum class State
    {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    Envelope() noexcept;
    ~Envelope() = default;

    // --- Configuration ---
    void setSampleRate(double newSampleRate) noexcept;

    // --- Parameters (Thread-safe) ---
    void setAttackTime(float ms) noexcept;
    void setDecayTime(float ms) noexcept;
    void setSustainLevel(float level) noexcept; // 0.0 to 1.0
    void setReleaseTime(float ms) noexcept;
    // New: set all ADSR parameters at once (thread‑safe)
    void setParameters(float attack, float decay, float sustain, float release) noexcept;

    // --- Control ---
    void noteOn() noexcept;
    void noteOff() noexcept;
    void reset() noexcept;

    // --- Processing ---
    float processSample() noexcept;
    
    bool isActive() const noexcept { return currentState_ != State::Idle; }
    State getCurrentState() const noexcept { return currentState_; }
    float getLastOutput() const noexcept { return currentLevel_; }

private:
    // --- Internal Logic ---
    void updateMultipliers() noexcept;
    float calculateMultiplier(float ms) const noexcept;

    // --- State ---
    State currentState_ = State::Idle;
    double sampleRate_ = 48000.0;
    float currentLevel_ = 0.0f;
    
    // Multipliers for each phase
    float attackMult_ = 0.0f;
    float decayMult_ = 0.0f;
    float releaseMult_ = 0.0f;

    // --- Parameters (Atomics for thread safety) ---
    std::atomic<float> attackTimeMs_{ 10.0f };
    std::atomic<float> decayTimeMs_{ 100.0f };
    std::atomic<float> sustainLevel_{ 0.7f };
    std::atomic<float> releaseTimeMs_{ 200.0f };
    std::atomic<bool> parametersDirty_{ true };

    // Targets for exponential curves
    // To reach 1.0 exponentially, we target slightly above 1.0 (e.g. 1.1) 
    // and clip at 1.0 to avoid infinite tail.
    static constexpr float ATTACK_TARGET = 1.1f;
    static constexpr float RELEASE_TARGET = 0.0001f; // Target near zero
};

} // namespace NEURONiK::DSP::Core
