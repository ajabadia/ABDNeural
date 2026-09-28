/*
  ==============================================================================

    ModulationContractTest.cpp
    Created: 28 Sep 2026
    Description: EL PUENTE ENTRE EL CONTRATO Y EL MOTOR, campo a campo.

                 Hay cuatro copias de la verdad de la matriz: el contrato JSON
                 de ABDSharedAssets, el componente modMatrix.js, la tabla de
                 destinos de C++ y la tabla del motor. La Fase A ato la tercera
                 con la cuarta a nivel de fila; esta ato la primera con la
                 tercera a nivel de campo. Entre las dos, ninguna se separa sola.

                 Antes, un destino con regla de reemplazo se anadia a la tabla de
                 C++, se anadia al JSON, y la lista que el motor usaba —un array
                 escrito a mano dentro de un test— se quedaba intacta sin que
                 nadie se enterara. Aqui eso ya no puede pasar: `perNote` y
                 `replaces` viven en ModDestination y se comparan uno a uno.

    ── SI EL REPO HERMANO NO ESTA, ESTE TEST FALLA A PROPOSITO ───────────────
                 No hace un "skip" y sale con 0, que es lo que haria un guard
                 bienintencionado: en un clon limpio de ABDNeural, o en un CI que
                 solo clona este repo, el guard pasaria en verde SIN COMPARAR
                 NADA. Verde significa "el contrato y la tabla coinciden", y eso
                 es mentira si no se ha mirado el contrato.

                 Un guard que se salta en silencio es peor que no tener guard: da
                 una confianza que no existe y nadie la nota hasta que un destino
                 aparece en la pagina y no module nada. Asi que el repo hermano
                 ausente es un FALLO, con un mensaje que dice que no se ha
                 podido comprobar y por que.

                 Si el CI lo necesita verde en un clon de un solo repo, lo que hay
                 que hacer es clonar tambien ABDSharedAssets y no relajar el
                 test.

  ==============================================================================
*/

#include <cstdio>

#include "ModulationContractJson.h"

namespace
{
using namespace NEURONiK;

int passed = 0;
int failed = 0;

void check (bool condition, const juce::String& name)
{
    if (condition) { ++passed; std::printf ("  [PASS] %s\n", name.toRawUTF8()); }
    else           { ++failed; std::printf ("  [FAIL] %s\n", name.toRawUTF8()); }
}

/** El fallo de "no he podido comprobar", que NO es un fallo del contrato. */
int couldNotCompare (const juce::String& reason)
{
    std::printf ("\n=== NO SE HA PODIDO COMPARAR EL CONTRATO ===\n\n"
                 "  %s\n\n"
                 "  Esto no es un fallo del contrato: es que no se ha podido\n"
                 "  MIRAR, y por lo tanto el puente entre ABDSharedAssets y el\n"
                 "  motor no esta comprobado. Un 0 aqui seria un verde que no\n"
                 "  dice nada.\n\n"
                 "  Para que el guard funcione hacen falta los dos repos:\n"
                 "    - clona ABDSharedAssets junto a ABDNeural, o\n"
                 "    - apunta NEURONiK_SIBLING_ASSETS en el build.\n\n",
                 reason.toRawUTF8());
    ++failed;
    return 1;
}
} // namespace

