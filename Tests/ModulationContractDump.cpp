/*
  ==============================================================================

    ModulationContractDump.cpp
    Created: 28 Sep 2026
    Description: EL LADO DE C++ DEL CONTRATO, impreso.

                 Vuelca la tabla de destinos y la de fuentes al mismo formato
                 que contracts/neuronik_modulation_matrix.json, para poder
                 mirar de que dice una y de que dice la otra sin bucear por el
                 codigo:

                   NEURONiK_ModulationContractDump

                 No necesita el repo hermano, y por eso es una HERRAMIENTA y no
                 un test: sirve precisamente cuando ABDSharedAssets no esta a
                 mano, que es cuando mas falta hace ver el lado propio. El que
                 COMPARA los dos es ModulationContractTest.cpp, y ese si necesita
                 los dos.

                 Que sean dos y no uno es a proposito. Si el comparador
                 escribiera su propio JSON, estariamos comparando el JSON del
                 contrato contra OTRO volcado, no contra la tabla: dos medidas de
                 la misma cosa que pueden mentir a la vez. Aqui el volcado es el
                 mismo codigo en los dos.

  ==============================================================================
*/

#include <cstdio>

#include "ModulationContractJson.h"

int main()
{
    const auto json = NEURONiK::Tests::modulationContractJsonFromCpp();
    std::printf ("%s\n", json.toRawUTF8());

    // Si esto no cuadra con el contrato, la culpa casi siempre es de aqui y no
    // del JSON, y saberlo antes de mirar 31 filas ahorra media hora.
    std::fprintf (stderr,
                  "contrato de C++: %d ranuras, %d fuentes, %d destinos\n",
                  NEURONiK::State::kModMatrixSlots,
                  (int) NEURONiK::State::getModSources().size(),
                  (int) NEURONiK::State::getModDestinationTable().size());
    return 0;
}
