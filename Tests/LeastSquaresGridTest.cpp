/*
  ==============================================================================

    LeastSquaresGridTest.cpp
    Created: 24 Sep 2026
    Description: El ajuste de rejilla por MINIMOS CUADRADOS de detectPitch
                 con material sintetico cuyo f0 se DESPLAZA entre ventanas.

                   Contrato "f0 por LS sobre los picos de TODAS las
                   ventanas, no solo la cabecera":

                   1. Estatico: 6 ventanas identicas a 220 Hz -> la lectura
                      es 220 +/- 2 Hz y el residuo es ruido sub-bin (<15
                      cents), con >= 20 observaciones (las 6 ventanas
                      aportan picos).
                   2. Deriva: MISMAS 6 primeras 8192 muestras (la cabecera
                      220 Hz es bitwise identica al control), pero las
                      ventanas 2..6 suben 4 Hz cada una (220..240). La
                      cabecera sola diria 220; el LS lee la MEDIA de las
                      ventanas (230 Hz: f0* = SUM(k*p)/SUM(k*k) con
                      p = k*f0_ventana da el promedio ponderado por k^2,
                      que al compartir parciales entre ventanas es la
                      media) y el residuo DELATA la deriva (~50 cents RMS
                      de las desviaciones -77..+74 frente a 230).
                   3. Temporal: con el mismo material desplazado,
                      analyzeTemporal ajusta por frame la SUYA (LS por
                      frame, fitGridFromSpectrum) mientras frameSpanHz
                      sigue canonicamente en la f0 del llamador.
                   4. Material pobre: un seno puro (una sola ventana, un
                      unico pico) queda por debajo de kMinLsObservaciones
                      -> el ajuste se abstiene y manda la semilla HPS.
                   5. Sin material: el indicador queda en n/d (residuo -1,
                      0 observaciones).

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstdio>

namespace
{
using SpectralAnalyzer = NEURONiK::ModelMaker::Analysis::SpectralAnalyzer;

int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

constexpr double sr = 44100.0;
constexpr int fftSize = 8192;   // SpectralAnalyzer::fftSize
constexpr int kWindows = 6;     // maxFrames del ajuste por fichero
constexpr int kPartials = 4;    // parciales por ventana (sin aliasing de combs)

/** Bloque armonico 1/n (fase reiniciada por bloque, como el emparejador de
    TemporalAnalysisTest). CUATRO parciales: todos ellos caben en la banda de
    media espaciado de su rejilla aunque el bloque este desplazado, de modo
    que cada observacion p = k*f0_ventana es EXACTA y el LS converge al
    promedio de las ventanas sin aliasing entre combs de armónicos. */
void fillStack (juce::AudioBuffer<float>& audio, int start, double f0)
{
    const double norm = 1.0 + 0.5 + 1.0 / 3.0 + 0.25; // suma de 1/n
    for (int i = 0; i < fftSize; ++i)
    {
        double s = 0.0;
        for (int n = 1; n <= kPartials; ++n)
            s += std::sin (2.0 * juce::MathConstants<double>::pi * f0 * (double) n
                           * (double) i / sr) / (double) n;
        audio.setSample (0, start + i, (float) (0.6 * s / norm));
    }
}
} // namespace

