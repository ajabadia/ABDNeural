/*
  ==============================================================================

    FxCatalogExport.cpp
    El generador del catalogo de efectos. Ver `FxCatalogExport.h` para el por que.

  ==============================================================================
*/

#include "FxCatalogExport.h"

#include "FxCatalogue.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <locale>
#include <sstream>

namespace NEURONiK::DSP
{

namespace
{

/** Escapa una cadena para un JSON y para un literal JS (los dos casos coinciden
    aqui porque el catalogo no lleva comillas ni barras, pero se escapa igual:
    un nombre de efecto es texto de otra persona y no se fia de nadie). */
std::string escape (const char* text)
{
    std::string out;
    if (text == nullptr)
        return out;

    for (const char* c = text; *c != 0; ++c)
    {
        switch (*c)
        {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += *c;     break;
        }
    }

    return out;
}

/** Un float con la coma como separador decimal SIEMPRE, para que el fichero
    generado no cambie de formato con la configuracion regional de quien lo
    regenera (y un `0,5` ahi es un fichero que se regenera distinto cada vez). */
std::string number (float value)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << value;
    return out.str();
}

/** El ancho de mandos que necesita el catalogo: el MAYOR de los seis, no el
    `kFxMaxParams` (12) del modulo compartido. El ancho del bus lo decide el
    catalogo de este producto, y un bus de 12 para un catalogo que como mucho
    pide 4 son ocho parametros de mas por hueco en cada preset y en el contrato de la
    pagina. Cuando entre un motor de ABDEep con 12, esto sube solo y el cambio
    del bus se ve en la paridad del contrato. */
int maxParamsInCatalogue()
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

/** El bloque `params` de una fila, en JSON. El hueco `id..id+numParams-1` del
    bus de la pagina lo ocupa; los que el motor no declara no existen para el. */
std::string paramsAsJson (const abd::dsp::FxEffectInfo* effect)
{
    std::string out = "[]";

    if (effect == nullptr || effect->params == nullptr)
        return out;

    out = "[";

    for (int p = 0; p < effect->numParams; ++p)
    {
        const auto& spec = effect->params[p];

        if (p > 0)
            out += ",";

        out += "\n        { \"index\": " + std::to_string (p)
             + ", \"name\": \"" + escape (spec.name)
             + "\", \"min\": " + number (spec.minValue)
             + ", \"max\": " + number (spec.maxValue)
             + ", \"default\": " + number (spec.defaultValue)
             + ", \"skew\": " + number (spec.skew)
             + ", \"steps\": " + std::to_string (spec.steps) + " }";
    }

    out += effect->numParams > 0 ? "\n      ]" : "]";
    return out;
}

std::string effectsAsJson()
{
    std::string out;

    // El 0 es SIEMPRE bypass: el panel puede elegir "nada" sin que ningun
    // producto tenga que acordarlo (la misma regla que `fxEffectAt`).
    out = "{\n    \"id\": 0, \"sharedId\": 0, \"name\": \"Bypass\", \"displayName\": \"Bypass\","
          " \"family\": \"bypass\", \"aligned\": true, \"technicalName\": null, \"params\": [] }";

    for (int i = 1; i <= fxNeuronikCatalogueSize(); ++i)
    {
        const auto entry = fxNeuronikEffectAt (i);

        if (entry.effect == nullptr || entry.identity == nullptr)
            continue;

        out += ",\n    { \"id\": " + std::to_string (i)
             + ", \"sharedId\": " + std::to_string (entry.identity->id)
             + ", \"name\": \"" + escape (entry.identity->name)
             + "\", \"displayName\": \"" + escape (entry.effect->displayName)
             + "\", \"family\": \"" + escape (entry.identity->family)
             + "\", \"aligned\": " + (entry.identity->aligned ? "true" : "false")
             + ", \"technicalName\": \"" + escape (entry.effect->name)
             + "\", \"params\": " + paramsAsJson (entry.effect) + " }";
    }

    return out;
}

//==============================================================================
/**
    Escribe `contents` en `path` de forma que un fallo a mitad NO pueda dejar un
    fichero a medias, y que el fichero bueno anterior siga intacto.

    Escribe a `<path>.tmp`, lo relee para confirmar que ha llegado entero, y solo
    entonces lo renombra encima del destino. Es el patron de PUBLICAR un
    fichero y no de guardar uno: el destino lo lee la pagina, y un
    `fx-catalog.generated.json` a medias es un modulo de efectos entero que no
    se puede pintar.
*/
bool writeAtomically (const std::string& path, const std::string& contents,
                      std::string& error)
{
    namespace fs = std::filesystem;

    const std::string temporary = path + ".tmp";

    // Un temporal de una tirada anterior que se quedo a medias no puede
    // ensuciar esta: `trunc` lo pisa entero.
    {
        std::ofstream out (temporary, std::ios::binary | std::ios::trunc);

        if (! out)
        {
            error = "no se pudo abrir el temporal para escribir: " + temporary;
            return false;
        }

        out << contents;

        if (! out)
        {
            error = "fallo al escribir el temporal: " + temporary;
            std::error_code ignored;
            fs::remove (temporary, ignored);
            return false;
        }
    }

    // RELEER, y no confiar en que el `ofstream` no ha mentido. Un disco lleno o
    // un antivirus que corta la escritura se muestran aqui y no cuando la
    // pagina intente importar el JSON.
    {
        std::ifstream back (temporary, std::ios::binary);

        if (! back)
        {
            error = "no se pudo releer el temporal: " + temporary;
            std::error_code ignored;
            fs::remove (temporary, ignored);
            return false;
        }

        const std::string readBack ((std::istreambuf_iterator<char> (back)),
                                    std::istreambuf_iterator<char>());

        if (readBack != contents)
        {
            error = "el temporal releido no coincide con lo escrito ("
                  + std::to_string (readBack.size()) + " bytes leidos, "
                  + std::to_string (contents.size()) + " escritos): " + temporary;
            std::error_code ignored;
            fs::remove (temporary, ignored);
            return false;
        }
    }

    // En POSIX `rename` pisa el destino; en Windows falla si ya existe, asi que
    // hay que apartarlo antes. El `remove` deja una ventana en la que el
    // destino no existe, pero dura microsegundos y es mucho menor que la que
    // abria escribir encima: un fichero a medias dura hasta el siguiente
    // arranque de la pagina.
    std::error_code renameError;

    fs::rename (temporary, path, renameError);

    if (renameError)
    {
        std::error_code removeError;
        fs::remove (path, removeError);

        if (removeError)
        {
            error = "no se pudo apartar el destino " + path + ": " + removeError.message();
            std::error_code ignored;
            fs::remove (temporary, ignored);
            return false;
        }

        fs::rename (temporary, path, renameError);

        if (renameError)
        {
            error = "no se pudo renombrar " + temporary + " a " + path
                  + ": " + renameError.message();
            std::error_code ignored;
            fs::remove (temporary, ignored);
            return false;
        }
    }

    return true;
}

} // namespace

