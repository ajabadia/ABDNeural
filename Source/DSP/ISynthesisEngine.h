/*
  ==============================================================================

    ISynthesisEngine.h
    Created: 29 Jan 2026
    Description: Interface for the main synthesis engine.

  ==============================================================================
*/

#pragma once

// GlobalParams vive en DspTypes.h (libre de JUCE) para que hosts sin JUCE
// (fachada Runtime, wrapper WASM) puedan hablar de parámetros con el motor.
// DspCore.h: dsp::AudioBuffer / dsp::MidiBuffer / dsp::MidiMessage (ports
// libres de JUCE) son los tipos que cruzan la frontera del bloque.
#include "DspTypes.h"
#include "DspCore.h"
#include "DspMidiBuffer.h"

namespace NEURONiK::Common { struct SpectralModel; }

namespace NEURONiK::DSP {

/**
 * Interface for the NEURONiK synthesis engine.
 *
 * CONTRATO (Fase 1 — frontera del núcleo DSP):
 *
 * Ciclo de vida del host:
 *   1. prepare(sampleRate, samplesPerBlock)   — antes de renderizar; el host
 *      puede volver a llamarlo si cambian ambos (reutiliza el estado).
 *   2. setGlobalParams(p) / setVoiceParams    — en cualquier momento; la
 *      implementación hace el handoff real-time safe (double-buffer interno).
 *   3. renderNextBlock(buffer, midi)          — SOLO en el hilo de audio.
 *   4. reset() / allNotesOff()                — cualquier hilo, con cuidado:
 *      allNotesOff solo dispara releases (RT-safe); reset libera estado.
 *
 * Seguridad real-time de renderNextBlock:
 *   - Sin asignaciones de heap, sin locks, sin llamadas al sistema.
 *   - Los parámetros se consumen del snapshot pendiente (setGlobalParams),
 *     nunca se leen parámetros del host directamente dentro del render.
 *   - Los eventos MIDI llegan con sampleOffset dentro del bloque.
 *
 * Hilos de los getters de visualización: seguros desde cualquier hilo
 * (atómicos/dobles buffers internos); pensados para un timer de UI (~30 Hz).
 *
 * NO depende del host: los tipos que cruza la frontera (GlobalParams,
 * dsp::AudioBuffer, dsp::MidiBuffer, dsp::MidiMessage) no incluyen JUCE. Un
 * host sin JUCE usa la fachada Runtime::DspEngineFacade (punteros crudos +
 * Runtime::Event); un host JUCE traduce con Runtime::JuceMidiAdapter.
 */
class ISynthesisEngine {
public:
    virtual ~ISynthesisEngine() = default;

    enum class Type { Neuronik, Neurotik };
    virtual Type getType() const = 0;

    /** Prepares the engine for playback. */
    virtual void prepare(double sampleRate, int samplesPerBlock) = 0;

    /** Processes a block of audio. */
    virtual void renderNextBlock(dsp::AudioBuffer<float>& buffer, dsp::MidiBuffer& midiMessages) = 0;

    /** Real-time safe parameter update. */
    virtual void updateParameters() = 0;

    /** Returns the current polyphony. */
    virtual int getNumActiveVoices() const = 0;

    /** Resets the engine state. */
    virtual void reset() = 0;

    /** Inject a MIDI message from the UI or external source. */
    virtual void handleMidiMessage(const dsp::MidiMessage& msg) = 0;

    /** Visualization getters (Thread-safe) */
    virtual float getLfoValue(int index) const = 0;
    virtual void getSpectralData(float* destination64) const = 0;
    virtual void getEnvelopeLevels(float& amp, float& filter) const = 0;
    virtual void getModulationValues(float* destination, int count) const = 0;

    /** Load a spectral model into the engine. */
    virtual void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) = 0;

    /** Set the maximum number of active voices. */
    virtual void setPolyphony(int numVoices) = 0;

    /**
     * @brief Releases every sounding voice.
     * @details Called when the input channel changes, so notes received on the
     *          previous channel cannot hang forever, and useful as a panic.
     *          Real-time safe: it only triggers the release stage of each voice.
     */
    virtual void allNotesOff() = 0;

    /** Set global parameters. */
    virtual void setGlobalParams(const GlobalParams& p) = 0;
};

} // namespace NEURONiK::DSP
