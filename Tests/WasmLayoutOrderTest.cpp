/*
  ==============================================================================

    WasmLayoutOrderTest.cpp

    EL ORDEN DEL LAYOUT DE GlobalParams, que es un ABI y no se puede ver.

    La pagina escribe el espejo de los parametros por INDICE de campo, no por
    offset (`WebUI/src/wasm/audioParams.js` -> CONTRACT_TO_GP_FIELD). Un orden
    equivocado no da error de compilacion, no da crash y no da excepcion: la
    escritura cae en el campo de al lado y el knob que se gira no hace nada,
    mientras otro mando que nadie esta girando se mueve solo. Eso es lo que
    paso con la ganancia y la mezcla del hueco 1, que la pagina situaba en los
    campos 38 y 39 y el puente publicaba en los del primer parametro del hueco
    2: el fallo estaba en el UNICO sitio donde nada mira, porque los params si
    coincidian (los del hueco 1 van los primeros en las dos formas del orden).

    Este test es el que hacia falta: no reconstruye el wasm (que ademas
    hornearia en un binario versionado cualquier cambio en vuelo del DSP), sino
    que mira el layout que construyen los bucles y lo confronta con los numeros
    que publicita la pagina. Si alguien cambia un bucle, este test lo dice con
    el campo concreto, en vez de que se entere un oido en un bus.

  ==============================================================================
*/

#include <cstddef>
#include <cstdio>
#include <vector>

#include "Wasm/GlobalParamsLayout.h"

using GP = NEURONiK::DSP::GlobalParams;

namespace
{

int gFailures = 0;

void check (bool condition, const char* what)
{
    if (condition)
        return;

    std::printf ("  [FALLO] %s\n", what);
    ++gFailures;
}

void checkField (const std::vector<std::size_t>& layout, int index,
                 std::size_t expected, const char* what)
{
    const bool inRange = index >= 0 && static_cast<std::size_t> (index) < layout.size();
    const std::size_t actual = inRange ? layout[static_cast<std::size_t> (index)]
                                       : static_cast<std::size_t> (-1);

    if (actual == expected)
    {
        std::printf ("  [ok]   %-46s campo %2d\n", what, index);
        return;
    }

    std::printf ("  [FALLO] %-46s campo %2d: vale %lld, deberia ser %lld\n",
                 what, index, static_cast<long long> (actual),
                 static_cast<long long> (expected));
    ++gFailures;
}

constexpr int kExpectedScalars = 22;                                  // masterLevel..lfo2Depth
constexpr int kExpectedModMatrix = 12;                                // 4 rutas x 3 campos
constexpr int kBusBlock = NEURONiK::DSP::kFxBusParams + 2;           // params + gain + mix

// EL NUMERO QUE PUBLICITA LA PAGINA. No es una copia: es el unico sitio donde
// vive, y las dos mitades --el puente y la pagina-- se comprueban contra el.
constexpr int kPageFx1Param1 = 34;
constexpr int kPageFx1Param4 = 37;
constexpr int kPageFx1Gain = 38;
constexpr int kPageFx1Mix = 39;

} // namespace

