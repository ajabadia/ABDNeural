/*
  ==============================================================================

    LayerViewTest.cpp
    Created: 25 Sep 2026
    Description: FASE 11.5 — los DATOS DE VISTA de las capas que pinta la
                 pestana "CAPAS" del ModelMaker (Source/ModelMaker/Analysis/
                 LayerView.h). Modulo PURO: se prueba con modelos sinteticos,
                 sin abrir una ventana.

                   1. Modelo de DOS capas: cada parcial queda en SU capa, la
                      leyenda cuenta los indices por capa, la envolvente se
                      normaliza a SU pico (la forma) y el pico al GLOBAL (la
                      altura), y los inactivos quedan fuera (-1).
                   2. Modelo de UNA capa (el legado): todo va a la capa 0 y no
                      se declara layered.
                   3. Modelo VACIO: sin parciales activos y sin trazas.

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/LayerView.h"
#include "../Source/Common/SpectralModel.h"

#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <vector>

using NEURONiK::Common::SpectralModel;
using NEURONiK::ModelMaker::Analysis::LayerView;
using NEURONiK::ModelMaker::Analysis::buildLayerView;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

constexpr float tol = 1.0e-5f;

bool near (float a, float b) { return std::abs (a - b) < tol; }

/** Escribe los frames de UN parcial de UNA capa, en orden. */
void setPartial (SpectralModel& model, int layer, int partial, std::initializer_list<float> values)
{
    int f = 0;

    for (const float v : values)
        model.ampsOf (layer, f++)[(size_t) partial] = v;
}
} // namespace

