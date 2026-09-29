/*
  ==============================================================================

    ParameterDefinitions.h
    Created: 22 Jan 2026
    Description: Centralized parameter definitions for NEURONiK.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

#include "ModDestinationTable.h"
#include "../DSP/FxCatalogue.h"

namespace NEURONiK::State {

namespace IDs {
    // Oscillator / Neural Core
    static constexpr const char* engineType       = "engineType";
    static constexpr const char* oscLevel         = "oscLevel";
    // oscPitchCoarse was retired on 2026-09-19: declared here since day one but never added
    // to the layout, so no engine, panel or preset ever read it. It only existed as a
    // promise (a leftover of the old NexusParams draft, whose siblings oscPitchFine,
    // oscPitchOctave and oscHarmonicCount were never adopted either). Coarse tuning would
    // be a FEATURE, not pending wiring: the pitch path that does exist is the per-voice MPE
    // bend (IVoice::notePitchBend, in semitones), and a global transpose would belong in
    // the host next to velocityCurve/midiChannel — no engine or WASM ABI change needed.
    // See DSP_PARAMETERS.md, "Parámetros sin consumidor".
    static constexpr const char* oscInharmonicity = "oscInharmonicity";
    static constexpr const char* oscRoughness     = "oscRoughness";
    // harmMix was retired on 2026-09-16: it was never read by any engine, so it only
    // existed as a promise. Old presets that still contain it load normally, and the
    // child is dropped the next time they are saved (see PresetManager migration).
    static constexpr const char* morphX           = "morphX";
    static constexpr const char* morphZ           = "morphZ";
    // FASE 11.3: eje temporal de las capas 1 y 2 (sampler por capa). Son dos
    // parametros de verdad —no estado interno— porque la matriz tiene que
    // poder modularlos y un preset tiene que guardarlos; su default (0.0) es el
    // frame canonico, asi que con un modelo de una capa (todo el legado) son
    // inertes y el sonido no cambia.
    static constexpr const char* morphZ2          = "morphZ2";
    static constexpr const char* morphZ3          = "morphZ3";
    static constexpr const char* morphY           = "morphY";
    static constexpr const char* oscExciteNoise   = "oscExciteNoise";
    static constexpr const char* excitationColor  = "excitationColor";
    static constexpr const char* impulseMix       = "impulseMix";
    static constexpr const char* oscExciteBow     = "oscExciteBow";
    static constexpr const char* resonatorRes     = "resonatorRes";
    static constexpr const char* unisonDetune     = "unisonDetune";
    static constexpr const char* unisonSpread     = "unisonSpread";
    static constexpr const char* unisonEnabled    = "unisonEnabled";

    // Resonator Advanced (Legacy)
    static constexpr const char* resonatorRolloff = "resonatorRolloff";
    static constexpr const char* resonatorParity  = "resonatorParity";
    static constexpr const char* resonatorShift   = "resonatorShift";

    // Envelope
    static constexpr const char* envAttack  = "envAttack";
    static constexpr const char* envDecay   = "envDecay";
    static constexpr const char* envSustain = "envSustain";
    static constexpr const char* envRelease = "envRelease";

    // Filter
    static constexpr const char* filterCutoff    = "filterCutoff";
    static constexpr const char* filterRes       = "filterRes";
    // filterEnvAmount was retired on 2026-09-26: with ENV 2 travelling through
    // the mod matrix (default route ENV 2 -> Filter Cutoff), the knob was the
    // SECOND depth on the same path (matrix amount × knob). The matrix amount
    // is now THE depth (bipolar, +/-1: negative inverts the envelope); the
    // destination "Filter Env Amt" (index 12) survives and adds to the 1.0
    // routing factor, so the label keeps its meaning. Old presets that still
    // contain it load normally and the child is dropped on next save
    // (see PresetManager migration, same precedent as harmMix).
    static constexpr const char* filterAttack    = "filterAttack";
    static constexpr const char* filterDecay     = "filterDecay";
    static constexpr const char* filterSustain   = "filterSustain";
    static constexpr const char* filterRelease   = "filterRelease";

    // FX: el BUS POR HUECO. Los CUATRO huecos lo tienen (2026-09-29): el tipo
    // del hueco, su ganancia, su mezcla y sus doce mandos. Los ids del hueco 1
    // son los unicos escritos aqui, porque son los unicos que otro fichero cita
    // de texto (`ModDestinationTable.h`); los de los otros tres se COMPONEN con
    // `fxBusFieldId`/`fxBusParamId` mas abajo, que es lo que evita tener
    // sesenta literales que se pueden separar del layout sin que nada lo note.
    // Ver `Source/DSP/FxSlots.h` y `Source/DSP/FxCatalogue.h`.
    //
    // El prefijo `fx1` (y no `fxSlot1`) es el que usa el panel para encadenar:
    // `fx1Type`, `fx1Param1`..`fx1Param12`. Un hueco con id fijo y mandos
    // numerados es lo unico que el APVTS, un preset y la matriz pueden hablar
    // sin saber que efecto hay puesto.
    // Del hueco 1, y SOLO del hueco 1: el layout compone los cuatro con
    // `fxBusFieldId`/`fxBusParamId` y aqui solo hacen falta los que otro
    // fichero cita de texto, que es la tabla de destinos de modulacion
    // (`ModDestinationTable.h`, el destino 17). Los doce mandos del hueco 1 se
    // declaran igual; esta lista no es la fuente, es el sitio donde se mira el
    // destino 17, y por eso se queda con los cuatro que existen como literales.
    static constexpr const char* fx1Type    = "fx1Type";
    static constexpr const char* fx1Gain    = "fx1Gain";
    static constexpr const char* fx1Mix     = "fx1Mix";
    static constexpr const char* fx1Param1  = "fx1Param1";
    static constexpr const char* fx1Param2  = "fx1Param2";
    static constexpr const char* fx1Param3  = "fx1Param3";
    static constexpr const char* fx1Param4  = "fx1Param4";

    static constexpr const char* fxDelayTime     = "fxDelayTime";
    static constexpr const char* fxDelayFeedback = "fxDelayFeedback";
    static constexpr const char* fxDelaySync     = "fxDelaySync";
    static constexpr const char* fxDelayDivision = "fxDelayDivision";
    
    // Chorus
    static constexpr const char* fxChorusRate  = "fxChorusRate";
    static constexpr const char* fxChorusDepth = "fxChorusDepth";
    static constexpr const char* fxChorusMix   = "fxChorusMix";

    // Reverb
    static constexpr const char* fxReverbSize    = "fxReverbSize";
    static constexpr const char* fxReverbDamping = "fxReverbDamping";
    static constexpr const char* fxReverbWidth   = "fxReverbWidth";
    static constexpr const char* fxReverbMix     = "fxReverbMix";

    // Master / Global
    static constexpr const char* masterLevel    = "masterLevel";
    static constexpr const char* masterBPM      = "masterBPM";
    static constexpr const char* midiThru       = "midiThru";
    static constexpr const char* midiChannel    = "midiChannel";
    static constexpr const char* randomStrength = "randomStrength";
    static constexpr const char* freezeResonator = "freezeResonator";
    static constexpr const char* freezeFilter    = "freezeFilter";
    static constexpr const char* freezeEnvelopes = "freezeEnvelopes";
    static constexpr const char* velocityCurve    = "velocityCurve";

    // LFO 1
    static constexpr const char* lfo1Waveform = "lfo1Waveform";
    static constexpr const char* lfo1RateHz = "lfo1RateHz";
    static constexpr const char* lfo1SyncMode = "lfo1SyncMode";
    static constexpr const char* lfo1RhythmicDivision = "lfo1RhythmicDivision";
    static constexpr const char* lfo1Depth = "lfo1Depth";

    // LFO 2
    static constexpr const char* lfo2Waveform = "lfo2Waveform";
    static constexpr const char* lfo2RateHz = "lfo2RateHz";
    static constexpr const char* lfo2SyncMode = "lfo2SyncMode";
    static constexpr const char* lfo2RhythmicDivision = "lfo2RhythmicDivision";
    static constexpr const char* lfo2Depth = "lfo2Depth";

    // Modulation Matrix
    static constexpr const char* mod1Source = "mod1Source";
    static constexpr const char* mod1Destination = "mod1Destination";
    static constexpr const char* mod1Amount = "mod1Amount";
    static constexpr const char* mod2Source = "mod2Source";
    static constexpr const char* mod2Destination = "mod2Destination";
    static constexpr const char* mod2Amount = "mod2Amount";
    static constexpr const char* mod3Source = "mod3Source";
    static constexpr const char* mod3Destination = "mod3Destination";
    static constexpr const char* mod3Amount = "mod3Amount";
    static constexpr const char* mod4Source = "mod4Source";
    static constexpr const char* mod4Destination = "mod4Destination";
    static constexpr const char* mod4Amount = "mod4Amount";
}

//==============================================================================
// LOS IDS DEL BUS POR HUECO, COMPUESTOS Y NO ESCRITOS.
//
// Los cuatro ids del hueco 1 estan como literales en `IDs::` (los usan el panel
// y la tabla de destinos por nombre), pero la MIGRACION DE PRESETS tiene que
// poder escribir los de los cuatro huecos sin tener los cuatro en una lista. Se
// compone el id y se ata la composicion a los literales con `static_assert`, de
// modo que un rename que se lleve el compositor se rompe al compilar aqui y no
// en la migracion, donde el fallo seria un preset que pierde un mando sin decir
// nada.
//
// POR QUE UN BUFFER Y NO UN `juce::String`: `juce::String` no es una expresion
// constante (asigna memoria), asi que un `static_assert` que lo compare no
// compila (C2131). El buffer de `char` si lo es, y la comparacion de texto es
// `modDestinationTextIs`, que ya vive en este namespace.

/** Escribe en `out` el id de un hueco y devuelve cuantos caracteres escribio.
    `out` tiene que caber: dos del prefijo, uno o dos del numero y el campo.

    EL NUMERO SIN CERO A LA IZQUIERDA, que es lo que hizo tropezar esto: la
    primera version escribia los dos digitos siempre y componia `fx01Param1`,
    que no es el id de nadie. Un `static_assert` que compara contra el literal
    es justo lo que caza eso antes de que llegue a un preset. */
