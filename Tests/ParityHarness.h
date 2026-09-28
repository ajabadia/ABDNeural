/*
  ==============================================================================

    ParityHarness.h
    Created: 28 Sep 2026
    Description: EL RIG DE LAS DOS REDES DE PARIDAD, en un solo sitio. Las dos
                 rendian con el mismo modelo y las mismas voces, escritos dos
                 veces, con el cutoff a 300 Hz duplicado y su explicacion: si uno
                 cambiaba, compararian audio distinto y ninguna fallaria.

  ==============================================================================
*/

#pragma once

#include "CoreModules/NeuronikEngine.h"
#include "Common/SpectralModel.h"

namespace NEURONiK::Tests::Parity
{

/** 64 como en la red original: con 128 la reserva de voces se paga por el mismo silencio. */
inline constexpr int kBlockSize = 64;

/** Dos frames: el 0 seno puro, el 1 con el segundo parcial. Con morph a 0 el audio es estable. */
inline Common::SpectralModel model()
{
    Common::SpectralModel m;
    m.amplitudes.fill (0.0f);
    m.frequencyOffsets.fill (0.0f);
    m.amplitudes[0] = 1.0f;
    m.isValid = true;
    m.extraAmps[0].fill (0.0f);
    m.extraOffsets[0].fill (0.0f);
    m.extraAmps[0][1] = 0.5f;
    m.frameCount = 2;
    return m;
}

/**
 * @brief Las voces con las que se rinde TODO el rig.
 *
 * Y aqui esta el numero que parece arbitrario y no lo es: EL FILTRO A 300 Hz.
 * Con el cutoff por defecto (20000 Hz) el `jlimit (20, 20000, ...)` de
 * AdditiveVoice satura siempre, con lo que la envolvente del filtro no mueve
 * NADA: los seis destinos de ENV 2 rendian audio identico y la red era ciega
 * justo en la rama que mas importa, la de reemplazo. Con el corte a 300 Hz la
 * envolvente SI mueve el corte y cada destino se distingue de los otros. Un
 * `filterCutoff = 20000.0f` aqui no es un valor por defecto: es la red apagada,
 * y no falla, que es lo peor.
 */
inline DSP::Synthesis::AdditiveVoice::Params voiceParams()
{
    DSP::Synthesis::AdditiveVoice::Params voice;
    voice.attack = 1.0f;
    voice.decay = 1000.0f;
    voice.sustain = 0.7f;
    voice.release = 10.0f;
    voice.morphX = 0.0f;
    voice.morphY = 0.0f;
    voice.filterCutoff = 300.0f;
    voice.filterRes = 0.5f;
    voice.fAttack = 1.0f;
    voice.fDecay = 200.0f;
    voice.fSustain = 0.5f;
    voice.fRelease = 10.0f;
    return voice;
}

/** El motor listo. DOS voces y no dieciseis: la reserva es perezosa y las otras quince
    solo anaden silencio. */
inline void prepare (DSP::NeuronikEngine& engine, const DSP::GlobalParams& params)
{
    engine.setPolyphony (2);
    engine.prepare (44100.0, kBlockSize);
    engine.loadModel (model(), 0);
    engine.setVoiceParams (voiceParams());
    engine.setGlobalParams (params);
    engine.updateParameters();
}

} // namespace NEURONiK::Tests::Parity