//==============================================================================
/**
    Comprueba que el catalogo este EMPAREJADO antes de escribir nada, y explica
    el desajuste nombrando el motor que sobra o el que falta.

    POR QUE ESTO VIVE AQUI Y NO SOLO EN EL TOOL. El fallo que motivo el guard
    --devolver `true` por el hecho de que escribir funcionara, escribiendo un
    stub de 591 bytes con `maxParams: 0` y solo bypass-- es del MODULO DE
    EXPORTACION, no del ejecutable: cualquier otro que llame a
    `writeFxCatalogArtifacts` hereda el mismo "ha ido bien". Poner el fallo en el
    sitio que de verdad puede equivocarse hace que todos los llamantes hereden
    el fallo en vez de tener que acordarse de mirar.

    Y POR QUE EL MENSAJE NOMBRA EL MOTOR. El desajuste real de este catalogo
    siempre ha sido el mismo: entra un motor en el modulo compartido y aqui no
    se le anade su identidad. Un error que solo dice "las cuentas no casan"
    obliga a abrir dos ficheros a mano para deducir cual de los dos sobra, y no
    ha ahorrado nada.

    @returns  true si el catalogo esta completo; false con `error` puesto
*/
bool fxCatalogIsPaired (std::string& error)
{
    error.clear();

    int identityCount = 0;
    const auto* identities = fxNeuronikIdentities (identityCount);

    int effectCount = 0;
    const auto* effects = abd::dsp::fxDefaultCatalogue (effectCount);

    if (identityCount == effectCount)
    {
        // Las cuentas pueden casar y aun asi haber una fila que no empareja: es
        // lo que pasaria si `fxEffectAt` devolviera nullptr para un indice
        // dentro del rango, y en ese caso el JSON tendria un hueco con la fila
        // saltada (el `continue` de `effectsAsJson`). Se comprueba fila a fila
        // para que el catalogo escrito tenga TODAS las entradas.
        for (int i = 1; i <= identityCount; ++i)
        {
            if (fxNeuronikEffectAt (i).effect == nullptr)
            {
                error = "la fila " + std::to_string (i) + " de "
                      + std::to_string (identityCount)
                      + " no empareja: `fxEffectAt` la devuelve nula";
                return false;
            }
        }

        return true;
    }

    error = "el catalogo esta incompleto: " + std::to_string (effectCount)
          + " motores en `fxDefaultCatalogue()` y " + std::to_string (identityCount)
          + " identidades en `fxNeuronikIdentities()`";

    // ── POR POSICION, Y POR QUE NO SE PUEDE ACUSAR A NADIE ─────────────────
    //
    // La version anterior senalaba al motor culpable por nombre y senalaba al
    // equivocado, dos veces por dos razones distintas:
    //
    //   1. senalaba por POSICION (`effects[identityCount]`), y al quitar una
    //      fila del medio todas las posiciones se desplazan: el culpable real
    //      (shelf) quedaba emparejado con la identidad de al lado (phaser).
    //   2. luego se intento comparar por NOMBRE, y es que no se puede: los
    //      nombres de contrato y los tecnicos difieren A PROPOSITO
    //      ("Stereo Chorus" / "chorus", "Shelf Filter" / "Shelf EQ",
    //      "FreeVerb Reverb" / "Reverb"). No hay ni una pareja que encaje, y
    //      el codigo acababa acusando al primer motor de la tabla.
    //
    // Lo que SI se puede afirmar es lo que dice la cabecera de
    // `fxNeuronikIdentities`: las dos tablas van EN EL MISMO ORDEN. Si los
    // recuentos no casan, el sintoma es que a partir de una fila una de las dos
    // tablas deja de cubrir a la otra. Se dice eso, y se dan los dos
    // candidatos de ESA fila sin elegir entre ellos.
    //
    // Un diagnostico que acusa al equivocado es peor que uno que no acusa a
    // nadie, porque el que no acusa obliga a abrir los dos ficheros y a leerlos
    // enteros, y el que acusa mal se sigue sin a mano y ahora con mas
    // conviccion. Este dice DONDE mirar, que es lo que hacia falta.
    const int cubierta = identityCount < effectCount ? identityCount : effectCount;

    if (cubierta < effectCount)
    {
        error += ". A partir de la fila " + std::to_string (cubierta + 1)
               + " los motores no tienen identidad que los cubra: el motor de esa fila es '"
               + std::string (effects[cubierta].name) + "' ('"
               + std::string (effects[cubierta].displayName)
               + "'). O le falta su fila en `fxNeuronikIdentities()`, o sobra una de las que hay antes";
    }
    else
    {
        error += ". A partir de la fila " + std::to_string (cubierta + 1)
               + " las identidades no tienen motor que las cubra: la identidad sobrante es '"
               + std::string (identities[cubierta].name) + "' (id "
               + std::to_string (identities[cubierta].id)
               + "). O le falta su fila en `fxDefaultCatalogue()`, o sobra una de las que hay antes";
    }

    return false;
}