constexpr int composeFxBusId (char* out, int slot, const char* field, int fieldLength) noexcept
{
    int n = 0;
    out[n++] = 'f';
    out[n++] = 'x';

    const int number = slot + 1;

    if (number >= 10)
        out[n++] = static_cast<char> ('0' + (number / 10));

    out[n++] = static_cast<char> ('0' + (number % 10));

    for (int i = 0; i < fieldLength; ++i)
        out[n++] = field[i];

    out[n] = '\0';
    return n + 1;
}

/** `fieldLength` a partir de un literal, sin `strlen` (que no es constexpr). */
constexpr int literalLength (const char* text) noexcept
{
    return *text == '\0' ? 0 : 1 + literalLength (text + 1);
}

/** El id del mando `param` (base 0) del hueco `slot`: `fx1Param1`...

    EL NUMERO DEL MANDO SE ESCRIBE AQUI Y NO EN `composeFxBusId`, que compone el
    hueco y el campo pero no el mando. Se puede hacer de las dos maneras, y la
    que se parece mas a la que hay que evitar es concatenar el numero fuera: el
    `static_assert` de abajo solo ve `fx1Param`, no `fx1Param1`, asi que la
    comprobacion de la composicion se queda en la mitad y deja pasar justo el
    error que deberia cazar. Con el numero dentro, el aserto mira la cadena
    entera. */
