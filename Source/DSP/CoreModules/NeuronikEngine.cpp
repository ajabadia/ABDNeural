/*
  ==============================================================================

    NeuronikEngine.cpp
    Created: 30 Jan 2026
    Description: Implementation of the central synthesis engine.

  ==============================================================================
*/

#include "DspCore.h"
#include "NeuronikEngine.h"

// Los NOMBRES de la tabla de destinos, en su forma constexpr. Es el unico
// include que hace falta para atar las dos tablas, y entra aqui precisamente
// porque ModDestinationTable.h NO arrastra juce: cinco targets compilan este
// fichero (NEURONiK, WebPilotHost, WasmParityTest, ModulationMatrixTest,
// ModulationParityDump, PresetMigrationParity) y varios no enlazan
// juce_audio_processors, que es lo que traeria ParameterDefinitions.h.
#include "State/ModDestinationTable.h"

// El NUCLEO de la matriz compartida. ModDestinationTable.h ya lo incluye (su
// tabla SON filas de este descriptor), pero el motor usa de el una cosa
// propia: el id inerte, que es kNoModSource/kNoModDestination y no un 0
// escrito a mano en el hilo de audio.
#include "SynthCore/ModMatrix.h"

// Los simbolos de la tabla de destinos, para que los static_assert de mas abajo
// los nombren sin repetir el namespace en cada linea.
using NEURONiK::State::kModDestinationCount;
using NEURONiK::State::kModDestinationTable;
using NEURONiK::State::modDestinationLabelIs;
using abd::synth::kNoModDestination;
using abd::synth::kNoModSource;
#include "../Synthesis/AdditiveVoice.h"
#include "../DspSafety.h"

namespace NEURONiK::DSP {

NeuronikEngine::NeuronikEngine()
{
    // Reserva PEREZOSA: nace con las voces de su limite (16 por defecto), no 32.
    // `ensureVoices` crece cuando `setPolyphony` sube el techo.
    ensureVoices (activeVoiceLimit.load());
}

std::unique_ptr<IVoice> NeuronikEngine::createVoice(int)
{
    return std::make_unique<Synthesis::AdditiveVoice>();
}

void NeuronikEngine::prepare(double sampleRate, int samplesPerBlock)
{
    BaseEngine::prepare(sampleRate, samplesPerBlock);
}

void NeuronikEngine::renderNextBlock(dsp::AudioBuffer<float>& buffer, dsp::MidiBuffer& midiMessages)
{
    // 1. Parametros del bloque (config de LFO, FX y params de voz)
    updateParameters();

    // 2. Process MIDI events
    processMidiBuffer(midiMessages);

    // 3. Voces + modulacion, en tramos de tasa de control fija: el LFO se lee y la
    //    matriz se aplica cada kControlBlockSize muestras, no cada bloque del host.
    renderVoicesWithControlRate(buffer);

    // 4. Global FX
    applyGlobalFX(buffer);
}

// ============================================================================
//  LA TABLA DE DESTINOS: lo que el switch de 31 casos contaba, en datos
// ============================================================================
//  Un destino de la matriz es una de TRES cosas, y la tabla lo dice entero en
//  vez de repartirlo entre 31 ramas de codigo:
//
//    voiceAdd  -> SUMA `rawMod * scale` a un miembro de la voz.
//    globalFxAdd -> lo mismo, pero a un MANDO DEL BUS de un hueco, direccionado
//                   por (hueco, indice). Existe desde que el hueco 1 tiene bus
//                   propio (2026-09-29) y por lo que se ha explicado arriba: el
//                   bus es un array y C++ no tiene puntero a miembro de un array.
//    globalAdd -> SUMA `rawMod * scale` a un parametro global (el bus de FX, no
//                 la voz).
//    envAssign -> si la fuente es la envolvente que pide el destino, ASIGNA el
//                 amount a un factor de routing. envAdd lo SUMA. Los dos son la
//                 "sintesis de reemplazo": la envolvente entra con ese factor,
//                 no encima del valor de la fuente.
//
//  Cada destino tiene DOS ranuras porque hay destinos con las dos ramas: el 1
//  (Osc Level) y el 10 (Filter Cutoff) suman con un LFO y REEMPLAZAN cuando la
//  fuente es su envolvente. La ranura `env` se consulta antes que la `add`, que
//  es exactamente la precedencia que tenia el switch: dentro de cada case, la
//  pregunta por la fuente iba antes que la suma.
//
//  El indice de la tabla ES el indice de preset del destino. Anadir un destino
//  es anadir una fila al FINAL y ampliar kNumModDestinations: insertar en medio
//  re-mapearia los presets ya guardados.
//
//  ── POR QUE CADA FILA ESCRIBE LOS CINCO CAMPOS A MANO, SIN FABRICAS ─────────
//  La primera version de esta tabla se construia con las fabricas `voiceAdd()`,
//  `globalAdd()` y `envRule()`, que son constexpr. Compilaba sin un solo aviso
//  y en ejecucion LOS PUNTEROS-MIEMBRO DE LA TABLA VALIAN TODOS CERO: el motor
//  se comia las rutas y de la UI no se/caia nadie, solo un hash de audio
//  distinto. MSVC no constant-initializa un array constexpr cuyos elementos
//  salen de una funcion constexpr que devuelve un agregado con punteros-miembro
//  de una clase POLIMORFICA (IVoice tiene funciones virtuales): los deja a
//  cero. Con inicializacion DIRECTA del agregado --lo que hay aqui-- si
//  funciona, y los static_assert de abajo lo comprueban en cada compilacion.
//
//  O sea que lo de no poner fabricas no es estilo: es que aqui mienten, y lo
//  unico que las distingue de la version rota es que la rota no decia nada.
namespace
{

/** Una regla de destino. Los campos se escriben SIEMPRE, punteros incluidos: un
    puntero-miembro sin inicializar es justamente el valor que hace que la tabla
    valga cero, asi que "dejarlo a su cuenta" no es una opcion. Los dos `int` del
    bus (que solo usa `globalFxAdd`) van a -1, que es "no aplica", y en la fila
    que si los usa van a su hueco y su mando. */
struct ModRule
{
    enum class Kind
    {
        none,        ///< ranura vacia
        voiceAdd,    ///< suma al miembro de una voz
        globalAdd,   ///< suma a un parametro global (FX)
        globalFxAdd, ///< suma al mando del bus que dice `ModDestinationDescriptor::busParam`
        envAssign,   ///< ASIGNA el amount a un factor de routing, si la fuente es la envolvente
        envAdd       ///< SUMA el amount a un factor de routing, si la fuente es la envolvente
    };

