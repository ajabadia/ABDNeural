/*
  ==============================================================================

    FxCatalogue.h
    QUE EFECTOS HAY EN NEURONiK, con la identidad que habla el VOCABULARIO
    COMPARTIDO. Namespace NEURONiK::DSP.

    QUE ES. El motor de los efectos es del modulo compartido y ya esta montado
    (`FxEngine` + `fxDefaultCatalogue()`, en `FxSlots.h`): seis motores, JUCE-free,
    que es lo que sostiene la paridad nativa <-> WASM. Lo que NO existia era la
    IDENTIDAD de cada uno fuera de C++: un nombre tecnico, un `displayName` y su
    posicion en una tabla. Sin eso la pagina no puede pintar un hueco de efecto
    sin escribir a mano la lista de los seis, y en cuanto el catalogo crezca
    habria dos listas y la que se olvide.

    POR QUE HAY UN `id` COMPARTIDO Y NO EL NOMBRE. El contrato de efectos
    compartido (`ABDSharedAssets/contracts/fx-effects.json`, 57 ids: 0 es bypass
    y 1..56 son efectos) es el sitio donde ya viven el nombre y la FAMILIA de cada
    efecto, y la familia es lo que decide el tema del modulo (`fxTheme.js`, once
    temas, no cincuenta y siete). El identificador de una fila del motor
    (`FxEffectInfo`) es tecnico, sin espacios, y solo se busca por
    `fxFindEffect`; no es un id que ninguna otra superficie pueda citar. Traer el
    `id` compartido aqui es lo que hace que la pagina, el tema y (el dia que
    quiera) otro producto hablen el mismo numero para el mismo efecto.

    Y LA COLUMNA `params` DEL CONTRATO NO ES TRASLADABLE. Dice cuantos mandos
    ACEPTA LA IMPLEMENTACION DE ABDEep, y las implementaciones no son las mismas:
    ahi el coro tiene 11 y el delay 12, mientras que los motores de este
    catalogo tienen 2, 2, 4, 1, 4 y 4. El numero que vale para pintar es el del
    motor (`FxEffectInfo::numParams`), y por eso la exportacion lo lee de ahi y no
    del JSON. Es el mismo descuido que el contrato ya documenta dos veces sobre
    los NOMBRES: una tabla compartida describe el vocabulario, no la
    implementacion de cada producto.

    LAS OCHO FILAS ESTAN ALINEADAS. Los ocho motores tienen fila propia en el
    contrato compartido:

        chorus     -> 10  "Stereo Chorus"   misma clase (Chorus)
        delay      -> 13  "Delay"           misma clase (Delay)
        reverb     -> 57  "FreeVerb Reverb"  motor compartido (Reverb)
        saturation -> 58  "Saturation"       motor compartido (Saturation)
        schroeder  -> 59  "Schroeder Reverb" motor compartido (SchroederReverb)
        bbd        -> 36  "BBD Chorus"      misma clase (RolandBBDChorus)
        shelf      -> 60  "Shelf Filter"     motor compartido (ShelfFilter)
        phaser     ->  9  "Stereo Phaser"   misma clase (Phaser)

    HASTA 2026-09-29 ESTA TABLA TENIA SEIS FILAS Y EL CATALOGO COMPARTIDO, OCHO,
    Y ESO NO DABA UN AVISO: daba un catalogo VACIO. `fxNeuronikEffectAt` exige
    que las dos tablas midan lo mismo (es la comprobacion que esta misma
    cabecera pide, mas abajo), asi que con 6 contra 8 devolvia `{nullptr,
    nullptr}` para CADA indice. El desplegable de tipo del hueco se quedaba con
    una sola entrada, "Bypass" --porque `ParameterDefinitions.h` solo anade un
    efecto cuando la fila no es nula--, con un indice por defecto (4) que no
    existia en la lista. Ni error, ni excepcion, ni motor mal: un catalogo de
    un solo efecto que era el de ninguno. Las dos filas que faltaban eran las
    dos ULTIMAS del modulo (repisa y phaser), las que estan al final porque son
    las menos usuales, y por eso se nota al final de la lista y no en la
    primera fila.

    LAS DOS FILAS NUEVAS TOMARON SU ID DEL CONTRATO, Y UNA LO PIDIO. El phaser
    es el 9, "Stereo Phaser", cuya `engine` es la clase `Phaser`: es el mismo
    motor que el nuestro (`Phaser4<4>`), asi que la fila dice la verdad y el
    test de paridad la puede comprobar. La repisa no tenia fila, porque el
    contrato lo genera la fabrica de ABDEep y alli no hay ningun filtro de
    repisa; la fila 60 la produce este modulo, como las 57..59, y su `engine`
    es el nombre de la clase real (`ShelfFilter`). Reservar un id ajeno habria
    sido mas corto, y es el parche que este mismo fichero critica arriba.

    HASTA 2026-09-29 LOS ULTIMOS TRES NO TENIAN FILA, y ocupaban ids ajenos como
    RESERVA: el 1 ("Hall"), el 50 ("Oversampled Dist") y el 22 ("Deep Verb"). Una
    reserva es un parche: funciona, pero significa "aqui vendria esto" sobre un id
    que hoy habla de otra cosa, y el aviso leeria el nombre del efecto equivocado.
    El contrato gano las tres filas y los ids se movieron, que es lo que la
    cabecera pedia: cambiar el flag Y el numero, en ESTE fichero y en ningun otro
    sitio, y dejar que el test de paridad (que si lee el JSON de verdad) lo
    compruebe.

    Los ids 57..59 los produce `ABDSharedCode/DspEffects`, no la fabrica de
    ABDEep: son las primeras filas del contrato que no salen de `generatedFrom`.
    Por eso son AL FINAL del array y por eso no se pueden insertar en su sitio por
    familia sin renumerar el resto.

  ==============================================================================
*/

