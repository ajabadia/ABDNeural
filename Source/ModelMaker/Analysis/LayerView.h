/*
  ==============================================================================

    LayerView.h
    Created: 25 Sep 2026
    Description: FASE 11.5 — DATOS DE VISTA de las capas para el ModelMaker.

                 La pestana "CAPAS" del ModelMaker dibuja POR QUE el analizador
                 partio (o no) lo que partio: que indice de la rejilla n=1..64
                 fue a parar a que capa, la envolvente temporal de cada indice
                 por frame (su "traza") y la leyenda (nombre, peso y numero de
                 indices por capa). Este modulo produce esos datos y NADA MAS:
                 es PURO y deterministico (sin JUCE de graficos, sin audio), asi
                 que la GUI solo pinta lo que devuelve y el test lo prueba con
                 modelos sinteticos, sin abrir una ventana.

                 El invariante que explota es el de 11.2: cada indice de la
                 rejilla pertenece a UNA sola capa (las capas son sub-indices de
                 la misma rejilla), asi que "capa de un parcial" es una funcion
                 bien definida del modelo.

  ==============================================================================
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "../../../Source/Common/SpectralModel.h"
#include "SpectralAnalyzer.h"

namespace NEURONiK::ModelMaker::Analysis
{

/** Vista de las capas de un SpectralModel (ver el comentario del fichero). */
struct LayerView
{
    static constexpr int kMaxPartials = 64;
    static constexpr int kMaxLayers = NEURONiK::Common::SpectralModel::kMaxLayers;
    static constexpr int kMaxFrames = NEURONiK::Common::SpectralModel::kMaxFrames;

    /** Por debajo de esto el parcial no cuenta como activo. El modelo ya viene
        con el suelo de -60 dB aplicado por el analizador; el epsilon solo cubre
        las amplitudes que el lector deja a cero. */
    static constexpr float kActivityFloor = 1.0e-6f;

    int layerCount = 1;
    int frameCount = 1;

    /** Parciales con energia en alguna capa/frame, y el mas alto (1-based;
        0 = ninguno): acotan lo que la vista tiene que dibujar. */
    int activePartials = 0;
    int highestPartial = 0;

    /** Capa de cada parcial: -1 = inactivo, 0..layerCount-1. */
    std::array<int, kMaxPartials> layerOfPartial {};

    /** Pico del parcial sobre SUS frames, normalizado al pico GLOBAL (0..1):
        es la ALTURA de la barra (cuanto pesa en el conjunto). */
    std::array<float, kMaxPartials> peakOfPartial {};

    /** Envolvente del parcial por frame, normalizada a SU propio pico (0..1):
        es la FORMA de la traza (cuando suena), no su nivel. Los frames validos
        de un parcial son los de su capa — usa frameCountOf(). */
    std::array<std::array<float, kMaxFrames>, kMaxPartials> envelope {};

    struct LayerInfo
    {
        juce::String name;
        float weight = 1.0f;
        int frames = 1;      // frames de ESTA capa
        int partials = 0;    // parciales activos de ESTA capa
    };

    std::array<LayerInfo, kMaxLayers> layers {};

    /**
     * FASE 11.4: el VEREDICTO de la puerta de plegado de octava (10.3)
     * sobre ESTE material, tal y como lo midio el analizador. La pestana
     * CAPAS lo ensena para que la razon del reparto no se adivine: es la
     * MISMA estructura (y los MISMOS numeros) que consumen la sonda y el
     * clustering — cero politicas nuevas en la vista.
     */
    struct FoldVerdict
    {
        bool measured = false;      // false = sin datos (sin ventanas utiles)
        bool biGrid = false;        // true = raices genuinamente distintas
        float foldCents = -1.0f;    // dispersion post-plegado (la que decide)
        float rawCents = 0.0f;      // sin plegar (el salto del estimador a la vista)
        int observations = 0;       // ventanas que sostienen la medida
        int octaveFlips = 0;        // saltos de octava que el plegado cuenta como acuerdo
    };

    FoldVerdict fold {};

