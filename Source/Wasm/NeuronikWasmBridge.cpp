/*
  ==============================================================================

    NeuronikWasmBridge.cpp
    Created: 17 Sep 2026
    Description: extern "C" bridge exposing the synthesis engine to JS
                 (AudioWorklet) through the Phase-1 boundary
                 (Runtime::DspEngineFacade + Runtime::Event + GlobalParams).

    ABI conventions follow the ABDMS2000 pattern (flat C exports,
    EMSCRIPTEN_KEEPALIVE, JS owns the heap buffers via _malloc), but the
    engine behind it is the REAL DSP — no port, so native and WASM render
    the same deterministic output.

    Event layout: Runtime::Event is standard-layout, 24 bytes:
      [0]i32 type  [1]i32 channel  [2]i32 note  [3]i32 value14
      [4]f32 value [5]i32 sampleOffset
    GlobalParams: JS never hardcodes offsets — call globalParamsLayout()
    once and write fields by the returned offsets.
    ADSR de la voz: el POD VoiceEnvelopeWire (8 floats) en su propio canal,
    con el mismo patron (voiceEnvelopeSize/Layout + setVoiceEnvelope).

  ==============================================================================
*/

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define WASM_EXPORT
#endif

#include "../DSP/Runtime/DspEngineFacade.h"
#include "../DSP/Runtime/DspEvent.h"
#include "../DSP/DspTypes.h"
#include "../DSP/CoreModules/NeuronikEngine.h"
#include "../DSP/CoreModules/NeurotikEngine.h"
#include "GlobalParamsLayout.h"

#include <cmath>
#include <cstddef>
#include <vector>
#include <cstring>
#include <memory>

namespace
{
    // Static asserts: the JS side depends on these exact sizes.
    static_assert(std::is_standard_layout<NEURONiK::DSP::Runtime::Event>::value,
                  "Runtime::Event must cross the WASM boundary as raw bytes");
    static_assert(sizeof(NEURONiK::DSP::Runtime::Event) == 24,
                  "Unexpected Runtime::Event layout (JS ABI is 24 bytes)");

    struct EngineInstance
    {
        std::unique_ptr<NEURONiK::DSP::ISynthesisEngine> engine;
        std::unique_ptr<NEURONiK::DSP::Runtime::DspEngineFacade> facade;
        double sampleRate = 48000.0;
        int blockSize = 128;
    };

    /** El ADSR de la voz en el HIELO: el POD que JS rellena, en el mismo
        estilo que GlobalParams pero para el canal de VoiceParams. Son ocho
        floats en MILISEGUNDOS (los dos sustains en 0..1) y NO el struct
        `AdditiveVoice::Params` / `NeurotikVoice::Params`: esos dos structs son
        distintos entre si (el neurotik no tiene envolvente de filtro) y
        cambiar uno no puede cambiar el ABI de la frontera. El POD es la
        interseccion de los dos, mas estable que cualquiera de ellos; el
        read-modify-write de cada campo lo hace el motor (setVoiceEnvelope). */
    struct VoiceEnvelopeWire
    {
        float ampAttackMs;
        float ampDecayMs;
        float ampSustain;
        float ampReleaseMs;
        float filterAttackMs;
        float filterDecayMs;
        float filterSustain;
        float filterReleaseMs;
    };

    static_assert(std::is_standard_layout<VoiceEnvelopeWire>::value,
                  "The ADSR POD must cross the WASM boundary as raw bytes");
    static_assert(sizeof(VoiceEnvelopeWire) == 32,
                  "Eight floats: the JS mirror allocates exactly this");

    EngineInstance& instance()
    {
        static EngineInstance inst;
        return inst;
    }

    void rebuildEngine (int engineType)
    {
        auto& inst = instance();
        if (engineType == 1)
            inst.engine = std::make_unique<NEURONiK::DSP::NeurotikEngine>();
        else
            inst.engine = std::make_unique<NEURONiK::DSP::NeuronikEngine>();

        inst.facade = std::make_unique<NEURONiK::DSP::Runtime::DspEngineFacade> (*inst.engine);
        inst.facade->prepare (inst.sampleRate, inst.blockSize);
    }
}

