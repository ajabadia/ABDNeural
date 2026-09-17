/*
  ==============================================================================

    DspTypes.h
    Created: 17 Sep 2026
    Description: Plain-data types shared by the DSP core and every host
                 (JUCE processor, tests, future WASM wrapper). NO JUCE
                 includes here on purpose: this header must compile in any
                 C++17 toolchain without the JUCE modules.

  ==============================================================================
*/

#pragma once

namespace NEURONiK::DSP
{

/**
 * Common structures for engine parameters. Plain data only: the host fills it
 * and hands it to the engine (ISynthesisEngine::setGlobalParams / the Runtime
 * facade), which owns the real-time safe handoff.
 */
struct GlobalParams {
    float masterLevel = 0.8f;
    float saturationAmt = 0.0f;

    /** Tempo used by every tempo-synced modulation. */
    double bpm = 120.0;

    /** Already resolved to seconds by the host (free time or note length). */
    float delayTime = 0.3f, delayFB = 0.4f;

    float chorusRate = 1.0f, chorusDepth = 0.2f, chorusMix = 0.0f;
    float reverbSize = 0.5f, reverbDamping = 0.5f, reverbWidth = 1.0f, reverbMix = 0.0f;

    struct LFOParams {
        int waveform = 0;
        float rateHz = 1.0f;
        int syncMode = 0;            //!< 0 = Free, 1 = TempoSync
        int rhythmicDivision = 0;    //!< Index into Core::rhythmicDivisionInQuarterNotes
        float depth = 1.0f;
    } lfo1, lfo2;

    struct ModRoute {
        int source = 0;
        int destination = 0;
        float amount = 0.0f;
    };
    ModRoute modMatrix[4];
};

} // namespace NEURONiK::DSP