//==============================================================================
std::string fxCatalogAsJson()
{
    std::string out;
    out += "{\n";
    out += "  \"generatedFrom\": \"Source/DSP/FxCatalogue.h + ABDSharedCode/DspEffects/fxDefaultCatalogue()\",\n";
    out += "  \"note\": \"El id y la familia vienen del vocabulario compartido de efectos;"
           " el numero y las tablas de los mandos vienen del MOTOR, que es el unico que"
           " sabe cuantos acepta. La columna params del contrato compartido cuenta los"
           " mandos de la implementacion de ABDEep y no es trasladable.\",\n";
    out += "  \"numSlots\": " + std::to_string (abd::dsp::kFxNumSlots) + ",\n";
    out += "  \"maxParams\": " + std::to_string (maxParamsInCatalogue()) + ",\n";
    out += "  \"effects\": [\n    " + effectsAsJson() + "\n  ]\n";
    out += "}\n";

    return out;
}

//==============================================================================
std::string fxCatalogAsJavaScript()
{
    // El mismo patron que `parameters.generated.js`: una constante congelada,
    // sin codigo ejecutable, para que la pagina no pueda empujarla y deshacer el
    // contrato por la puerta de atras.
    std::string out;
    out += "/**\n";
    out += " * GENERADO POR NEURONiK_FxExport — NO EDITAR.\n";
    out += " *\n";
    out += " * El catalogo de efectos de NEURONiK: que efectos hay, con que id del\n";
    out += " * vocabulario compartido, de que familia (y por tanto con que tema) y con que\n";
    out += " * mandos. La pagina pinta un hueco de efecto leyendo solo esto.\n";
    out += " *\n";
    out += " * El numero de mandos sale del MOTOR, no del contrato compartido: la columna\n";
    out += " * `params` de ese contrato cuenta los mandos de la implementacion de ABDEep.\n";
    out += " */\n\n";
    out += "export const FX_CATALOG = Object.freeze(" + fxCatalogAsJson() + ");\n\n";
    out += "/** Los ids del vocabulario compartido, indexados por id de fila. */\n";
    out += "export const FX_CATALOG_BY_SHARED_ID = Object.freeze(\n";
    out += "  Object.fromEntries (FX_CATALOG.effects.map ((effect) => [effect.sharedId, effect])),\n";
    out += ");\n";

    return out;
}

