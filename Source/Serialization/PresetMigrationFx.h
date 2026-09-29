/*
  ==============================================================================

    PresetMigrationFx.h
    Created: 29 Sep 2026
    Description: Convierte los doce ids planos de efectos en el BUS por hueco.

                 Antes del rack, los efectos globales eran doce mandos con
                 nombre fijo (`fxSaturation`, `fxChorusRate`, `fxReverbWidth`...)
                 y el motor los repartia a mano en cuatro huecos. El hueco 1 ya
                 publica un BUS (`fx1Type`, `fx1Gain`, `fx1Mix`, `fx1Param1..4`)
                 y un preset guardado antes de eso trae los doce nombres viejos,
                 que el APVTS ya no reconoce.

                 Esta es la MIGRACION de esa frontera. Vive fuera de
                 `PresetMigration.cpp` a proposito: aquel fichero es JUCE + el
                 arbol de preset y no sabe nada de efectos, y este necesita la
                 tabla de las filas del catalogo (`FxCatalogue.h`), que es
                 JUCE-free. Un solo fichero con las dos cosas obligaria a uno de
                 los dos targets a enlazar lo que el otro no necesita.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace NEURONiK::Serialization
{

/**
 * @brief Reescribe los ids planos de efectos de un preset al bus del hueco que les toca.
 * @details Solo migra los huecos que el plugin publica HOY: cada fila de la tabla
 *          lleva el id de destino, y si ese id no esta en el layout la fila no se
 *          aplica. Asi los doce ids tienen su sitio escrito desde ya, y migrar el
 *          hueco 2 dentro de un mes es cambiar el layout, no tocar este fichero:
 *          la fila del coro se arma sola, sin tocar el resto.
 *
 *          LA CONVERSION PASA POR UNIDADES FISICAS y no copia el numero. Un
 *          preset guarda el valor NORMALIZADO del APVTS viejo, y el bus guarda
 *          el normalizado de la FILA, y los dos normalizados no son el mismo
 *          numero: el coro iba 0.1..8 Hz con sesgo 0.5 en el APVTS y la fila lo
 *          hace con sesgo 0.55, y el retardo sesgo 0.5 contra 0.30. Copiar el
 *          valor habria movido el mando de sitio al migrar, que es el fallo
 *          invisible de esta clase de migracion. Se desnormaliza con el rango
 *          viejo, se renormaliza con la fila.
 *
 *          LO QUE NO SE MIGRA, y es a proposito: `fxDelaySync` y
 *          `fxDelayDivision` no son mandos del motor de retardo (el motor de
 *          huecos no tiene sync), los resuelve el host. Cuando su destino sea el
 *          hueco 3, el tiempo del retardo tiene que CRUZAR esa resolucion: un
 *          preset con sync puesto guardaba un `fxDelayTime` que el host
 *          IGNORABA, y migrar ese valor seria escribir un tiempo que no es el
 *          que sonaba. Por eso la fila del retardo lee el sync, y con sync
 *          puesto calcula los segundos con la division y el bpm del preset
 *          (la MISMA cuenta que hace `NEURONiKProcessor::fillGlobalParams`).
 *
 * @returns el numero de `<PARAM>` escritos.
 */
int migrateFlatFxToSlotBus (juce::ValueTree& state, const juce::AudioProcessor& processor);

/**
 * @brief Los ids planos que esta migracion sabe convertir, para diagnostico y tests.
 * @details La lista esta CONGELADA a proposito: es la foto de lo que habia antes
 *          del rack. Un id que se anada ahi sin que exista todavia en el layout
 *          hace que la fila no se aplique, que es el comportamiento correcto, y
 *          uno que se OLVIDE hace que un preset con ese id lo pierda en silencio.
 */
juce::StringArray retiredFlatFxParameterIds();

} // namespace NEURONiK::Serialization
