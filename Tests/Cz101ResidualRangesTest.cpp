/*
  ==============================================================================

    Cz101ResidualRangesTest.cpp
    Created: 25 Sep 2026
    Description: LOS RANGOS DEL RESIDUO DE REJILLA con los cinco WAV reales del
                 banco CZ101 — el numero que el ModelMaker enseña en el
                 indicador y (desde hoy) junto al aviso de pitch inestable.

                 El residuo no es un adorno: es el RMS en cents de los picos de
                 TODAS las ventanas contra la rejilla k*f0, o sea la respuesta a
                 "¿este material ES una rejilla?". Medido sobre el banco:

                   TONALES (CZ-BASS1, CZ-HAMOG, CZ-PAD1) ..... VERDE
                     4.5 - 7.1 cents: hay UNA f0 y los picos caen en ella.
                   CZ-SWEP1 (fundamental debil con sub-octava)  AMARILLO
                     25.2 cents: sigue siendo una rejilla, con estructura de
                     sobra por debajo (el sub-armonico que la escalera no baja).
                   CZ-RRISE (barrido de pitch) ................ NARANJA
                     243.9 cents: NO hay UNA rejilla — y ademas LA GUARDIA
                     DISPARA (548 cents), que es el caso en el que la UI enseña
                     el aviso de pitch con el residuo al lado.

                 Lo que se fija es el RANGO de cada material (regresion: si un
                 cambio del analizador mueve un residuo de banda, esto lo dice
                 antes que el ojo) y la BANDA que le toca, leida de las
                 constantes del analizador (SpectralAnalyzer::residualGreenCents
                 / residualAmberCents) — las mismas con las que la UI pinta el
                 indicador. Cruzar una frontera de banda es una DECISION, no una
                 regresion: si un cambio mejora SWEP1 hasta el verde, hay que
                 venir aqui y decirlo.

                 El material vive en el repo hermano: CMake inyecta
                 NEURONiK_CZ101_WAV_DIR (= ../ABDCZ101/DOCS/patches) cuando
                 existe. Sin inyeccion el test SALTEA y pasa (el repo se puede
                 compilar sin el banco); con el banco presente, los CINCO
                 ficheros son obligatorios (un WAV que falte es un FAIL, no un
                 skip: la inyeccion de la ruta ya significa "el banco esta").

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>

using SpectralAnalyzer = NEURONiK::ModelMaker::Analysis::SpectralAnalyzer;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

/** La banda en la que cae un residuo, con los umbrales del ANALIZADOR (los
    mismos que el pincel del indicador): una sola definicion de las bandas. */
juce::String bandOf (float residCents)
{
    if (residCents < 0.0f) return "n/d";
    if (residCents <= SpectralAnalyzer::residualGreenCents) return "verde";
    if (residCents <= SpectralAnalyzer::residualAmberCents) return "amarillo";
    return "naranja";
}

/** Lo medido el 2026-09-25 sobre el banco (el rango es lo medido con margen
    para el ruido sub-bin de la rejilla; la banda es lo que ese numero declara
    en la UI). */
struct Expectation
{
    const char* fileName;
    float minResidCents;
    float maxResidCents;
    int   minObservations;
    const char* band;
    bool  firesPitchGuard;
};

const Expectation kExpected[] =
{
    { "CZ-BASS1.wav",   2.0f,  12.0f, 100, "verde",    false },
    { "CZ-HAMOG.wav",   2.0f,  10.0f, 100, "verde",    false },
    { "CZ-PAD1.wav",    3.0f,  13.0f,  50, "verde",    false },
    { "CZ-SWEP1.wav",  16.0f,  38.0f, 100, "amarillo", false },
    { "CZ-RRISE.wav", 150.0f, 400.0f,  10, "naranja",  true  },
};

constexpr int kNumExpected = (int) (sizeof (kExpected) / sizeof (kExpected[0]));
} // namespace

#ifdef NEURONiK_CZ101_WAV_DIR