int main()
{
    std::printf ("LayerView: los datos de vista de las capas (11.5)\n");

    // ---------- 1. Modelo de DOS capas ----------
    std::printf ("\nDos capas\n");
    {
        SpectralModel model;
        model.setLayerCount (2);
        model.setLayerNameAt (0, "drone");
        model.setLayerWeightAt (0, 1.0f);
        model.setNumFramesOf (0, 3);
        model.setLayerNameAt (1, "lead");
        model.setLayerWeightAt (1, 0.6f);
        model.setNumFramesOf (1, 4);

        setPartial (model, 0, 0, { 0.5f, 1.0f, 0.5f });       // el drone: pico 1.0
        setPartial (model, 0, 1, { 0.2f, 0.2f, 0.2f });
        setPartial (model, 0, 2, { 0.1f, 0.0f, 0.0f });
        setPartial (model, 1, 6, { 0.4f, 0.8f, 0.4f, 0.2f }); // la voz: pico 0.8
        setPartial (model, 1, 7, { 0.05f, 0.05f, 0.05f, 0.05f });

        const auto view = buildLayerView (model);

        check (view.layerCount == 2 && view.layered() && view.frameCount == 3,
               "la vista declara las 2 capas (frameCount de la capa 0 = 3)");

        check (view.activePartials == 5 && view.highestPartial == 8,
               "5 parciales activos y el mas alto es el 8 (1-based)");

        check (view.layerOfPartial[0] == 0 && view.layerOfPartial[1] == 0 && view.layerOfPartial[2] == 0
                   && view.layerOfPartial[6] == 1 && view.layerOfPartial[7] == 1,
               "cada parcial esta en SU capa (0-2 en la 0, 6-7 en la 1)");
        check (view.layerOfPartial[3] == -1 && view.layerOfPartial[4] == -1
                   && view.layerOfPartial[5] == -1 && view.layerOfPartial[8] == -1,
               "los indices vacios quedan inactivos (-1)");

        check (view.layers[0].partials == 3 && view.layers[1].partials == 2,
               "la leyenda cuenta los indices por capa (drone 3, lead 2)");
        check (view.layers[0].name == "drone" && view.layers[1].name == "lead"
                   && near (view.layers[1].weight, 0.6f),
               "los nombres y pesos de capa viajan en la vista");
        check (view.layers[0].frames == 3 && view.layers[1].frames == 4,
               "los frames por capa viajan (3 y 4)");

        check (view.frameCountOf (0) == 3 && view.frameCountOf (6) == 4 && view.frameCountOf (3) == 0,
               "frameCountOf sigue a la capa del parcial (0 = inactivo)");

        // Altura: pico normalizado al GLOBAL (el drone manda en el conjunto).
        check (near (view.peakOfPartial[0], 1.0f) && near (view.peakOfPartial[1], 0.2f)
                   && near (view.peakOfPartial[6], 0.8f) && near (view.peakOfPartial[7], 0.05f)
                   && near (view.peakOfPartial[3], 0.0f),
               "las alturas son el pico GLOBAL-normalizado (1.0 / 0.2 / 0.8 / 0.05 / 0)");

        // Forma: envolvente normalizada a SU pico.
        check (near (view.envelope[0][0], 0.5f) && near (view.envelope[0][1], 1.0f)
                   && near (view.envelope[0][2], 0.5f) && near (view.envelope[0][3], 0.0f),
               "la traza del drone es su forma (0.5 / 1.0 / 0.5 y el resto a 0)");
        check (near (view.envelope[6][0], 0.5f) && near (view.envelope[6][1], 1.0f)
                   && near (view.envelope[6][2], 0.5f) && near (view.envelope[6][3], 0.25f),
               "la traza de la voz es SU forma (0.5 / 1.0 / 0.5 / 0.25), no su nivel");
        check (near (view.envelope[3][0], 0.0f),
               "un parcial inactivo no tiene traza (todo a 0)");
    }

    // ---------- 2. Modelo de UNA capa (el legado) ----------
    std::printf ("\nUna capa\n");
    {
        SpectralModel model;
        model.amplitudes[0] = 0.6f;
        model.amplitudes[1] = 0.3f;
        model.amplitudes[2] = 0.1f;

        const auto view = buildLayerView (model);

        check (view.layerCount == 1 && ! view.layered() && view.frameCount == 1,
               "un modelo de una capa NO se declara layered");
        check (view.activePartials == 3 && view.highestPartial == 3
                   && view.layers[0].partials == 3,
               "los 3 parciales activos son de la capa 0 (la raiz)");
        check (view.layerOfPartial[0] == 0 && view.layerOfPartial[1] == 0
                   && view.layerOfPartial[2] == 0,
               "todos en la capa 0 (sin capas, la raiz es la unica)");
        check (near (view.peakOfPartial[0], 1.0f) && near (view.peakOfPartial[1], 0.5f),
               "la altura se normaliza al pico global (0.6 -> 1.0, 0.3 -> 0.5)");
        check (view.layers[0].frames == 1 && view.frameCountOf (0) == 1,
               "la capa 0 tiene el frame unico del modelo estatico");
    }

    // ---------- 3. El VEREDICTO de la puerta (FASE 11.4) ----------
    // La pestana CAPAS ensena la razon del reparto: mono/bi-rejilla y sus
    // cents, LA MISMA medida (measureOctaveFold, 10.3) que decide el
    // clustering. Aqui se pinea que la vista la transporta sin inventarse
    // nada — y que sin datos (o sin llamada) NO declara veredicto.
    std::printf ("\nVeredicto de la puerta de plegado\n");
    {
        using NEURONiK::ModelMaker::Analysis::SpectralAnalyzer;

        SpectralModel model;
        model.amplitudes[0] = 0.6f;

        // Mono: ventanas estables cerca de la referencia (deviaciones < corte).
        const auto stable = SpectralAnalyzer::measureOctaveFold (
            { 200.0f, 203.0f, 197.0f, 201.0f }, 200.0f);
        const auto mono = buildLayerView (model, stable);

        check (mono.fold.measured && ! mono.fold.biGrid && mono.fold.observations == 4,
               "veredicto mono-rejilla medido (4 ventanas, bajo el corte)");
        check (mono.fold.foldCents >= 0.0f
                   && mono.fold.foldCents < SpectralAnalyzer::kOctaveFoldCents,
               "el cents post-plegado del mono queda bajo el corte (lo que pinta el panel)");

        // Bi: una quinta (701.96) sobrevive al plegado como -498.04.
        const auto fifth = SpectralAnalyzer::measureOctaveFold ({ 200.0f, 300.0f }, 200.0f);
        const auto bi = buildLayerView (model, fifth);

        check (bi.fold.measured && bi.fold.biGrid
                   && bi.fold.foldCents > SpectralAnalyzer::kOctaveFoldCents,
               "veredicto bi-rejilla medido (la quinta sobrevive al plegado)");

        // Sin ventanas utiles NO hay veredicto: la pestana dira "sin medida".
        const auto empty = SpectralAnalyzer::measureOctaveFold ({ 0.0f, 0.0f }, 200.0f);
        const auto none = buildLayerView (model, empty);

        check (! none.fold.measured && ! none.fold.biGrid && none.fold.foldCents < 0.0f,
               "sin ventanas utiles la vista NO declara veredicto");

        // El camino de siempre (1 argumento) tampoco: no inventa un veredicto.
        const auto legacy = buildLayerView (model);
        check (! legacy.fold.measured && ! legacy.fold.biGrid,
               "la vista sin fold (el legado) no declara veredicto");
    }

    // ---------- 4. Modelo VACIO ----------
    std::printf ("\nModelo vacio\n");
    {
        SpectralModel model;
        const auto view = buildLayerView (model);

        check (view.activePartials == 0 && view.highestPartial == 0,
               "sin parciales activos no hay nada que dibujar");
        check (view.layerOfPartial[0] == -1 && view.layerOfPartial[63] == -1
                   && near (view.peakOfPartial[0], 0.0f) && near (view.envelope[0][0], 0.0f),
               "todos los indices quedan inactivos y sin traza");
    }

    std::printf ("\nRESULT: %s (%d fallos)\n", failures == 0 ? "OK" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
