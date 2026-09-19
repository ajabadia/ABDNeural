/*
  ==============================================================================

    BaseEngine.h
    Created: 30 Jan 2026
    Description: Base class for synthesis engines, providing shared FX and LFO logic.

  ==============================================================================
*/

#pragma once

#include "DspCore.h"

#include "ISynthesisEngine.h"
#include "IVoice.h"
#include "Effects/Saturation.h"
#include "Effects/Delay.h"
#include "Effects/Chorus.h"
#include "Effects/Reverb.h"
#include "CoreModules/LFO.h"
#include <vector>
#include <memory>
#include <atomic>

namespace NEURONiK::DSP {

class BaseEngine : public ISynthesisEngine
{
public:
    BaseEngine();
    virtual ~BaseEngine() override = default;

    // --- ISynthesisEngine Shared Implementation ---
    void prepare(double sampleRate, int samplesPerBlock) override;
    void updateParameters() override;
    void reset() override;
    void handleMidiMessage(const dsp::MidiMessage& msg) override;
    
    float getLfoValue(int index) const override;
    void getModulationValues(float* destination, int count) const override;
    
    // Voices management
    int getNumActiveVoices() const override;
    void setPolyphony(int numVoices) override;
    void allNotesOff() override;

protected:
    /** Tamano de la rejilla de control, en muestras de audio.

        La modulacion (LFO -> matriz) NO se actualiza por bloque del host: se
        actualiza cada kControlBlockSize muestras, con el resto encadenado entre
        bloques. Asi la tasa de control la fija el tiempo y no el buffer (128 en el
        AudioWorklet, 512 en un host grande), que es lo que hace que web y nativo
        den la misma senal. 64 = dos sub-bloques de voz (las voces ya trabajan en
        tramos de 32, asi que no ven fronteras nuevas) y ~1,3 ms a 48 kHz: con el LFO
        a 20 Hz son ~37 escalones por ciclo, frente a los ~19 de 128 muestras. */
    static constexpr int kControlBlockSize = 64;

    /** Renderiza las voces en tramos de tasa de control fija: en cada tramo avanza
        los LFOs, aplica la matriz de modulacion y solo despues renderiza esas
        muestras, de modo que el paso de la modulacion no depende del troceado.
        Las subclases llaman a esto en renderNextBlock en lugar del bucle de voces. */
    void renderVoicesWithControlRate(dsp::AudioBuffer<float>& buffer);

    /** Subclasses must call this at the end of their renderNextBlock. */
    void applyGlobalFX(dsp::AudioBuffer<float>& buffer);
    
    /** Subclasses must implement this to route MIDI to their specific voice types. */
    virtual void handleMidiEvent(const dsp::MidiMessage& m) = 0;

    /** Aplica la matriz de modulacion (LFO -> voces). Se llama UNA vez por tramo de
        control (ver kControlBlockSize), no una vez por bloque del host. */
    virtual void applyModulation() = 0;
    
    /** Common MIDI processing loop. */
    void processMidiBuffer(dsp::MidiBuffer& midiMessages);

    std::vector<std::unique_ptr<IVoice>> voices;
    std::atomic<int> activeVoiceLimit { 16 };

    // Shared FX
    Effects::Saturation saturation;
    Effects::Delay delay;
    Effects::Chorus chorus;
    Effects::Reverb reverb;
    dsp::LinearSmoothedValue<float> masterLevelSmoother;

    // Shared LFOs (semillas distintas: S&H decorrelacionado entre lfo1/lfo2)
    Core::LFO lfo1 { 0x1D0F1u }, lfo2 { 0x1D0F2u };
    std::atomic<float> lfo1Value { 0.0f };
    std::atomic<float> lfo2Value { 0.0f };

    GlobalParams pendingGlobalParams;
    GlobalParams currentGlobalParams;

    double currentSampleRate = 48000.0;
    int currentSamplesPerBlock = 512;
    int controlCarry = 0;   // muestras ya consumidas del tramo de control en curso

    dspDeclareNonCopyableWithLeakDetector(BaseEngine)
};

} // namespace NEURONiK::DSP
