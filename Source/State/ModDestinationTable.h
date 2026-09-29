/*
  ==============================================================================

    ModDestinationTable.h

    LA TABLA DE DESTINOS DE LA MATRIZ DE MODULACION, como constexpr.

    Vive en su propia cabecera y no dentro de ParameterDefinitions.h a proposito:
    esta no tiene NINGUN include de juce, y ParameterDefinitions.h arrastra
    juce_audio_processors entero. El motor (NeuronikEngine.cpp) la necesita para
    sus static_assert, y cinco targets compilan ese .cpp --tres de ellos
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

    -- EL TIPO DE LA TABLA ES EL COMPARTIDO, NO UNO PROPIO ------------------
    Las filas son `abd::synth::ModDestinationDescriptor`, el MISMO descriptor que
    consumen ABDEep y ABDMS2000 (`ABDSharedCode/SynthCore/ModMatrix.h`). Antes
    de esto ABDNeural tenia aqui un struct propio con los mismos cuatro campos
    (label, parameterId, perNote, replaces): la forma, duplicada, con un nombre
    distinto. Que la tabla sea la fuente unica de la UI no se cumple si el
    contrato que se publica sale de un struct que el motor no mira.

    Este es el include que ModMatrix.h anade, y el unico motivo de que esta
    cabecera deje de depender de cero cabeceras: `include_directories
    (ABDSHARED_CODE_DIR)` es de nivel de directorio en CMakeLists.txt, asi que
    los cinco targets que compilan NeuronikEngine.cpp lo tienen en el camino sin
    tocar su target_include_directories.

    -- POR QUE AQUI NO HAY NINGUN PUNTERO-MIEMBRO (y sigue sin haberlo) ------
    Es la misma trampa de NeuronikEngine.cpp, vista desde el otro lado. MSVC no
    constant-inicializa un array constexpr cuyos elementos salen de funciones
    fabrica constexpr que devuelven un agregado con punteros-miembro de una
    clase POLIMORFICA (IVoice tiene funciones virtuales): los punteros-miembro
    quedan TODOS a cero, sin un solo aviso, y el motor se come las rutas en
    silencio.

    Por eso la tabla se construye con una fabrica (que si es legal en constexpr
    y pone el `id` de la fila sin escribirlo a mano) y esa fabrica NO puede
    llevar punteros-miembro: el descriptor compartido no los tiene, y por eso
    este consume el compartido sin volver a la mitad del problema. El
    puntero-miembro sigue donde puede vivir, en la tabla del motor, con
    inicializacion DIRECTA del agregado y sus static_assert.

  ==============================================================================
*/

#pragma once

#include <cstddef>

// El descriptor compartido. Cero dependencias (solo <cstddef> y <cstdint>).
#include "SynthCore/ModMatrix.h"

