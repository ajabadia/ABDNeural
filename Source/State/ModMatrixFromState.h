/*
  ==============================================================================

    ModMatrixFromState.h
    Created: 28 Sep 2026
    Description: LA TRADUCCION DE LA MATRIZ, en un solo sitio.

                 Un preset no guarda la matriz de modulacion: guarda tres
                 parametros por ranura (`mod3Source`, `mod3Destination`,
                 `mod3Amount`) y el motor recibe tres numeros. El camino de
                 punta a punta son tres traduciones distintas, y hasta ahora
                 cada una estaba escrita por su cuenta:

                   preset (XML)  ->  ValueTree  ->  GlobalParams  ->  la voz

                 El trozo que se duplicaba era el ultimo: el nombre del
                 parametro ("mod" + ranura + campo) y el orden en que los tres
                 campos se leen. NEURONiKProcessor lo hacia con getRawParameterValue
                 (los atomicos vivos, que es lo correcto: el arbol puede ir
                 delay) y Tests/PresetMigrationParity lo hacia con el ValueTree
                 (que es lo que tiene un preset guardado). Misma politica,
                 dos copias.

                 El resultado era que un test de audio ("migrar este preset no
                 cambia ni una muestra") demostraba la migracion bajo una
                 traduccion escrita DENTRO del test. Si la del producto
                 cambiaba —un id renombrado, dos campos cambiados de orden— el
                 test seguia en verde y el producto roto, porque no eran el
                 mismo codigo.

    ── DONDE ESTA LA FRONTERA, Y POR QUE ────────────────────────────────────
    Aqui viven el NOMBRE de cada campo y el ORDEN en que se leen. En el
    procesador NO viven: ahi solo se leen tres atomicos.

    Eso es deliberado, y es lo que hace que el test siga teniendo sentido. Un
    id de parametro se renombra, y si cada sitio construye su nombre, un
    rename rompe la carga de presets en silencio: el `getRawParameterValue`
    devuelve nullptr y el preset parece vacio. Con el nombre en un solo sitio,
    el rename rompe aqui y en el test a la vez, que es la mitad del motivo por
    el que esto existe.

    Lo que NO se puede compartir es de donde se lee: el procesador lee los
    atomicos vivos del APVTS y el test lee un ValueTree, porque son dos cosas
    distintas por diseno (el arbol va con retardo; por eso el producto usa
    getRawParameterValue y no apvts.state). Por eso la funcion es una plantilla
    sobre un lector, y no una funcion que reciba el estado: la politica —
    nombres, orden, conversiones— es compartida, el almacen no.

  ==============================================================================
*/

#pragma once

#include <array>

#include <juce_core/juce_core.h>

#include "../DSP/DspTypes.h"

namespace NEURONiK::State
{

/** @brief Los tres campos de una ranura de la matriz, en el orden del preset. */
enum class ModField
{
    source,
    destination,
    amount
};

/** @brief Cuantas ranuras tiene la matriz (y el preset, que guarda las mismas). */
inline constexpr int kModMatrixSlots = 4;

/**
 * @brief El id del parametro APVTS de ese campo en esa ranura.
 * @details `mod1Source` … `mod4Amount`. La numeracion de ranura es de UNO, que
 *          es como la escribe el layout y como la que hay guardado en los
 *          presets: `mod0Source` no existe y no debe existir.
 */
inline juce::String modMatrixParameterId (int slotIndex, ModField field)
{
    const char* const suffix = field == ModField::source       ? "Source"
                             : field == ModField::destination ? "Destination"
                                                              : "Amount";
    return juce::String ("mod") + juce::String (slotIndex + 1) + suffix;
}

/**
 * @brief Las cuatro rutas de la matriz, leidas de donde sea.
 * @param readSlot `(slotIndex, field) -> float`. El que decide de donde sale
 *        cada numero es el llamante: el procesador pasa los atomicos del APVTS,
 *        un test puede pasar un preset guardado. Lo que NO se decide fuera de
 *        aqui es que campo va en que miembro ni en que orden se leen.
 */
template <typename ReadSlot>
std::array<DSP::ModRoute, kModMatrixSlots> readModMatrix (ReadSlot&& readSlot)
{
    std::array<DSP::ModRoute, kModMatrixSlots> routes {};

    for (int slot = 0; slot < kModMatrixSlots; ++slot)
    {
        routes[slot].source      = (int) readSlot (slot, ModField::source);
        routes[slot].destination = (int) readSlot (slot, ModField::destination);
        routes[slot].amount      =         readSlot (slot, ModField::amount);
    }

    return routes;
}

} // namespace NEURONiK::State
