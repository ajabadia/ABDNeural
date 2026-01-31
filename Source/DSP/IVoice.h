/*
  ==============================================================================

    IVoice.h
    Created: 29 Jan 2026
    Description: Interface for a synthesis voice, decoupled from JUCE Synthesiser.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace NEURONiK::DSP {

/**
 * Interface for any synthesis voice in the NEURONiK engine.
 * Allows the engine to manage different types of voices (Additive, Subtractive, etc.)
 * without being tied to a specific framework's voice management.
 */
class IVoice {
public:
    virtual ~IVoice() = default;

    /** Prepares the voice for playback. */
    virtual void prepare(double sampleRate, int samplesPerBlock) = 0;

    /** Triggers a new note. */
    virtual void noteOn(int midiNoteNumber, float velocity) = 0;

    /** Stops the note. */
    virtual void noteOff(float velocity, bool allowTail) = 0;

    // --- MPE / Per-Note Modulation ---
    virtual void notePitchBend(float bendSemitones) = 0;
    virtual void notePressure(float pressure) = 0;
    virtual void noteTimbre(float timbre) = 0;

    /** 
     * Renders audio for this voice into the provided buffer.
     * Returns true if the voice is still active, false if it has finished its tail.
     */
    virtual bool renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) = 0;

    /** Returns true if the voice is currently producing sound. */
    virtual bool isActive() const = 0;

    /** Returns the current MIDI note number being played. */
    virtual int getCurrentlyPlayingNote() const = 0;

    /** Real-time safe parameter update for the voice. */
    virtual void updateParameters() = 0;
    
    // --- Modulation Hooks ---
    float modLevel = 0.0f;
    float modCutoff = 0.0f;
    float modResonance = 0.0f;
    float modFilterRes = 0.0f; // Filter or Resonator
    float modMorphX = 0.0f;
    float modMorphY = 0.0f;
    float modInharmonicity = 0.0f;
    float modRoughness = 0.0f;
    float modParity = 0.0f;
    float modShift = 0.0f;
    float modRolloff = 0.0f;
    float modUnison = 0.0f;
    
    // Neurotik Excite
    float modExciteNoise = 0.0f;
    float modExciteColor = 0.0f;
    float modImpulseMix = 0.0f;

    // Envelope
    float modAmpAttack = 0.0f;
    float modAmpDecay = 0.0f;
    float modAmpSustain = 0.0f;
    float modAmpRelease = 0.0f;

    virtual void resetModulations() {
        modLevel = modCutoff = modResonance = modFilterRes = modMorphX = modMorphY = 0.0f;
        modInharmonicity = modRoughness = modParity = modShift = modRolloff = modUnison = 0.0f;
        modExciteNoise = modExciteColor = modImpulseMix = 0.0f;
        modAmpAttack = modAmpDecay = modAmpSustain = modAmpRelease = 0.0f;
    }
    
    /** Resets the internal state of the voice. */
    virtual void reset() = 0;

    /** MPE Channel tracking */
    virtual void setChannel(int channel) = 0;
    virtual int getChannel() const = 0;
};

} // namespace NEURONiK::DSP