    Kind kind = Kind::none;
    int envSource = -1;                            ///< envAssign/envAdd: 6 = ENV 1, 7 = ENV 2
    float IVoice::* voice = nullptr;               ///< voiceAdd / envAssign / envAdd
    float GlobalParams::* global = nullptr;        ///< globalAdd

    // NO hay `scale` aqui, y su ausencia es el punto: la escala del destino
    // (1.0 para lo normalizado, 18000 Hz para el cutoff) es POLITICA, y la
    // politica vive en el descriptor COMPARTIDO, en kModDestinationTable.
    // La fila 10 la llevaba duplicada --una copia que el motor aplicaba y otra
    // que la UI leia--, y dos copias de un numero que se ven distintas es
    // exactamente como se rompe un destino sin que nadie lo note.
    //
    // Este struct se queda con lo que el descriptor compartido no puede
    // llevar: los PUNTEROS-MIEMBRO, que son lo que MSVC no constant-inicializa
    // (ver el aviso de mas arriba) y lo que un descriptor de la suite
    // compartida no puede tener, porque los otros dos consumidores no tienen
    // un IVoice al que apuntar.
};

struct ModDestinationDescriptor
{
    ModRule env;   ///< la rama que PREGUNTA por la fuente (se consulta antes)
    ModRule add;   ///< la rama aditiva, para cualquier otra fuente

