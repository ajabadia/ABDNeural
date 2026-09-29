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

    LAS TRES FILAS SIN ALINEAR. De los seis motores, TRES tienen fila propia en el
    contrato compartido y TRES NO:

        chorus     -> 10  "Stereo Chorus"   misma clase (Chorus)
        delay      -> 13  "Delay"           misma clase (Delay)
        bbd        -> 36  "BBD Chorus"      misma clase (RolandBBDChorus)
        reverb     -> (1)  la reverb de este catalogo es FreeVerb; el contrato no
                          lista FreeVerb. Se RESERVA el 1 ("Hall") como sitio
                          donde caer, sin declarar que sean el mismo motor.
        saturation -> (50) no hay saturacion de una banda en el contrato; el 50 es
                          "Oversampled Dist" y el 51 "Wave Shaper". Se RESERVA el
                          50, que es la familia (distortion) correcta.
        schroeder  -> (22) un reverberador de Schroeder no esta en el contrato.
                          Se RESERVA el 22 ("Deep Verb"), familia reverb.

    Una reserva NO es una equivalencia: mientras `aligned` sea falso, el id esta
    ocupado por una reserva declarada aqui y no por una fila del contrato. Por eso
    el campo viaja a la pagina: un hueco con una reserva se puede pintar igual,
    pero la interfaz sabe que no es una fila del vocabulario. Cuando el contrato
    compartido gane esas tres filas, se cambia el flag y el numero en ESTE
    fichero — un solo sitio — y el test de paridad (que lee el JSON de verdad)
    pasa a exigir la coincididencia.

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
    @param aligned   true si el contrato compartido tiene YA una fila para este
                     motor con ese id; false si el id es una RESERVA declarada
                     aqui (ver la cabecera)

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
        {   1, "Hall",               "reverb",     false },   // reverb     RESERVA
        {  50, "Oversampled Dist",   "distortion", false },   // saturation RESERVA
        {  22, "Deep Verb",          "reverb",     false },   // schroeder  RESERVA
        {  36, "BBD Chorus",         "chorus",     true  },   // bbd
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
