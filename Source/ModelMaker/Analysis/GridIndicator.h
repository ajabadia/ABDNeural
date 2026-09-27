/*
  ==============================================================================

    GridIndicator.h
    Created: 27 Sep 2026
    Description: EL MODELO PURO del indicador de rejilla del ModelMaker (la fila
                 "Rejilla: f0 ... | residuo ... | N picos" con su color por
                 bandas y su clic que carga la f0 en el editor de pitch).

                 Antes el texto, el ternario de colores y la decision del clic
                 vivian escritos dentro de MainComponent::updateGridIndicator —
                 la GUI era lo unico que los verificaba, y lo unico que podia
                 romperlos sin que nada se quejara. Este modulo produce el
                 ESTADO DEL INDICADOR (texto, color RGBA, carga del clic) y
                 NADA MAS: es PURO y sin JUCE (string/cmath/cstdint), asi que
                 la GUI solo pinta lo que devuelve y el test lo prueba sin
                 abrir una ventana.

                 Las BANDAS del residuo son las mismas de siempre (2026-09-25):
                 verde <= residualGreenCents (el material ES una rejilla),
                 amarillo <= residualAmberCents (una rejilla con estructura de
                 sobra: sub-armonico, offsets), naranja por encima (no hay UNA
                 rejilla). Sus constantes viven AQUI desde hoy: el analizador
                 las aliasa (SpectralAnalyzer::residualGreenCents) para que el
                 test de rangos del banco CZ101 y la UI sigan leyendo el mismo
                 numero de la misma fuente.

  ==============================================================================
*/

#pragma once

#include <cstdint>
#include <cmath>
#include <cstdio>
#include <string>

namespace NEURONiK::ModelMaker::Analysis
{

/** Estado completo del indicador de rejilla (entrada + salida, sin JUCE). */
struct GridIndicatorModel
{
    // ---- entrada: lo que la GUI (o la sonda) ya tienen medido -------------

    /** La f0 detectada/ajustada. <= 0 = sin material: indicador vacio. */
    double detectedHz = 0.0;

    /** Residuo RMS de rejilla en cents. < 0 = n/d (material insuficiente). */
    double residualCents = -1.0;

    /** Picos que sostienen el residuo. */
    int observations = 0;

    /** Modo REJILLA FIJA declarado por el usuario (sin seguimiento por ventana). */
    bool fixedGrid = false;

    /** Modo OFFSETS TRANSPONIBLES declarado por el usuario. */
    bool offsetsTranspose = false;

    /** La f0 queda por DEBAJO del suelo del estimador (anchorFloorHz): la
        rejilla la fijo el usuario a mano porque el HPS no llega a leerla. */
    bool belowEstimatorFloor = false;

    // ---- las bandas: la fuente UNICA de los umbrales ----------------------

    /** Verde: el material ES una rejilla (medido: BASS1 9.4, HAMOG 0.0,
        PAD1 4.2 cents). */
    static constexpr float residualGreenCents = 15.0f;

    /** Amarillo: rejilla con estructura de sobra (SWEP1 17.5, el sub-armonico
        que la escalera no baja). */
    static constexpr float residualAmberCents = 40.0f;

    enum class Band { NoData, Green, Amber, Orange };

    Band band() const noexcept
    {
        if (residualCents < 0.0)
            return Band::NoData;
        if (residualCents <= residualGreenCents)
            return Band::Green;
        if (residualCents <= residualAmberCents)
            return Band::Amber;
        return Band::Orange;
    }

    /** El color de la banda, RGBA directo (sin JUCE): los mismos valores que
        pintaba el ternario de MainComponent (grey/green/yellow/orange). */
    std::uint32_t argb() const noexcept
    {
        switch (band())
        {
            case Band::Green:  return 0xFF2ECC71u;
            case Band::Amber:  return 0xFFFFFF00u;
            case Band::Orange: return 0xFFFFA500u;
            default:           return 0xFF808080u;   // NoData: gris
        }
    }

    // ---- salida: el texto de la fila, tal y como lo escribia la GUI -------

    /** La fila completa del indicador. Vacia si no hay material (la GUI deja
        el label sin texto). Los avisos de modo viajan DETRAS del residuo, en
        el mismo orden de siempre: REJILLA FIJA, luego OFFSETS TRANSPONIBLES,
        y si no hay transposibles pero la f0 queda bajo el suelo del estimador,
        "fijada a mano". */
    std::string text() const
    {
        if (detectedHz <= 0.0)
            return {};

        char f0[32];
        std::snprintf (f0, sizeof (f0), "%.2f", detectedHz);

        std::string out = "Rejilla: f0 ";
        out += f0;
        out += " Hz";

        if (residualCents >= 0.0)
        {
            char resid[32];
            std::snprintf (resid, sizeof (resid), "%.1f", residualCents);
            out += "  |  residuo ";
            out += resid;
            out += " cents  |  ";
            out += std::to_string (observations);
            out += " picos";
        }
        else
        {
            out += "  |  residuo n/d (material insuficiente)";
        }

        if (fixedGrid)
            out += "  |  REJILLA FIJA (sin seguimiento de pitch por ventana)";

        if (offsetsTranspose)
            out += "  |  OFFSETS TRANSPONIBLES (la inharmonicidad sigue al teclado)";
        else if (belowEstimatorFloor)
            out += "  |  fijada a mano (el estimador no llega hasta aqui)";

        return out;
    }

    // ---- salida: lo que el CLIC carga en el editor de pitch ---------------

    /** La f0 que el clic del indicador ofrece al editor (la MISMA pareja que
        el camino de grabar: texto a 2 decimales + nota/octava). 0 = el clic
        no hace nada (sin material no hay rejilla que ofrecer). El modelo
        entrega la f0 SIN cuantizar a la nota mas cercana: 64.50 Hz de rejilla
        no es 65.41 de C2 — la nota/octava la escriben los combos sin
        notificacion, que es justo lo que evita la ida y vuelta. */
    double clickLoadsHz() const noexcept
    {
        return detectedHz > 0.0 ? detectedHz : 0.0;
    }

    /** El texto que el clic escribe en el editor de pitch (2 decimales, el
        mismo formato que el camino de grabar). Vacio si el clic no hace nada. */
    std::string clickEditorText() const
    {
        if (clickLoadsHz() <= 0.0)
            return {};

        char f0[32];
        std::snprintf (f0, sizeof (f0), "%.2f", clickLoadsHz());
        return f0;
    }
};

} // namespace NEURONiK::ModelMaker::Analysis