inline juce::String fxBusParamId (int slot, int param)
{
    char buffer[32] {};
    int n = composeFxBusId (buffer, slot, "Param", literalLength ("Param"));
    n -= 1;   // el terminador, que se sobreescribe

    const int number = param + 1;

    if (number >= 10)
        buffer[n++] = static_cast<char> ('0' + (number / 10));

    buffer[n++] = static_cast<char> ('0' + (number % 10));
    buffer[n] = '\0';

    return juce::String (buffer);
}

/** El id de un campo del hueco: `fx1Type`, `fx1Gain`, `fx1Mix`. */
inline juce::String fxBusFieldId (int slot, const char* field)
{
    char buffer[32] {};
    composeFxBusId (buffer, slot, field, literalLength (field));
    return juce::String (buffer);
}

/** Que hueco es ESTE id de tipo (`fx1Type`...), o -1 si no es un id de tipo.

    Lo usa `parameterChanged` para no escribir cuatro `if`: el id lo compone
    el layout y el numero de hueco sale de ahi, no de una tabla escrita al lado
    que se pueda quedar vieja (que es lo que paso con el unico `if` que habia,
    que atendia el hueco 1 y solo el hueco 1).

    Devolver -1 en vez de un hueco de mentira es lo que deja que el resto de
    `parameterChanged` siga atendiendo los demas parametros: si esto devolviera
    0 para "no lo conozco", un `fxChorusRate` acabaria en el hueco 1.
*/
inline int fxBusSlotOfTypeId (const juce::String& parameterId)
{
    for (int slot = 0; slot < NEURONiK::DSP::kFxBusSlots; ++slot)
        if (parameterId == fxBusFieldId (slot, "Type"))
            return slot;

    return -1;
}

