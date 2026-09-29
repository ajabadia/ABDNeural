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

    NO ESCRIBE NADA SI EL CATALOGO ESTA INCOMPLETO. Es decir: si los motores de
    `fxDefaultCatalogue()` y las identidades de `fxNeuronikIdentities()` no
    miden lo mismo, esta funcion falla y deja los ficheros que hubiera como
    estaban. La razon esta en la cabecera del `.cpp`: antes de este guard, un
    desajuste entre las dos tablas devolvia `true` y escribia un catalogo de una
    sola fila (bypass) que la WebUI se comia sin quejarse.

    Por lo demas, escribe a temporal y renombra: un fallo a mitad no puede
    dejar un `fx-catalog.generated.json` a medias, que es un modulo de efectos
    entero que no se puede pintar.

    @param directory  la carpeta de salida (`WebUI/generated`)
    @param error      el motivo si fallo; vacio si salio bien
    @returns          true si los dos ficheros quedaron escritos
*/
bool writeFxCatalogArtifacts (const std::string& directory, std::string& error);

/**
    Si el catalogo esta COMPLETO, sin escribir nada.

    Expuesto aparte, y no solo como paso interno de la escritura, porque la
    pregunta "se puede generar el catalogo?" tiene respuestas utiles sin
    escribir: es lo que puede preguntar una toolchain antes de compilar, o un
    test que quiera caer con la fila donde las dos tablas dejan de cubrirse en
    vez de con un JSON de una fila.

    OJO CON LO QUE PROMETE EL MENSAJE: dice DONDE dejan de cubrirse las dos
    tablas, y no a que motor le falta el renglon. No se puede decir, y no por
    falta de trabajo: los nombres de contrato y los tecnicos difieren a proposito
    ("Stereo Chorus" / "chorus", "Shelf Filter" / "Shelf EQ"), y las dos tablas
    van en el mismo orden, asi que en cuanto falta una fila todas las posiciones
    posteriores se desplazan y cualquier acusacion senalaria al equivocado. El
    `.cpp` explica el intento y por que se descarto.

    @param error  el motivo si esta incompleto, con la fila donde falla; vacio si lo esta
    @returns      true si esta completo
*/
bool fxCatalogIsPaired (std::string& error);

/** El catalogo como JSON, en memoria. Es lo que escribe el fichero `.json`. */
std::string fxCatalogAsJson();

/** El catalogo como un modulo ESM, en memoria. */
std::string fxCatalogAsJavaScript();

} // namespace NEURONiK::DSP
