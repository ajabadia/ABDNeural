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
 * @brief Una ruta de la matriz de modulacion: fuente, destino y cantidad.
 * @details Vive en el namespace y no dentro de `GlobalParams` para poder
 *          devolver un array de ellas (State/ModMatrixFromState.h) sin arrastrar
 *          el struct entero a la capa de State. No depende de nada.
 *
 *          Y va ANTES que `GlobalParams`, que lo tiene por miembro: declararlo
 *          despues rompe con "no es un miembro de GlobalParams", que es un error
 *          bastante menos util que el que uno espera.
 */
struct ModRoute {
    int source = 0;         //!< indice en getModSources()
    int destination = 0;    //!< indice en getModDestinationTable()
    float amount = 0.0f;    //!< profundidad bipolar
};


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

    ModRoute modMatrix[4];
};

} // namespace NEURONiK::DSP