// Los cuatro ids del hueco 1, comprobados UNO POR UNO y sin macro. La macro
// que habia aqui metia mas logica de la que la cuenta daba: tenia ramas
// y solo una se ejercitaba, y la que no se ejercita es justo la que compone
// el numero del mando. Cuatro lineas que se leen enteras valen mas que una
// que hay que seguir con el dedo.
static_assert ([&] { char b[32] {}; composeFxBusId (b, 0, "Type", 4);
                      return modDestinationTextIs (b, "fx1Type"); }(),
               "el compositor de ids del bus no compone `fx1Type`");

static_assert ([&] { char b[32] {}; composeFxBusId (b, 0, "Gain", 4);
                      return modDestinationTextIs (b, "fx1Gain"); }(),
               "el compositor de ids del bus no compone `fx1Gain`");

static_assert ([&] { char b[32] {}; composeFxBusId (b, 0, "Mix", 3);
                      return modDestinationTextIs (b, "fx1Mix"); }(),
               "el compositor de ids del bus no compone `fx1Mix`");

// El del mando lleva su numero DENTRO de la cadena comprobada, y por eso se
// compone a mano aqui en vez de llamar a `fxBusParamId`: esa funcion es
// `inline` y devuelve un `juce::String`, que no es una expresion constante, y
// un `static_assert` que la llame no compila (C2131, que es justo el error que
// dio la primera version). Se repite aqui el numero a proposito: es la
// composicion ENTERA la que se mira, no media.
static_assert ([&] { char b[32] {};
                      int n = composeFxBusId (b, 0, "Param", 5);
                      b[n - 1] = '1';
                      b[n] = '\0';
                      return modDestinationTextIs (b, "fx1Param1"); }(),
               "el compositor de ids del bus no compone `fx1Param1`");

// Los tres ids que el layout NUEVO compone y que los dos anteriores no
// tocaban: el hueco 2 (la rama de una cifra del numero del hueco, que hasta
// aqui solo se habia probado con el 1), el hueco 10 (la de dos cifras, que con
// cuatro huecos es una rama que NADIE ejecuta y que se ejecuta el dia que
// haya diez huecos: sin este aserto nadie sabria si compone `fx1` o `fx10`),
// y el mando 12 (el ultimo de los doce, y el unico que pasa por la rama de dos
// cifras del numero del mando).
static_assert ([&] { char b[32] {}; composeFxBusId (b, 1, "Type", 4);
                      return modDestinationTextIs (b, "fx2Type"); }(),
               "el compositor de ids del bus no compone `fx2Type`");

static_assert ([&] { char b[32] {}; composeFxBusId (b, 9, "Type", 4);
                      return modDestinationTextIs (b, "fx10Type"); }(),
               "el compositor de ids del bus no compone `fx10Type` (la rama de dos cifras)");

