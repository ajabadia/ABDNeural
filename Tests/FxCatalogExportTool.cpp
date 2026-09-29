/*
  ==============================================================================

    FxCatalogExportTool.cpp
    Regenera el catalogo de efectos que consume la WebUI. Usage:

                   NEURONiK_FxExport [outputDirectory]

                 Default output directory is WebUI/generated, resolved relative
                 to the current working directory when the argument is not
                 absolute.

    SIN JUCE, y no por descuido: el modulo de efectos es header-only y JUCE-free
    para poder sostener la paridad nativa <-> WASM, y un exportador que
    necesara JUCE para leer una tabla de structs seria el primer sitio donde el
    modulo dejaria de compilar solo. Este ejecutable es la prueba de que la
    lectura del catalogo no necesita el motor de audio.

  ==============================================================================
*/

#include "../Source/DSP/FxCatalogExport.h"
#include "../Source/DSP/FxCatalogue.h"

#include <iostream>
#include <string>

int main (int argc, char* argv[])
{
    const std::string directory = argc > 1 ? std::string (argv[1])
                                           : std::string ("WebUI/generated");

    std::string error;

    if (! NEURONiK::DSP::writeFxCatalogArtifacts (directory, error))
    {
        std::cerr << "NEURONiK fx catalog export failed: " << error << '\n';
        return 1;
    }

    int aligned = 0;
    int reserved = 0;
    int widest = 0;

    for (int i = 1; i <= NEURONiK::DSP::fxNeuronikCatalogueSize(); ++i)
    {
        const auto entry = NEURONiK::DSP::fxNeuronikEffectAt (i);

        if (entry.effect == nullptr || entry.identity == nullptr)
            continue;

        if (entry.identity->aligned)
            ++aligned;
        else
            ++reserved;

        if (entry.effect->numParams > widest)
            widest = entry.effect->numParams;
    }

    std::cout << "NEURONiK fx catalog export\n";
    std::cout << "  output directory : " << directory << '\n';
    std::cout << "  effects          : " << NEURONiK::DSP::fxNeuronikCatalogueSize() << '\n';
    std::cout << "  aligned          : " << aligned << '\n';
    // Las reservas son ids reservados aqui que el contrato compartido todavia no
    // lista. Se imprimen porque son un aviso, no un error: son el trabajo que
    // queda hecho en el otro repositorio, y callarlas las convertiria en un
    // bug accidental.
    std::cout << "  reserved         : " << reserved << '\n';
    std::cout << "  bus width        : " << widest << " params per slot\n";
    std::cout << "  files            : " << NEURONiK::DSP::FxCatalogArtifacts::jsonFileName
              << ", " << NEURONiK::DSP::FxCatalogArtifacts::javaScriptFileName << '\n';

    return 0;
}
