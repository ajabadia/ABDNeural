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
#include <cstdint>
#include <vector>

#include "../DSP/DspTypes.h"

using GP = NEURONiK::DSP::GlobalParams;

// El layout se construye FUERA del `extern "C"` de abajo, y no por gusto: una
// funcion con enlace C no puede devolver `std::vector` (C2526), que es el tipo
// que hace falta para construir el layout entero de una vez.
namespace
{

/**
 * COMO SE ESCRIBE UN CAMPO DEL ESPEJO en JS. No es un detalle: el espejo es
 * un `ArrayBuffer` sobre el struct, asi que el mismo indice se escribe
 * con `Float32Array`, `Int32Array` o `Float64Array` segun lo que sea el
 * miembro de C++.
 *
 * POR QUE ESTA AQUI Y NO EN EL TRABAJADOR: son DOS hechos del mismo struct
 * (su orden y su tipo) y por tanto que vivan en dos sitios. La lista que
 * tenia el worklet (`INT_FIELDS` con doce indices, `BPM_FIELD` con uno) era
 * justo eso: si un campo nuevo del struct es `int` y nadie lo anade a la
 * lista, el espejo lo escribe como float y el motor lee el patron de bits de
 * IEEE. Sin error, sin rojo, sin sonido. Ahora la publica el puente, que es
 * el unico que sabe que es cada miembro.
 *
 * Viajan por el cable como `int` la matriz (source, destination) y las
 * tres position del LFO (waveform, syncMode, rhythmicDivision).
 */
enum class GlobalParamFieldKind
{
    Float32 = 0,   //!< float
    Int32   = 1,   //!< int
    Float64 = 2,   //!< double (hoy solo bpm)
};

/** UN CAMPO del layout: donde esta, y de que clase se escribe. */
struct GlobalParamField
{
    std::size_t offset;
    GlobalParamFieldKind kind;
};

/**
 * @brief LOS ESCALARES, con su clase, en el orden en que la pagina los
 *        escribe (indices 0..21).
 *
 * @details Vive a NIVEL DE NAMESPACE y no dentro del constructor, y esa
 *          es toda la gracia: `scalarFieldCount()` cuenta ESTA tabla (que es
 *          el corte del segundo export) y el constructor la empieza. Las dos
 *          mitades no pueden separarse porque no hay dos mitades: hay una.
 *
 *          `offsetof` es una expresion constante sobre un struct de datos
 *          planos, asi que la tabla es `constexpr` y el nucleo de la
 *          comprueba la ve al compilar, no al ejecutar.
 */
inline constexpr GlobalParamField kScalarFields[] = {
    { offsetof (GP, masterLevel), GlobalParamFieldKind::Float32 },
    { offsetof (GP, saturationAmt), GlobalParamFieldKind::Float32 },
    { offsetof (GP, bpm), GlobalParamFieldKind::Float64 },   // el unico double
    { offsetof (GP, delayTime), GlobalParamFieldKind::Float32 },
    { offsetof (GP, delayFB), GlobalParamFieldKind::Float32 },
    { offsetof (GP, chorusRate), GlobalParamFieldKind::Float32 },
    { offsetof (GP, chorusDepth), GlobalParamFieldKind::Float32 },
    { offsetof (GP, chorusMix), GlobalParamFieldKind::Float32 },
    { offsetof (GP, reverbSize), GlobalParamFieldKind::Float32 },
    { offsetof (GP, reverbDamping), GlobalParamFieldKind::Float32 },
    { offsetof (GP, reverbWidth), GlobalParamFieldKind::Float32 },
    { offsetof (GP, reverbMix), GlobalParamFieldKind::Float32 },
    { offsetof (GP, lfo1.waveform), GlobalParamFieldKind::Int32 },
    { offsetof (GP, lfo1.rateHz), GlobalParamFieldKind::Float32 },
    { offsetof (GP, lfo1.syncMode), GlobalParamFieldKind::Int32 },
    { offsetof (GP, lfo1.rhythmicDivision), GlobalParamFieldKind::Int32 },
    { offsetof (GP, lfo1.depth), GlobalParamFieldKind::Float32 },
    { offsetof (GP, lfo2.waveform), GlobalParamFieldKind::Int32 },
    { offsetof (GP, lfo2.rateHz), GlobalParamFieldKind::Float32 },
    { offsetof (GP, lfo2.syncMode), GlobalParamFieldKind::Int32 },
    { offsetof (GP, lfo2.rhythmicDivision), GlobalParamFieldKind::Int32 },
    { offsetof (GP, lfo2.depth), GlobalParamFieldKind::Float32 },
};

/**
 * @brief EL LAYOUT COMPLETO de GlobalParams, en el orden que publica la pagina.
 *
 * @details UNA SOLA COPIA DEL ORDEN Y DE LOS TIPOS, que es lo que hacen
 *          falta: la pagina escribe el espejo por INDICES
 *          (`WebUI/src/wasm/audioParams.js`), no por offset, asi que un orden
 *          distinto no da ningun error, escribe en el campo de al lado. Y el
 *          tipo tampoco: escribe en el sitio correcto con la vista equivocada.
 *
 *          Lo consumen los TRES exports de layout
 *          (`neuronikGlobalParamsLayout`, `neuronikModMatrixLayout` y
 *          `neuronikGlobalParamsFieldKinds`): con tablas propias cada uno, el
 *          dia que uno cambiara los dos estarian mintiendo a la vez, y el
 *          unico que se entera es el navegador.
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
inline std::vector<GlobalParamField> globalParamsLayout ()
{
    std::vector<GlobalParamField> layout (std::begin (kScalarFields),
                                           std::end (kScalarFields));

    for (int i = 0; i < 4; ++i)
    {
        layout.push_back ({ offsetof (GP, modMatrix[i].source),
                              GlobalParamFieldKind::Int32 });
        layout.push_back ({ offsetof (GP, modMatrix[i].destination),
                              GlobalParamFieldKind::Int32 });
        layout.push_back ({ offsetof (GP, modMatrix[i].amount),
                              GlobalParamFieldKind::Float32 });
    }

    // UN HUECO ENTERO: sus params, su ganancia y su mezcla, y luego el
    // siguiente. Es lo que hace que el bloque de un hueco sea CONTIGUO, que es
    // lo que la pagina necesita para localizar el hueco sin una tabla de indices.
    // Todo float: el bus no tiene ningun entero.
    for (int slot = 0; slot < NEURONiK::DSP::kFxBusSlots; ++slot)
    {
        for (int p = 0; p < NEURONiK::DSP::kFxBusParams; ++p)
            layout.push_back ({ offsetof (GP, fx[slot].params[p]),
                                  GlobalParamFieldKind::Float32 });

        layout.push_back ({ offsetof (GP, fx[slot].gain),
                              GlobalParamFieldKind::Float32 });
        layout.push_back ({ offsetof (GP, fx[slot].mix),
                              GlobalParamFieldKind::Float32 });
    }

    return layout;
}

/**
 * @brief Cuantos escalares hay DELANTE de la matriz, que es donde empieza el
 *        tramo que publica `neuronikModMatrixLayout`.
 *
 * @details Lo cuenta la propia tabla, y no un 22 escrito a mano: un escalar
 *          nuevo anadido a la tabla y el corte se iba solo, con el segundo
 *          export publicando desde el campo equivocado y, como se solapan, sin
 *          decir nada.
 */constexpr std::size_t scalarFieldCount ()
{
    return sizeof (kScalarFields) / sizeof (kScalarFields[0]);
}

/**
 * @brief Una firma del layout ENTERO, para que la pagina sepa si el
 *        `.wasm` que tiene delante es de esta tabla.
 *
 * @details POR QUE HACE FALTA, y por que no basta con la cuenta de campos.
 *
 *          Contar campos detecta el `.wasm` que publica MENOS de los que la
 *          pagina escribe, que era el fallo de los seis mandos del hueco 1.
 *          No detecta el otro: un `.wasm` que publica 58 campos igual de
 *          muchos, pero con el orden o la clase cambiados. Ahi la pagina
 *          escribe en el hueco equivocado y todo parece funcionar, porque
 *          los dos lados hablan de 58. La cuenta no lo ve; esta firma si,
 *          porque mezcla los TRES hechos del layout --cuantos campos son,
 *          donde cae cada uno y de que clase se escribe-- y cualquier
 *          cambio en cualquiera de los tres la mueve.
 *
 *          Es un FNV-1a de 32 bits. No es criptografia: es una firma para
 *          que una pagina y un binario se reconozcan, y para eso 32 bits
 *          sobran. El fallo que se perdona es el de una colision fortuita,
 *          que daria un falso aviso; el que no se perdona es el contrario,
 *          dejar pasar un `.wasm` viejo sin que nadie se entere, que es
 *          el que costo el hueco entero.
 *
 *          Sale de la TABLA y no de un numero escrito al lado: cambiar el
 *          layout cambia la firma porque cambia la tabla de la que sale.
 *          Por eso la pagina puede llevar la suya en un fichero generado
 *          sin estar comparando contra una copia del struct.
 */
inline std::uint32_t layoutFingerprintOf (const std::vector<GlobalParamField>& layout)
{
    constexpr std::uint32_t fnvOffsetBasis = 2166136261u;
    constexpr std::uint32_t fnvPrime      = 16777619u;

    // Mezclar UN byte por vuelta, en little endian, que es como el struct
    // esta en memoria. Asi la firma no depende de como se imprima el
    // entero en la pagina: los dos lados mezclan los mismos bytes.
    const auto mix = [&fnvPrime] (std::uint32_t& hash, std::uint32_t value)
    {
        for (int shift = 0; shift < 32; shift += 8)
            hash = (hash ^ ((value >> shift) & 0xFFu)) * fnvPrime;
    };

    std::uint32_t hash = fnvOffsetBasis;

    // La cuenta va la primera y por su cuenta, para que un layout de 58
    // campos y otro de 58 con los campos desplazados uno no se confundan
    // por un accidente de la suma.
    mix (hash, static_cast<std::uint32_t> (layout.size ()));

    for (const auto& field : layout)
    {
        mix (hash, static_cast<std::uint32_t> (field.offset));
        mix (hash, static_cast<std::uint32_t> (field.kind));
    }

    return hash;
}

/**
 * La firma DE ESTA TABLA, que es la que se publica.
 *
 * Delgada a proposito: toda la cuenta vive en `layoutFingerprintOf`, que
 * recibe el layout como parametro. Asi `Tests/WasmLayoutOrderTest.cpp`
 * puede darle un layout perturbado --un offset movido, una clase
 * cambiada, un campo de mas-- y comprobar que la firma se mueve, sin
 * tener que tocar el struct de verdad para ver lo mismo. Un test que
 * solo puede passarse con el valor bueno no prueba que la firma dependa
 * de lo que dice depender.
 */
inline std::uint32_t globalParamsLayoutFingerprint ()
{
    return layoutFingerprintOf (globalParamsLayout ());
}

} // namespace


