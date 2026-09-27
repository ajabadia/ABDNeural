/*
  ==============================================================================

    GridIndicatorTest.cpp
    Created: 27 Sep 2026
    Description: EL MODELO PURO del indicador de rejilla del ModelMaker
                 (Source/ModelMaker/Analysis/GridIndicator.h): el texto de la
                 fila "Rejilla: f0 ... | residuo ... | N picos", el color por
                 bandas del residuo y lo que el clic carga en el editor de
                 pitch. Antes todo eso vivia escrito dentro de
                 MainComponent::updateGridIndicator — la GUI era lo unico que
                 lo verificaba, y lo unico que podia romperlo sin que nada se
                 quejara. Sin JUCE: se prueba con valores sinteticos.

                   1. Las BANDAS del residuo y sus fronteras (verde <= 15,
                      ambar <= 40, naranja por encima, n/d -> gris).
                   2. El RGBA exacto que pinta la GUI (el del ternario viejo).
                   3. El TEXTO de la fila, aviso a aviso, en el orden de
                      siempre (FIJA, TRANSPONIBLES o "fijada a mano").
                   4. El CLIC: la f0 SIN cuantizar y el texto a 2 decimales;
                      sin material, el clic no hace nada.

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/GridIndicator.h"

#include <cstdio>
#include <string>

using NEURONiK::ModelMaker::Analysis::GridIndicatorModel;

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what);
    if (! ok) ++failures;
}

void checkText (const std::string& got, const std::string& expected, const char* what)
{
    const bool ok = (got == expected);
    std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what);
    if (! ok)
    {
        std::printf ("       esperado: \"%s\"\n", expected.c_str());
        std::printf ("       obtenido: \"%s\"\n", got.c_str());
        ++failures;
    }
}
} // namespace

int main()
{
    // ---------- 1. Las BANDAS del residuo ----------
    std::printf ("\nBandas del residuo\n");
    {
        GridIndicatorModel m;

        check (m.band() == GridIndicatorModel::Band::NoData,
               "por defecto (residuo n/d) NO hay banda: NoData");
        check (m.residualCents < 0.0 && m.band() == GridIndicatorModel::Band::NoData,
               "residuo negativo (material insuficiente) -> NoData");

        m.residualCents = 0.0;
        check (m.band() == GridIndicatorModel::Band::Green,
               "0.0 cents -> verde (la rejilla ES el material)");

        m.residualCents = GridIndicatorModel::residualGreenCents;
        check (m.band() == GridIndicatorModel::Band::Green,
               "frontera INCLUSIVA: 15.0 cents -> verde (BASS1 9.4, HAMOG 0.0, PAD1 4.2)");

        m.residualCents = 15.1;
        check (m.band() == GridIndicatorModel::Band::Amber,
               "justo por encima del verde -> ambar");

        m.residualCents = GridIndicatorModel::residualAmberCents;
        check (m.band() == GridIndicatorModel::Band::Amber,
               "frontera INCLUSIVA: 40.0 cents -> ambar (SWEP1 17.5)");

        m.residualCents = 40.1;
        check (m.band() == GridIndicatorModel::Band::Orange,
               "justo por encima del ambar -> naranja (no hay UNA rejilla)");

        m.residualCents = 120.0;
        check (m.band() == GridIndicatorModel::Band::Orange,
               "muy lejos -> naranja");
    }

    // ---------- 2. El RGBA que pinta la GUI ----------
    std::printf ("\nColor por banda (RGBA del ternario viejo)\n");
    {
        GridIndicatorModel m;

        check (m.argb() == 0xFF808080u, "NoData -> gris 0xFF808080");

        m.residualCents = 9.4;   // BASS1, medido
        check (m.argb() == 0xFF2ECC71u, "verde -> 0xFF2ECC71");

        m.residualCents = 17.5;  // SWEP1, medido
        check (m.argb() == 0xFFFFFF00u, "ambar -> 0xFFFFFF00");

        m.residualCents = 55.0;
        check (m.argb() == 0xFFFFA500u, "naranja -> 0xFFFFA500");
    }

    // ---------- 3. El TEXTO de la fila ----------
    std::printf ("\nTexto de la fila\n");
    {
        checkText (GridIndicatorModel {}.text(), {},
                   "sin material la fila queda VACIA (la GUI deja el label sin texto)");
        checkText (GridIndicatorModel {}.clickEditorText(), {},
                   "sin material el clic tampoco escribe nada");

        GridIndicatorModel m;
        m.detectedHz = 64.5;
        checkText (m.text(),
                   "Rejilla: f0 64.50 Hz  |  residuo n/d (material insuficiente)",
                   "f0 con residuo n/d: \"material insuficiente\"");

        m.residualCents = 9.4;
        m.observations = 3;
        checkText (m.text(),
                   "Rejilla: f0 64.50 Hz  |  residuo 9.4 cents  |  3 picos",
                   "f0 + residuo a 1 decimal + numero de picos");

        m.detectedHz = 440.0;
        checkText (m.text(),
                   "Rejilla: f0 440.00 Hz  |  residuo 9.4 cents  |  3 picos",
                   "la f0 sale a 2 decimales SIEMPRE (440, no 440.0)");

        m.residualCents = 17.53;
        checkText (m.text(),
                   "Rejilla: f0 440.00 Hz  |  residuo 17.5 cents  |  3 picos",
                   "el residuo se redondea a 1 decimal (17.53 -> 17.5)");
    }

    std::printf ("\nAvisos de modo (orden de siempre, DETRAS del residuo)\n");
    {
        GridIndicatorModel m;
        m.detectedHz = 64.5;
        m.residualCents = 9.4;
        m.observations = 3;
        const std::string base = "Rejilla: f0 64.50 Hz  |  residuo 9.4 cents  |  3 picos";

        m.fixedGrid = true;
        checkText (m.text(), base + "  |  REJILLA FIJA (sin seguimiento de pitch por ventana)",
                   "REJILLA FIJA se anuncia");

        m.offsetsTranspose = true;
        checkText (m.text(),
                   base + "  |  REJILLA FIJA (sin seguimiento de pitch por ventana)"
                       "  |  OFFSETS TRANSPONIBLES (la inharmonicidad sigue al teclado)",
                   "FIJA va ANTES que TRANSPONIBLES (pueden coexistir)");

        m.fixedGrid = false;
        checkText (m.text(), base + "  |  OFFSETS TRANSPONIBLES (la inharmonicidad sigue al teclado)",
                   "solo TRANSPONIBLES");

        m.offsetsTranspose = false;
        m.belowEstimatorFloor = true;
        checkText (m.text(), base + "  |  fijada a mano (el estimador no llega hasta aqui)",
                   "bajo el suelo del estimador y sin transponibles -> \"fijada a mano\"");

        m.offsetsTranspose = true;
        checkText (m.text(), base + "  |  OFFSETS TRANSPONIBLES (la inharmonicidad sigue al teclado)",
                   "TRANSPONIBLES GANA sobre \"fijada a mano\" (no se apilan)");

        m.offsetsTranspose = false;
        m.belowEstimatorFloor = true;
        m.residualCents = -1.0;
        checkText (m.text(),
                   "Rejilla: f0 64.50 Hz  |  residuo n/d (material insuficiente)"
                       "  |  fijada a mano (el estimador no llega hasta aqui)",
                   "el aviso viaja tambien con el residuo n/d");

        GridIndicatorModel sinAvisos;
        sinAvisos.detectedHz = 64.5;
        sinAvisos.residualCents = 9.4;
        sinAvisos.observations = 3;
        sinAvisos.belowEstimatorFloor = false;
        checkText (sinAvisos.text(), base, "sin modos declarados la fila no avisa de nada");
    }

    // ---------- 4. El CLIC ----------
    std::printf ("\nLo que carga el clic en el editor de pitch\n");
    {
        GridIndicatorModel m;

        check (m.clickLoadsHz() == 0.0 && m.clickEditorText().empty(),
               "sin material el clic NO hace nada");

        m.detectedHz = 64.5;
        check (m.clickLoadsHz() == 64.5,
               "el clic entrega la f0 SIN cuantizar (64.50 no es 65.41 de C2)");

        checkText (m.clickEditorText(), "64.50",
                   "el texto del editor sale a 2 decimales (el camino de grabar)");

        m.detectedHz = 64.987;
        checkText (m.clickEditorText(), "64.99",
                   "el redondeo del texto es el de snprintf (%.2f)");
    }

    // ---------- 5. La fuente UNICA de las bandas ----------
    std::printf ("\nLa fuente unica de los umbrales\n");
    {
        check (GridIndicatorModel::residualGreenCents == 15.0f,
               "verde sigue en 15.0 cents (el analizador lo aliasa; CZ101 y UI leen esto)");
        check (GridIndicatorModel::residualAmberCents == 40.0f,
               "ambar sigue en 40.0 cents");
    }

    std::printf ("\nRESULT: %s (%d fallos)\n", failures == 0 ? "OK" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