    /**
     * Cual MANDO DEL BUS manda `add` cuando es `globalFxAdd`, codificado como
     * `hueco * kFxBusParams + mando`. -1 = no aplica.
     *
     * POR QUE ESTE CAMPO ESTA EN EL DESCRIPTOR Y NO EN LA REGLA, y es una
     * trampa que ya ha mordido una vez. La tabla se inicializa
     * POSICIONALMENTE y con ELISION DE LLAVES: al empezar una fila, el
     * compilador reparte inicializadores entre `env` y `add` y rellena cada
     * sub-agregado HASTA LLENARLO antes de pasar al siguiente. Anadir un
     * miembro a `ModRule` hacia que `env` se comiera uno mas de los que le
     * tocaban, y las cincuenta filas que ya estaban
     * escritas se empezaban a leer como otra cosa sin que nadie lo viera. Aqui no: los
     * dos `ModRule` se llenan igual que antes y este campo, que va detras, se
     * queda con su -1 en todas las filas que no lo usan.
     */
    int busParam = -1;
};

using Kind = ModRule::Kind;

constexpr int kNumModDestinations = 31;   // = getModDestinationTable().size()

constexpr ModDestinationDescriptor kModDestinations[kNumModDestinations] =
{
    { {}, {} },   //  0  Off  --  inerte A PROPOSITO: la ausencia de destino, no un destino que aplica un 0
    { Kind::envAssign, 6, &IVoice::modEnvLevel, nullptr, Kind::voiceAdd, -1, &IVoice::modLevel, nullptr },   //  1  Osc Level  --  ENV 1 REEMPLAZA el factor de routing; otra fuente suma sobre el nivel
    { {}, Kind::voiceAdd, -1, &IVoice::modInharmonicity, nullptr },   //  2  Inharmonicity
    { {}, Kind::voiceAdd, -1, &IVoice::modRoughness, nullptr },   //  3  Roughness
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphX, nullptr },   //  4  Morph X
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphY, nullptr },   //  5  Morph Y
    { {}, Kind::voiceAdd, -1, &IVoice::modAmpAttack, nullptr },   //  6  Amp Attack
    { {}, Kind::voiceAdd, -1, &IVoice::modAmpDecay, nullptr },   //  7  Amp Decay
    { {}, Kind::voiceAdd, -1, &IVoice::modAmpSustain, nullptr },   //  8  Amp Sustain
    { {}, Kind::voiceAdd, -1, &IVoice::modAmpRelease, nullptr },   //  9  Amp Release
    { Kind::envAssign, 7, &IVoice::modEnvCutoff, nullptr, Kind::voiceAdd, -1, &IVoice::modCutoff, nullptr },   // 10  Filter Cutoff  --  la voz SUMA este valor al cutoff en hercios; ENV 2 REEMPLAZA el factor
    { {}, Kind::voiceAdd, -1, &IVoice::modFilterRes, nullptr },   // 11  Filter Res
    { Kind::envAdd, 7, &IVoice::modEnvFltDepth, nullptr, {} },   // 12  Filter Env Amt  --  profundidad del knob filterEnvAmount (retirado 2026-09-26); solo ENV 2
    { Kind::envAdd, 7, &IVoice::modEnvFltAttack, nullptr, {} },   // 13  Flt Attack  --  ADSR del filtro por ENV 2 (aditiva)
    { Kind::envAdd, 7, &IVoice::modEnvFltDecay, nullptr, {} },   // 14  Flt Decay  --  aditiva
    { Kind::envAdd, 7, &IVoice::modEnvFltSustain, nullptr, {} },   // 15  Flt Sustain  --  aditiva
    { Kind::envAdd, 7, &IVoice::modEnvFltRelease, nullptr, {} },   // 16  Flt Release  --  aditiva
    { {}, Kind::globalFxAdd, -1, nullptr, nullptr, 0 * kFxBusParams + 0 },   // 17  Saturation  --  el DRIVE del hueco 1 (2026-09-29)
    { {}, Kind::globalAdd, -1, nullptr, &GlobalParams::delayTime },   // 18  Delay Time  --  FX del bus
    { {}, Kind::globalAdd, -1, nullptr, &GlobalParams::delayFB },   // 19  Delay FB  --  FX del bus
    { {}, Kind::voiceAdd, -1, &IVoice::modParity, nullptr },   // 20  Odd/Even Bal
    { {}, Kind::voiceAdd, -1, &IVoice::modShift, nullptr },   // 21  Spectral Shift
    { {}, Kind::voiceAdd, -1, &IVoice::modRolloff, nullptr },   // 22  Harm Roll-off
    { {}, Kind::voiceAdd, -1, &IVoice::modExciteNoise, nullptr },   // 23  Excite Noise
    { {}, Kind::voiceAdd, -1, &IVoice::modExciteColor, nullptr },   // 24  Excite Color
    { {}, Kind::voiceAdd, -1, &IVoice::modImpulseMix, nullptr },   // 25  Impulse Mix
    { {}, Kind::voiceAdd, -1, &IVoice::modResonance, nullptr },   // 26  Res Bank Res
    { {}, Kind::voiceAdd, -1, &IVoice::modUnison, nullptr },   // 27  Unison Detune
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphZ, nullptr },   // 28  Morph Z  --  FASE 11.3
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphZ2, nullptr },   // 29  Morph Z 2  --  FASE 11.3
    { {}, Kind::voiceAdd, -1, &IVoice::modMorphZ3, nullptr },   // 30  Morph Z 3  --  FASE 11.3
};

// La tabla DEBE seguir la longitud de la tabla de etiquetas. La longitud se
// compara contra el constexpr de ModDestinationTable.h y no contra un 31
// escrito a mano: asi el numero vive en un solo sitio, que es el que la UI
// lee, y anadir un destino obliga a las dos tablas a crecer a la vez.
static_assert (kNumModDestinations == static_cast<int> (kModDestinationCount),
               "la tabla de destinos debe seguir la longitud de getModDestinationTable()");