//==============================================================================
//==============================================================================
bool writeFxCatalogArtifacts (const std::string& directory, std::string& error)
{
    error.clear();

    // ── ANTES DE ABRIR NADA ─────────────────────────────────────────────────
    //
    // El catalogo se valida PRIMERO, y el fallo sale antes de haber tocado un
    // solo fichero. Antes, un desajuste entre `fxDefaultCatalogue()` y
    // `fxNeuronikIdentities()` producia un `true` y un `fx-catalog.generated.json`
    // de 591 bytes con `maxParams: 0` y una unica fila: bypass. O sea, la
    // herramienta decia que habia exportado el catalogo entero y habia
    // exportado la news de que no hay catalogo. Ocurrio dos veces el mismo dia,
    // y en las dos la WebUI se quedo sin pintar un solo efecto sin que nada
    // fallara.
    //
    // Que el error salga aqui y no en el `ifstream` de mas abajo es lo que
    // convierte "no he podido escribir" en "no he podido construir lo que iba
    // a escribir", y son fallos que se arreglan de forma distinta.
    if (! fxCatalogIsPaired (error))
        return false;

    const std::string jsonPath = directory + "/" + FxCatalogArtifacts::jsonFileName;
    const std::string jsPath   = directory + "/" + FxCatalogArtifacts::javaScriptFileName;

    // Los dos textos se COMPONEN ANTES de escribir nada. Con el cuerpo
    // anterior, si el `.json` salia bien y el `.js` fallaba, el repositorio
    // quedaba con los dos a versiones distintas del catalogo y sin que nada
    // lo dijera. Compuestos aqui, o entran los dos o no entra ninguno.
    const std::string json = fxCatalogAsJson();
    const std::string js   = fxCatalogAsJavaScript();

    if (! writeAtomically (jsonPath, json, error))
        return false;

    if (! writeAtomically (jsPath, js, error))
        return false;

    return true;
}

} // namespace NEURONiK::DSP