static_assert ([&] { char b[32] {};
                      int n = composeFxBusId (b, 0, "Param", 5);
                      b[n - 1] = '1';
                      b[n++] = '2';
                      b[n] = '\0';
                      return modDestinationTextIs (b, "fx1Param12"); }(),
               "el compositor de ids del bus no compone `fx1Param12`");

/**
 * @brief Cada `parameterId` de la tabla es exactamente el `IDs::` que le toca.
 *
 * La tabla de ModDestinationTable.h lleva los ids como TEXTO, porque esa
 * cabecera no puede incluir `IDs::` sin arrastrar juce_audio_processors a los
 * targets de DSP. El precio de esa vuelta son treinta y una cadenas duplicadas,
 * y este es el punto donde se pagan: si alguien renombra `IDs::filterCutoff`, el
 * build se rompe aqui, en el sitio del rename.
 *
 * Las dos filas sin parametro (0 = "Off" y 12 = "Filter Env Amt", retirado el
 * 2026-09-26) se comprueban aparte, porque para ellas la verdad es que NO hay
 * id, no que el id es una cadena vacia.
 */
static_assert (kModDestinationTable[0].parameterId == nullptr,
               "destino 0 (Off) no conduce parametro");
static_assert (kModDestinationTable[12].parameterId == nullptr,
               "destino 12 (Filter Env Amt): el parametro se retiro, la etiqueta sigue");

