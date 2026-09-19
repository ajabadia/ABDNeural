/*
  ==============================================================================

    LFO.cpp
    Created: 27 Jan 2026
    Description: Implementation of the Low Frequency Oscillator (LFO) core module.

  ==============================================================================
*/

#include "DspCore.h"
#include "LFO.h"
#include "../DspSafety.h"

namespace NEURONiK::DSP::Core {

LFO::LFO (dsp::uint32 seed) noexcept
{
    // Seed determinista sin entropia de sistema (regla 7A del skill JUCE
    // hybrid: la entropia crashea en AudioWorklet). El reloj que habia aqui
    // ademas hacia el patron S&H irreproducible entre sesiones.
    random_.setSeed (seed);
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
    // Upper bound matches masterBPM's 20-400 range, so a fast tempo is not silently trimmed.
    double validatedBPM = validateAudioParam(static_cast<float>(newTempoBPM), 10.0f, 400.0f, 120.0f, "LFO tempoBPM");
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

    // El avance de fase se hace POR MUESTRA, no de golpe: multiplicar la fase por
    // (numSamples - 1) redondea distinto segun el troceado, y el motor consume
    // bloques de control de tamano fijo (BaseEngine::kControlBlockSize), asi que dos
    // troceados del mismo material darian fases (y por tanto LFO) distintas.
    // De paso desaparece la segunda implementacion del Sample & Hold que vivia
    // aqui: avanzaba la interpolacion dos veces por muestra (aqui y dentro de
    // generateRandomSampleAndHold) y el doble avance dependia del numero de
    // llamadas, o sea que el ruido del S&H cambiaba con el buffer del host.
    float out = processSample();     // valor en la primera muestra del bloque

    for (int i = 1; i < numSamples; ++i)
        (void) processSample();

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

    // rhythmicDivision_ is a note LENGTH in quarter notes (1.0 = 1/4, 0.5 = 1/8,
    // 4.0 = whole), as documented in the header. The cycle rate is therefore the
    // beat rate divided by that length: multiplying here used to make "1/8" run a
    // cycle every two beats. Shared table: Core/RhythmicDivision.h.
    const double divisionInQuarterNotes = static_cast<double> (rhythmicDivision_.load(std::memory_order_relaxed));
    const double safeDivision = divisionInQuarterNotes > 0.0 ? divisionInQuarterNotes : 1.0;

    return static_cast<float>(quarterNoteFreq / safeDivision);
}

float LFO::generateSine() const noexcept
{
    // Fast Parabolic Sine Approximation (Bhaskara I variant)
    // t is phase in [0, 1]
    // x is shifted to [-PI, PI] for the approximation
    float x = (phase_ - 0.5f) * dsp::MathConstants<float>::twoPi * -1.0f; // Shifted and flipped to match sine starting at 0
    
    constexpr float B = 4.0f / dsp::MathConstants<float>::pi;
    constexpr float C = -4.0f / (dsp::MathConstants<float>::pi * dsp::MathConstants<float>::pi);
    
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
        float interpolatedValue = dsp::jmap(randomInterpolationPhase_,
                                           0.0f, 1.0f, lastRandomValue_, nextRandomValue_);
        randomInterpolationPhase_ += randomInterpolationSpeed_ * phaseIncrement_ / 0.01f; // Adjust speed
        return interpolatedValue;
    }

    return nextRandomValue_;
}

} // namespace NEURONiK::DSP::Core