// ── LA RED, en tres capas ───────────────────────────────────────────────────
//
// (1) Ninguna fila, salvo el Off, puede estar VACIA. Este es el que caza la
//     tabla constant-inicializada a cero —el fallo de MSVC de arriba— en las 31
//     filas de una vez. La condicion mira que la fila DECLARE ALGUNA REGLA, no
//     que tenga punteros: una fila a cero tiene `kind == Kind::none` en las dos
//     ramas, mientras que una fila buena tiene al menos una. Un aserto de
//     "ningun puntero es nullptr" NO serviria aqui, porque el fallo que hay que
//     cazar ES un puntero a cero y compararlo contra nullptr no distingue el
//     puntero a cero de uno de verdad.
//
// (2) Cada fila, una por una, es la que su ETIQUETA dice. El nombre sale de
//     kModDestinationTable, que es la tabla que pinta la pagina: si alguien
//     intercambia dos filas ahi, el nombre de la fila N deja de cuadrar con la
//     regla de la fila N y el build se rompe aqui, que es donde esta la regla.
//     Este era el hueco de verdad: hasta ahora el puntero estaba atado en
//     18 de 31 filas y el nombre en ninguna, asi que cruzar las dos tablas
//     —intercambiar "Inharmonicity" y "Roughness"— no rompia NADA.
//
// (3) El puntero de cada fila. Con la tabla a cero estos asertos entran a
//     compilar y dejan el build en rojo, en vez de dejar que el motor se coma
//     las rutas en silencio y que solo se entere el oido.
constexpr bool everyDestinationDeclaresSomething (int index = 1)
{
    return index >= kNumModDestinations
        ? true
        : (kModDestinations[index].add.kind != Kind::none
           || kModDestinations[index].env.kind != Kind::none)
          && everyDestinationDeclaresSomething (index + 1);
}

static_assert (everyDestinationDeclaresSomething(),
               "toda fila salvo el Off declara una regla: la tabla no esta a cero");

// `perNote` en la tabla de parametros y "esta fila PREGUNTA por la fuente" en
// esta son la MISMA COSA, y hasta ahora no lo decia nadie. El contrato JSON
// publica `perNote` para que otras superficies (la pagina, ABDEep) sepan que
// hay una ruta por voz detras, asi que si las dos tablas se separan, el
// contrato esta mintiendo. Se comprueba en las 31 filas con dos funciones, y no
// con treinta y una lineas que se quedan viejas el dia que anadas un destino.
constexpr bool perNoteMatchesTheEngine (int index = 0)
{
    return index >= kNumModDestinations
        ? true
        : (kModDestinationTable[index].perNote
            == (kModDestinations[index].env.kind != Kind::none))
          && perNoteMatchesTheEngine (index + 1);
}

static_assert (perNoteMatchesTheEngine(),
               "perNote dice lo mismo que la rama env de cada fila");

// -- `replaces` (descriptor compartido) <=> REEMPLAZAR (envAssign) ---------
//
// Las dos cosas son la MISMA politica desde los dos lados: "con su envolvente,
// la ruta REEMPLAZA el factor en vez de sumar encima" es exactamente lo que hace
// `envAssign` y lo que no hace `envAdd`. Hasta ahora solo se ataba `perNote` con
// "esta fila PREGUNTA por la fuente", asi que los dos campos podian separarse en
// silencio: pasar una fila de envAssign a envAdd sin tocar el `replaces`
// publicado no rompia NADA, y el contrato JSON de ABDSharedAssets seguia
// diciendo "reemplaza" para un destino que suma.
//
// Al atarlo aparece una DIVERGENCIA VIVA, y por eso esto no es un simple par de
// asertos: los destinos 12..16 se publican como `replaces: true` --en el
// contrato, en la UI y en la lista escrita a mano de ModulationMatrixTest-- y el
// motor los SUMA (`envAdd`). Los tres dicen lo mismo, y los tres dicen una cosa
// que el motor no hace.
//
// NO se corrige aqui, y el motivo es el orden de las cosas: pasar esas cinco
// filas de `envAdd` a `envAssign` cambia el SONIDO de cinco destinos --una ENV 2
// negativa deja de invertir el ADSR del filtro y pasa a borrarlo--, y con el
// sonido se mueven los 41 hashes de la paridad. Eso es una decision de producto
// (que de las dos verdades es la buena) y no un refactor. Lo que si se hace es
// que deje de ser invisible: los dos asertos de abajo la nombran fila a fila, de
// modo que tocarla --en cualquiera de los dos lados-- pone el build en rojo en el
// sitio de la regla, que es donde se puede ver gratis.

// (a) Lo que REEMPLAZA esta publicado como `replaces`. Sin esto, una fila nueva
//     con envAssign saldria al contrato diciendo que suma.
constexpr bool assignIsPublishedAsReplaces (int index = 0)
{
    return index >= kNumModDestinations
        ? true
        : (kModDestinations[index].env.kind != Kind::envAssign
            || kModDestinationTable[index].replaces)
          && assignIsPublishedAsReplaces (index + 1);
}

static_assert (assignIsPublishedAsReplaces(),
               "toda fila que REEMPLAZA (envAssign) se publica como replaces");

// (b) Las filas que se publican `replaces` y que el motor SUMA. Este no es un
//     aserto de que la divergencia este bien: es un aserto de que la lista sigue
//     siendo ESTA, para que mover una de estas cinco filas en cualquiera de los
//     dos lados avise, en vez de decidirlo el oido.
constexpr bool publishedReplacesThatTheEngineAdds (int index = 0)
{
    return index >= kNumModDestinations
        ? true
        : ((index < 12 || index > 16)
            || (kModDestinationTable[index].replaces
                && kModDestinations[index].env.kind == Kind::envAdd))
          && publishedReplacesThatTheEngineAdds (index + 1);
}