#pragma once

#include "DspEffects/FxDefaultCatalogue.h"
#include "DspEffects/FxRegistry.h"

#include "DspTypes.h"

namespace NEURONiK::DSP
{

//==============================================================================
/**
    La identidad compartida de un efecto del catalogo.

    @param id        el id en el vocabulario compartido (0 = bypass)
    @param name      como se llama en ESE vocabulario
    @param family    la familia del tema (`fx-effects.json` -> `family`)
    @param aligned   true si el contrato compartido tiene una fila para este motor
                     con ese id. Las ocho la tienen hoy; el campo sigue en la
                     tabla porque un motor nuevo sin fila todavia necesita un id
                     que no sea el de otro efecto, y ese caso tiene que verse
                     MARCADO en la pagina y no solo en un comentario (ver la
                     cabecera).

    POR QUE EL NOMBRE COMPARTE ESTA TABLA Y NO SE SACA DEL JSON. El generador es
    un ejecutable de C++ que no puede leer el fichero de otro repositorio sin
    anadir una dependencia de rutas, y el nombre de un efecto es POLITICA DE
    PRODUCTO: lo que la pagina pinta es el `displayName` del motor. El nombre
    compartido viaja entonces como una COLUMNA mas, y es el test de paridad (que
    si lee el JSON de verdad) quien obliga a que las dos copias digan lo mismo.
*/
struct FxSharedIdentity
{
    int         id;
    const char* name;
    const char* family;
    bool        aligned;
};

//==============================================================================
/**
    Las identidades, en el MISMO orden que `abd::dsp::fxDefaultCatalogue()`.

    Y EN EL MISMO ORDEN, que es la parte que no se ve y es la que importa: esta
    tabla se une por indice con la del modulo compartido, asi que una fila
    insertada en una de las dos y no en la otra asignaria a un motor el nombre y
    la familia de otro, y el sintoma seria un modulo con el tema equivocado y
    los knobs del efecto equivocado. Por eso el test de paridad comprueba el
    emparejamiento uno a uno (cada motor con SU id y SU familia), no solo que
    las dos tablas tengan el mismo tamano: dos tablas del mismo tamano y
    distinta permutacion pasan cualquier comprobacion de recuento.
*/
inline const FxSharedIdentity* fxNeuronikIdentities (int& count) noexcept
{
    static const FxSharedIdentity table[] = {
        {  10, "Stereo Chorus",      "chorus",     true  },   // chorus
        {  13, "Delay",              "delay",      true  },   // delay
        {  57, "FreeVerb Reverb",    "reverb",     true  },   // reverb
        {  58, "Saturation",         "distortion", true  },   // saturation
        {  59, "Schroeder Reverb",   "reverb",     true  },   // schroeder
        {  36, "BBD Chorus",         "chorus",     true  },   // bbd
        {  60, "Shelf Filter",       "filter",     true  },   // shelf
        {   9, "Stereo Phaser",      "modulation", true  },   // phaser
    };

    count = static_cast<int> (sizeof (table) / sizeof (table[0]));
    return table;
}

//==============================================================================
/**
    Que efecto va de serie en cada hueco: la CADENA POR DEFECTO del producto.

    Vive aqui y no en `FxSlots.h` porque lo necesitan los DOS lados y tienen que
    ser el mismo numero: el APVTS lo usa para el valor por defecto de `fx1Type`
    (que es lo que hace que un preset nuevo suene como el anterior y no en
    silencio), y el motor lo usa al preparar. Con el numero en dos sitios, un
    hueco recien migrado arrancaria en bypass o con el efecto equivocado, y
    ninguno de los dos fallos suena a error: suenan a "no suena" o a "suena
    distinto".

    Los indices son del CATALOGO + 1, porque el 0 es bypass en `fxEffectAt`. El
    orden es el de la cadena de siempre: saturacion, coro, retardo,
    reverberacion.
*/
inline constexpr int fxDefaultTypeForSlot (int slot) noexcept
{
    switch (slot)
    {
        case 0:  return 4;   // saturacion
        case 1:  return 1;   // coro
        case 2:  return 2;   // retardo
        case 3:  return 3;   // reverberacion
        default: return 0;   // bypass
    }
}

/** La misma tabla, como puntero y tamano. */
inline const FxSharedIdentity* fxNeuronikIdentities() noexcept
{
    int count = 0;
    return fxNeuronikIdentities (count);
}

/** Cuantas filas tiene el catalogo de NEURONiK. */
inline int fxNeuronikCatalogueSize() noexcept
{
    int count = 0;
    (void) fxNeuronikIdentities (count);
    return count;
}

//==============================================================================
/**
    El catalogo de NEURONiK: el motor de la fila MAS su identidad compartida,
    ya emparejados. El indice 0 es SIEMPRE bypass, como en `fxEffectAt`, para que
    el panel pueda elegir "nada" sin que ningun producto tenga que acordarlo.

    @param index  0 = bypass; 1..N = el indice de la tabla + 1
    @returns      la fila, o nullptr si el indice no vale
*/
struct FxCatalogueEntry
{
    const abd::dsp::FxEffectInfo* effect;    ///< la fila del modulo compartido
    const FxSharedIdentity*       identity;  ///< su id y su familia
};

inline FxCatalogueEntry fxNeuronikEffectAt (int index) noexcept
{
    if (index <= 0)
        return { nullptr, nullptr };

    int identityCount = 0;
    const auto* identities = fxNeuronikIdentities (identityCount);
    int effectCount = 0;
    const auto* effects = abd::dsp::fxDefaultCatalogue (effectCount);

    // Las dos tablas tienen que medir lo mismo. Si un motor entra en el modulo
    // compartido y aqui no se le anade su identidad, la pagina lo receberia sin
    // id ni familia: se veria, pero el fallo se nota tarde. Se comprueba al
    // mirar el catalogo, no solo en un test, porque esta funcion se llama al
    // pintar.
    if (identityCount != effectCount)
        return { nullptr, nullptr };

    const auto* effect = abd::dsp::fxEffectAt (effects, effectCount, index);

    return { effect, effect != nullptr ? &identities[index - 1] : nullptr };
}

//==============================================================================
/**
    La fila del catalogo que va de serie en un hueco, o nullptr si el indice no
    vale. Es `fxNeuronikEffectAt (fxDefaultTypeForSlot (slot))` sin repetir la
    cuenta, y la usan el APVTS (para el default de los mandos), el motor (para
    `setType` al preparar) y la migracion de presets (para las unidades de la
    fila). Tres sitios, un numero.
*/
inline const abd::dsp::FxEffectInfo* fxNeuronikDefaultRow (int slot) noexcept
{
    return fxNeuronikEffectAt (fxDefaultTypeForSlot (slot)).effect;
}

//==============================================================================
//==============================================================================
// LOS FISICOS DE LA CADENA DE SIEMPRE, hueco a hueco. Solo llegan hasta el
// numero de mandos que tiene la fila; lo que sobra lo cubre el 0.5 de
// `fxDefaultSlotParam`, que es el mismo que pone `FxSlotParams`.
//
// Vive a nivel de cabecera, y no como `static` dentro de `fxDefaultSlotParam`,
// por una razon concreta: `fxDefaultSlotDelaySeconds` tiene que LEER el retardo
// de aqui para que el 0,3 de esta tabla y el que usa el test sean el MISMO
// numero y no dos copias que hoy coinciden. Con la tabla dentro de la funcion no
// se puede leer, y con dos copias el retardo se separa el dia que se cambie uno,
// sin avisar.
inline constexpr float kFisicos[kFxBusSlots][kFxBusParams] = {
    { 2.0f },                              // saturacion: drive
    { 1.0f, 0.2f },                        // coro: 1 Hz, 0.2
    { 0.3f, 0.4f },                        // retardo: 0.3 s, 0.4
    { 0.5f, 0.5f, 1.0f, 0.0f }            // reverb: size, damping, width, levels
};

/**
    El retardo por defecto del hueco 3, en SEGUNDOS, y con el hueco al que
    pertenece.

    POR QUE HACE FALTA, Y POR QUE NO BASTA CON `GlobalParams::delayTime`. Ese
    campo existe y vale 0.3, pero es el retardo que le PUEDE LEGER el host: lo
    rellena `NEURONiKProcessor` desde el APVTS cuando el tiempo viene en free y
    no en Tempo Sync. El motor, al que se le conduce sin host, saca el retardo
    del hueco por su bus (`kFisicos[2][0]`), y ese es el que de verdad suena.

    Son DOS 0,3 que hoy coinciden, y por eso el test del destino 17 no lo
    delata: derivando las ventanas de `GlobalParams::delayTime` sale el mismo
    numero, y no se ve que sean dos cosas distintas. El dia que uno se cambie y el
    otro no, el test estaria midiendo una cola calculada de un retardo que no
    es el que suena, y el destino 18 --Delay Time-- no volveria a sonar sin
    que nada fallara. Que el retardo salga de aqui es lo que ata el test a lo
    que el motor hace.

    @param slot  el hueco; solo el 3 tiene retardo
    @returns      los segundos, o 0 si ese hueco no es el del retardo
*/
constexpr float fxDefaultSlotDelaySeconds (int slot) noexcept
{
    if (slot != 2)          // el 3, que es donde va el retardo en la cadena
        return 0.0f;

    // El 0,3 de la TABLA, no una copia. Es el mismo numero por construccion, no
    // por casualidad: es la razon de que la tabla viva fuera de la funcion.
    return kFisicos[2][0];
}

/**
    El mando `param` (base 0) del hueco `slot` con su valor POR DEFECTO, ya
    NORMALIZADO 0..1. Es lo que declara el APVTS y lo que lleva un preset nuevo.

    Y POR QUE ESTOS NUMEROS Y NO LOS DE LA FILA, que es la pregunta que hace
    falta responder primero. La fila sabe lo que el efecto necesita para sonar
    (la saturacion nace en drive 2, la reverb en size 0.5) pero NO sabe lo que
    es el PRESET NUEVO de este producto, que es otra cosa: es la cadena de
    siempre con sus cuatro mezclas, y dos de esas mezclas NO son el default de
    su fila. El retardo, por ejemplo, nacio siempre al 50 % en paralelo
    (`FxSlots.h` explica que baja la seca 6 dB y por que se acepta), y el
    default de un mando de mezcla es 0. Con el default de la fila el plugin
    nuevo saldria con el retardo mudo, que es un cambio de sonido en cada
    instalacion y no en cada preset.

    LOS NUMEROS ESTAN EN FISICOS y se normalizan con la fila, por el mismo
    motivo que en `PresetMigrationFx.cpp`: el normalizado de una fila y el de un
    parametro del APVTS no son el mismo numero (el coro iba 0.1..8 Hz con sesgo
    0.5 y la fila lo hace con 0.55), asi que escribir el normalizado a mano
    seria dejar el mando de sitio el dia que la fila cambie de sesgo.

    QUE ES, NUMER A NUMER, LA CADENA DE ANTES: cada valor es el default del
    mando plano que el hueco tenia (`fxChorusRate` a 1 Hz, `fxDelayTime` a
    0.3 s, `fxReverbSize` a 0.5...), asi que un preset nuevo suena igual que
    antes de exponer el bus. `FxSlotsTest` lo mide etapa a etapa.
*/
inline float fxDefaultSlotParam (int slot, int param) noexcept
{
    const auto* row = fxNeuronikDefaultRow (slot);

    if (row == nullptr || row->params == nullptr)
        return 0.5f;

    if (param < 0 || param >= kFxBusParams)
        return 0.5f;

    // Un mando que la fila de este hueco NO tiene se queda en el centro y no
    // en un extremo: la fila solo lee los suyos, y un 0 o un 1 aqui no lo lee
    // nadie, pero el dia que el hueco cambie de tipo sin que el preset se
    // vuelva a guardar ese numero es el primero que veria la fila nueva.
    if (param >= row->numParams)
        return 0.5f;

    return abd::dsp::fxNormalise (row->params[param], kFisicos[slot][param]);
}



//==============================================================================
/**
    La mezcla por defecto de un hueco: la del BUS del hueco, no la del motor.

    Son cuatro numeros escritos a mano porque son lo unico de la cadena que es
    de este producto y no de la fila: el retardo al 50 % es una decision de
    sonido (razonada en `FxSlots.h`) y los otros tres a cero son el bypass de
    siempre. `kFisicos` de arriba no puede hacerlos porque `mix` no es un mando
    de la fila: vive en el hueco, no en el motor.
*/
inline float fxDefaultSlotMix (int slot) noexcept
{
    static constexpr float kPorHueco[kFxBusSlots] = { 0.0f, 0.0f, 0.5f, 0.0f };
    return (slot >= 0 && slot < kFxBusSlots) ? kPorHueco[slot] : 0.0f;
}

//==============================================================================
/**
    El ancho del bus de la pagina, frente a lo que pide el catalogo de verdad.

    `kFxBusParams` (DspTypes.h) es un numero escrito a mano, y un numero escrito
    a mano que se queda corto no falla: el hueco acepta 4 mandos, el motor pide
    5, y el quinto se pierde en silencio con el slot sonando raro.

    POR QUE ESTO NO ES UN `static_assert`, que es lo que se querria. Para que un
    `static_assert` pueda leerlo, la funcion tiene que ser `constexpr`, y el
    catalogo NO puede serlo: las tablas de los efectos las construyen funciones
    de los adaptadores (`ChorusFx::specs()` y.companyas), no literales, asi que
    `numParams` no es una expresion constante. Se probo y no compila
    (C3615), que es la forma que tiene el compilador de decir "esto no se puede
    saber al compilar".

    Asi que la comprobacion vive en `Tests/FxCatalogueTest.cpp`, que mide lo
    mismo (`el motor mas ancho pide 4 mandos` y `el ancho cabe en el bus del
    motor`). Se pierde el aviso en tiempo de compilacion y no se pierde el
    fallo: un motor mas ancho rompe el test, que corre en cada build.
*/
inline int fxWidestParamCount() noexcept
{
    int widest = 0;

    for (int i = 1; i <= fxNeuronikCatalogueSize(); ++i)
    {
        const auto entry = fxNeuronikEffectAt (i);

        if (entry.effect != nullptr && entry.effect->numParams > widest)
            widest = entry.effect->numParams;
    }

    return widest;
}

// Este SI puede ser constante: los dos numeros son enteros escritos en el
// codigo, no salen de una tabla. Si el modulo compartido cambiara su numero de
// huecos, el bus se quedaria con un hueco de mas o de menos y esto lo dice al
// compilar.
static_assert (kFxBusSlots == abd::dsp::kFxNumSlots,
               "el bus tiene un hueco mas o menos de los que tiene el motor");
static_assert (kFxBusParams <= abd::dsp::kFxMaxParams,
               "el bus es mas ancho que lo que el motor acepta");

} // namespace NEURONiK::DSP
