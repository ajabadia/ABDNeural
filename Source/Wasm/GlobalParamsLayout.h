/*
  ==============================================================================

    GlobalParamsLayout.h

    EL ORDEN DEL LAYOUT DE GlobalParams, en una cabecera y no en el .cpp.

    Vive aqui porque el orden es un ABI --la pagina escribe el espejo por
    INDICES, no por offset-- y porque comprobarlo necesita un include:
    `neuronikModMatrixLayout` y `neuronikGlobalParamsLayout` lo consumen, y
    `Tests/WasmLayoutOrderTest.cpp` lo confronta con los numeros que publicita
    la pagina. En el .cpp no habria forma de probarlo sin rebuildar el wasm,
    y reconstruirlo cada vez que se toca el orden no es un plan.

    Y POR QUE NO USA ENLACE C: una funcion con enlace `extern "C"` no puede
    devolver `std::vector` (C2526), que es justo lo que hace falta para
    construir el layout entero de una vez.

  ==============================================================================
*/

#pragma once

#include <cstddef>
#include <vector>

#include "../DSP/DspTypes.h"

// El layout se construye FUERA del `extern "C"` de abajo, y no por gusto: una
// funcion con enlace C no puede devolver `std::vector` (C2526), que es el tipo
// que hace falta para construir el layout entero de una vez.
namespace
{

/**
 * @brief EL LAYOUT COMPLETO de GlobalParams, en el orden que publica la pagina.
 *
 * @details UNA SOLA COPIA DEL ORDEN, y esa es toda la razon de que este bloque
 *          exista. Lo consumen los dos exports de layout
 *          (`neuronikGlobalParamsLayout` y `neuronikModMatrixLayout`): con bucles
 *          propios cada uno, el dia que uno cambiara de orden los dos estarian
 *          mintiendo a la vez, y el unico que se entera es el navegador. La
 *          pagina escribe el espejo por INDICES (`WebUI/src/wasm/audioParams.js`),
 *          no por offset, asi que un orden distinto no da ningun error: escribe
 *          en el campo de al lado.
 *
 *          EL ORDEN, que es el ABI que documenta la cabecera del export y el que
 *          asume la pagina:
 *
 *              0..21   los escalares (masterLevel..lfo2Depth)
 *             22..33   la matriz: 4 rutas x (source, destination, amount)
 *             34..39   el hueco 1: params[0..3], gain, mix
 *             40..45   el hueco 2, igual
 *             ...      un bloque de `kFxBusParams + 2` por hueco
 *
 *          "params, gain, mix POR HUECO" es lo que dice el ABI y lo que asume la
 *          pagina. Lo que NO lo decia eran los bucles, que ponian todos los
 *          params de todos los huecos y despues todas las ganancias y mezclas.
 *          Con esa forma el campo 38, que la pagina cree que es la ganancia del
 *          hueco 1, era el primer parametro del hueco 2: los dos knobs de
 *          ganancia y mezcla del hueco 1 no llegaban a ningun sitio y en su
 *          lugar escribian en los mandos del hueco 2. Los params si coincidian,
 *          porque los del hueco 1 van los primeros en las dos formas; por eso el
 *          fallo estaba donde no se miraba.
 *
 *          Los `offsetof` salen de los MISMOS `kFxBusSlots`/`kFxBusParams` que
 *          usa el motor, y no de una lista de 24 numeros escrita a mano, que es
 *          la forma de que el layout y el struct se separen sin que nada lo note.
 */
inline std::vector<std::size_t> globalParamsLayout ()
{
    using GP = NEURONiK::DSP::GlobalParams;

    // Los escalares van en tabla porque son pocos y estan estables (no cambian);
    // la matriz y el bus se anaden con bucles, por lo de arriba.
    const std::size_t scalarOffsets[] = {
        offsetof (GP, masterLevel), offsetof (GP, saturationAmt), offsetof (GP, bpm),
        offsetof (GP, delayTime),   offsetof (GP, delayFB),
        offsetof (GP, chorusRate),  offsetof (GP, chorusDepth),   offsetof (GP, chorusMix),
        offsetof (GP, reverbSize),  offsetof (GP, reverbDamping),
        offsetof (GP, reverbWidth), offsetof (GP, reverbMix),
        offsetof (GP, lfo1.waveform), offsetof (GP, lfo1.rateHz),
        offsetof (GP, lfo1.syncMode), offsetof (GP, lfo1.rhythmicDivision),
        offsetof (GP, lfo1.depth),
        offsetof (GP, lfo2.waveform), offsetof (GP, lfo2.rateHz),
        offsetof (GP, lfo2.syncMode), offsetof (GP, lfo2.rhythmicDivision),
        offsetof (GP, lfo2.depth),
    };

    std::vector<std::size_t> layout (std::begin (scalarOffsets), std::end (scalarOffsets));

    for (int i = 0; i < 4; ++i)
    {
        layout.push_back (offsetof (GP, modMatrix[i].source));
        layout.push_back (offsetof (GP, modMatrix[i].destination));
        layout.push_back (offsetof (GP, modMatrix[i].amount));
    }

    // UN HUECO ENTERO: sus params, su ganancia y su mezcla, y luego el
    // siguiente. Es lo que hace que el bloque de un hueco sea CONTIGUO, que es
    // lo que la pagina necesita para localizar el hueco sin una tabla de indices.
    for (int slot = 0; slot < NEURONiK::DSP::kFxBusSlots; ++slot)
    {
        for (int p = 0; p < NEURONiK::DSP::kFxBusParams; ++p)
            layout.push_back (offsetof (GP, fx[slot].params[p]));

        layout.push_back (offsetof (GP, fx[slot].gain));
        layout.push_back (offsetof (GP, fx[slot].mix));
    }

    return layout;
}

/**
 * @brief Cuantos escalares hay DELANTE de la matriz, que es donde empieza el
 *        tramo que publica `neuronikModMatrixLayout`.
 *
 * @details Lo cuenta la propia lista, con los MISMOS `offsetof` que el
 *          constructor, en vez de un 22 escrito a mano. Un escalar nuevo anadido
 *          a una tabla y no a la otra dejaba el segundo export publicando desde
 *          el campo equivocado, y como se solapan, sin decir nada.
 */
constexpr std::size_t scalarFieldCount ()
{
    using GP = NEURONiK::DSP::GlobalParams;

    const std::size_t scalarOffsets[] = {
        offsetof (GP, masterLevel), offsetof (GP, saturationAmt), offsetof (GP, bpm),
        offsetof (GP, delayTime),   offsetof (GP, delayFB),
        offsetof (GP, chorusRate),  offsetof (GP, chorusDepth),   offsetof (GP, chorusMix),
        offsetof (GP, reverbSize),  offsetof (GP, reverbDamping),
        offsetof (GP, reverbWidth), offsetof (GP, reverbMix),
        offsetof (GP, lfo1.waveform), offsetof (GP, lfo1.rateHz),
        offsetof (GP, lfo1.syncMode), offsetof (GP, lfo1.rhythmicDivision),
        offsetof (GP, lfo1.depth),
        offsetof (GP, lfo2.waveform), offsetof (GP, lfo2.rateHz),
        offsetof (GP, lfo2.syncMode), offsetof (GP, lfo2.rhythmicDivision),
        offsetof (GP, lfo2.depth),
    };

    return sizeof (scalarOffsets) / sizeof (scalarOffsets[0]);
}

} // namespace