static_assert (publishedReplacesThatTheEngineAdds(),
               "las filas 12..16 se publican replaces y el motor las SUMA: si la "
               "divergencia se decide, esta frase es la que hay que cambiar");

static_assert (kModDestinations[0].add.kind == Kind::none
               && kModDestinations[0].env.kind == Kind::none,
               "el destino 0 (Off) es inerte A PROPOSITO: no es un destino que aplica un 0");

// El nombre va en la MISMA expresion que el puntero. El literal del nombre esta
// duplicado aqui a proposito: es el clavo. Si el nombre de una fila cambia en
// la tabla de parametros, hay que venir aqui a confirmar el cambio a mano, y
// mientras tanto el build esta rojo en vez de servir Inharmonicity donde la
// pagina dice Roughness.
static_assert (modDestinationLabelIs ( 0, "Off"),          "fila 0 de la tabla: Off");
static_assert (modDestinationLabelIs ( 1, "Osc Level")     && kModDestinations[ 1].env.kind == Kind::envAssign
                                                          && kModDestinations[ 1].env.voice == &IVoice::modEnvLevel
                                                          && kModDestinations[ 1].add.voice == &IVoice::modLevel,
               "destino 1: Osc Level -- ENV 1 asigna modEnvLevel, otra fuente suma a modLevel");
static_assert (modDestinationLabelIs ( 2, "Inharmonicity") && kModDestinations[ 2].add.voice == &IVoice::modInharmonicity,
               "destino 2: Inharmonicity");
static_assert (modDestinationLabelIs ( 3, "Roughness")     && kModDestinations[ 3].add.voice == &IVoice::modRoughness,
               "destino 3: Roughness");
static_assert (modDestinationLabelIs ( 4, "Morph X")       && kModDestinations[ 4].add.voice == &IVoice::modMorphX,
               "destino 4: Morph X");
static_assert (modDestinationLabelIs ( 5, "Morph Y")       && kModDestinations[ 5].add.voice == &IVoice::modMorphY,
               "destino 5: Morph Y");
static_assert (modDestinationLabelIs ( 6, "Amp Attack")    && kModDestinations[ 6].add.voice == &IVoice::modAmpAttack,
               "destino 6: Amp Attack");
static_assert (modDestinationLabelIs ( 7, "Amp Decay")     && kModDestinations[ 7].add.voice == &IVoice::modAmpDecay,
               "destino 7: Amp Decay");
static_assert (modDestinationLabelIs ( 8, "Amp Sustain")   && kModDestinations[ 8].add.voice == &IVoice::modAmpSustain,
               "destino 8: Amp Sustain");
static_assert (modDestinationLabelIs ( 9, "Amp Release")   && kModDestinations[ 9].add.voice == &IVoice::modAmpRelease,
               "destino 9: Amp Release");
static_assert (modDestinationLabelIs (10, "Filter Cutoff") && kModDestinations[10].env.kind == Kind::envAssign
                                                          && kModDestinations[10].env.voice == &IVoice::modEnvCutoff
                                                          && kModDestinations[10].add.voice == &IVoice::modCutoff
                                                          && kModDestinationTable[10].scale == 18000.0f,
               "destino 10: Filter Cutoff -- ENV 2 asigna modEnvCutoff, otra fuente suma 18000 Hz");
static_assert (modDestinationLabelIs (11, "Filter Res")    && kModDestinations[11].add.voice == &IVoice::modFilterRes,
               "destino 11: Filter Res");
static_assert (modDestinationLabelIs (12, "Filter Env Amt")&& kModDestinations[12].env.kind == Kind::envAdd
                                                          && kModDestinations[12].env.voice == &IVoice::modEnvFltDepth,
               "destino 12: Filter Env Amt -- ENV 2 SUMA la profundidad (no asigna)");
static_assert (modDestinationLabelIs (13, "Flt Attack")    && kModDestinations[13].env.voice == &IVoice::modEnvFltAttack,
               "destino 13: Flt Attack");
static_assert (modDestinationLabelIs (14, "Flt Decay")     && kModDestinations[14].env.voice == &IVoice::modEnvFltDecay,
               "destino 14: Flt Decay");
static_assert (modDestinationLabelIs (15, "Flt Sustain")   && kModDestinations[15].env.voice == &IVoice::modEnvFltSustain,
               "destino 15: Flt Sustain");
static_assert (modDestinationLabelIs (16, "Flt Release")   && kModDestinations[16].env.voice == &IVoice::modEnvFltRelease,
               "destino 16: Flt Release");
