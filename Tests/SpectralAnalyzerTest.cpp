/*
  ==============================================================================

    SpectralAnalyzerTest.cpp
    Created: 22 Sep 2026
    Description: El analizador del ModelMaker (multiframe + peak-picking +
                 offsets sub-bin) verificado como numero:

                   1. generateHarmonicTone (como el RoundTrip) -> detectPitch
                      -> analyze: tabla normalizada, fundamental dominante.
                   2. Un seno puro: el parcial medio debe caer sobre su bin
                      (offset ~ 0: el floor no inventa inharmonicidad).
                   3. Inharmonicidad REAL (offsets fijos en la fuente): el
                      analizador la reconstruye en frequencyOffsets — el
                      canal que antes era un TODO a cero.
                   4. Multiframe: un ataque fuerte al inicio NO domina ya el
                      modelo (las ventanas se reparten por todo el fichero).
                 5. Fundamental debil con sub-octava fuerte (CZ-SWEP1): el
                      HPS refinado no cae al sub-armonico (62 Hz) y pilla la
                      nota del patch (124 Hz).

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"
#include "../Source/Common/SpectralModel.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <iostream>

namespace
{
    using namespace NEURONiK;

    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  [ok]   " << description << '\n';
            return;
        }
        std::cout << "  [FAIL] " << description << '\n';
        ++failures;
    }

    /** Tono armonico 1/n como el del RoundTrip, con offsets e inharmonicidad
        opcionales y envolvente opcional (ataque fuerte al principio). */
    juce::AudioBuffer<float> generateTone (double sampleRate, float f0, int numSamples,
                                           float offsetPerPartial = 0.0f,
                                           bool strongAttack = false)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            double sample = 0.0;

            for (int n = 1; n <= 8; ++n) // 8 parciales: los que el test examina
            {
                const auto decay = 1.0 / (double) n;
                // Parcial n en n*(f0+offset) Hz: offset real por parcial.
                const auto freq = 2.0 * juce::MathConstants<double>::pi * (double) n
                                    * ((double) f0 + (double) offsetPerPartial);
                sample += decay * std::sin (freq * t);
            }

            float value = (float) (0.6 * sample / 2.0);
            if (strongAttack)
            {
                // Los primeros 20 ms: transitorio 10x (un "golpe" de la fuente).
                if (i < (int) (0.02 * sampleRate))
                    value *= 10.0f;
            }
            left[i] = value;
            right[i] = value;
        }

        return buffer;
    }
}