int main()
{
    std::printf ("Ajuste LS de rejilla con f0 desplazado entre ventanas\n");

    // ---------- 1. Control estatico: 6 ventanas identicas a 220 Hz ----------
    juce::AudioBuffer<float> stat (1, kWindows * fftSize);
    for (int w = 0; w < kWindows; ++w)
        fillStack (stat, w * fftSize, 220.0);

    SpectralAnalyzer aStat;
    const float fStat = aStat.detectPitch (stat, sr);
    const float rStat = aStat.lastGridResidualCents();
    const int   oStat = aStat.lastGridObservations();
    std::printf ("    estatico : f0=%.2f Hz  residuo=%.1f cents  obs=%d\n",
                 (double) fStat, (double) rStat, oStat);
    check (std::abs (fStat - 220.0f) < 2.0f,
           "estatico: detectPitch = 220 +/- 2 Hz (LS sobre las 6 ventanas)");
    check (rStat >= 0.0f && rStat < 15.0f,
           "estatico: residuo < 15 cents (solo ruido sub-bin)");
    check (oStat >= 20,
           "estatico: >= 20 observaciones (todas las ventanas aportan)");

    // ---------- 2. Deriva entre ventanas: misma cabecera, cola subida ----------
    juce::AudioBuffer<float> drift (1, kWindows * fftSize);
    for (int w = 0; w < kWindows; ++w)
        fillStack (drift, w * fftSize, 220.0 + 4.0 * (double) w); // 220..240

    bool sameHeader = true;
    for (int i = 0; i < fftSize && sameHeader; ++i)
        sameHeader = stat.getSample (0, i) == drift.getSample (0, i);
    check (sameHeader,
           "el material desplazado comparte cabecera bitwise con el control");

    SpectralAnalyzer aDrift;
    const float fDrift = aDrift.detectPitch (drift, sr);
    const float rDrift = aDrift.lastGridResidualCents();
    const int   oDrift = aDrift.lastGridObservations();
    std::printf ("    deriva   : f0=%.2f Hz  residuo=%.1f cents  obs=%d\n",
                 (double) fDrift, (double) rDrift, oDrift);
    check (std::abs (fDrift - 230.0f) < 2.0f,
           "deriva: el LS lee la MEDIA de las ventanas (230 Hz), no la cabecera (220)");
    check (fDrift - fStat >= 6.0f,
           "deriva: la lectura se separa >= 6 Hz de la del control estatico");
    check (rDrift >= 25.0f && rDrift < 150.0f,
           "deriva: el residuo delata el desplazamiento (25..150 cents)");
    check (rDrift > rStat + 10.0f,
           "deriva: el residuo supera al del material estatico");
    check (oDrift >= 20,
           "deriva: >= 20 observaciones (las ventanas desplazadas tambien aportan)");

    // ---------- 3. El mismo material desplazado por la via TEMPORAL ----------
    {
        const auto model = aDrift.analyzeTemporal (drift, sr, 220.0f, kWindows);
        check (model.isValid && model.frameCount == kWindows,
               "temporal: analyzeTemporal produce los 6 frames pedidos");
        check (std::abs (model.frameSpanHz - 220.0f) < 1.0e-4f,
               "temporal: frameSpanHz sigue siendo la f0 del llamador (canonico)");

        bool framesOk = true;
        for (int fr = 1; fr < model.frameCount; ++fr)
        {
            const float want = 220.0f + 4.0f * (float) fr;
            const float got = model.f0At (fr);
            std::printf ("    frame %d: f0=%.2f Hz (quiere %.1f)\n",
                         fr, (double) got, (double) want);
            framesOk &= std::abs (got - want) < 1.5f;
        }
        check (framesOk,
               "temporal: cada frame ajusta POR LS la SU f0 (desplazamiento seguido)");
    }

    // ---------- 4. Material pobre: un seno puro, una sola ventana ----------
    {
        juce::AudioBuffer<float> thin (1, fftSize);
        const double w = 2.0 * juce::MathConstants<double>::pi * 220.0 / sr;
        double phase = 0.0;
        for (int i = 0; i < fftSize; ++i)
        {
            thin.setSample (0, i, (float) (0.6 * std::sin (phase)));
            phase += w;
            if (phase > 2.0 * juce::MathConstants<double>::pi)
                phase -= 2.0 * juce::MathConstants<double>::pi;
        }

        SpectralAnalyzer aThin;
        const float fThin = aThin.detectPitch (thin, sr);
        const float rThin = aThin.lastGridResidualCents();
        const int   oThin = aThin.lastGridObservations();
        std::printf ("    seno puro: f0=%.2f Hz  residuo=%.1f cents  obs=%d\n",
                     (double) fThin, (double) rThin, oThin);
        check (oThin >= 1 && oThin < 4,
               "material pobre: el ajuste se abstiene (< observaciones minimas)");
        check (std::abs (fThin - 220.0f) < 6.0f,
               "material pobre: manda la semilla HPS (~220 Hz)");
        check (rThin >= 0.0f && rThin < 40.0f,
               "material pobre: el indicador publica un residuo coherente");
    }

    // ---------- 5. Sin material: indicador n/d ----------
    {
        juce::AudioBuffer<float> empty (1, 0);
        SpectralAnalyzer aEmpty;
        const float fEmpty = aEmpty.detectPitch (empty, sr);
        check (fEmpty == 0.0f
                   && aEmpty.lastGridResidualCents() == -1.0f
                   && aEmpty.lastGridObservations() == 0,
               "sin material: f0=0 e indicador n/d (residuo -1, 0 observaciones)");
    }

    std::printf ("\nRESULT: %s (%d fallos)\n",
                 failures == 0 ? "OK" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