namespace NEURONiK::State
{

/**
 * @brief One modulation destination: la fila de ABDNeural del descriptor
 *        COMPARTIDO, sin un tipo propio que lo replique.
 *
 * @details THIS ORDER IS PRESET STATE. A mod destination is stored as its INDEX
 *          (`mod1Destination` = 20 is what an old preset means by "Odd/Even
 *          Bal"), so entries may be APPENDED but never reordered or removed.
 *          That is why the labels and the ids live in the same table instead of
 *          being matched by hand in two files.
 *
 *          De los nueve campos del descriptor compartido, esta tabla escribe
 *          seis --`id`, `label`, `parameterId`, `perNote`, `replaces` y
 *          `scale`-- y deja los tres restantes en su valor por defecto:
 *
 *            - `min`/`max` se quedan en 0..1. Son "el rango real del parametro
 *              destino" y ABDNeural no lo publica en ningun sitio todavia: lo
 *              que la UI normaliza contra ellos son los anillos de modulacion,
 *              y eso no existe. Poner 20..20000 en el cutoff seria inventar un
 *              dato, y un descriptor compartido que publica un rango inventado
 *              es peor que uno que no publica rango. Cuando se publiquen, se
 *              anaden a la fabrica y se avisa aqui.
 *            - `engineMask` se queda en "todos los motores" A PROPOSITO, y no
 *              por descuido: el comentario de mas abajo dice que la cobertura
 *              de motores NO vive en esta tabla (se deriva de
 *              `engineCoverageFor (parameterId)`), porque un destino no puede
 *              declarar un motor distinto del que declara el parametro que
 *              conduce. Escribir aqui una mascara la convertiria en una segunda
 *              verdad que se puede desincronizar de la primera sin que nada lo
 *              note, que es justo lo que se hizo con `label` y `parameterId`
 *              antes de que esta tabla los juntara.
 *
 *          `scale` es lo que el motor multiplica por el amount: 1.0 para todo
 *          lo normalizado, y 18000.0 para el cutoff, que se suma en hercios.
 *          Es el unico numero de la tabla que el motor lee para escalar, y por
 *          eso vive aqui y no en la tabla de reglas del motor, que es la parte
 *          que si lleva punteros-miembro.
 */
using ModDestination = abd::synth::ModDestinationDescriptor;

using abd::synth::ModDestinationId;

/**
 * @brief Fabrica constexpr de una fila, con el `id` puesto desde su indice.
 *
 * @details POR QUE UNA FABRICA Y NO INICIALIZACION DIRECTA DEL AGREGADO: para
 *          que MSVC constant-inicialice el array, el tipo tiene que ser
 *          built-in en todas sus filas (ver el aviso de la cabecera). La
 *          fabrica se permite con este descriptor porque no tiene ni un
 *          puntero-miembro: si algun dia se le anadiera uno, esta tabla volveria
 *          a valer cero en silencio y habria que deshacerla entera.
 *
 *          El `Index` como parametro de plantilla es lo que evita escribir el
 *          indice DOS veces --una en la posicion del array y otra en el `id`--,
 *          que es la forma de que los dos se separen sin que nada lo note. El
 *          aserto de mas abajo ata el uno con el otro.
 */
template <ModDestinationId Index>
constexpr ModDestination makeModDestination (const char* label,
                                             const char* parameterId,
                                             bool perNote   = false,
                                             bool replaces  = false,
                                             float scale    = 1.0f)
{
    // min y max se dejan a 0..1 (ver el comentario de `ModDestination`).
    return { Index, label, parameterId, 0.0f, 1.0f, scale, perNote, replaces };
}

/** @brief Modulation destinations in preset-index order (see ModDestination). */
inline constexpr ModDestination kModDestinationTable[] =
{
    makeModDestination< 0> ("Off",            nullptr),
    makeModDestination< 1> ("Osc Level",      "oscLevel",         true, true),
    makeModDestination< 2> ("Inharmonicity",  "oscInharmonicity"),
    makeModDestination< 3> ("Roughness",      "oscRoughness"),
    makeModDestination< 4> ("Morph X",        "morphX"),
    makeModDestination< 5> ("Morph Y",        "morphY"),
    makeModDestination< 6> ("Amp Attack",     "envAttack"),
    makeModDestination< 7> ("Amp Decay",      "envDecay"),
    makeModDestination< 8> ("Amp Sustain",    "envSustain"),
    makeModDestination< 9> ("Amp Release",    "envRelease"),
    makeModDestination<10> ("Filter Cutoff",  "filterCutoff",     true, true, 18000.0f),
    makeModDestination<11> ("Filter Res",     "filterRes"),
    // Index 12: "Filter Env Amt" -- the parameter was retired (2026-09-26;
    // the matrix amount IS the depth) but the destination LABEL stays: the
    // indices are the preset format. The engine adds it to the 1.0 routing
    // factor of ENV 2 (AdditiveVoice::modEnvFltDepth), so the label keeps
    // its meaning: more/less/inverted envelope through the route.
    //
    // LOS CINCO (12..16) NO SON `replaces`, Y ESO SE DECIDIO EL 2026-09-29.
    // Antes las cinco filas decian `replaces` y el motor las SUMABA, que es
    // la divergencia que dos asertos del motor nombraban fila a fila. Se
    // decidio que la verdad es la del motor, y no por tastes:
    //
    //   - `IVoice.h` los declara como ACUMULADORES A CERO y los documenta
    //     como "aditivo"; el sustain lleva "clamp 0..1 en la voz", que solo
    //     tiene sentido si se suma a un factor con neutro.
    //   - `resetModulations()` los pone a cero ANTES de cada aplicacion, asi
    //     que sumar y asignar dan EL MISMO NUMERO, siempre. Medido: los 41
    //     hashes de `ModulationParityDump` no se mueven ni un ULP al pasar las
    //     cinco filas a `envAssign`. Es un cambio de etiqueta, no de sonido,
    //     y por eso no habia nada que decidir por el oido.
    //   - el neutro de 1.0, que es lo que hace falta para que "reemplazar" sea
    //     distinto de "sumar", solo lo tienen los destinos 1 y 10 (los dos
    //     unicos que de verdad REEMPLAZAN, y los que lo necesitan: alli la
    //     envolvente ES la senal, no una profundidad).
    //
    // Lo que NO se pierde: siguen siendo `perNote`, que es otra cosa. Se
    // resuelven POR VOZ (una envolvente no es global), y eso no cambia.
    makeModDestination<12> ("Filter Env Amt", nullptr,          true),
    makeModDestination<13> ("Flt Attack",     "filterAttack",    true),
    makeModDestination<14> ("Flt Decay",      "filterDecay",     true),
    makeModDestination<15> ("Flt Sustain",    "filterSustain",   true),
    makeModDestination<16> ("Flt Release",    "filterRelease",   true),
    makeModDestination<17> ("Saturation",     "fx1Param1"),   // el drive del hueco 1 (2026-09-29)
    makeModDestination<18> ("Delay Time",     "fxDelayTime"),
    makeModDestination<19> ("Delay FB",       "fxDelayFeedback"),
    makeModDestination<20> ("Odd/Even Bal",   "resonatorParity"),
    makeModDestination<21> ("Spectral Shift", "resonatorShift"),
    makeModDestination<22> ("Harm Roll-off",  "resonatorRolloff"),
    makeModDestination<23> ("Excite Noise",   "oscExciteNoise"),
    makeModDestination<24> ("Excite Color",   "excitationColor"),
    makeModDestination<25> ("Impulse Mix",    "impulseMix"),
    makeModDestination<26> ("Res Bank Res",   "resonatorRes"),
    makeModDestination<27> ("Unison Detune",  "unisonDetune"),
    // APPEND siempre: los choice de la matriz guardan INDICE de preset
    // (insertar en medio re-mapearia presets guardados).
    makeModDestination<28> ("Morph Z",        "morphZ"),
    makeModDestination<29> ("Morph Z 2",      "morphZ2"),
    makeModDestination<30> ("Morph Z 3",      "morphZ3"),
};

inline constexpr std::size_t kModDestinationCount =
    sizeof (kModDestinationTable) / sizeof (kModDestinationTable[0]);

/** @brief Compara dos textos en tiempo de compilacion, sin `strcmp`.

    Corta en cuanto los dos textos se acaban a la vez, asi que dos textos de
    distinta longitud NO cuadran: si uno se acaba antes que el otro, ese caracter
    es `'\0'` en uno y no en el otro, y la comparacion falla ahi. */
constexpr bool modDestinationTextIs (const char* actual, const char* expected,
                                     std::size_t character = 0)
{
    return actual[character] == expected[character]
        && (actual[character] == '\0' || modDestinationTextIs (actual, expected, character + 1));
}

/** @brief ¿La fila `index` de la tabla lleva la etiqueta `expected`?

    Esto es lo que ata las DOS tablas del motor. Los static_assert de
    NeuronikEngine.cpp comprueban que la fila N de `kModDestinations` es la que
    lleva el nombre que la tabla de parametros le pone a esa misma fila N. Si
    alguien intercambia dos filas ahi, el nombre deja de cuadrar y el build se
    rompe en el sitio donde esta la REGLA, que es donde se puede ver gratis.

    El `parameterId` no se comprueba aqui sino en `checkModDestinationIds`, al
    lado de `IDs::`, que es donde vive su verdad. */
constexpr bool modDestinationLabelIs (std::size_t index, const char* expected)
{
    return index < kModDestinationCount
        && modDestinationTextIs (kModDestinationTable[index].label, expected);
}

/** @brief ¿El `id` de la fila `index` es su propia posicion?

    La matriz compartida direcciona por id, no por posicion, asi que un `id`
    mentiroso no rompe el motor (el motor indexa el array) pero haria que
    cualquier superficie que hable el idioma de ModMatrix.h --ABDEep, y el
    contrato-- apuntase a la fila equivocada. Sin esto, escribir
    `makeModDestination<17>` en la fila 16 pasaria desapercibido. */
constexpr bool modDestinationIdIs (std::size_t index)
{
    return index >= kModDestinationCount
        || kModDestinationTable[index].id == static_cast<ModDestinationId> (index);
}

constexpr bool everyModDestinationIdIsItsIndex (std::size_t index = 0)
{
    return index >= kModDestinationCount
        ? true
        : modDestinationIdIs (index) && everyModDestinationIdIsItsIndex (index + 1);
}

static_assert (everyModDestinationIdIsItsIndex(),
               "el id de cada fila es su indice: la matriz compartida direcciona por id");

} // namespace NEURONiK::State
