/*
  ==============================================================================

    ISynthesisEngine.h
    Created: 29 Jan 2026
    Description: Interface for the main synthesis engine.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

namespace NEURONiK::Common { struct SpectralModel; }

namespace NEURONiK::DSP {

/**
 * Common structures for engine parameters.
 */
struct GlobalParams {
    float masterLevel = 0.8f;
    float saturationAmt = 0.0f;
    float delayTime = 0.3f, delayFB = 0.4f;
    float chorusMix = 0.0f;
    float reverbMix = 0.0f;
    
    struct LFOParams {
        int waveform = 0;
        float rateHz = 1.0f;
        int syncMode = 0;
        int rhythmicDivision = 0;
        float depth = 1.0f;
    } lfo1, lfo2;

    struct ModRoute {
        int source = 0;
        int destination = 0;
        float amount = 0.0f;
    };
    ModRoute modMatrix[4];
};

/**
 * Interface for the NEURONiK synthesis engine.
 */
class ISynthesisEngine {
public:
    virtual ~ISynthesisEngine() = default;

    /** Prepares the engine for playback. */
    virtual void prepare(double sampleRate, int samplesPerBlock) = 0;

    /** Processes a block of audio. */
    virtual void renderNextBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) = 0;

    /** Real-time safe parameter update. */
    virtual void updateParameters() = 0;

    /** Returns the current polyphony. */
    virtual int getNumActiveVoices() const = 0;

    /** Resets the engine state. */
    virtual void reset() = 0;

    /** Inject a MIDI message from the UI or external source. */
    virtual void handleMidiMessage(const juce::MidiMessage& msg) = 0;

    /** Visualization getters (Thread-safe) */
    virtual float getLfoValue(int index) const = 0;
    virtual void getSpectralData(float* destination64) const = 0;
    virtual void getEnvelopeLevels(float& amp, float& filter) const = 0;
    virtual void getModulationValues(float* destination, int count) const = 0;

    /** Load a spectral model into the engine. */
    virtual void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) = 0;

    /** Set the maximum number of active voices. */
    virtual void setPolyphony(int numVoices) = 0;

    /** Set global parameters. */
    virtual void setGlobalParams(const GlobalParams& p) = 0;
};

} // namespace NEURONiK::DSP
