/*
  ==============================================================================

    FxCatalogExport.h
    Escribe el CATALOGO DE EFECTOS que consume la pagina, a partir de la tabla
    del motor. Namespace NEURONiK::DSP.

    POR QUE UN EXPORTADOR PROPIO Y NO UN `.json` COPIADO. El numero de mandos de
    cada efecto, sus nombres y sus rangos viven en el motor (C++, JUCE-free) y no
    se pueden escribir a mano en la pagina sin que las dos copias se separen. Es
    el mismo motivo por el que existe `NEURONiK_ParameterExport` con los
    parametros: una sola fuente de verdad, y la pagina la lee. Aqui la fuente de
    verdad es `FxCatalogue.h` mas la tabla del modulo compartido.

    Y NO ES LO MISMO QUE EL CONTRATO COMPARTIDO. `fx-effects.json` describe el
    VOCABULARIO (que ids existen, como se llaman, de que familia son), y su
    columna `params` cuenta los mandos de la implementacion de ABDEep —que no es
    la de este motor—. Este fichero genera el catalogo REAL: los ids y las
    familias de ahi, y los mandos de aqui. Quien pinte un hueco tiene que leer
    esto, no el JSON.

    SIN JUCE A PROPOSITO. El modulo de efectos es header-only y JUCE-free, y esta
    es la razon de que los 48 motores privados de ABDEep no puedan entrar todavia
    (meterlos obligaria a que el modulo dejara de serlo, y con el caeria la
    paridad nativa <-> WASM). Un exportador que dependiera de JUCE para leer una
    tabla de structs seria una razon mas para no poder compilar el modulo sin el,
    y este exportador es justamente la prueba de que no hace falta.

  ==============================================================================
*/

#pragma once

#include <string>

namespace NEURONiK::DSP
{

/** Los nombres de los ficheros que escribe `writeFxCatalogArtifacts`. */
struct FxCatalogArtifacts
{
    static constexpr const char* jsonFileName = "fx-catalog.generated.json";
    static constexpr const char* javaScriptFileName = "fx-catalog.generated.js";
};

/**
    Escribe el catalogo en `directory`.

    @param directory  la carpeta de salida (`WebUI/generated`)
    @param error      el motivo si fallo; vacio si salio bien
    @returns          true si los dos ficheros quedaron escritos
*/
bool writeFxCatalogArtifacts (const std::string& directory, std::string& error);

/** El catalogo como JSON, en memoria. Es lo que escribe el fichero `.json`. */
std::string fxCatalogAsJson();

/** El catalogo como un modulo ESM, en memoria. */
std::string fxCatalogAsJavaScript();

} // namespace NEURONiK::DSP
