/*
  ==============================================================================

    ModulationContractJson.h
    Created: 28 Sep 2026
    Description: LA TABLA DE C++ EN FORMATO DE CONTRATO.

                 El contrato de la matriz vive en ABDSharedAssets
                 (contracts/neuronik_modulation_matrix.json) y lo que el motor
                 hace vive en ABDNeural. Son dos repos y no hay nada que los una:
                 se puede cambiar la tabla de C++ y el JSON se queda como estaba,
                 y la pagina —que lee el JSON— sigue enseñando un destino que el
                 motor ya no modula.

                 Este es el puente por el lado de C++: vuelca las tablas al MISMO
                 formato del contrato, para que la comparacion sea campo a campo
                 y no "mas o menos lo mismo".

                 Devuelve TEXTO, no un `juce::var`, y a proposito. El exportador
                 lo escribe por stdout y el test lo parsea: los dos comparan los
                 MISMOS bytes, que es la unica forma de que "el exportador y el
                 test miran lo mismo" sea verdad y no una Speranza. Construirlo
                 con DynamicObject obligaria a los dos a serializar por separado
                 y a que un dia difieran en como escriben un booleano.

    ── LO QUE ESTA AQUI, Y LO QUE NO ─────────────────────────────────────────
                 Se vuelcan las CUATRO cosas que las dos tablas tienen: el numero
                 de ranuras, las ocho etiquetas de fuente, y las 31 filas de
                 destino con label, parameterId, perNote y replaces.

                 NO se vuelcan tres cosas del JSON porque no tienen contraparte
                 en C++, y no se inventa una:

                   - `category` de cada fuente ("lfo", "performance",
                     "envelope", "none"): la usa el JSON para pintar la fuente;
                     el motor no la consulta.
                   - `perNote` de cada FUENTE: es una propiedad de la fuente (ENV
                     1 y ENV 2 son per-note) y C++ no tiene tabla de fuentes con
                     campos, solo etiquetas.
                   - `authority`, `provenance`, `displayName`, `description`,
                     `id` y `schemaVersion`: metadatos del documento.

                 La comparacion no las ignora en silencio: las cuenta y las dice
                 en voz alta, en cada ejecucion. Un guard que mira 133 campos y
                 deja 16 fuera sin decirlo parece que cubre el contrato entero:
                 por eso el numero va en la salida y no solo en este comentario.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

#include "State/ParameterDefinitions.h"
#include "State/ModMatrixFromState.h"

namespace NEURONiK::Tests
{

/** Las etiquetas de la matriz no llevan comillas ni barras, pero un volcado que
    depende de eso se rompe el dia que alguien escribe "Off / Silent" en una. */
inline juce::String jsonString (const char* text)
{
    juce::String escaped;
    for (const char* p = text != nullptr ? text : ""; *p != '\0'; ++p)
    {
        if (*p == '"' || *p == '\\') escaped += '\\';
        escaped += *p;
    }
    return "\"" + escaped + "\"";
}

inline juce::String jsonBool (bool value) { return value ? "true" : "false"; }

/** @brief Las tablas de C++ con la forma del contrato JSON, como texto. */
inline juce::String modulationContractJsonFromCpp()
{
    juce::String out;
    out << "{\n";
    out << "  \"id\": \"neuronik_modulation_matrix\",\n";
    out << "  \"slots\": " << State::kModMatrixSlots << ",\n";

    out << "  \"sources\": [\n";
    const auto sources = State::getModSources();
    for (int i = 0; i < sources.size(); ++i)
        out << "    { \"label\": " << jsonString (sources[i].toRawUTF8()) << " }"
            << (i + 1 < sources.size() ? ",\n" : "\n");
    out << "  ],\n";

    out << "  \"destinations\": [\n";
    const auto& destinations = State::getModDestinationTable();
    for (int i = 0; i < destinations.size(); ++i)
    {
        const auto& destination = destinations[i];
        out << "    { \"label\": " << jsonString (destination.label)
            << ", \"parameterId\": "
            << (destination.parameterId != nullptr ? jsonString (destination.parameterId)
                                                   : juce::String ("null"))
            // Se emiten SIEMPRE, tambien cuando son false. El JSON los omite en
            // las 24 filas que no los lleva, y "no lo dice" tiene que contar
            // igual que "dice que no": si no, el guard dejaria pasar un destino
            // que el contrato no declara.
            << ", \"perNote\": " << jsonBool (destination.perNote)
            << ", \"replaces\": " << jsonBool (destination.replaces)
            << " }" << (i + 1 < destinations.size() ? ",\n" : "\n");
    }
    out << "  ]\n";
    out << "}\n";

    return out;
}

} // namespace NEURONiK::Tests