#define ABD_CHECK_MOD_DEST_ID(index, id) \
    static_assert (modDestinationTextIs (kModDestinationTable[index].parameterId, IDs::id), \
                   "destino " #index ": parameterId tiene que ser IDs::" #id)

ABD_CHECK_MOD_DEST_ID ( 1, oscLevel);
ABD_CHECK_MOD_DEST_ID ( 2, oscInharmonicity);
ABD_CHECK_MOD_DEST_ID ( 3, oscRoughness);
ABD_CHECK_MOD_DEST_ID ( 4, morphX);
ABD_CHECK_MOD_DEST_ID ( 5, morphY);
ABD_CHECK_MOD_DEST_ID ( 6, envAttack);
ABD_CHECK_MOD_DEST_ID ( 7, envDecay);
ABD_CHECK_MOD_DEST_ID ( 8, envSustain);
ABD_CHECK_MOD_DEST_ID ( 9, envRelease);
ABD_CHECK_MOD_DEST_ID (10, filterCutoff);
ABD_CHECK_MOD_DEST_ID (11, filterRes);
ABD_CHECK_MOD_DEST_ID (13, filterAttack);
ABD_CHECK_MOD_DEST_ID (14, filterDecay);
ABD_CHECK_MOD_DEST_ID (15, filterSustain);
ABD_CHECK_MOD_DEST_ID (16, filterRelease);
// El destino 17 modulaba el mando suelto de la saturacion; con el hueco 1
    // migrado a bus (2026-09-29) el mando sigue siendo el drive, y el id que lo
    // publica es el del hueco. Esta comprobacion existe justo para que el
    // nombre de la tabla de modulacion y el id del APVTS no se separen.
    ABD_CHECK_MOD_DEST_ID (17, fx1Param1);
ABD_CHECK_MOD_DEST_ID (18, fxDelayTime);
ABD_CHECK_MOD_DEST_ID (19, fxDelayFeedback);
ABD_CHECK_MOD_DEST_ID (20, resonatorParity);
ABD_CHECK_MOD_DEST_ID (21, resonatorShift);
ABD_CHECK_MOD_DEST_ID (22, resonatorRolloff);
ABD_CHECK_MOD_DEST_ID (23, oscExciteNoise);
ABD_CHECK_MOD_DEST_ID (24, excitationColor);
ABD_CHECK_MOD_DEST_ID (25, impulseMix);
ABD_CHECK_MOD_DEST_ID (26, resonatorRes);
ABD_CHECK_MOD_DEST_ID (27, unisonDetune);
ABD_CHECK_MOD_DEST_ID (28, morphZ);
ABD_CHECK_MOD_DEST_ID (29, morphZ2);
ABD_CHECK_MOD_DEST_ID (30, morphZ3);

#undef ABD_CHECK_MOD_DEST_ID

/** @brief Modulation destinations in preset-index order (see ModDestination).

    Los NOMBRES viven en ModDestinationTable.h, que es una tabla `constexpr` sin
    includes. Este vector se construye a partir de ella, asi que la firma y
    todos los consumidores siguen igual, pero ahora los nombres estan en un sitio
    donde el motor puede usarlos en un static_assert: ese es el punto de que la
    tabla sea constexpr y no un `std::vector` construido a mano. */
inline const std::vector<ModDestination>& getModDestinationTable()
{
    static const std::vector<ModDestination> table (std::begin (kModDestinationTable),
                                                     std::end (kModDestinationTable));
    return table;
}

/** @brief Engine selector labels, in APVTS index order (also preset state). */
inline juce::StringArray getEngineChoiceLabels()
{
    return { "NEURONiK", "Neurotik" };
}

/** @brief Destination labels in index order, derived from the table above. */
inline juce::StringArray getModDestinations()
{
    juce::StringArray labels;

    for (const auto& destination : getModDestinationTable())
        labels.add (destination.label);

    return labels;
}

inline juce::StringArray getModSources()
{
    // ENV 1 (amplitud) y ENV 2 (filtro) al FINAL: los choice se guardan por
    // indice y anadir en medio re-mapearia los presets guardados (6/7 son
    // indices nuevos, nunca usados antes).
    return { "Off", "LFO 1", "LFO 2", "Pitch Bend", "Mod Wheel", "Aftertouch",
             "ENV 1", "ENV 2" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::masterLevel, "Master Level", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::masterBPM, "Master BPM", juce::NormalisableRange<float>(20.0f, 400.0f), 120.0f));
    
    const auto engines = getEngineChoiceLabels();
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::engineType, "Engine Type", engines, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscLevel, "Osc Level", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscInharmonicity, "Inharmonicity", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscRoughness, "Roughness", juce::NormalisableRange<float>(0.0f, 0.5f), 0.0f)); // Range reduced
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphX, "Morph X", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphY, "Morph Y", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // Morph Z (FASE 10): eje temporal sobre los frames del modelo. Default 0.0
    // = frame canonico => bit-compatible con todo el legado (paridad A-E).
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphZ, "Morph Z", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // FASE 11.3: los z de las capas 1 y 2 (mismo rango y mismo default que
    // morphZ). Con un modelo de una capa no llegan al sonido.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphZ2, "Morph Z 2", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphZ3, "Morph Z 3", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscExciteNoise, "Excite Noise", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::excitationColor, "Excite Color", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::impulseMix, "Impulse Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    // Bow (arco continuo): default 0.0 = comportamiento bit-compatible con
    // lo existente (impulso+ruido); con >0 el sostenido canta con ganancia
    // plena (modo arco), arreglando el gain staging de las notas largas.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscExciteBow, "Bow Excite", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorRes, "Res Bank Resonance", juce::NormalisableRange<float>(0.5f, 1.0f, 0.0f, 0.4f), 0.99f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::unisonDetune, "Spectral Detune", juce::NormalisableRange<float>(0.0f, 0.1f, 0.0f, 0.5f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::unisonSpread, "Spectral Spread", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::unisonEnabled, "Spectral Unison", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envAttack, "Attack", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envDecay, "Decay", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envSustain, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envRelease, "Release", juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.5f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterCutoff, "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.3f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterRes, "Resonance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterAttack, "Filter Attack", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterDecay, "Filter Decay", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterSustain, "Filter Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterRelease, "Filter Release", juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.5f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorRolloff, "Harmonic Roll-off", juce::NormalisableRange<float>(0.1f, 4.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorParity, "Odd/Even Balance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorShift, "Spectral Shift", juce::NormalisableRange<float>(0.5f, 2.0f, 0.0f, 0.5f), 1.0f));
    // --- EL BUS DE LOS CUATRO HUECOS (2026-09-29) --------------------------
    // Que efectos hay y como se llaman sale del CATALOGO (`FxCatalogue.h`), no
    // de una lista escrita aqui: con dos copias, el desplegable del host y el
    // modulo siempre se separan, y el que se olvide de actualizar es el que no
    // da error. La lista del desplegable es el `displayName` del motor.
    //
    // LA MISMA LISTA PARA LOS CUATRO, y no cuatro desplegables con la misma
    // lista: el indice es el indice del catalogo, y el hueco elige el suyo con
    // el default. Cuatro listas serian cuatro sitios donde se puede olvidar uno
    // y que sus indices dejen de hablar de lo mismo.
    juce::StringArray fxTypes { "Bypass" };

    for (int i = 1; i <= NEURONiK::DSP::fxNeuronikCatalogueSize(); ++i)
    {
        const auto entry = NEURONiK::DSP::fxNeuronikEffectAt (i);

        if (entry.effect != nullptr)
            fxTypes.add (juce::String (entry.effect->displayName));
    }

    // Un hueco es un tipo, una ganancia, una mezcla y doce mandos. Los CUATRO
    // huecos son esa misma frase con otro numero delante, asi que se escribe una
    // vez y se recorre: sesenta `push_back` escritos son sesenta sitios donde el
    // hueco 3 puede acabar con trece mandos y el 4 con once, y el que se
    // equivoque no da error, da un hueco con un knob de mas.
    for (int slot = 0; slot < NEURONiK::DSP::kFxBusSlots; ++slot)
    {
        const juce::String etiqueta { "FX " + juce::String (slot + 1) };

        // El indice 0 es bypass y el resto va en el MISMO orden que el catalogo,
        // porque `typeForSlot`/`setSlotType` hablan ese indice. Elegir por indice
        // y no por nombre es lo que deja que el hueco cambie de efecto sin que el
        // APVTS tenga que saber nombres.
        //
        // Y el default sale de `fxDefaultTypeForSlot`, la MISMA tabla que usa el
        // motor al preparar. Si los dos tuvieran numeros distintos, un preset
        // nuevo sonaria distinto de lo que acaba de guardar el host, y no habria
        // ningun error.
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            fxBusFieldId (slot, "Type"), etiqueta + " Type", fxTypes,
            NEURONiK::DSP::fxDefaultTypeForSlot (slot)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            fxBusFieldId (slot, "Gain"), etiqueta + " Gain",
            juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            fxBusFieldId (slot, "Mix"), etiqueta + " Mix",
            juce::NormalisableRange<float>(0.0f, 1.0f),
            NEURONiK::DSP::fxDefaultSlotMix (slot)));

        // Los doce mandos del hueco van NORMALIZADOS 0..1, y no en las unidades
        // fisicas del efecto, y no por gusto: el RANGO depende de que efecto
        // este puesto, y el hueco lo cambia el usuario en caliente. Un parametro
        // del APVTS tiene un rango fijo para siempre (un preset lo guarda, el
        // host lo automatiza), asi que publicar "Drive 1..8" seria mentir para
        // los otros siete efectos del catalogo. El hueco habla normalizado
        // (`FxSlot::setParameter` toma 0..1) y el sesgo lo aplica la fila. El
        // panel web, que SI sabe que efecto hay puesto, pone la etiqueta y las
        // unidades leyendo el catalogo exportado.
        //
        // El default sale del PRODUCTO, de `fxDefaultSlotParam`, que es la
        // cadena de siempre (ver alli por que no sale de la fila). Los mandos
        // que la fila del hueco no usa se quedan en 0.5, que es lo que pone
        // `FxSlotParams` y no un extremo.
        for (int i = 0; i < NEURONiK::DSP::kFxBusParams; ++i)
            params.push_back(std::make_unique<juce::AudioParameterFloat>(
                fxBusParamId (slot, i), etiqueta + " Param " + juce::String (i + 1),
                juce::NormalisableRange<float>(0.0f, 1.0f),
                NEURONiK::DSP::fxDefaultSlotParam (slot, i)));
    }

    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxDelayTime, "Delay Time", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.5f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxDelayFeedback, "Delay FB", juce::NormalisableRange<float>(0.0f, 0.95f), 0.4f));
    
    juce::StringArray lfoSyncModes = { "Free", "Tempo Sync" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::fxDelaySync, "Delay Sync", lfoSyncModes, 0));
    juce::StringArray rhythmicDivisions = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4t", "1/8t", "1/16t" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::fxDelayDivision, "Delay Division", rhythmicDivisions, 2));

    // Chorus. El techo del `rate` es 8 Hz y no 10 porque el tope ahora lo pone
    // la FILA del catalogo de huecos (DspEffects/adapters/BasicAdapters.h), que
    // es donde vive la tabla: medido, el motor modula 5..30 ms de retardo y a
    // 10 Hz la linea da una vuelta cada 100 muestras, o sea un tremolo. Dejar el
    // panel en 10 seria un tramo de recorrido sin efecto en el extremo.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxChorusRate, "Chorus Rate", juce::NormalisableRange<float>(0.1f, 8.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxChorusDepth, "Chorus Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxChorusMix, "Chorus Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Reverb
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxReverbSize, "Reverb Size", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxReverbDamping, "Reverb Damping", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxReverbWidth, "Reverb Width", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxReverbMix, "Reverb Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::midiThru, "MIDI Thru", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::randomStrength, "Random Strength", 0.0f, 1.0f, 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::freezeResonator, "Freeze Resonator", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::freezeFilter, "Freeze Filter", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::freezeEnvelopes, "Freeze Envelopes", false));
    
    juce::StringArray curves = { "Linear", "Soft", "Hard" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::velocityCurve, "Velocity Curve", curves, 0));

    juce::StringArray midiChannels = { "Omni", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::midiChannel, "MIDI Channel", midiChannels, 0));

    juce::StringArray lfoWaveforms = { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random S&H" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1Waveform, "LFO 1 Wave", lfoWaveforms, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo1RateHz, "LFO 1 Rate", juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.5f), 1.0f));
    lfoSyncModes = { "Free", "Tempo Sync" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1SyncMode, "LFO 1 Sync", lfoSyncModes, 0));
    rhythmicDivisions = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4t", "1/8t", "1/16t" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1RhythmicDivision, "LFO 1 Div", rhythmicDivisions, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo1Depth, "LFO 1 Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2Waveform, "LFO 2 Wave", lfoWaveforms, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo2RateHz, "LFO 2 Rate", juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2SyncMode, "LFO 2 Sync", lfoSyncModes, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2RhythmicDivision, "LFO 2 Div", rhythmicDivisions, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo2Depth, "LFO 2 Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    // Derived from the one table (getModDestinationTable), never re-typed here: a
    // destination added there shows up in every mod slot, in the same order.
    const auto modDestinations = getModDestinations();
    // SSOT: la lista de fuentes sale de getModSources() (8 items, con las ENV
    // al final): los defaults 6/7 de abajo son indices VALIDOS de esta lista.
    const juce::StringArray modSources = getModSources();

    // Defaults del PRESET NUEVO (las envolventes viajan por la matriz):
    // mod1 = ENV 1 -> Osc Level (profundidad nominal 1.0) y mod2 = ENV 2 ->
    // Filter Cutoff (1.0). Indices de choice: env1=6, env2=7 en getModSources();
    // Osc Level=1, Filter Cutoff=10 en getModDestinationTable(). Los presets
    // guardados llegan por PresetMigration, que inserta las mismas rutas en
    // una ranura libre (o no toca nada si no hay sitio: mismo sonido).
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod1Source, "Mod 1 Source", modSources, 6));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod1Destination, "Mod 1 Dest", modDestinations, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod1Amount, "Mod 1 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod2Source, "Mod 2 Source", modSources, 7));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod2Destination, "Mod 2 Dest", modDestinations, 10));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod2Amount, "Mod 2 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod3Source, "Mod 3 Source", modSources, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod3Destination, "Mod 3 Dest", modDestinations, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod3Amount, "Mod 3 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod4Source, "Mod 4 Source", modSources, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod4Destination, "Mod 4 Dest", modDestinations, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod4Amount, "Mod 4 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    return { params.begin(), params.end() };
}

} // namespace NEURONiK::State