int main()
{
    constexpr double sampleRate = 44100.0;
    constexpr float f0 = 440.0f;
    constexpr int numSamples = (int) sampleRate; // 1 s: varias ventanas

    NEURONiK::ModelMaker::Analysis::SpectralAnalyzer analyzer;

    std::cout << "SpectralAnalyzer: multiframe + peak-picking + offsets sub-bin\n";

    // --- 1. Tono armonico: pitcheo + tabla normalizada -----------------------
    std::cout << "\nTono armonico 1/n\n";
    {
        const auto source = generateTone (sampleRate, f0, numSamples);
        const auto detected = analyzer.detectPitch (source, sampleRate);
        check (std::abs (detected - f0) < 10.0f,
               "detectPitch encuentra la fundamental: " + juce::String (detected, 2) + " Hz");

        const auto model = analyzer.analyze (source, sampleRate, detected);
        check (model.amplitudes[0] > 0.5f && model.amplitudes[0] <= 1.0f + 1.0e-4f,
               "tabla normalizada con fundamental dominante: " + juce::String (model.amplitudes[0], 3));

        // Un seno limpio muestreado en bins de 5.38 Hz: el pico local puede
        // caer hasta medio bin del nominal -> offset < 2.7 Hz (floor 0 si el
        // parcial queda bajo el suelo del ruido de ventana).
        // La propiedad que importa: root_detectado*n + offset_k debe caer
        // sobre la frecuencia REAL de la fuente (440*n). El offset cancela
        // el error residual de detectPitch (aqui ~1.4 Hz -> offset ~ -1.4*n)
        // y de paso codifica la desviacion verdadera: es el canal que los
        // motores renderizan como baseFreq*n + offset.
        bool reconstruye = true;
        for (int k = 0; k < 8; ++k)
        {
            const float n = (float) (k + 1);
            const float rendered = detected * n + model.frequencyOffsets[(size_t) k];
            if (std::abs (rendered - 440.0f * n) > 3.0f)
                reconstruye = false;
        }
        check (reconstruye, "root*n + offset reconstruye la frecuencia real de la fuente (<= 3 Hz)");
    }

    // --- 2. Inharmonicidad real reconstruida ---------------------------------
    std::cout << "\nInharmonicidad\n";
    {
        constexpr float realOffset = 6.0f; // Hz por parcial (cuerda rigida)
        const auto source = generateTone (sampleRate, f0, numSamples, realOffset);
        const auto model = analyzer.analyze (source, sampleRate, f0);

        // La fuente canta en n*(440+6) Hz; el modelo debe reproducirla:
        // root_detectado*n + offset_k ~ n*446 para los 8 parciales.
        bool losOcho = true;
        for (int k = 0; k < 8; ++k)
        {
            const float n = (float) (k + 1);
            const float rendered = f0 * n + model.frequencyOffsets[(size_t) k];
            if (std::abs (rendered - (440.0f + realOffset) * n) > 3.5f)
                losOcho = false;
        }
        check (losOcho, "los 8 parciales de la fuente inarmonica (446*n Hz) se reconstruyen (<= 3.5 Hz)");
    }

    // --- 3. Multiframe: el ataque ya no define el modelo ---------------------
    std::cout << "\nMultiframe\n";
    {
        const auto withAttack = generateTone (sampleRate, f0, numSamples, 0.0f, true);
        const auto withoutAttack = generateTone (sampleRate, f0, numSamples, 0.0f, false);

        const auto mWith = analyzer.analyze (withAttack, sampleRate, f0);
        const auto mWithout = analyzer.analyze (withoutAttack, sampleRate, f0);

        // Antes (una ventana al inicio) el transitorio 10x deformaba la tabla
        // entera. Con 6 ventanas repartidas, la diferencia debe ser pequena.
        double maxDiff = 0.0;
        for (int k = 0; k < 8; ++k)
            maxDiff = std::max (maxDiff,
                                (double) std::abs (mWith.amplitudes[(size_t) k]
                                                     - mWithout.amplitudes[(size_t) k]));
        check (maxDiff < 0.25,
               "el ataque fuerte apenas mueve la tabla (maxDiff=" + juce::String ((float) maxDiff, 3) + " < 0.25)");
    }

    // --- 4. Fundamental debil con sub-octava fuerte (CZ-SWEP1) ---------------
    std::cout << "\nFundamental debil (sub-octava fuerte)\n";
    {
        // Firma REAL del CZ-SWEP1 (medida por la sonda, serie de la raiz
        // 62): sub-octava 62 (0.39), fundamental 124 (1.0, dominante),
        // 186 (0.56), 248 practicamente vacio y migajas de banda ancha
        // encima — la nota del patch vive en 124 con un sub-oscilador
        // fuerte. El HPS clasico cae al sub-armonico (el producto en 62
        // multiplica los tres parciales reales que comparten).
        constexpr float f0Real = 124.0f;
        struct Partial { float freq; float amp; };
        constexpr Partial partials[] = {
            {  62.0f, 0.39f }, { 124.0f, 1.00f }, { 186.0f, 0.56f },
            { 248.0f, 0.05f }, { 372.0f, 0.06f },
        };
        juce::AudioBuffer<float> source (2, numSamples);
        source.clear();
        double peak = 0.0;
        for (const auto& p : partials)
            peak = juce::jmax (peak, (double) p.amp);
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            double s = 0.0;
            for (const auto& p : partials)
                s += (double) p.amp
                        * std::sin (2.0 * juce::MathConstants<double>::pi
                                        * (double) p.freq * t);
            const float v = (float) (0.6 * s / (1.9 * peak));
            source.setSample (0, i, v);
            source.setSample (1, i, v);
        }

        const auto detected = analyzer.detectPitch (source, sampleRate);
        check (std::abs (detected - f0Real) < 5.0f,
               "fundamental debil con sub-octava fuerte detecta la nota ("
                   + juce::String (detected, 2) + " Hz ~ "
                   + juce::String (f0Real, 1) + ")");

        // Y el modelo se analiza contra la raiz correcta: la ventana de
        // busqueda de cada parcial (mitad del espaciado - 1 bin) deja fuera
        // tanto la sub-octava de 62 como el medio-entero de 186, asi que la
        // tabla queda dominada por la fundamental, con el hueco de 248 casi
        // vacio (guardia de maximo local: sin fantasmas de faldas ajenas) y
        // el 3er parcial debil pero presente — el reparto REAL de la fuente.
        const auto model = analyzer.analyze (source, sampleRate, detected);
        check (model.amplitudes[(size_t) 0] > 0.8f
                   && model.amplitudes[(size_t) 0] > model.amplitudes[(size_t) 1]
                   && model.amplitudes[(size_t) 1] < 0.2f
                   && model.amplitudes[(size_t) 2] > 0.03f,
               "el modelo reproduce el reparto (fundamental dominante, hueco de "
                   "248 casi vacio: 124=" + juce::String (model.amplitudes[(size_t) 0], 3)
                   + " 248=" + juce::String (model.amplitudes[(size_t) 1], 3)
                   + " 372=" + juce::String (model.amplitudes[(size_t) 2], 3) + ")");

        // Y la via POR VENTANA (detectPitchFromSpectrum, la que alimenta
        // analyzeTemporal) tambien cae de pie sobre la firma del patch:
        // los frames detectan ~124 Hz y el modelo temporal no arrastra la
        // sub-octava a su trayectoria frameF0. Es la ruta que producia
        // 62.3 Hz en la ventana de cabecera antes del refinado.
        const auto temporal = analyzer.analyzeTemporal (source, sampleRate, detected, 4);
        check (temporal.numFrames() == 4, "el modelo temporal tiene los 4 frames pedidos");
        check (std::abs (temporal.f0At (0) - f0Real) < 5.0f
                   && std::abs (temporal.f0At (3) - f0Real) < 5.0f,
               "la via por ventana detecta la nota en frames 0 y 3 ("
                   + juce::String (temporal.f0At (0), 2) + " / "
                   + juce::String (temporal.f0At (3), 2) + " Hz ~ 124)");


    // --- 5. Guardia de desviacion de pitch --------------------------------
    std::cout << "\nGuardia de desviacion de pitch\n";
    {
        // (a) Cuasi-monotonico: seno con vibrato suave (+-6 cents) en 220 Hz.
        //     La guardia NO dispara: el modelo estatico es representable.
        juce::AudioBuffer<float> steady (2, numSamples);
        steady.clear();
        for (int i = 0; i < numSamples; ++i)
        {
            const double t = (double) i / sampleRate;
            const double f = 220.0 * std::pow (2.0, 0.006 * std::sin (
                2.0 * juce::MathConstants<double>::pi * 0.8 * t));
            const double s = 0.5 * std::sin (2.0 * juce::MathConstants<double>::pi * f * t)
                                 + 0.2 * std::sin (2.0 * juce::MathConstants<double>::pi * 2.0 * f * t);
            const float v = (float) s;
            steady.setSample (0, i, v);
            steady.setSample (1, i, v);
        }
        analyzer.measurePitchDeviation (steady, sampleRate, 220.0f);
        check (analyzer.lastPitchGuardCents() == 0.0f,
               "material cuasi-monotonico: guardia a 0 ("
                   + juce::String (analyzer.lastPitchGuardCents(), 1) + " cents)");

        // (b) Barrido real: chirp exponencial de una octava (220 -> 440 Hz).
        //     Una sola rejilla no lo describe: la guardia DISPARA y el
        //     llamador sabe que el estatico saldria des-afinado.
        juce::AudioBuffer<float> sweepBuf (2, numSamples);
        sweepBuf.clear();
        {
            double phase = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                const double t = (double) i / sampleRate;
                const double f = 220.0 * std::pow (2.0, t);
                phase += 2.0 * juce::MathConstants<double>::pi * f / sampleRate;
                const float v = (float) (0.5 * std::sin (phase)
                                             + 0.12 * std::sin (2.0 * phase));
                sweepBuf.setSample (0, i, v);
                sweepBuf.setSample (1, i, v);
            }
        }
        analyzer.measurePitchDeviation (sweepBuf, sampleRate, 220.0f);
        check (analyzer.lastPitchGuardCents() > 150.0f,
               "barrido de una octava dispara la guardia ("
                   + juce::String (analyzer.lastPitchGuardCents(), 0) + " cents)");
    }
    }

    std::cout << "\nRESULT: " << ((failures == 0) ? "OK" : "FAIL") << " (" << failures << " fallos)\n";
    return (failures == 0) ? 0 : 1;
}
