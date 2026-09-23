/*
  ==============================================================================

    TemporalAnalysisTest.cpp
    Created: 23 Sep 2026
    Description: FASE 10.4 — el emparejador del análisis temporal.

      1. Equivalencia: sobre UNA ventana (fftSize), analyzeTemporal(1 frame)
         == analyze() — el camino estático no cambió.
      2. Emparejador: cinco bloques de seno puro con offset sub-bin escalonado
         (+0/+9/+3/−6/−9 Hz) => cada frame debe medir el offset de SU bloque.
         La identidad de parcial es el índice k; la ventana de media banda f0
         garantiza que el parcial k nunca invade al vecino.
      3. Evolución de nivel: una señal que decae conserva el decaimiento entre
         frames (normalización GLOBAL, no por frame).
      4. Flujo completo temporal: analyzeTemporal -> export v2 (dialecto GUI)
         -> PresetManager::loadModelFromFile -> sampleFrame (z=0.5 con 3
         frames ES el frame 1 exacto; z=0.25 es el punto medio 0↔1).

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/FrameSampler.h"
#include "../Source/Serialization/PresetManager.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>
#include <vector>

using NEURONiK::Common::SpectralModel;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

constexpr int fftSize = 8192; // debe coincidir con SpectralAnalyzer::fftSize
constexpr double sr = 44100.0;
constexpr float f0 = 220.0f;

/** Seno puro de amplitud a con fase integrada (frecuencia exacta, sin
    sesgo de chirp: sin(2p f t) con f variable NO es FM). */
