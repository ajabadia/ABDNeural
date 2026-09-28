/*
  ==============================================================================

    ModDestinationTable.h

    LA TABLA DE DESTINOS DE LA MATRIZ DE MODULACION, como constexpr.

    Vive en su propia cabecera y no dentro de ParameterDefinitions.h a proposito:
    esta no tiene NINGUN include, y ParameterDefinitions.h arrastra
    juce_audio_processors entero. El motor (NeuronikEngine.cpp) la necesita para
    sus static_assert, y cinco targets compilan ese .cpp —tres de ellos
    (ModulationMatrixTest, ModulationParityDump, PresetMigrationParity) NO
    enlazan juce_audio_processors. Que el motor incluyera ParameterDefinitions.h
    los obligaria a hacerlo, y un target de test que deja de enlazar no es un
    detalle de CMake: es un exe que aparece y desaparece segun el include de un
    fichero que no tiene nada que ver con el.

    Los ids van como TEXTO, no como `IDs::`. `IDs::` vive en
    ParameterDefinitions.h, que es justo lo que esta cabecera no puede incluir.
    El puente es `checkModDestinationIds (IDs::...)` al final de
    ParameterDefinitions.h: alla, donde vive la verdad de los ids, se comprueba
    que cada `parameterId` de esta tabla es exactamente el `IDs::` que le
    corresponde. Un id renombrado rompe la compilacion en el sitio del rename,
    no en el sitio del destino.

    ── SOLO label Y parameterId, NUNCA UN PUNTERO-MIEMBRO ───────────────────
    Es la misma trampa de NeuronikEngine.cpp, vista desde el otro lado. MSVC no
    constant-inicializa un array constexpr cuyos elementos salen de funciones
    fabrica constexpr que devuelven un agregado con punteros-miembro de una
    clase POLIMORFICA (IVoice tiene funciones virtuales): los punteros-miembro
    quedan TODOS a cero, sin un solo aviso, y el motor se come las rutas en
    silencio.

    Por eso aqui solo hay `const char*`. Un array de punteros a caracter es
    justamente la forma que MSVC si constant-inicializa, asi que la tabla de
    nombres es estable y se puede atar en un static_assert. Si algun dia esta
    cabecera quisiera meter un puntero-miembro, dejaria de ser una tabla de
    nombres y pasaria a ser la segunda mitad del problema.

  ==============================================================================
*/

#pragma once

#include <cstddef>

namespace NEURONiK::State
{

/**
 * @brief One modulation destination: the label a mod slot stores, and the
 *        parameter that destination drives (nullptr for "Off").
 *
 * @details THIS ORDER IS PRESET STATE. A mod destination is stored as its INDEX
 *          (`mod1Destination` = 20 is what an old preset means by "Odd/Even
 *          Bal"), so entries may be APPENDED but never reordered or removed.
 *          That is why the labels and the ids live in the same table instead of
 *          being matched by hand in two files.
 *
 *          Which ENGINE can use a destination is deliberately NOT here: it is
 *          derived from `engineCoverageFor (parameterId)` in
 *          ParameterDescriptors.h, so a destination can never claim a different
 *          engine than the parameter it actually drives.
 */
struct ModDestination
{
    const char* label;
    const char* parameterId;   //!< nullptr for "Off" (drives nothing)
};

/** @brief Modulation destinations in preset-index order (see ModDestination). */
inline constexpr ModDestination kModDestinationTable[] =
{
    { "Off",            nullptr },
    { "Osc Level",      "oscLevel" },
    { "Inharmonicity",  "oscInharmonicity" },
    { "Roughness",      "oscRoughness" },
    { "Morph X",        "morphX" },
    { "Morph Y",        "morphY" },
    { "Amp Attack",     "envAttack" },
    { "Amp Decay",      "envDecay" },
    { "Amp Sustain",    "envSustain" },
    { "Amp Release",    "envRelease" },
    { "Filter Cutoff",  "filterCutoff" },
    { "Filter Res",     "filterRes" },
    // Index 12: "Filter Env Amt" — the parameter was retired (2026-09-26;
    // the matrix amount IS the depth) but the destination LABEL stays: the
    // indices are the preset format. The engine adds it to the 1.0 routing
    // factor of ENV 2 (AdditiveVoice::modEnvFltDepth), so the label keeps
    // its meaning: more/less/inverted envelope through the route.
    { "Filter Env Amt", nullptr },
    { "Flt Attack",     "filterAttack" },
    { "Flt Decay",      "filterDecay" },
    { "Flt Sustain",    "filterSustain" },
    { "Flt Release",    "filterRelease" },
    { "Saturation",     "fxSaturation" },
    { "Delay Time",     "fxDelayTime" },
    { "Delay FB",       "fxDelayFeedback" },
    { "Odd/Even Bal",   "resonatorParity" },
    { "Spectral Shift", "resonatorShift" },
    { "Harm Roll-off",  "resonatorRolloff" },
    { "Excite Noise",   "oscExciteNoise" },
    { "Excite Color",   "excitationColor" },
    { "Impulse Mix",    "impulseMix" },
    { "Res Bank Res",   "resonatorRes" },
    { "Unison Detune",  "unisonDetune" },
    // APPEND siempre: los choice de la matriz guardan INDICE de preset
    // (insertar en medio re-mapearia presets guardados).
    { "Morph Z",        "morphZ" },
    { "Morph Z 2",      "morphZ2" },
    { "Morph Z 3",      "morphZ3" },
};

inline constexpr std::size_t kModDestinationCount =
    sizeof (kModDestinationTable) / sizeof (kModDestinationTable[0]);

/** @brief Compara dos textos en tiempo de compilacion, sin `strcmp`.

    Corta en cuanto los dos textos se acaban a la vez, asi que dos textos de
    distinta longitud NO cuadran: si uno se acaba antes que el otro, ese caracter
    es `'\\0'` en uno y no en el otro, y la comparacion falla ahi. */
constexpr bool modDestinationTextIs (const char* actual, const char* expected,
                                     std::size_t character = 0)
{
    return actual[character] == expected[character]
        && (actual[character] == '\0' || modDestinationTextIs (actual, expected, character + 1));
}

/** @brief ¿La fila `index` de la tabla lleva la etiqueta `expected`?

    Esto es lo que ata las DOS tablas. Los static_assert del motor comprueban
    que la fila N de `kModDestinations` es la que lleva el nombre que la tabla
    de parametros le pone a esa misma fila N. Si alguien intercambia dos filas de
    `kModDestinationTable`, el nombre deja de cuadrar y el build se rompe en el
    sitio donde esta la REGLA, que es donde se puede ver gratis.

    El `parameterId` no se comprueba aqui sino en `checkModDestinationIds`, al
    lado de `IDs::`, que es donde vive su verdad. */
constexpr bool modDestinationLabelIs (std::size_t index, const char* expected)
{
    return index < kModDestinationCount
        && modDestinationTextIs (kModDestinationTable[index].label, expected);
}

} // namespace NEURONiK::State
