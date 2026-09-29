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
 * @brief Cuantos huecos tiene el rack y cuantos mandos caben en cada uno.
 * @details El ancho lo fija el CATALOGO DEL PRODUCTO (el mayor numero de mandos
 *          que pide cualquiera de sus efectos), no el maximo que el motor
 *          compartido ACEPTA (`abd::dsp::kFxMaxParams`, que son 12 y estan ahi
 *          para los productos que monten los 48 motores de ABDEep). Con un
 *          catalogo que pide 4, un bus de 12 serian ocho parametros muertos por
 *          hueco en cada preset y en el contrato de la pagina.
 *
 *          Los dos numeros se comprueban contra el catalogo en tiempo de
 *          compilacion: `Source/DSP/FxCatalogue.h` los confronta con la tabla
 *          real, asi que un motor nuevo mas ancho rompe la compilacion y no
 *          painta un hueco con los knobs cortados.
 */
inline constexpr int kFxBusSlots = 4;   //!< == abd::dsp::kFxNumSlots
inline constexpr int kFxBusParams = 4;  //!< el mas ancho del catalogo de NEURONiK

/**
 * @brief Un HUECO del rack de efectos: lo que el hilo de audio necesita de el.
 * @details El TIPO (que efecto va puesto) NO esta aqui a proposito:
 *          `FxSlot::setType` CREA y DESTRUYE la instancia, y en el hilo de
 *          audio no puede haber un `new` (ver `Source/DSP/FxSlots.h`). El tipo
 *          vive en el hilo de mensajes, en el procesador, y llega al hueco por
 *          ahi.
 *
 *          Los mandos van NORMALIZADOS 0..1, que es lo que habla un panel; la
 *          fila del catalogo decide el sesgo y las unidades fisicas, y ese viaje
 *          ocurre una sola vez, al empujarlos al hueco.
 *
 * POR QUE UN BUS FIJO Y NO UNA TABLA DE TAMANO VARIABLE. El hueco tiene un
 * numero fijo de mandos porque el APVTS tambien: son parametros con id, que un
 * preset puede guardar, una matriz puede modular y un host puede automatizar. Y
 * porque el motor ya lo es: `FxSlot` habla `setParameter(index, valor)` con un
 * `kFxMaxParams` fijo.
 */
struct FxSlotParams
{
    float params[kFxBusParams] = { 0.5f, 0.5f, 0.5f, 0.5f };  //!< normalizado 0..1
    float gain = 1.0f;                                         //!< salida del hueco
    float mix  = 0.0f;                                         //!< mezcla mojado/seco
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

    // El RACK de efectos, al FINAL y sin reordenar nada de lo de arriba. Este
    // struct tiene un ABI publico: `neuronikGlobalParamsLayout` publica el
    // `offsetof` de cada campo y la pagina escribe el espejo por esos indices
    // (`WebUI/src/wasm/audioParams.js`, 0..33). Anadir al final es SEGURO —los
    // indices viejos no se mueven y el espejo se dimensiona solo con
    // `neuronikGlobalParamsSize`—, mientras que quitar o reordenar un campo
    // cambia lo que la pagina escribe sin que nadie lo note. Por eso el hueco 1
    // se migra sin sacar `saturationAmt` de su sitio: se apaga su ultimo uso (que
    // era el destino 17 de la matriz) y el bus entra por detras.
    //
    // Los huecos 2, 3 y 4 los manejan TODAVIA los mandos planos de antes
    // (chorusRate, delayTime, reverbSize...): la migracion va hueco a hueco y
    // este es el primero. Ver `Source/DSP/FxSlots.h`.
    FxSlotParams fx[kFxBusSlots];
};

} // namespace NEURONiK::DSP
