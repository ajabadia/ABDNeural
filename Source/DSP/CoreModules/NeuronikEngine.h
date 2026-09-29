/*
  ==============================================================================

    NeuronikEngine.h
    Created: 29 Jan 2026
    Description: Central synthesis engine.

  ==============================================================================
*/

#pragma once

#include "../BaseEngine.h"
#include "../Synthesis/AdditiveVoice.h"
#include "../../Common/SpectralModel.h"
#include <array>

namespace NEURONiK::DSP {

class NeuronikEngine : public BaseEngine
{
public:
    NeuronikEngine();
    ~NeuronikEngine() override = default;

    // --- ISynthesisEngine Implementation ---
    Type getType() const override { return Type::Neuronik; }
    void prepare(double sampleRate, int samplesPerBlock) override;
    void renderNextBlock(dsp::AudioBuffer<float>& buffer, dsp::MidiBuffer& midiMessages) override;
    void updateParameters() override;
    void getSpectralData(float* destination64) const override;
    void getEnvelopeLevels(float& amp, float& filter) const override;
    void getModulationValues(float* destination, int count) const override;

    // --- Specific API ---
    void setVoiceParams(const ::NEURONiK::DSP::Synthesis::AdditiveVoice::Params& p);

    /** MORPH del pad XY (2026-09-26): read-modify-write de
        pendingVoiceParams (el mismo canal que setVoiceParams), con el
        morphZ del struct sin tocar — el eje temporal vive en su propio
        parametro y no lo pisa el pad. */
    void setMorph (float morphX, float morphY) override;
    void setMorphZ (float morphZ) override;
    void setVoiceLayerMorph (float layerGain2, float layerGain3) override;

    /** Los ocho tramos ADSR (ms; sustains en 0..1) de las DOS envolventes de la
        voz aditiva: solo toca los ocho campos de pendingVoiceParams, asi que el
        morph que ya haya cruzado la frontera se conserva. */
    void setVoiceEnvelope (float attack, float decay, float sustain, float release,
                           float fAttack, float fDecay, float fSustain, float fRelease) override;
    
    void setGlobalParams(const GlobalParams& p) override { pendingGlobalParams = p; }

    void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) override;

private:
    void handleMidiEvent(const dsp::MidiMessage& m) override;
    void applyModulation() override;

    std::unique_ptr<IVoice> createVoice (int index) override;

    ::NEURONiK::DSP::Synthesis::AdditiveVoice::Params pendingVoiceParams;
    std::array<float, 64> lastModulations { 0.0f };

    // LAS TRES FUENTES QUE ESTABAN MUERTAS (2026-09-28). La tabla de fuentes
    // de la matriz offers Pitch Bend, Mod Wheel y Aftertouch, pero el array
    // `sources` de applyModulation los ponia a 0 con un TODO: se podian
    // SELECCIONAR en la UI y no modulaban nada. Es la clase de fallo mas
    // incomoda que hay, porque la pagina dice que la ruta existe.
    //
    // Antes de estos miembros solo se aplicaban POR VOZ (notePitchBend,
    // notePressure, noteTimbre), asi que no habia de donde leerlas para la
    // matriz: por eso eran un TODO y no un descuido. Ahora el mismo gesto MIDI
    // que mueve la voz guarda ademas el valor normalizado aqui, y applyModulation
    // lo lee. Una sola verdad para las dos.
    //
    // Escribe quien las recibe (handleMidiEvent) y lee applyModulation, ambos en
    // el hilo de audio; no hacen falta atomics porque no hay otro lector.
    float pitchBendSource_ { 0.0f };    ///< -1..+1, 0 = sin deflection
    float modWheelSource_ { 0.0f };     ///< 0..1
    float aftertouchSource_ { 0.0f };   ///< 0..1

    dspDeclareNonCopyableWithLeakDetector(NeuronikEngine)
};

} // namespace NEURONiK::DSP