// El destino 17 modulaba antes `GlobalParams::saturationAmt`, el mando suelto
// de la saturacion. Con el hueco 1 migrado a bus ese campo ya no lo maneja
// nadie, y el mando que la matriz sigue siendo el MISMO de siempre: el drive
// del hueco, que es `fx[0].params[0]`. La comprobacion sigue mirandolo, y ahora
// ademas mira que apunte al hueco y no a un parametro que ya no existe.
static_assert (modDestinationLabelIs (17, "Saturation")    && kModDestinations[17].add.kind == Kind::globalFxAdd
                                                          && kModDestinations[17].busParam == 0 * kFxBusParams + 0
                                                          && kModDestinations[17].add.global == nullptr,
               "destino 17: Saturacion es el drive del hueco 1, no de voz");
static_assert (modDestinationLabelIs (18, "Delay Time")    && kModDestinations[18].add.global == &GlobalParams::delayTime,
               "destino 18: Delay Time es un parametro global (FX)");
static_assert (modDestinationLabelIs (19, "Delay FB")      && kModDestinations[19].add.global == &GlobalParams::delayFB,
               "destino 19: Delay FB es un parametro global (FX)");
static_assert (modDestinationLabelIs (20, "Odd/Even Bal")  && kModDestinations[20].add.voice == &IVoice::modParity,
               "destino 20: Odd/Even Bal");
static_assert (modDestinationLabelIs (21, "Spectral Shift")&& kModDestinations[21].add.voice == &IVoice::modShift,
               "destino 21: Spectral Shift");
static_assert (modDestinationLabelIs (22, "Harm Roll-off") && kModDestinations[22].add.voice == &IVoice::modRolloff,
               "destino 22: Harm Roll-off");
static_assert (modDestinationLabelIs (23, "Excite Noise")  && kModDestinations[23].add.voice == &IVoice::modExciteNoise,
               "destino 23: Excite Noise");
static_assert (modDestinationLabelIs (24, "Excite Color")  && kModDestinations[24].add.voice == &IVoice::modExciteColor,
               "destino 24: Excite Color");
static_assert (modDestinationLabelIs (25, "Impulse Mix")   && kModDestinations[25].add.voice == &IVoice::modImpulseMix,
               "destino 25: Impulse Mix");
static_assert (modDestinationLabelIs (26, "Res Bank Res")  && kModDestinations[26].add.voice == &IVoice::modResonance,
               "destino 26: Res Bank Res");
static_assert (modDestinationLabelIs (27, "Unison Detune") && kModDestinations[27].add.voice == &IVoice::modUnison,
               "destino 27: Unison Detune");
static_assert (modDestinationLabelIs (28, "Morph Z")       && kModDestinations[28].add.voice == &IVoice::modMorphZ,
               "destino 28: Morph Z");
static_assert (modDestinationLabelIs (29, "Morph Z 2")     && kModDestinations[29].add.voice == &IVoice::modMorphZ2,
               "destino 29: Morph Z 2");
static_assert (modDestinationLabelIs (30, "Morph Z 3")     && kModDestinations[30].add.voice == &IVoice::modMorphZ3,
               "destino 30: Morph Z 3");

} // namespace

