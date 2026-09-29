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

    // ── EL GUARD, Y POR QUE ESTA AQUI Y NO DENTRO DE LA ESCRITURA ───────────
    //
    // Se comprueba el emparejamiento del catalogo ANTES de llamar a la
    // escritura, y no solo por el mensaje: el codigo de salida es otro.
    //
    // Este ejecutable ya comprobaba `ERRORLEVEL` en `build.bat`, asi que el
    // guard de la build existia y estaba bien puesto. Lo que no existia era
    // algo que comprobar: `writeFxCatalogArtifacts` devolvia `true` porque
    // escribir a disco habia funcionado, aunque lo que hubiera escrito fuera un
    // stub de 591 bytes con `maxParams: 0` y una unica fila, bypass. O sea:
    // un `true` que queria decir "he exportado el catalogo" cuando lo que
    // acababa de hacer era borrarlo. La build se ponia verde, la WebUI se
    // quedaba sin un solo efecto, y no habia ningun fallo en ninguna parte.
    //
    // Salir con 2 y no con 1 no es cosmetico: 1 es "no he podido escribir" (una
    // carpeta que no existe, un disco lleno) y 2 es "el catalogo esta roto"
    // (aun escrebiendo bien se habria escrito una basura). Quien lo dispare
    // necesita poder contarlos por separado sin leer el texto.
    if (! NEURONiK::DSP::fxCatalogIsPaired (error))
    {
        std::cerr << "NEURONiK fx catalog export: NO EXPORTADO.\n"
                  << "  " << error << "\n"
                  << "  Que hacer: anadir la fila que falta a\n"
                  << "  Source/DSP/FxCatalogue.h (fxNeuronikIdentities), o quitar de\n"
                  << "  ABDSharedCode/DspEffects/FxDefaultCatalogue.h el motor que sobra.\n"
                  << "  NO se ha escrito ningun fichero: los que hubiera siguen como estaban.\n";
        return 2;
    }

    if (! NEURONiK::DSP::writeFxCatalogArtifacts (directory, error))
    {
        std::cerr << "NEURONiK fx catalog export failed: " << error << '\n';
        return 1;
    }

    int aligned = 0;
    int reserved = 0;
    int widest = 0;
    int rows = 0;

    for (int i = 1; i <= NEURONiK::DSP::fxNeuronikCatalogueSize(); ++i)
    {
        const auto entry = NEURONiK::DSP::fxNeuronikEffectAt (i);

        // El `continue` que habia aqui se ha ido porque, con el guard de
        // arriba, una fila nula ya es imposible: si lo hubiera, el propio guard
        // habria salido con 2. Dejar el `continue` seria aceptar en silencio el
        // mismo fallo que este ejecutable existe para impedir.
        if (entry.effect == nullptr || entry.identity == nullptr)
        {
            std::cerr << "NEURONiK fx catalog export: la fila " << i
                      << " no empareja, con el catalogo ya validado. Esto no deberia "
                         "llegar aqui: avisame del fallo.\n";
            return 3;
        }

        ++rows;

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
    std::cout << "  rows written     : " << rows << '\n';
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