int main()
{
    std::printf ("Residuo de rejilla sobre los cinco WAV del banco CZ101\n");

    const juce::File bankDir (juce::String (NEURONiK_CZ101_WAV_DIR));
    std::printf ("    banco: %s\n", bankDir.getFullPathName().toRawUTF8());
    std::printf ("    bandas: verde <= %.0f cents, amarillo <= %.0f, naranja por encima\n",
                 (double) SpectralAnalyzer::residualGreenCents,
                 (double) SpectralAnalyzer::residualAmberCents);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    int measured = 0;

    for (int e = 0; e < kNumExpected; ++e)
    {
        const auto& expect = kExpected[e];
        const juce::String name (expect.fileName);
        const juce::File wav = bankDir.getChildFile (name);

        if (! wav.existsAsFile())
        {
            // La ruta la inyecta CMake SOLO si el banco esta: si falta un WAV,
            // el banco ha cambiado y el test no puede cumplir su contrato.
            check (false, name + ": falta el WAV del banco (" + wav.getFullPathName() + ")");
            continue;
        }

        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (wav));

        if (reader == nullptr)
        {
            check (false, name + ": sin reader para el WAV");
            continue;
        }

        juce::AudioBuffer<float> audio ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&audio, 0, (int) reader->lengthInSamples, 0, true, true);

        const double sampleRate = reader->sampleRate;

        // El MISMO orden que la GUI: detectPitch publica el residuo del
        // material cargado (es el que el indicador enseña) y analyze mide la
        // guardia de desviacion sobre el mismo buffer.
        SpectralAnalyzer analyzer;
        const float f0 = analyzer.detectPitch (audio, sampleRate);
        const auto  model = analyzer.analyze (audio, sampleRate, f0);

        const float resid = analyzer.lastGridResidualCents();
        const int   obs   = analyzer.lastGridObservations();
        const float guard = analyzer.lastPitchGuardCents();

        std::printf ("    %-14s residuo %7.1f cents  (%3d picos)  banda %-8s  f0 %7.2f Hz  "
                     "guardia %5.0f cents  %d frame(s)  [%d ch @ %.0f Hz]\n",
                     expect.fileName, (double) resid, obs, bandOf (resid).toRawUTF8(),
                     (double) f0, (double) guard, model.frameCount,
                     audio.getNumChannels(), sampleRate);

        ++measured;

        check (resid >= expect.minResidCents && resid <= expect.maxResidCents,
               name + ": residuo " + juce::String (resid, 1) + " cents dentro del rango medido ["
                   + juce::String (expect.minResidCents, 1) + " .. "
                   + juce::String (expect.maxResidCents, 1) + "]");

        check (bandOf (resid) == juce::String (expect.band),
               name + ": y cae en la banda " + juce::String (expect.band)
                   + " (medido: " + bandOf (resid) + ")");

        check (obs >= expect.minObservations,
               name + ": sostenido por >= " + juce::String (expect.minObservations)
                   + " observaciones (medido: " + juce::String (obs) + ")");

        check (f0 > 20.0f && f0 < 5000.0f,
               name + ": la f0 detectada esta en el rango audible (" + juce::String (f0, 2) + " Hz)");

        // El PAR que la UI enseña junto tras ANALYZE: con un barrido la guardia
        // dispara (> pitchGuardCents) y el mecanismo estatico no lo representa;
        // con material cuasi-monotonico la guardia esta de pie (0).
        check (expect.firesPitchGuard ? guard > SpectralAnalyzer::pitchGuardCents
                                      : guard == 0.0f,
               name + ": guardia de pitch " + (expect.firesPitchGuard ? "DISPARA" : "de pie")
                   + " (" + juce::String (guard, 0) + " cents, umbral "
                   + juce::String (SpectralAnalyzer::pitchGuardCents, 0) + ")");
    }

    check (measured == kNumExpected,
           "los cinco WAV del banco se midieron (" + juce::String (measured) + "/"
               + juce::String (kNumExpected) + ")");

    std::printf ("\nRESULT: %s (%d fallos)\n",
                 failures == 0 ? "OK" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}

#else

int main()
{
    std::printf ("Residuo de rejilla sobre los cinco WAV del banco CZ101\n");
    std::printf ("  [skip] sin NEURONiK_CZ101_WAV_DIR: el banco hermano no esta "
                 "(../ABDCZ101/DOCS/patches)\n");
    std::printf ("\nRESULT: OK (0 WAV, banco ausente)\n");
    return 0;
}

#endif