void NeuronikEngine::applyModulation()
{
    // Snapshot LFO values from base. Las fuentes ENV (6/7) se resuelven
    // per-voz dentro de los casos per-note: aqui valen 0 y nunca entran
    // por el camino LFO (una envolvente no es global, es de cada nota).
    // Las ocho fuentes, en el orden de la tabla (getModSources). Las tres que
    // eran un TODO (PB, MW, AT) ahora traen el valor real: lo escribe el mismo
    // gesto MIDI que mueve la voz, y se guarda normalizado para que la matriz
    // lo pueda sumar igual que a un LFO. Ver los miembros en NeuronikEngine.h.
    float sources[8] = { 
        0.0f,                   // Off
        lfo1Value.load(),       // LFO 1
        lfo2Value.load(),       // LFO 2
        pitchBendSource_,       // Pitch Bend   (-1..+1)
        modWheelSource_,        // Mod Wheel    (0..1)
        aftertouchSource_,      // Aftertouch   (0..1)
        0.0f,                   // ENV 1: per-voz (VCA)
        0.0f                    // ENV 2: per-voz (ADSR del filtro)
    };

    // Reset voice mod values
    for (auto& v : voices) v->resetModulations();
    
    // Reset visualization values
    lastModulations.fill(0.0f);

    for (int i = 0; i < 4; ++i)
    {
        const auto& route = currentGlobalParams.modMatrix[i];
        if (route.source == kNoModSource || route.destination == kNoModDestination) continue;
        
        float rawMod = sources[dsp::jlimit(0, 5, route.source)] * route.amount;
        
        // Update visualization
        if (route.destination >= 0 && route.destination < 64)
            lastModulations[route.destination] += rawMod;
        
        // Dest logic, ahora POR TABLA (ver kModDestinos, mas arriba).
        // Los destinos PER-NOTE (1, 10, 12-16) preguntan por la fuente: los
        // LFOs suman como siempre; si la ruta es ENV, la voz recibe el amount
        // como factor de routing (sintesis de reemplazo, ver arriba).
        //
        // Un destino fuera de la tabla no hace NADA, y aqui no se avisa porque
        // avisar en el hilo de audio no es opcion. El aviso vive en el
        // static_assert de arriba, que es donde se puede ver gratis.
        if (route.destination < 0 || route.destination >= kNumModDestinations)
            continue;

        const auto& descriptor = kModDestinations[route.destination];

        // Las unidades del destino salen del descriptor COMPARTIDO y no de la
        // fila de reglas: son el `scale` de kModDestinationTable, que es el
        // mismo numero que lee la UI y que se publica en el contrato. En las
        // filas sin rama aditiva (0, 12..16) no se llega a usar.
        const auto& units = kModDestinationTable[route.destination];

        // 1) La rama que PREGUNTA por la fuente. Va antes que la aditiva porque
        //    es lo que hacia el switch dentro de cada case; si la fuente no es
        //    la envolvente que este destino pide, la ruta cae en la rama
        //    aditiva de abajo, igual que entonces.
        if (descriptor.env.kind != ModRule::Kind::none
            && route.source == descriptor.env.envSource)
        {
            for (auto& v : voices)
            {
                if (descriptor.env.kind == ModRule::Kind::envAssign)
                    v.get()->* (descriptor.env.voice) = route.amount;   // factor: sobrescribe
                else
                    v.get()->* (descriptor.env.voice) += route.amount;   // profundidad: suma
            }
        }
        // 2) La rama aditiva: a un miembro de la voz, o al bus de FX.
        else if (descriptor.add.kind == ModRule::Kind::voiceAdd)
        {
            const float amount = rawMod * units.scale;
            for (auto& v : voices)
                v.get()->* (descriptor.add.voice) += amount;
        }
        else if (descriptor.add.kind == ModRule::Kind::globalAdd)
        {
            currentGlobalParams.* (descriptor.add.global) += rawMod * units.scale;
        }
        else if (descriptor.add.kind == ModRule::Kind::globalFxAdd)
        {
            // Un mando DEL BUS se direcciona por (hueco, indice) y no con un
            // puntero a miembro: `&GlobalParams::fx[0].params[0]` no existe en
            // C++ (un puntero a miembro no puede atravesar un array), y esta es
            // la razon de que el Kind exista. El recorte contra `kFxBusParams`
            // no es decorativo: si el catalogo tuviera un hueco con mas mandos
            // que el bus y la tabla no se actualizara, esto escriberia fuera.
            if (descriptor.busParam >= 0 && descriptor.busParam < kFxBusSlots * kFxBusParams)
                currentGlobalParams.fx[descriptor.busParam / kFxBusParams]
                                 .params[descriptor.busParam % kFxBusParams]
                    += rawMod * units.scale;
        }
    }
}

void NeuronikEngine::updateParameters()
{
    // Propagate parameters to all voices
    for (auto& v : voices)
    {
        if (v->getType() == VoiceType::Additive)
            static_cast<Synthesis::AdditiveVoice*>(v.get())->setParams(pendingVoiceParams);
    }

    BaseEngine::updateParameters();
    // La modulacion NO se aplica aqui: la aplica renderVoicesWithControlRate() en la
    // rejilla de control, con el valor de LFO de cada tramo (antes se aplicaba una vez
    // por bloque del host y con el valor del bloque anterior).
}


void NeuronikEngine::getSpectralData(float* destination64) const
{
    bool found = false;
    for (const auto& v : voices)
    {
        if (v->isActive() && v->getType() == VoiceType::Additive)
        {
            auto* av = static_cast<Synthesis::AdditiveVoice*>(v.get());
            auto& partials = av->getResonator().getPartialAmplitudes();
            for (int i = 0; i < 64; ++i) destination64[i] = partials[i];
            found = true;
            break;
        }
    }
    
    if (!found)
    {
        for (int i = 0; i < 64; ++i) destination64[i] = 0.0f;
    }
}

void NeuronikEngine::getEnvelopeLevels(float& amp, float& filter) const
{
    for (const auto& v : voices)
    {
        if (v->isActive() && v->getType() == VoiceType::Additive)
        {
            auto* av = static_cast<Synthesis::AdditiveVoice*>(v.get());
            amp = av->getAmpEnvelopeLevel();
            filter = av->getFilterEnvelopeLevel();
            return;
        }
    }
    amp = 0.0f;
    filter = 0.0f;
}

void NeuronikEngine::getModulationValues(float* destination, int count) const
{
    if (destination == nullptr || count <= 0) return;
    
    int numToCopy = dsp::jmin(count, (int)lastModulations.size());
    for (int i = 0; i < numToCopy; ++i)
        destination[i] = lastModulations[i];
        
    // Fill remaining with 0
    for (int i = numToCopy; i < count; ++i)
        destination[i] = 0.0f;
}