extern "C" {

WASM_EXPORT void neuronikInit (double sampleRate, int blockSize)
{
    auto& inst = instance();
    inst.sampleRate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    inst.blockSize  = blockSize > 0 ? blockSize : 128;

    if (inst.engine == nullptr)
        rebuildEngine (0);
    else
        inst.facade->prepare (inst.sampleRate, inst.blockSize);
}

WASM_EXPORT void neuronikSetEngine (int engineType)
{
    rebuildEngine (engineType == 1 ? 1 : 0);
}

/**
 * Renders one block. outL/outR are Float32Array-backed heap pointers owned by
 * JS (allocate once via _malloc, reuse every block). events is a pointer to
 * numEvents consecutive 24-byte Runtime::Event records (may be null).
 */
WASM_EXPORT void neuronikProcess (float* outL, float* outR, int numSamples,
                                  const NEURONiK::DSP::Runtime::Event* events,
                                  int numEvents)
{
    auto& inst = instance();
    if (inst.facade == nullptr)
        return;

    inst.facade->process (outL, outR, numSamples, events, numEvents);
}

WASM_EXPORT void neuronikSetGlobalParams (const void* pod, int byteSize)
{
    if (pod == nullptr || byteSize != (int) sizeof (NEURONiK::DSP::GlobalParams))
        return;

    NEURONiK::DSP::GlobalParams params;
    std::memcpy (&params, pod, sizeof (params));
    instance().facade->setGlobalParams (params);
}

/** Byte size JS must allocate for the GlobalParams mirror. */
WASM_EXPORT int neuronikGlobalParamsSize()
{
    return (int) sizeof (NEURONiK::DSP::GlobalParams);
}

/**
 * Spectral model slots (preset timbre data). A preset is APVTS state PLUS up to
 * four SpectralModel slots (64 partials each) the Resonator morphs between;
 * they never live in GlobalParams, so they cross the boundary separately.
 * data is 128 consecutive floats: amplitudes[0..63] then frequencyOffsets[0..63].
 * engineType selects the active engine (0 = NEURONiK, 1 = Neurotik) because the
 * models hang off the concrete engine, not the facade. loadModel fans the model
 * out to every voice (whole-POD swap + rebuild latch, same as the native plugin).
 * FASE 11.1: el v1 del puente cruza 128 floats PLANOS (amplitudes + offsets) y no
 * tiene sitio para los frames, asi que un modelo de CAPAS suena su capa 0: el
 * clamp esta abajo, junto al struct que se rellena.
 */
WASM_EXPORT void neuronikLoadModel (int slot, int engineType, const float* data, int isValid)
{
    if (data == nullptr || slot < 0 || slot >= 4)
        return;

    NEURONiK::Common::SpectralModel model;
    std::memcpy (model.amplitudes.data(), data, 64 * sizeof (float));
    std::memcpy (model.frequencyOffsets.data(), data + 64, 64 * sizeof (float));

    // FASE 11.1 — CLAMP DE CAPAS DEL v1: el struct nace con layerCount = 1 (la
    // capa 0, la que viaja en estos 128 floats) y aqui se deja EXPLICITO, porque
    // es una decision de formato y no un accidente: el bloque por slot se queda
    // en ~33 KB (un modelo de 3 capas x 16 frames seria ~100 KB por slot, el
    // presupuesto que el plan de capas deja fuera del v1 WASM). El DSP no lee
    // ningun campo de capa, de modo que el camino nativo/WASM sigue bit-exacto
    // (paridad A-E, 0 ulps) mientras la WebUI avisa de que suena la capa 0.
    model.layerCount = 1;
    model.isValid = isValid != 0;

    auto& inst = instance();
    if (inst.engine == nullptr)
        return;

    if (engineType == 1)
        static_cast<NEURONiK::DSP::NeurotikEngine*> (inst.engine.get())->loadModel (model, slot);
    else
        static_cast<NEURONiK::DSP::NeuronikEngine*> (inst.engine.get())->loadModel (model, slot);
}

/**
 * Layout descriptor so JS writes the GlobalParams mirror without hardcoded
 * offsets: outOffsets points at an i32 array of numFields entries where the
 * bridge stores offsetof() of every field, in the documented field order
 * (masterLevel, saturationAmt, bpm, delayTime, delayFB, chorusRate,
 * chorusDepth, chorusMix, reverbSize, reverbDamping, reverbWidth, reverbMix,
 * lfo1.waveform, lfo1.rateHz, lfo1.syncMode, lfo1.rhythmicDivision,
 * lfo1.depth, lfo2.<same five>, modMatrix[r].{source,destination,amount} r=0..3,
 * fx[s].{params[0..3], gain, mix} for s=0..kFxBusSlots-1).
 * Returns the number of fields written.
 *
 * EL BUS DE EFECTOS VA AL FINAL, Y POR ESO NO ROMPE NADA. Este array es un ABI:
 * la pagina escribe el espejo por INDICES (`WebUI/src/wasm/audioParams.js`), no
 * por offset, asi que cualquier campo nuevo tiene que ir detras para no
 * desplazarlos. `saturationAmt` sigue en el sitio 1 aunque ya no lo maneje nadie
 * (el hueco 1 tiene bus desde 2026-09-29): sacarlo de ahi cambio lo que la
 * pagina escribe en el 1 sin que ningun aviso lo dijera.
 */
WASM_EXPORT int neuronikGlobalParamsLayout (int* outOffsets, int maxFields)
{
    // El orden lo dice `globalParamsLayout` (arriba), y lo comparten los dos
    // exports de layout: duplicar los bucles aqui era la forma de que uno de
    // los dos acabara mintiendo sobre el ABI sin que se notara.
    const auto layout = globalParamsLayout();

    const auto count = static_cast<int> (layout.size());

    if (outOffsets != nullptr)
    {
        const int n = count < maxFields ? count : maxFields;
        for (int i = 0; i < n; ++i)
            outOffsets[i] = static_cast<int> (layout[static_cast<std::size_t> (i)]);
        return n;
    }

    return count;
}

/**
 * FASE 11.3 — LAS CAPAS cruzan la frontera (2026-09-26). UNA llamada que
 * carga el slot COMPLETO: la raiz (`data`, el layout v1 de 128 floats) como
 * capa 0 y las capas extras (`extraData`) como capas 1..2. `loadModel(model,
 * slot)` REEMPLAZA el struct entero, asi que la carga en dos llamadas
 * BORRARIA la raiz; con este export el slot queda atomico y por el camino
 * de siempre. El v1 (`neuronikLoadModel`) no cambia ni un byte: quien no
 * llama aqui tiene exactamente el clamp de 11.1 (capa 0 unica).
 *
 * Layout del buffer de capas (JS lo escribe en un scratch de heap), repetido
 * `layerFrames` veces: { amps[64], offsets[64], frameF0 } = 193 floats por
 * frame; los pesos temporales viajan detras (`layerFrames` floats) y el peso
 * estatico como argumento escalar. Frames clampeados a kMaxFrames (16), capas
 * a kMaxLayers (3) — el mismo trato que el lector de presets.
 */
WASM_EXPORT void neuronikLoadModelLayers (int slot, int engineType,
                                          const float* data, int isValid,
                                          const float* extraData, int layerFrames,
                                          float layerWeight, const float* frameWeights)
{
    if (data == nullptr || slot < 0 || slot >= 4)
        return;

    NEURONiK::Common::SpectralModel model;
    std::memcpy (model.amplitudes.data(), data, 64 * sizeof (float));
    std::memcpy (model.frequencyOffsets.data(), data + 64, 64 * sizeof (float));
    model.isValid = isValid != 0;

    using Model = NEURONiK::Common::SpectralModel;

    if (extraData != nullptr && layerFrames > 0)
    {
        const int count = layerFrames > Model::kMaxFrames ? Model::kMaxFrames : layerFrames;
        constexpr int floatsPerFrame = 3 * 64 + 1;

        model.setLayerCount (2);
        model.setLayerWeightAt (1, layerWeight);
        model.setNumFramesOf (1, count);

        for (int f = 0; f < count; ++f)
        {
            const float* src = extraData + (size_t) f * (size_t) floatsPerFrame;
            float* amps = model.ampsOf (1, f);
            float* offs = model.offsetsOf (1, f);

            for (int i = 0; i < 64; ++i)
            {
                amps[(size_t) i] = src[(size_t) i];
                offs[(size_t) i] = src[64 + (size_t) i];
            }

            model.setF0At (1, f, src[128]);   // 0 = rejilla comun (sin remapeo)

            const float w = (frameWeights != nullptr) ? frameWeights[(size_t) f] : 1.0f;
            model.setFrameWeightAt (1, f, w);
        }
    }
    // (sin extraData el modelo queda en layerCount = 1: el clamp del v1)

    auto& inst = instance();
    if (inst.engine == nullptr)
        return;

    if (engineType == 1)
        static_cast<NEURONiK::DSP::NeurotikEngine*> (inst.engine.get())->loadModel (model, slot);
    else
        static_cast<NEURONiK::DSP::NeuronikEngine*> (inst.engine.get())->loadModel (model, slot);
}

/**
 * MORPH del pad XY + eje temporal (2026-09-26): los tres son VoiceParams y
 * antes no cruzaban la frontera por NINGUN camino — en el plugin los escribe
 * NEURONiKProcessor::synchronizeEngineParameters desde el APVTS, pero el worklet
 * no tiene APVTS. Read-modify-write de pendingVoiceParams en el motor ACTIVO
 * (los defaults del struct no se tocan), misma seguridad que setVoiceParams.
 * Valores fuera de [0,1] se clampean aqui, en la frontera.
 */
WASM_EXPORT void neuronikSetVoiceMorph (float morphX, float morphY, float morphZ)
{
    auto& inst = instance();
    if (inst.engine == nullptr)
        return;

    const float x = morphX < 0.0f ? 0.0f : (morphX > 1.0f ? 1.0f : morphX);
    const float y = morphY < 0.0f ? 0.0f : (morphY > 1.0f ? 1.0f : morphY);
    const float z = morphZ < 0.0f ? 0.0f : (morphZ > 1.0f ? 1.0f : morphZ);

    if (inst.engine->getType() == NEURONiK::DSP::ISynthesisEngine::Type::Neurotik)
    {
        auto* neurotik = static_cast<NEURONiK::DSP::NeurotikEngine*> (inst.engine.get());
        neurotik->setMorph (x, y);
        neurotik->setMorphZ (z);
    }
    else
    {
        auto* neuronik = static_cast<NEURONiK::DSP::NeuronikEngine*> (inst.engine.get());
        neuronik->setMorph (x, y);
        neuronik->setMorphZ (z);
    }
}

/**
 * FASE 11.4: el VOLUMEN de las capas 1 y 2 (la capa 0 es el fondo, siempre
 * al maximo). VoiceParams como los tres de arriba; en el plugin los escribe
 * synchronizeEngineParameters desde el APVTS y aqui los cruza el worklet.
 * Read-modify-write en el motor ACTIVO, clampeo a [0,1] en la frontera.
 */
WASM_EXPORT void neuronikSetVoiceLayerMorph (float layerGain2, float layerGain3)
{
    auto& inst = instance();
    if (inst.engine == nullptr)
        return;

    const float g2 = layerGain2 < 0.0f ? 0.0f : (layerGain2 > 1.0f ? 1.0f : layerGain2);
    const float g3 = layerGain3 < 0.0f ? 0.0f : (layerGain3 > 1.0f ? 1.0f : layerGain3);

    if (inst.engine->getType() == NEURONiK::DSP::ISynthesisEngine::Type::Neurotik)
        static_cast<NEURONiK::DSP::NeurotikEngine*> (inst.engine.get())->setVoiceLayerMorph (g2, g3);
    else
        static_cast<NEURONiK::DSP::NeuronikEngine*> (inst.engine.get())->setVoiceLayerMorph (g2, g3);
}

/** ============================================================================
 *  ADSR DE LA VOZ (canal `neuronik:voice` del worklet, 2026-09-28).
 *
 *  Por que existe: el navegador no tiene APVTS. Los knobs de envolvente de la
 *  pagina (envAttack..filterRelease) son VoiceParams, no GlobalParams, asi que
 *  hasta ahora NO habia por donde llegaran al motor local: vivian en los
 *  defaults de C++ y la ENV 2 era indistinguible de la ENV 1. Aqui se cruzan
 *  los ocho tramos, con el mismo diseno que el canal de GlobalParams — el
 *  espejo vive en JS, que pregunta el tamano y el layout y escribe por indice,
 *  y el puente lo convierte en el read-modify-write del motor.
 *
 *  El camino NO es `setVoiceParams` (reemplaza el struct entero y borraria el
 *  morph que la pagina ya cruzo): es `setVoiceEnvelope`, que solo toca los ocho.
 * ========================================================================= */

/** Byte size JS must allocate for the ADSR mirror (8 floats). */
WASM_EXPORT int neuronikVoiceEnvelopeSize()
{
    return (int) sizeof (VoiceEnvelopeWire);
}

/** Field offsets of the ADSR POD, in the documented order (amp attack,
 *  decay, sustain, release; then the four of the filter envelope), all in
 *  MILLISECONDS except the two sustains. Same contract as
 *  neuronikGlobalParamsLayout: called with out==0 it answers how many fields
 *  there are, so JS never hardcodes an offset. */
WASM_EXPORT int neuronikVoiceEnvelopeLayout (int* outOffsets, int maxFields)
{
    const std::size_t offsets[] = {
        offsetof (VoiceEnvelopeWire, ampAttackMs),
        offsetof (VoiceEnvelopeWire, ampDecayMs),
        offsetof (VoiceEnvelopeWire, ampSustain),
        offsetof (VoiceEnvelopeWire, ampReleaseMs),
        offsetof (VoiceEnvelopeWire, filterAttackMs),
        offsetof (VoiceEnvelopeWire, filterDecayMs),
        offsetof (VoiceEnvelopeWire, filterSustain),
        offsetof (VoiceEnvelopeWire, filterReleaseMs),
    };

    const int count = (int) (sizeof (offsets) / sizeof (offsets[0]));
    if (outOffsets != nullptr)
    {
        const int n = count < maxFields ? count : maxFields;
        for (int i = 0; i < n; ++i)
            outOffsets[i] = (int) offsets[i];
        return n;
    }
    return count;
}

/** Push one ADSR snapshot (8 floats, see the layout above) to the ACTIVE
 *  engine. A snapshot with a non-finite field (NaN travels through
 *  structuredClone, and the mirror is a plain ArrayBuffer) is DROPPED whole:
 *  the page always sends all eight, so the next good snapshot re-applies
 *  everything and a single NaN cannot freeze the envelope at a bogus time.
 *  Clamping is the envelope's (`Envelope::setParameters`: >= 0.1 ms, sustain
 *  0..1), same as on the native path. */
WASM_EXPORT void neuronikSetVoiceEnvelope (const void* pod, int byteSize)
{
    if (pod == nullptr || byteSize != (int) sizeof (VoiceEnvelopeWire))
        return;

    VoiceEnvelopeWire wire;
    std::memcpy (&wire, pod, sizeof (wire));

    const float fields[] = {
        wire.ampAttackMs, wire.ampDecayMs, wire.ampSustain, wire.ampReleaseMs,
        wire.filterAttackMs, wire.filterDecayMs, wire.filterSustain, wire.filterReleaseMs,
    };
    for (float value : fields)
        if (!std::isfinite (value))
            return;

    auto& inst = instance();
    if (inst.engine == nullptr)
        return;

    // El cast a motor concreto es el de los otros canales de voz (setMorphZ,
    // setVoiceLayerMorph): setVoiceEnvelope vive en BaseEngine, no en la
    // interfaz, asi que el motor ACTIVO decide que de los ocho campos son suyos.
    if (inst.engine->getType() == NEURONiK::DSP::ISynthesisEngine::Type::Neurotik)
        static_cast<NEURONiK::DSP::NeurotikEngine*> (inst.engine.get())->setVoiceEnvelope (
            wire.ampAttackMs, wire.ampDecayMs, wire.ampSustain, wire.ampReleaseMs,
            wire.filterAttackMs, wire.filterDecayMs, wire.filterSustain, wire.filterReleaseMs);
    else
        static_cast<NEURONiK::DSP::NeuronikEngine*> (inst.engine.get())->setVoiceEnvelope (
            wire.ampAttackMs, wire.ampDecayMs, wire.ampSustain, wire.ampReleaseMs,
            wire.filterAttackMs, wire.filterDecayMs, wire.filterSustain, wire.filterReleaseMs);
}

/**
 * EL TRAMO QUE VA DETRAS DE LOS ESCALARES: la matriz de modulacion y el bus de
 * los huecos, con la numeracion desde cero (returns count).
 *
 * Es el MISMO tramo, con los MISMOS indices y en el MISMO orden, que
 * `neuronikGlobalParamsLayout` publica a partir del campo
 * `scalarFieldCount()`. Los dos leen el mismo constructor, asi que no pueden
 * discrepar; quien lo necesite en bloque, sin los escalares delante, usa este.
 */
WASM_EXPORT int neuronikModMatrixLayout (int* outOffsets, int maxFields)
{
    // EL TRAMO QUE VA DETRAS DE LOS ESCALARES: la matriz (4 rutas x 3 campos) y
    // el bus de los huecos.
    //
    // Antes publicaba SOLO los doce de la matriz y se comia los del bus, que
    // estaban recogidos en un vector que no se usaba para nada: `total` se
    // calculaba y lo que se devolvia era `count`, o sea la mitad. Publicar la
    // mitad sin avisar es PEOR que no publicar, porque el que lo llama ve un
    // numero que parece completo y le faltan los mandos del bus.
    //
    // El corte lo pone `scalarFieldCount()`, que CUENTA los escalares de la
    // tabla: anadir un escalar nuevo a una de las dos listas y no a la otra
    // dejaba a este export.publicando desde el campo equivocado, en silencio.
    //
    // Los indices que publica son los MISMOS, y en el MISMO orden, que los de
    // `neuronikGlobalParamsLayout` a partir de ahi: este es ese mismo tramo, con
    // su numeracion desde cero. Los dos salen del constructor de arriba.
    const std::size_t first = scalarFieldCount();
    const auto& layout = globalParamsLayout();
    const auto total = static_cast<int> (layout.size() - first);

    if (outOffsets != nullptr)
    {
        const int n = total < maxFields ? total : maxFields;
        for (int i = 0; i < n; ++i)
            outOffsets[i] = static_cast<int> (layout[first + static_cast<std::size_t> (i)]);
        return n;
    }

    return total;
}

WASM_EXPORT void neuronikAllNotesOff()
{
    auto& inst = instance();
    if (inst.facade != nullptr)
        inst.facade->allNotesOff();
}

WASM_EXPORT int neuronikNumActiveVoices()
{
    auto& inst = instance();
    return inst.facade != nullptr ? inst.facade->getNumActiveVoices() : 0;
}

/** Visualization feed for the UI (atomic reads inside the engine). */
WASM_EXPORT float neuronikGetLfo (int index)
{
    auto& inst = instance();
    return inst.facade != nullptr ? inst.engine->getLfoValue (index) : 0.0f;
}

/** Modulation contribution for one destination (visualization feed: the z-ring). */
WASM_EXPORT float neuronikGetMod (int targetIndex)
{
    auto& inst = instance();
    if (inst.engine == nullptr) return 0.0f;

    float mods[64] {};
    inst.engine->getModulationValues (mods, 64);
    return (targetIndex >= 0 && targetIndex < 64) ? mods[targetIndex] : 0.0f;
}

/**
 * Envelope levels for the UI (the WebUI's ADSR needles). Same feed the native
 * bridge sends as telemetry `envelopes: [amp, filter]` (ParameterBridge reads
 * getEnvelopeLevels); the worklet meter polls it at its ~21 ms cadence so the
 * browser page (SOUND ON) sees the same needles the plugin does. The engine
 * returns the FIRST active voice's levels (same convention as native).
 */
WASM_EXPORT void neuronikGetEnvelopeLevels (float* outAmp, float* outFilter)
{
    auto& inst = instance();
    if (inst.engine == nullptr || outAmp == nullptr || outFilter == nullptr)
    {
        if (outAmp != nullptr) *outAmp = 0.0f;
        if (outFilter != nullptr) *outFilter = 0.0f;
        return;
    }

    inst.engine->getEnvelopeLevels (*outAmp, *outFilter);
}

} // extern "C"
