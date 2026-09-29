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
#include "FxSlots.h"
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

    /** El tipo de un hueco. Hilo de mensajes; ver `ISynthesisEngine`. */
    void setFxSlotType (int slot, int type, const FxSlotParams& bus) override
    {
        fx.setSlotType (slot, type, bus);
    }

    /** @brief Voces RESERVADAS ahora mismo (las del heap), no las que suenan.

        Es el numero que dice si la reserva perezosa funciona: un motor recien
        creado reserva `activeVoiceLimit` voces —16 la aditiva, 8 la neurotik—,
        no las 32 de antes; subir la polifonia las añade y bajarla devuelve
        solo las OCiosas (ver `setPolyphony`): una voz que aun suelta su cola
        (isActive) no se desalojan; las demas, si. La CAPACIDAD en caliente
        (reserve 32) se mantiene, solo baja el tamano; volver a subir solo
        re-crea (prepare si ya estaba preparado). */
    int getNumAllocatedVoices() const noexcept { return (int) voices.size(); }

    /** MORPH del pad XY (2026-09-26): publica (morphX, morphY) en el canal
        de VoiceParams. La base solo publica (x, y): cada motor hace el
        read-modify-write de SU struct de params — y el que lleva eje Z
        (morphZ) lo anade en su override, porque el default del struct es
        el que manda. */
    virtual void setMorph (float morphX, float morphY)
    {
        (void) morphX; (void) morphY;
    }

    /** Eje temporal del morph (FASE 10): la base no lo publica — el motor
        que lo usa hace el read-modify-write en su override. */
    virtual void setMorphZ (float morphZ)
    {
        (void) morphZ;
    }

    /** FASE 11.4: el VOLUMEN de las capas 1 y 2 (la capa 0 siempre al
        fondo, el legado). La base no lo publica: el motor que tiene
        capas en su struct hace el read-modify-write en su override. */
    virtual void setVoiceLayerMorph (float layerGain2, float layerGain3)
    {
        (void) layerGain2; (void) layerGain3;
    }

    /** Los OCHO tramos ADSR de la voz, en MILISEGUNDOS (los sustains en 0..1):
        attack/decay/sustain/release de la ENV de AMP y los cuatro de la ENV de
        FILTRO. Es el canal que usa el worklet del navegador, que no tiene
        APVTS: en el plugin los ocho los escribe `synchronizeEngineParameters`
        por `setVoiceParams` (el struct entero), aqui entran por el read-modify-write
        de `pendingVoiceParams` para no pisar el morph que la pagina ya Cruzo.

        Por eso NO es `setVoiceParams`: ese metodo reemplaza el struct COMPLETO,
        asi que quien lo usara desde el worklet borraria morphX/Y/Z y los
        volumenes de las capas 1 y 2 en cada tecla. La base no lo publica (un
        motor puede no tener envolvente de filtro) y cada motor decide que
        campos de los ocho son suyos; las unidades las fija el engine, aqui
        no hay clampeo (lo hace `Envelope::setParameters`). */
    virtual void setVoiceEnvelope (float attack, float decay, float sustain, float release,
                                   float fAttack, float fDecay, float fSustain, float fRelease)
    {
        (void) attack; (void) decay; (void) sustain; (void) release;
        (void) fAttack; (void) fDecay; (void) fSustain; (void) fRelease;
    }

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

    /** Techo de voces: el mismo 32 que aplica `setPolyphony`. */
    static constexpr int kMaxVoices = 32;

    /** Crea la voz `index` (0-based). La implementa cada motor: la aditiva no
        usa el indice; la neurotik lo usa como semilla del ruido de excitacion. */
    virtual std::unique_ptr<IVoice> createVoice (int index) = 0;

    /** Reserva PEREZOSA: crea voces hasta `count` (y las prepara si el motor ya
        lo esta). Se llama SOLO desde el hilo de mensajes —constructor, prepare y
        setPolyphony—: el hilo de audio nunca reserva, solo indexa. */
    void ensureVoices (int count);

    /** Reclama voces ociosas al bajar el limite: solo las que no suenan
        (isActive()==false). Las que aun tienen cola se compactan al frente
        para no quedar cortadas; la capacidad reservada (32) no se toca,
        solo baja `size()`. Hilo de mensajes con el cerrojo del procesador. */
    void reclaimIdleVoices (int newLimit) noexcept;

    std::vector<std::unique_ptr<IVoice>> voices;
    std::atomic<int> activeVoiceLimit { 16 };

    /** `prepare()` ya paso: las voces nuevas que cree `ensureVoices` se preparan
        en el acto (si no, nacerian sordas al subir la polifonia en caliente). */
    bool voicesPrepared = false;

    // Shared FX. Cuatro huecos del motor de slots del modulo compartido, no
    // cuatro envoltorios: el reparto de efectos, el orden y el mapeo de los
    // mandos estan en `FxSlots.h`, que es el unico sitio donde se decide.
    FxSlots fx;
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