void NeuronikEngine::loadModel(const Common::SpectralModel& model, int slot)
{
    for (auto& v : voices)
    {
        if (v->getType() == VoiceType::Additive)
            static_cast<Synthesis::AdditiveVoice*>(v.get())->loadModel(model, slot);
    }
}

void NeuronikEngine::setVoiceParams(const NEURONiK::DSP::Synthesis::AdditiveVoice::Params& p)
{
    pendingVoiceParams = p;
}

// MORPH del pad XY: read-modify-write de pendingVoiceParams. No toca morphZ
// (el eje temporal es parametro propio, morphZ/morphZ2/morphZ3) ni el resto
// de la ficha: solo los dos ejes del pad. Viaja por el mismo canal que
// setVoiceParams, asi que el handoff a las voces es el de siempre.
void NeuronikEngine::setMorph (float morphX, float morphY)
{
    pendingVoiceParams.morphX = morphX;
    pendingVoiceParams.morphY = morphY;
}

// Eje temporal del morph (FASE 10): frame canonico de la capa 0.
void NeuronikEngine::setMorphZ (float morphZ)
{
    pendingVoiceParams.morphZ = morphZ;
}

// FASE 11.4: el VOLUMEN de las capas 1 y 2 (la capa 0 es el fondo, siempre
// al maximo). Mismo canal RT-safe que setMorphZ; el clampeo lo hace el
// resonador al consumirlo.
void NeuronikEngine::setVoiceLayerMorph (float layerGain2, float layerGain3)
{
    pendingVoiceParams.layerGain2 = layerGain2;
    pendingVoiceParams.layerGain3 = layerGain3;
}

// ADSR de las DOS envolventes (canal del worklet, sin APVTS): mismo
// read-modify-write de los tres de arriba, limitado a los ocho tramos. Las
// voces los reciben en el updateParameters() del proximo bloque
// (Envelope::setParameters ya clampea tiempos a >= 0.1 ms y el sustain a 0..1).
void NeuronikEngine::setVoiceEnvelope (float attack, float decay, float sustain, float release,
                                       float fAttack, float fDecay, float fSustain, float fRelease)
{
    pendingVoiceParams.attack  = attack;
    pendingVoiceParams.decay   = decay;
    pendingVoiceParams.sustain = sustain;
    pendingVoiceParams.release = release;
    pendingVoiceParams.fAttack  = fAttack;
    pendingVoiceParams.fDecay   = fDecay;
    pendingVoiceParams.fSustain = fSustain;
    pendingVoiceParams.fRelease = fRelease;
}

void NeuronikEngine::handleMidiEvent(const dsp::MidiMessage& m)
{
    int channel = m.getChannel();

    if (m.isNoteOn())
    {
        const int limit = activeVoiceLimit.load();
        for (int i = 0; i < limit; ++i)
        {
            if (!voices[i]->isActive())
            {
                voices[i]->setChannel(channel);
                voices[i]->noteOn(m.getNoteNumber(), m.getFloatVelocity());
                return;
            }
        }
        voices[0]->setChannel(channel);
        voices[0]->noteOn(m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        for (auto& v : voices)
        {
            if (v->isActive() && v->getCurrentlyPlayingNote() == m.getNoteNumber() && v->getChannel() == channel)
                v->noteOff(m.getFloatVelocity(), true);
        }
    }
    else if (m.isPitchWheel())
    {
        float bendSemitones = ((float)m.getPitchWheelValue() - 8192.0f) / 8192.0f * 48.0f; // Scale to 48 semitones
        // La misma deflection, normalizada, para la matriz: una sola verdad
        // para la voz y para la ruta de modulación.
        pitchBendSource_ = (float)m.getPitchWheelValue() / 8192.0f - 1.0f;
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel))
                v->notePitchBend(bendSemitones);
        }
    }
    else if (m.isAftertouch() || m.isChannelPressure())
    {
        float pressureVal = m.isAftertouch() ? (float)m.getAfterTouchValue() : (float)m.getChannelPressureValue();
        float pressure = pressureVal / 127.0f;
        // Igual que el pitch bend: la presión es tambien fuente de la matriz.
        aftertouchSource_ = pressure;
        
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel))
                v->notePressure(pressure);
        }
    }
    else if (m.isController() && m.getControllerNumber() == 1)
    {
        // CC 1 es la rueda de modulacion del MIDI estandar, y la tabla la
        // ofrece como fuente: sin esto seguia sin modular nada.
        modWheelSource_ = (float)m.getControllerValue() / 127.0f;
    }
    else if (m.isController() && m.getControllerNumber() == 74)
    {
        float timbre = (float)m.getControllerValue() / 127.0f;
        for (auto& v : voices)
        {
            if (v->isActive() && (v->getChannel() == channel))
                v->noteTimbre(timbre);
        }
    }
}


} // namespace NEURONiK::DSP