    bool layered() const noexcept { return layerCount > 1; }

    /** Frames de la capa a la que pertenece el parcial (0 si esta inactivo). */
    int frameCountOf (int partial) const noexcept
    {
        if (partial < 0 || partial >= kMaxPartials)
            return 0;

        const int l = layerOfPartial[(size_t) partial];
        return l < 0 ? 0 : layers[(size_t) l].frames;
    }
};

/** Construye la vista de un modelo (con o sin capas; con 1 capa la capa 0 es
    la raiz y todos los parciales activos son suyos). `fold` es el veredicto
    de la puerta de plegado (10.3) que la pestana CAPAS ensena junto a la
    leyenda: la MISMA medida que decide el clustering, no una nueva. */
inline LayerView buildLayerView (const NEURONiK::Common::SpectralModel& model,
                                 const SpectralAnalyzer::OctaveFold& fold)
{
    LayerView view;

    view.fold.measured = fold.observations > 0;
    view.fold.biGrid = fold.biGrid;
    view.fold.foldCents = fold.foldCents;
    view.fold.rawCents = fold.rawCents;
    view.fold.observations = fold.observations;
    view.fold.octaveFlips = fold.octaveFlips;

    view.layerCount = juce::jlimit (1, LayerView::kMaxLayers, model.layerCount);
    view.frameCount = juce::jlimit (1, LayerView::kMaxFrames, model.frameCount);

    for (int l = 0; l < view.layerCount; ++l)
    {
        auto& info = view.layers[(size_t) l];
        info.name = model.layerNameAt (l);
        info.weight = model.layerWeightAt (l);
        info.frames = juce::jlimit (1, LayerView::kMaxFrames, model.numFramesOf (l));
    }

    float globalPeak = 0.0f;

    for (int i = 0; i < LayerView::kMaxPartials; ++i)
    {
        // La capa del parcial es la PRIMERA con energia en el (invariante 11.2:
        // cada indice pertenece a UNA capa, asi que en el caso real solo una
        // tiene energia; se toma la mas baja para que sea determinista).
        int layer = -1;

        for (int l = 0; l < view.layerCount; ++l)
        {
            const int frames = view.layers[(size_t) l].frames;
            float layerPeak = 0.0f;

            for (int f = 0; f < frames; ++f)
                layerPeak = std::max (layerPeak, std::abs (model.ampAt (l, f, i)));

            if (layerPeak > LayerView::kActivityFloor)
            {
                layer = l;
                break;
            }
        }

        view.layerOfPartial[(size_t) i] = layer;

        if (layer < 0)
        {
            view.envelope[(size_t) i].fill (0.0f);   // inactivo: sin traza
            continue;
        }

        const int frames = view.layers[(size_t) layer].frames;
        float peak = 0.0f;

        for (int f = 0; f < frames; ++f)
            peak = std::max (peak, std::abs (model.ampAt (layer, f, i)));

        for (int f = 0; f < frames; ++f)
            view.envelope[(size_t) i][(size_t) f] = std::abs (model.ampAt (layer, f, i)) / peak;

        for (int f = frames; f < LayerView::kMaxFrames; ++f)
            view.envelope[(size_t) i][(size_t) f] = 0.0f;

        view.peakOfPartial[(size_t) i] = peak;
        globalPeak = std::max (globalPeak, peak);

        ++view.activePartials;
        ++view.layers[(size_t) layer].partials;
        view.highestPartial = i + 1;
    }

    for (int i = 0; i < LayerView::kMaxPartials; ++i)
        view.peakOfPartial[(size_t) i] = globalPeak > 0.0f
                                             ? view.peakOfPartial[(size_t) i] / globalPeak
                                             : 0.0f;

    return view;
}

/** La vista sin medida de la puerta: el veredicto queda "sin datos" — la
    pestana lo ensena como "sin medida" y no inventa un veredicto. */
inline LayerView buildLayerView (const NEURONiK::Common::SpectralModel& model)
{
    return buildLayerView (model, {});
}

} // namespace NEURONiK::ModelMaker::Analysis
