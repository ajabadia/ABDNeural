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
 * @details El ancho es el del MOTOR (`abd::dsp::kFxMaxParams`, 12) y hasta
 *          2026-09-29 fue el del CATALOGO DE ESTE PRODUCTO (4, el mayor numero
 *          de mandos que pedia cualquiera de sus seis efectos). El cambio es
 *          deliberado y el motivo es el mismo que hace que el bus exista: con el
 *          TIPO expuesto en el APVTS, el hueco 2 deja de ser "el coro" y pasa a
 *          ser "el hueco 2, con lo que le pongas". Un bus del ancho de la fila
 *          mas ancha que hay hoy es un bus que trunca en silencio el dia que
 *          entre una fila de cinco, y el sintoma --el hueco sonando raro, sin
 *          error-- es el que el propio `FxCatalogue.h` describe.
 *
 *          LO QUE SE PAGA, y hay que decirlo porque el numero anterior estaba
 *          escrito precisamente para evitarlo: ocho knobs por hueco que la
 *          fila de ese hueco no lee, y ocho numeros mas por hueco en cada
 *          preset y en el espejo del hilo de audio. Se acepta porque el mando
 *          que el host automatiza tiene que ser el mismo para cualquier efecto
 *          (con el rango de la fila, el skew con el sesgo), y porque es la
 *          forma que tiene ABDEep, donde el bus tambien es de doce.
 *
 *          `fxWidestParamCount()` (FxCatalogue.h) sigue diciendo cuanto pide el
 *          catalogo de verdad: es el numero que se imprime y el que la pagina
 *          ensea, y ya no es el que decide el ancho.
 */
inline constexpr int kFxBusSlots = 4;   //!< == abd::dsp::kFxNumSlots
inline constexpr int kFxBusParams = 12; //!< == abd::dsp::kFxMaxParams

// El ancho del motor y el del bus tienen que ser EL MISMO numero, y no porque
// un `static_assert` no pueda mirar los dos (los dos son `constexpr`): porque
// `FxSlotParams` se inicializa con doce `0.5f` escritos a mano, y un bus mas
// ancho que ellos se rellenaria de CERO en los knobs de mas, que es el peor
// default posible (un preset nuevo con un knob clavado en 0). Si algun dia hay
// doce y tres, este aserto es el que lo dice.
static_assert (kFxBusParams == 12,
               "el inicializador de `FxSlotParams::params` esta escrito para 12");

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
    float params[kFxBusParams] = { 0.5f, 0.5f, 0.5f, 0.5f,   //!< normalizado 0..1
                                   0.5f, 0.5f, 0.5f, 0.5f,
                                   0.5f, 0.5f, 0.5f, 0.5f };
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
    // LOS MANDOS PLANOS DE ARRIBA SIGUEN ESTANDO, y el motor ya no los mira
    // (desde 2026-09-29 los leen los cuatro huecos por el bus). No se borran
    // por las dos razones que pone este mismo comentario: `chorusRate`,
    // `delayTime` y `reverbSize` son indices publicados del espejo y quitarlos
    // correria todo lo que hay detras. Un preset guardado los trae y la pagina
    // los escribe; lo que ya no tienen es destino, y `PresetMigrationFx.cpp` es
    // quien los lleva al bus de su hueco al abrir un preset viejo. Ver
    // `Source/DSP/FxSlots.h`.
    FxSlotParams fx[kFxBusSlots];
};

} // namespace NEURONiK::DSP
