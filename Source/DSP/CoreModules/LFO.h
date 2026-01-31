/*
  ==============================================================================

    LFO.h
    Created: 27 Jan 2026
    Description: Low Frequency Oscillator (LFO) core module with multiple waveforms and sync options.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace NEURONiK::DSP::Core {

class LFO
{
public:
    enum class Waveform
    {
        Sine,
        Triangle,
        SawUp,
        SawDown,
        Square,
        RandomSampleAndHold
    };

    enum class SyncMode
    {
        Free,
        TempoSync
    };

    LFO() noexcept;
    ~LFO() = default;

    // --- Configuration (Non-Realtime) ---
    void setSampleRate(double newSampleRate) noexcept;
    void reset() noexcept;

    // --- Parameters (Realtime Safe) ---
    void setWaveform(Waveform newWaveform) noexcept;
    void setRate(float newRateHz) noexcept; // For Free mode
    void setSyncMode(SyncMode newSyncMode) noexcept;
    void setTempoBPM(double newTempoBPM) noexcept;
    void setRhythmicDivision(float newDivision) noexcept; // 1.0 = 1/4 note, 0.5 = 1/8, 2.0 = 1/2
    void setDepth(float newDepth) noexcept;

    // --- Processing (Realtime Safe) ---
    float processSample() noexcept;
    float processBlock(int numSamples) noexcept;

private:
    std::atomic<Waveform> currentWaveform_{ Waveform::Sine };
    std::atomic<SyncMode> currentSyncMode_{ SyncMode::Free };
    std::atomic<float> rateHz_{ 1.0f };
    std::atomic<float> depth_{ 1.0f };
    std::atomic<double> tempoBPM_{ 120.0 };
    std::atomic<float> rhythmicDivision_{ 1.0f }; // In quarter notes

    double sampleRate_ = 48000.0;
    float phase_ = 0.0f;
    float phaseIncrement_ = 0.0f;
    juce::Random random_;
    float lastRandomValue_ = 0.0f;
    float nextRandomValue_ = 0.0f;
    float randomInterpolationPhase_ = 0.0f;
    const float randomInterpolationSpeed_ = 0.1f; // How fast S&H interpolates

    void updatePhaseIncrement() noexcept;
    float getSyncedRateHz() const noexcept;

    // Waveform generation functions
    float generateSine() const noexcept;
    float generateTriangle() const noexcept;
    float generateSawUp() const noexcept;
    float generateSawDown() const noexcept;
    float generateSquare() const noexcept;
    float generateRandomSampleAndHold() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LFO)
};

} // namespace NEURONiK::DSP::Core