void fillSine (juce::AudioBuffer<float>& audio, float freq, float amp)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * (double) freq / sr;
    double phase = 0.0;
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        audio.setSample (0, i, (float) (amp * std::sin (phase)));
        phase += w;
        if (phase > 2.0 * juce::MathConstants<double>::pi)
            phase -= 2.0 * juce::MathConstants<double>::pi;
    }
}
} // namespace

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    NEURONiK::ModelMaker::Analysis::SpectralAnalyzer analyzer;

    // ---------- 1. Equivalencia sobre UNA ventana ----------
    {
        juce::AudioBuffer<float> audio (1, fftSize); // UNA ventana: ambos caminos ven lo mismo
        fillSine (audio, f0 + 4.0f, 0.6f);

        const auto mono  = analyzer.analyze (audio, sr, f0);
        const auto temp1 = analyzer.analyzeTemporal (audio, sr, f0, 1);

        bool same = true;
        for (int k = 0; k < 64 && same; ++k)
        {
            same &= std::abs (mono.amplitudes[(size_t) k] - temp1.amplitudes[(size_t) k]) < 1.0e-6f;
            same &= std::abs (mono.frequencyOffsets[(size_t) k] - temp1.frequencyOffsets[(size_t) k]) < 1.0e-4f;
        }
        check (same && temp1.frameCount == 1,
               "analyzeTemporal(1 frame) == analyze() sobre una ventana");

        check (std::abs (temp1.amplitudes[0] - 1.0f) < 1.0e-4f,
               "el pico del fundamental queda a 1.0 (normalizacion)");
    }

    // ---------- 2. El emparejador: bloques escalonados ----------
    {
        // 5 ventanas EXACTAS (num = 5*fftSize): la rejilla de frames del
        // analizador cae en los inicios de bloque. Cada bloque un seno puro
        // con offset distinto: la expectativa por frame es EXACTA.
        constexpr int nBlocks = 5;
        const float steps[nBlocks] = { 0.0f, 9.0f, 3.0f, -6.0f, -9.0f };

        juce::AudioBuffer<float> audio (1, nBlocks * fftSize);
        for (int b = 0; b < nBlocks; ++b)
            for (int i = 0; i < fftSize; ++i)
            {
                const double w = 2.0 * juce::MathConstants<double>::pi
                                 * (double) (f0 + steps[b]) / sr;
                audio.setSample (0, b * fftSize + i, (float) (0.6 * std::sin (w * (double) i)));
            }

        const auto model = analyzer.analyzeTemporal (audio, sr, f0, nBlocks);
        check (model.frameCount == nBlocks, "analyzeTemporal produce los 5 frames pedidos");
        check (model.isValid, "el modelo temporal queda valido");
        check (std::abs (model.frameSpanHz - f0) < 1.0e-4f, "frameSpanHz = f0 del analisis");

        bool follows = true;
        for (int fr = 0; fr < nBlocks; ++fr)
        {
            const float got = fr == 0 ? model.frequencyOffsets[0]
                                      : model.extraOffsets[(size_t) fr - 1][0];
            std::printf ("    frame %d: offset=%+.2f Hz (bloque %+.1f Hz)\n",
                         fr, (double) got, (double) steps[fr]);
            follows &= std::abs (got - steps[fr]) < 1.0f; // precision sub-bin de la fase 9
        }
        check (follows, "cada frame mide el offset de SU bloque (emparejador por indice)");
    }

    // ---------- 3. Evolucion de nivel (normalizacion GLOBAL) ----------
    {
        juce::AudioBuffer<float> audio (1, 4 * fftSize);
        for (int b = 0; b < 4; ++b)
        {
            // amplitud del bloque: decae 1.0 -> 0.5 -> 0.25 -> 0.125
            const double w = 2.0 * juce::MathConstants<double>::pi * (double) f0 / sr;
            const float amp = (float) std::pow (0.5, b);
            for (int i = 0; i < fftSize; ++i)
                audio.setSample (0, b * fftSize + i, (float) (0.6 * amp * std::sin (w * (double) i)));
        }

        const auto model = analyzer.analyzeTemporal (audio, sr, f0, 4);

        const float a0 = model.amplitudes[0];
        const float a3 = model.extraAmps[2][0];
        check (a0 > 0.6f && a3 < 0.45f && a0 > a3 * 1.5f,
               "el decaimiento se conserva entre frames (normalizacion global)");
        std::printf ("    amp frame0=%.3f  amp frame3=%.3f\n", (double) a0, (double) a3);
    }

    // ---------- 4. Flujo completo temporal (export v2 + lector + sampler) ----------
    {
        constexpr int nBlocks = 3;
        const float steps[nBlocks] = { 0.0f, 9.0f, -9.0f };

        juce::AudioBuffer<float> audio (1, nBlocks * fftSize);
        for (int b = 0; b < nBlocks; ++b)
            for (int i = 0; i < fftSize; ++i)
            {
                const double w = 2.0 * juce::MathConstants<double>::pi
                                 * (double) (f0 + steps[b]) / sr;
                audio.setSample (0, b * fftSize + i, (float) (0.6 * std::sin (w * (double) i)));
            }

        const auto model = analyzer.analyzeTemporal (audio, sr, f0, nBlocks);

        // Export v2 con el dialecto EXACTO de la GUI (escritor de produccion).
        const float span = model.frameSpanHz;
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        juce::Array<juce::var> amps, offs;
        for (int i = 0; i < 64; ++i)
        {
            const float gap = span > 0.0f ? span : (float) (i + 1);
            amps.add (model.amplitudes[(size_t) i]);
            offs.add (juce::jlimit (-0.5f * gap, 0.5f * gap, model.frequencyOffsets[(size_t) i]));
        }
        obj->setProperty ("amplitudes", amps);
        obj->setProperty ("frequencyOffsets", offs);
        obj->setProperty ("name", "temporal escalonado");
        obj->setProperty ("description", "Created with NEURONiK Model Maker");

        juce::Array<juce::var> frames;
        juce::DynamicObject::Ptr f0obj = new juce::DynamicObject();
        f0obj->setProperty ("amplitudes", amps);
        f0obj->setProperty ("frequencyOffsets", offs);
        frames.add (f0obj.get());
        for (int fr = 1; fr < model.frameCount; ++fr)
        {
            juce::Array<juce::var> fa, fo;
            for (int i = 0; i < 64; ++i)
            {
                const float gap = span > 0.0f ? span : (float) (i + 1);
                fa.add (model.ampAt (fr, i));
                fo.add (juce::jlimit (-0.5f * gap, 0.5f * gap, model.offsetAt (fr, i)));
            }
            juce::DynamicObject::Ptr fo_ = new juce::DynamicObject();
            fo_->setProperty ("amplitudes", fa);
            fo_->setProperty ("frequencyOffsets", fo);
            frames.add (fo_.get());
        }
        obj->setProperty ("format", 2);
        obj->setProperty ("frames", frames);
        obj->setProperty ("frameSpanHz", span);

        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("neuronik_temporal_test.neuronikmodel");
        file.replaceWithText (juce::JSON::toString (juce::var (obj.get())));

        const auto back = NEURONiK::Serialization::PresetManager::loadModelFromFile (file);
        check (back.isValid && back.frameCount == nBlocks,
               "el lector recupera los 3 frames del modelo temporal");
        check (std::abs (back.frameSpanHz - f0) < 1.0e-4f, "span recuperado");

        // Semantica REAL del sampler: z = k/(n-1) cae EXACTO en el frame k.
        // Los extremos y el frame intermedio se comparan contra los offsets
        // ALMACENADOS (roundtrip exacto); el tracking contra los pasos
        // ideales ya lo pino la seccion 2 del emparejador.
        SpectralModel out;
        NEURONiK::Common::sampleFrame (back, 0.0f, out);
        check (std::abs (out.frequencyOffsets[0] - back.frequencyOffsets[0]) < 1.0e-6f,
               "z=0 reproduce el frame 0 (bit-exacto)");
        NEURONiK::Common::sampleFrame (back, 1.0f, out);
        check (std::abs (out.frequencyOffsets[0] - back.offsetAt (2, 0)) < 1.0e-6f,
               "z=1 reproduce el frame 2 (bit-exacto)");
        NEURONiK::Common::sampleFrame (back, 0.5f, out);
        check (std::abs (out.frequencyOffsets[0] - back.offsetAt (1, 0)) < 1.0e-6f,
               "z=0.5 con 3 frames ES el frame 1 exacto (t = z*(n-1) = 1)");
        NEURONiK::Common::sampleFrame (back, 0.25f, out);
        const float mid = 0.5f * (back.frequencyOffsets[0] + back.offsetAt (1, 0));
        check (std::abs (out.frequencyOffsets[0] - mid) < 1.0e-6f,
               "z=0.25 es el punto medio aritmetico de los frames 0 y 1");

        file.deleteFile();
    }

    std::printf ("\n%s (%d fallos)\n", failures == 0 ? "RESULT: OK" : "RESULT: FAIL", failures);
    return failures == 0 ? 0 : 1;
}
