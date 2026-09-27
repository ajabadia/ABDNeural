/*
  ==============================================================================

    IVoice.h
    Created: 29 Jan 2026
    Description: Interface for a synthesis voice, decoupled from JUCE Synthesiser.

  ==============================================================================
*/

#pragma once

// dsp::AudioBuffer es el tipo de la frontera; antes se obtenia por rebote del
// include de juce_audio_basics (ya no hace falta JUCE aqui).
#include "DspCore.h"

namespace NEURONiK::DSP {

/**
 * Interface for any synthesis voice in the NEURONiK engine.
 * Allows the engine to manage different types of voices (Additive, Subtractive, etc.)
 * without being tied to a specific framework's voice management.
 */
enum class VoiceType { Additive, Neurotik };

class IVoice {
public:
    virtual ~IVoice() = default;

    /** Returns the type of the voice (Additive or Neurotik). */
    virtual VoiceType getType() const noexcept = 0;

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
    virtual bool renderNextBlock(dsp::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) = 0;

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
    float modMorphZ = 0.0f; // FASE 10: eje temporal (frames del modelo)
    // FASE 11.3: el eje temporal de las capas 1 y 2 (destinos 29/30 de la matriz).
    float modMorphZ2 = 0.0f;
    float modMorphZ3 = 0.0f;
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

    // FACTOR de routing de las rutas ENV -> destino, a control rate. La matriz
    // SOBREESCRIBE con el amount (asignacion, no suma); resetModulations los
    // deja a 1.0 == el cableado legacy de siempre (la envolvente entra entera).
    // amount 0.0 = envolvente silenciada de verdad; la voz multiplica por el
    // factor (ver AdditiveVoice/NeurotikVoice).
    float modEnvLevel = 1.0f;     // ENV 1 -> Osc Level (escala el VCA)
    float modEnvCutoff = 1.0f;    // ENV 2 -> Filter Cutoff (escala la env del filtro)
    float modEnvFltAttack = 0.0f; // ENV 2 -> Flt Attack (retrig ADSR filtro, aditivo)
    float modEnvFltDecay = 0.0f;  // ENV 2 -> Flt Decay (retrig ADSR filtro, aditivo)
    float modEnvFltSustain = 0.0f;  // ENV 2 -> Flt Sustain (aditivo, clamp 0..1 en la voz)
    float modEnvFltRelease = 0.0f;  // ENV 2 -> Flt Release (aditivo)
    // ENV 2 -> "Filter Env Amt" (destino 12): suma al FACTOR de routing de la
    // ruta ENV 2 -> Filter Cutoff (base 1.0 en modEnvCutoff). Es la profundidad
    // del knob retirado filterEnvAmount, viviendo como modulacion de matriz.
    float modEnvFltDepth = 0.0f;

    virtual void resetModulations() {
        // modMorphZ (y los de las capas de 11.3) TIENEN que estar aqui: la matriz
        // SUMA (case 28/29/30) en cada tramo de control, asi que un destino que
        // no se limpia se acaba acumulando solo: con una ruta a Morph Z, el z de
        // la voz corria sin freno hasta 1.0 y ahi se quedaba. Es el unico
        // destino que se habia quedado fuera de la lista.
        modLevel = modCutoff = modResonance = modFilterRes = modMorphX = modMorphY = 0.0f;
        modMorphZ = modMorphZ2 = modMorphZ3 = 0.0f;
        modInharmonicity = modRoughness = modParity = modShift = modRolloff = modUnison = 0.0f;
        modExciteNoise = modExciteColor = modImpulseMix = 0.0f;
        modAmpAttack = modAmpDecay = modAmpSustain = modAmpRelease = 0.0f;
        modEnvLevel = modEnvCutoff = 1.0f; // factores de routing (legacy)
        modEnvFltAttack = modEnvFltDecay = modEnvFltDepth = 0.0f;
        modEnvFltSustain = modEnvFltRelease = 0.0f;
    }
    
    /** Resets the internal state of the voice. */
    virtual void reset() = 0;

    /** MPE Channel tracking */
    virtual void setChannel(int channel) = 0;
    virtual int getChannel() const = 0;
};

} // namespace NEURONiK::DSP