int main()
{
    std::printf ("=== Contrato de la matriz: ABDSharedAssets contra el motor ===\n");

    // 1) El repo hermano. Va PRIMERO y decide si hay comparacion.
#ifndef NEURONiK_SIBLING_ASSETS
    return couldNotCompare ("la ruta del repo hermano no llego al binario");
#else
    const juce::File contract (juce::String (NEURONiK_SIBLING_ASSETS)
                               + "/contracts/neuronik_modulation_matrix.json");

    if (! contract.existsAsFile())
        return couldNotCompare ("no existe " + contract.getFullPathName()
                                + " — ABDSharedAssets no esta donde se esperaba");

    const auto parsed = juce::JSON::parse (contract);
    if (! parsed.isObject())
        return couldNotCompare (contract.getFullPathName() + " no es un objeto JSON");

    // Los MISMOS bytes que imprime el exportador, no otro volcado: si aqui se
    // construyera el JSON de otra forma, comparariamos el contrato contra una
    // segunda medida de la tabla, y las dos podrian mentir a la vez.
    const auto cpp = juce::JSON::parse (NEURONiK::Tests::modulationContractJsonFromCpp());
    if (! cpp.isObject())
        return couldNotCompare ("el volcado de C++ no es un objeto JSON valido");

    // 2) Lo que se compara, campo a campo.
    check (cpp.getProperty ("slots", -1) == parsed.getProperty ("slots", -2),
           "slots: la matriz tiene las mismas cuatro ranuras en los dos lados");

    {
        const auto cppSources = cpp.getProperty ("sources", juce::var());
        const auto jsonSources = parsed.getProperty ("sources", juce::var());
        const auto* a = cppSources.getArray();
        const auto* b = jsonSources.getArray();

        if (a == nullptr || b == nullptr)
            return couldNotCompare ("las fuentes no son un array en uno de los dos lados");

        check (a->size() == b->size(),
               "sources: los dos lados ofrecen " + juce::String (b->size()) + " fuentes");

        int mismatched = 0;
        for (int i = 0; i < juce::jmin (a->size(), b->size()); ++i)
            if ((*a)[i].getProperty ("label", juce::var())
                != (*b)[i].getProperty ("label", juce::var()))
                ++mismatched;

        char message[160];
        std::snprintf (message, sizeof (message),
                       "sources: las ocho etiquetas coinciden en orden (%d discrepancias)",
                       mismatched);
        check (mismatched == 0, message);
    }

    {
        const auto cppRows = cpp.getProperty ("destinations", juce::var());
        const auto jsonRows = parsed.getProperty ("destinations", juce::var());
        const auto* a = cppRows.getArray();
        const auto* b = jsonRows.getArray();

        if (a == nullptr || b == nullptr)
            return couldNotCompare ("los destinos no son un array en uno de los dos lados");

        check (a->size() == b->size(),
               "destinations: los dos lados tienen " + juce::String (b->size()) + " destinos");

        int labelDiff = 0, idDiff = 0, perNoteDiff = 0, replacesDiff = 0;
        juce::StringArray names;

        for (int i = 0; i < juce::jmin (a->size(), b->size()); ++i)
        {
            const auto& mine = (*a)[i];
            const auto& theirs = (*b)[i];
            const auto name = juce::String (i) + " ("
                            + theirs.getProperty ("label", juce::var()).toString() + ")";

            if (mine.getProperty ("label", juce::var())
                != theirs.getProperty ("label", juce::var()))
            { ++labelDiff; names.add ("label " + name); }

            // El C++ pone `parameterId` a una var vacia cuando no hay, y el JSON
            // pone null. Las dos cosas tienen que leer "no hay parametro".
            // El C++ escribe una var vacia y el JSON escribe null; en JUCE un
            // null parseado ES una var vacia, asi que los dos lados se comparan
            // con la misma pregunta.
            const auto mineId = mine.getProperty ("parameterId", juce::var());
            const auto theirsId = theirs.getProperty ("parameterId", juce::var());
            if (mineId != theirsId)
            { ++idDiff; names.add ("parameterId " + name); }

            // El JSON OMITE los flags en las filas que no los lleva. Omitir un
            // false y decir false es lo mismo, asi que se comparan como bool y
            // no como "existe la clave".
            if (((bool) mine.getProperty ("perNote", false))
                != ((bool) theirs.getProperty ("perNote", false)))
            { ++perNoteDiff; names.add ("perNote " + name); }

            if (((bool) mine.getProperty ("replaces", false))
                != ((bool) theirs.getProperty ("replaces", false)))
            { ++replacesDiff; names.add ("replaces " + name); }
        }

        char message[200];
        std::snprintf (message, sizeof (message), "destinations: las etiquetas (%d)", labelDiff);
        check (labelDiff == 0, message);
        std::snprintf (message, sizeof (message), "destinations: los parameterId (%d)", idDiff);
        check (idDiff == 0, message);
        std::snprintf (message, sizeof (message), "destinations: perNote (%d)", perNoteDiff);
        check (perNoteDiff == 0, message);
        std::snprintf (message, sizeof (message), "destinations: replaces (%d)", replacesDiff);
        check (replacesDiff == 0, message);

        if (names.size() > 0)
        {
            std::printf ("\n  Los que no cuadran:\n");
            for (const auto& name : names)
                std::printf ("    - %s\n", name.toRawUTF8());
        }
    }

    // 3) LO QUE NO SE COMPARA, dicho en voz alta.
    {
        const auto jsonSources = parsed.getProperty ("sources", juce::var());
        const auto* b = jsonSources.getArray();
        int uncompared = 0;
        if (b != nullptr)
            for (int i = 0; i < b->size(); ++i)
                for (const auto* field : { "category", "perNote" })
                    if (! (*b)[i].getProperty (field, juce::var()).isVoid())
                        ++uncompared;

        std::printf ("\n  RADIO REAL DEL GUARD, para que nadie lo lea como mas:\n"
                     "    COMPARADO    slots (1 campo), la etiqueta de las 8 fuentes\n"
                     "                (8) y, de los 31 destinos, label, parameterId,\n"
                     "                perNote y replaces (31 x 4 = 124).  Total: 133.\n"
                     "    SIN COMPARAR `category` y `perNote` de cada FUENTE (%d\n"
                     "                valores): no tienen contraparte en C++, que solo\n"
                     "                tiene etiquetas de fuente. La categoria la usa el\n"
                     "                JSON para pintar; el motor no la consulta.\n"
                     "    SIN COMPARAR authority, provenance, displayName,\n"
                     "                description, id y schemaVersion: metadatos.\n"
                     "    SIN COMPARAR components/modMatrix.js: la cuarta copia. La\n"
                     "                ataron los tests de ABDSharedAssets contra SU\n"
                     "                contrato, no contra esta tabla.\n",
                     uncompared);
    }

    std::printf ("\n=== %d pasan, %d fallan ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
#endif
}