int main()
{
    const auto layout = globalParamsLayout();

    std::printf ("WasmLayoutOrderTest: %zu campos publicados\n", layout.size());

    // ── 1) El total, que es lo primero que lee la pagina ─────────────────────
    const std::size_t expectedTotal = static_cast<std::size_t> (
        kExpectedScalars + kExpectedModMatrix + kBusBlock * NEURONiK::DSP::kFxBusSlots);

    check (layout.size() == expectedTotal,
           "el layout publica escalares + matriz + un bloque por hueco");

    // ── 2) La matriz, en el orden de GlobalParams.modMatrix[r] ───────────────
    for (int r = 0; r < 4; ++r)
    {
        const int first = kExpectedScalars + r * 3;
        checkField (layout, first + 0, offsetof (GP, modMatrix[r].source), "modMatrix.source");
        checkField (layout, first + 1, offsetof (GP, modMatrix[r].destination), "modMatrix.destination");
        checkField (layout, first + 2, offsetof (GP, modMatrix[r].amount), "modMatrix.amount");
    }

    // ── 3) El bus, y aqui esta el fallo que se escapo ───────────────────────
    // UN HUECO ENTERO: sus params, su ganancia y su mezcla, y luego el
    // siguiente. La comprobacion mira los huecos 0 y 1 porque la confusion que
    // hubo fue entre los dos: el 38 creia la pagina que era la ganancia del
    // hueco 1 y era el primer parametro del hueco 2.
    for (int slot = 0; slot < NEURONiK::DSP::kFxBusSlots; ++slot)
    {
        const int base = kExpectedScalars + kExpectedModMatrix + slot * kBusBlock;

        for (int p = 0; p < NEURONiK::DSP::kFxBusParams; ++p)
            checkField (layout, base + p, offsetof (GP, fx[slot].params[p]),
                        "hueco, params");

        checkField (layout, base + NEURONiK::DSP::kFxBusParams,
                    offsetof (GP, fx[slot].gain), "hueco, ganancia (tras SUS params)");
        checkField (layout, base + NEURONiK::DSP::kFxBusParams + 1,
                    offsetof (GP, fx[slot].mix), "hueco, mezcla (tras su ganancia)");
    }

    // ── 4) La ganancia de un hueco NO es el primer parametro del siguiente ───
    // Esta es la propiedad que estaba rota, dicha de la forma mas corta: la
    // ganancia del hueco 1 cae en su propio bloque, no en el del hueco 2.
    if (NEURONiK::DSP::kFxBusSlots > 1)
    {
        const int gain0 = kExpectedScalars + kExpectedModMatrix + NEURONiK::DSP::kFxBusParams;
        const int gain1 = kExpectedScalars + kExpectedModMatrix + kBusBlock
                        + NEURONiK::DSP::kFxBusParams;

        check (layout[static_cast<std::size_t> (gain0)] == offsetof (GP, fx[0].gain)
               && layout[static_cast<std::size_t> (gain1)] == offsetof (GP, fx[1].gain),
               "cada hueco tiene SU ganancia, y no la del parametro de al lado");
    }

    // ── 5) Los numeros que publicita la pagina ─────────────────────────────
    checkField (layout, kPageFx1Param1, offsetof (GP, fx[0].params[0]), "la pagina: fx1Param1");
    checkField (layout, kPageFx1Param4, offsetof (GP, fx[0].params[3]), "la pagina: fx1Param4");
    checkField (layout, kPageFx1Gain, offsetof (GP, fx[0].gain), "la pagina: fx1Gain");
    checkField (layout, kPageFx1Mix, offsetof (GP, fx[0].mix), "la pagina: fx1Mix");

    // Y que el hueco 2 empieza justo detras, que es lo que la pagina da por
    // hecho al no mapearlo todavia.
    if (NEURONiK::DSP::kFxBusSlots > 1)
        checkField (layout, kPageFx1Mix + 1, offsetof (GP, fx[1].params[0]),
                    "el hueco 2 empieza donde acaba el 1");

    // ── 6) El corte que usa el segundo export ───────────────────────────────
    check (scalarFieldCount() == static_cast<std::size_t> (kExpectedScalars),
           "scalarFieldCount() cuenta los escalares de la tabla, no un 22 escrito a mano");
    check (layout.size() - scalarFieldCount()
               == static_cast<std::size_t> (kExpectedModMatrix + kBusBlock * NEURONiK::DSP::kFxBusSlots),
           "el segundo export publica la matriz Y el bus, no solo la matriz");
    std::printf ("\n%s (%d fallos)\n", gFailures == 0 ? "RESULT: OK" : "RESULT: FALLOS", gFailures);
    return gFailures == 0 ? 0 : 1;
}
