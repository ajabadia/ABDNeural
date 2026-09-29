/*
  ==============================================================================

    FxCatalogExport.cpp
    El generador del catalogo de efectos. Ver `FxCatalogExport.h` para el por que.

  ==============================================================================
*/

#include "FxCatalogExport.h"

#include "FxCatalogue.h"

#include <fstream>
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

} // namespace

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
bool writeFxCatalogArtifacts (const std::string& directory, std::string& error)
{
    error.clear();

    const std::string jsonPath = directory + "/" + FxCatalogArtifacts::jsonFileName;
    const std::string jsPath   = directory + "/" + FxCatalogArtifacts::javaScriptFileName;

    {
        std::ofstream out (jsonPath, std::ios::binary | std::ios::trunc);

        if (! out)
        {
            error = "no se pudo abrir para escribir: " + jsonPath;
            return false;
        }

        out << fxCatalogAsJson();

        if (! out)
        {
            error = "fallo al escribir: " + jsonPath;
            return false;
        }
    }

    {
        std::ofstream out (jsPath, std::ios::binary | std::ios::trunc);

        if (! out)
        {
            error = "no se pudo abrir para escribir: " + jsPath;
            return false;
        }

        out << fxCatalogAsJavaScript();

        if (! out)
        {
            error = "fallo al escribir: " + jsPath;
            return false;
        }
    }

    return true;
}

} // namespace NEURONiK::DSP
