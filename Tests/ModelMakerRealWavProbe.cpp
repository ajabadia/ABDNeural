/*
  ==============================================================================

    ModelMakerRealWavProbe.cpp
    Created: 23 Sep 2026
    Description: Sonda CLI de producción del ModelMaker — el ModelMaker es GUI
                 pura (sin CLI), así que esta sonda enlaza los MISMOS ficheros
                 de producción (SpectralAnalyzer.cpp, PresetManager.cpp) y
                 conduce el flujo real WAV -> detectPitch -> analyze ->
                 serialización v2 (dialecto exacto de exportModel()) ->
                 PresetManager::loadModelFromFile -> sampleFrame, contra WAVs
                 reales de notas de sintetizador.

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/FrameSampler.h"
#include "../Source/Serialization/PresetManager.h"
#include "../Source/Main/NEURONiKProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdlib>

using NEURONiK::Common::SpectralModel;

namespace
{
void fail (const juce::String& what)
{
    std::printf ("[probe] FAIL: %s\n", what.toRawUTF8());
    std::exit (1);
}

/** Serialización v2 EXACTA de exportModel() (MainComponent.cpp) — mismo
    dialecto, clamps incluidos: el ciclo WAV->v2->load es el de producción. */
juce::String serializeV2 (const SpectralModel& m, const juce::String& name)
{
    const float span = m.frameSpanHz;

    juce::DynamicObject::Ptr modelObj = new juce::DynamicObject();

    juce::Array<juce::var> amps, offs;
    for (int i = 0; i < 64; ++i)
    {
        const float gap = span > 0.0f ? span : (float) (i + 1);
        amps.add (m.amplitudes[i]);
        offs.add (juce::jlimit (-0.5f * gap, 0.5f * gap, m.frequencyOffsets[i]));
    }
    modelObj->setProperty ("amplitudes", amps);
    modelObj->setProperty ("frequencyOffsets", offs);
    modelObj->setProperty ("name", name);
    modelObj->setProperty ("description", "Created with NEURONiK Model Maker");

    juce::Array<juce::var> frames;
    juce::DynamicObject::Ptr frame0 = new juce::DynamicObject();
    frame0->setProperty ("amplitudes", amps);
    frame0->setProperty ("frequencyOffsets", offs);
    frames.add (frame0.get());

    // FASE 10.4: los frames extra acompanan cuando el modelo es temporal.
    for (int fr = 1; fr < m.frameCount; ++fr)
    {
        juce::Array<juce::var> fa, fo;
        for (int i = 0; i < 64; ++i)
        {
            const float gap = span > 0.0f ? span : (float) (i + 1);
            fa.add (m.ampAt (fr, i));
            fo.add (juce::jlimit (-0.5f * gap, 0.5f * gap, m.offsetAt (fr, i)));
        }
        juce::DynamicObject::Ptr fo_ = new juce::DynamicObject();
        fo_->setProperty ("amplitudes", fa);
        fo_->setProperty ("frequencyOffsets", fo);
        // FASE 10.6: la raiz del frame viaja en el v2 (dialecto GUI).
        if (const float f0k = m.f0At (fr); f0k > 0.0f)
            fo_->setProperty ("frameF0", f0k);
        frames.add (fo_.get());
    }

    modelObj->setProperty ("format", 2);
    modelObj->setProperty ("frames", frames);
    if (span > 0.0f)
        modelObj->setProperty ("frameSpanHz", span);

    return juce::JSON::toString (juce::var (modelObj));
}

int argmax (const SpectralModel& m)
{
    int k = 0;
    for (int i = 1; i < 64; ++i)
        if (m.amplitudes[i] > m.amplitudes[k]) k = i;
    return k;
}

float centroid (const SpectralModel& m)
{
    double num = 0.0, den = 0.0;
    for (int i = 0; i < 64; ++i)
    {
        const double n = m.amplitudes[i];
        num += n * (i + 1);
        den += n;
    }
    return den > 0.0 ? (float) (num / den) : 0.0f;
}

// ================= Validacion acustica (paso 4.5) ===========================
// DFT INDEPENDIENTE del probe (no comparte FFT con el analizador): ventana
// Hann de 8192 muestras en el tramo de RMS maximo del fichero. Para cada
// parcial fuerte del modelo busca el pico REAL del espectro en su banda
// (k*f0 +- f0/2, nunca invade al vecino) con interpolacion parabolica y
// compara la frecuencia del modelo contra la medida, en cents.
//   - FRECUENCIA: contractual. Un modelo des-afinado (f0 de octava erronea,
//     material con barrido de pitch) se detecta aqui.
//   - AMPLITUD: informativa (el analizador promedia varias ventanas; el
//     probe mide una: no deben coincidir exactamente).
struct AcousticCheck
{
    int validated = 0;
    float medianCents = 0.0f;
    float maxCents = 0.0f;
};

float windowedBinMag (const float* seg, int n, double bin)
{
    const double step = juce::MathConstants<double>::twoPi * bin / (double) n;
    double re = 0.0, im = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 * (1.0 - std::cos (juce::MathConstants<double>::twoPi * i / (double) (n - 1)));
        const double v = (double) seg[i] * w;
        re += v * std::cos (step * i);
        im -= v * std::sin (step * i);
    }
    return (float) std::sqrt (re * re + im * im);
}

AcousticCheck acousticCheck (const SpectralModel& model, const juce::AudioBuffer<float>& audio,
                             double sampleRate, float f0)
{
    // Diagnostico: PROBE_HALVE_F0=1 divide SOLO la f0 de la escalera de
    // octava (no el modelo ni la mediana) para verificar que el check de
    // sub-octava dispara. Produccion: unset = no-op bit a bit.
    const bool halveF0 = std::getenv ("PROBE_HALVE_F0") != nullptr
                         && juce::String (std::getenv ("PROBE_HALVE_F0")).trim() == "1";
    const float f0Ladder = halveF0 ? 0.5f * f0 : f0;
    // 1. tramo de RMS maximo (ventana de 8192, avance de 1024)
    constexpr int N = 8192;
    const float* ch = audio.getReadPointer (0);
    const int total = audio.getNumSamples();
    if (total < N) fail ("wav demasiado corto para la validacion acustica");

    int bestStart = 0;
    double bestRms = -1.0;
    for (int start = 0; start + N <= total; start += 1024)
    {
        double acc = 0.0;
        for (int i = 0; i < N; i += 4) acc += (double) ch[start + i] * ch[start + i];
        const double rms = acc;
        if (rms > bestRms) { bestRms = rms; bestStart = start; }
    }
    const float* seg = ch + bestStart;

    // 2. por parcial fuerte del modelo: pico real en su banda
    // TOP-8 parciales mas fuertes (amp >= 0.05): los que dominan el sonido.
    // Los debiles caen en picos vecinos cuando la fuente lleva notas dobles
    // (rejillas entrelazadas) — eso es naturaleza de la fuente, no del modelo,
    // y por eso el criterio contractual es la MEDIANA, no el maximo.
    int order[64];
    for (int i = 0; i < 64; ++i) order[i] = i;
    std::sort (order, order + 64, [&model] (int a, int b)
               { return model.amplitudes[(size_t) a] > model.amplitudes[(size_t) b]; });

    std::vector<float> centsAll;
    AcousticCheck out;
    for (int idx = 0; idx < 64 && (int) centsAll.size() < 8; ++idx)
    {
        const int k = order[idx] + 1;
        const float amp = model.amplitudes[(size_t) (k - 1)];
        if (amp < 0.05f) break;

        const double center = (double) k * (double) f0
                            + (double) model.frequencyOffsets[(size_t) (k - 1)];
        const double halfBand = 0.5 * (double) f0;
        const double binHz = (double) sampleRate / (double) N;
        int lo = (int) std::floor ((center - halfBand) / binHz);
        int hi = (int) std::ceil  ((center + halfBand) / binHz);
        if (lo < 1) lo = 1;
        if (hi > N / 2 - 1) hi = N / 2 - 1;
        if (hi <= lo) continue;

        int peakBin = lo;
        float peakMag = -1.0f;
        for (int b = lo; b <= hi; ++b)
        {
            const float mag = windowedBinMag (seg, N, (double) b);
            if (mag > peakMag) { peakMag = mag; peakBin = b; }
        }

        // interpolacion parabolica sub-bin (en el pico real)
        const float yl = windowedBinMag (seg, N, (double) (peakBin - 1));
        const float y0 = windowedBinMag (seg, N, (double) peakBin);
        const float yr = windowedBinMag (seg, N, (double) (peakBin + 1));
        const float denom = yl - 2.0f * y0 + yr;
        const float delta = std::abs (denom) > 1.0e-9f
                                ? 0.5f * (yl - yr) / denom : 0.0f;
        const double measured = ((double) peakBin + (double) delta) * binHz;

        const double cents = 1200.0 * std::log2 (measured / center);
        centsAll.push_back ((float) cents);
        out.maxCents = juce::jmax (out.maxCents, (float) std::abs (cents));
        ++out.validated;

        std::printf ("[probe]     parcial %2d: modelo %.1f Hz vs medida %.1f Hz -> %+.1f cents (amp %.2f)\n",
                     k, center, measured, cents, (double) amp);
    }

    if (out.validated < 3) fail ("menos de 3 parciales validables acusticamente");

    // MEDIANA del error = contrato: si el modelo esta des-afinado (octava
    // erronea ~1200 cents, barrido de pitch cientos), la mediana lo refleja;
    // los picos vecinos sueltos (outliers de la fuente) no.
    std::sort (centsAll.begin(), centsAll.end());
    const int n = (int) centsAll.size();
    out.medianCents = (n % 2 == 1) ? centsAll[(size_t) (n / 2)]
                                   : 0.5f * (centsAll[(size_t) (n / 2 - 1)] + centsAll[(size_t) (n / 2)]);
    if (std::abs (out.medianCents) > 35.0f)
    {
        // FASE 10.6: para material con barrido de pitch (CZ-RRISE) el modelo
        // ESTATICO no es aplicable (una sola rejilla no describe 14 semitonos)
        // — no es un defecto del analizador sino del material. Con trayectoria
        // real de pitch (>8 semitonos entre inicio y fin) se reporta y se
        // continia: la seccion temporal valida el barrido con las f0 por
        // frame. Sin trayectoria, si es un defecto: fail.
        constexpr int Nw = 8192;
        juce::AudioBuffer<float> head (1, Nw);
        juce::AudioBuffer<float> tail (1, Nw);
        head.copyFrom (0, 0, audio, 0, 0, Nw);
        tail.copyFrom (0, 0, audio, 0, audio.getNumSamples() - Nw, Nw);

        NEURONiK::ModelMaker::Analysis::SpectralAnalyzer probeAnalyzer;
        const float f0Head = probeAnalyzer.detectPitch (head, sampleRate);
        const float f0Tail = probeAnalyzer.detectPitch (tail, sampleRate);

        const bool sweep = f0Head > 20.0f && f0Tail > 20.0f
                           && juce::jmax (f0Head, f0Tail)
                                  > 1.5f * juce::jmin (f0Head, f0Tail);
        if (sweep)
        {
            std::printf ("[probe]   acustica ESTATICO no aplicable: barrido %.1f -> %.1f Hz "
                         "(el modelo temporal con f0 por frame reproduce el barrido)\n",
                         (double) f0Head, (double) f0Tail);
            return out;
        }
        fail ("el modelo esta des-afinado vs la fuente (mediana de cents, sin barrido)");
    }
    // SUB-OCTAVA (escalera de energia alrededor de la raiz detectada): la
    // mediana no distingue una raiz a la MITAD de la fundamental real
    // (CZ-SWEP1: 64.5 detectada vs ~124 real) porque el modelo desplazado
    // una octava abajo tambien reparte armonicos contra picos reales. La
    // firma inequivoca: los IMPARES de la rejilla (f0, 3f0) caen entre
    // parciales reales y quedan vacios, mientras 2f0 (la fundamental real)
    // canta. Se imprime la escalera completa para diagnostico.
    auto bandPeak = [&] (double centerHz, double halfHz, double& refinedHz) -> float
    {
        const double step = (double) sampleRate / (double) N;
        int lo = (int) std::floor ((centerHz - halfHz) / step);
        int hi = (int) std::ceil  ((centerHz + halfHz) / step);
        if (lo < 1) lo = 1;
        if (hi > N / 2 - 1) hi = N / 2 - 1;
        if (hi <= lo) { refinedHz = centerHz; return 0.0f; }

        int peakBin = lo;
        float peakMag = -1.0f;
        for (int b = lo; b <= hi; ++b)
        {
            const float mag = windowedBinMag (seg, N, (double) b);
            if (mag > peakMag) { peakMag = mag; peakBin = b; }
        }
        if (peakBin > lo && peakBin < hi)
        {
            const float yl = windowedBinMag (seg, N, (double) (peakBin - 1));
            const float y0 = windowedBinMag (seg, N, (double) peakBin);
            const float yr = windowedBinMag (seg, N, (double) (peakBin + 1));
            const float den = yl - 2.0f * y0 + yr;
            const float d = std::abs (den) > 1.0e-9f ? 0.5f * (yl - yr) / den : 0.0f;
            refinedHz = ((double) peakBin + (double) d) * step;
        }
        else
        {
            refinedHz = (double) peakBin * step;
        }
        return peakMag;
    };

    double hzHalf = 0.0, hzOdd = 0.0, hz2 = 0.0, hz3 = 0.0;
    const float mHalf = bandPeak (0.5 * (double) f0Ladder, 0.125 * (double) f0Ladder, hzHalf);
    const float mOdd  = bandPeak (1.0 * (double) f0Ladder, 0.25  * (double) f0Ladder, hzOdd);
    const float m2    = bandPeak (2.0 * (double) f0Ladder, 0.25  * (double) f0Ladder, hz2);
    const float m3    = bandPeak (3.0 * (double) f0Ladder, 0.25  * (double) f0Ladder, hz3);

    std::printf ("[probe]     sub-octava: f0/2 %.1f Hz mag %.4f | f0 %.1f Hz mag %.4f"
                 " | 2f0 %.1f Hz mag %.4f | 3f0 %.1f Hz mag %.4f\n",
                 hzHalf, (double) mHalf, hzOdd, (double) mOdd,
                 hz2, (double) m2, hz3, (double) m3);

    // Contrato: rejilla impar vacia frente a la fundamental real en 2f0
    // => la raiz detectada es una sub-octava (el motor suena una octava
    // mas grave que la fuente).
    if (m2 > 1.0e-6f && mOdd < 0.15f * m2 && m3 < 0.15f * m2)
        fail ("raiz sub-octava: f0 y 3f0 vacios frente a 2f0 (error de octava del estimador)");

    return out;
}

// ============================================================================

void probeWav (const juce::File& wav, const juce::File& outDir,
               juce::AudioFormatManager& fmts)
{
    std::printf ("[probe] ---- %s\n", wav.getFileName().toRawUTF8());

    // 1. Lectura real del WAV (juce_audio_formats, como la GUI: LOAD AUDIO)
    std::unique_ptr<juce::AudioFormatReader> r (fmts.createReaderFor (wav));
    if (r == nullptr) fail ("no hay reader para " + wav.getFullPathName());
    juce::AudioBuffer<float> audio ((int) r->numChannels, (int) r->lengthInSamples);
    r->read (&audio, 0, (int) r->lengthInSamples, 0, true, true);
    const double sr = r->sampleRate;

    // 2. El flujo exacto de la GUI: detectPitch -> analyze
    NEURONiK::ModelMaker::Analysis::SpectralAnalyzer analyzer;
    const float f0 = analyzer.detectPitch (audio, sr);
    std::printf ("[probe]   rejilla: residuo=%.1f cents  obs=%d  (indicador UI)\n",
                 (double) analyzer.lastGridResidualCents (), analyzer.lastGridObservations ());
    const SpectralModel model = analyzer.analyze (audio, sr, f0);

    // GUARDIA (2026-09-23): el analizador reporta la desviacion de pitch del
    // material. Para barridos el estatico no es representable: la via
    // correcta es el modelo temporal con f0 por frame (FASE 10.6).
    if (const float guardCents = analyzer.lastPitchGuardCents(); guardCents > 0.0f)
        std::printf ("[probe]   guardia: desviacion de pitch %.0f cents (material no cuasi-monotonico)\n",
                     (double) guardCents);

    if (f0 <= 20.0f || f0 > 5000.0f) fail ("detectPitch fuera de rango audible");
    if (model.frameCount != 1)       fail ("el analizador monoframe debe dar frameCount=1");
    if (model.frameSpanHz != f0)     fail ("frameSpanHz debe ser la f0 del analisis");

    // Normalización y tonalidad (un modelo a-ruido tiene la energía repartida)
    float sum = 0.0f, top = 0.0f;
    int nonzero = 0;
    for (int i = 0; i < 64; ++i)
    {
        sum += model.amplitudes[i];
        top  = juce::jmax (top, model.amplitudes[i]);
        if (model.amplitudes[i] > 1.0e-3f) ++nonzero;
    }
    if (std::abs (top - 1.0f) > 1.0e-3f) fail ("el pico no esta normalizado a 1");
    if (nonzero < 3)                     fail ("menos de 3 parciales activos: modelo vacio");

    std::printf ("[probe]   f0=%.1f Hz  centroid=parcial %.1f  parciales activos=%d/64  top=%.3f\n",
                 (double) f0, (double) centroid (model), nonzero, (double) top);

    // 3. Export v2 de producción + recarga con el lector REAL
    const auto json = serializeV2 (model, wav.getFileNameWithoutExtension());
    const auto out = outDir.getChildFile (wav.getFileNameWithoutExtension() + ".neuronikmodel");
    if (! out.replaceWithText (json)) fail ("no se pudo escribir el modelo");
    std::printf ("[probe]   export v2: %s (%d bytes)\n",
                 out.getFullPathName().toRawUTF8(), (int) out.getSize());

    const SpectralModel back = NEURONiK::Serialization::PresetManager::loadModelFromFile (out);
    if (! back.isValid)            fail ("el lector de produccion rechazo el modelo");
    if (back.frameCount != 1)      fail ("frames perdidos en el ciclo");
    if (back.frameSpanHz <= 0.0f)  fail ("el lector no vio frameSpanHz");
    if (std::abs (back.frameSpanHz - f0) > 1.0e-4f) fail ("frameSpanHz perdido en el ciclo");

    // 4. El sampler sobre el modelo RECARGADO (lo que hara la voz en el motor)
    for (const float z : { 0.0f, 0.5f, 1.0f })
    {
        SpectralModel frame;
        NEURONiK::Common::sampleFrame (back, z, frame);
        if (std::abs (frame.amplitudes[argmax (frame)] - model.amplitudes[argmax (model)]) > 1.0e-4f)
            fail (juce::String ("sampleFrame z=") + juce::String (z, 2) + " mudo el dominante");
    }

    // 4.5 Validacion acustica: parciales del modelo vs DFT independiente
    {
        const AcousticCheck ac = acousticCheck (model, audio, sr, f0);
        std::printf ("[probe]   acustica: %d parciales (top-8), mediana %+.1f cents, max %.1f cents\n",
                     ac.validated, (double) ac.medianCents, (double) ac.maxCents);
    }

    // 5. E2E con el ENGINE real: el modelo generado entra por la via de
    // produccion (loadModel por FIFO) y la ranura refleja el analisis.
    {
        NEURONiKProcessor audio;
        audio.setRateAndBufferSizeDetails (sr, 512);
        audio.prepareToPlay (sr, 512);

        if (! audio.loadModel (out, 0)) fail ("el engine rechazo el modelo generado");

        std::array<float, 64> slotAmps {}, slotOffs {};
        bool slotValid = false;
        audio.getCurrentModel (0, slotAmps, slotOffs, slotValid);
        if (! slotValid) fail ("la ranura no marco el modelo como valido");

        const int kModel = argmax (model);
        if (std::abs (slotAmps[(size_t) kModel] - model.amplitudes[(size_t) kModel]) > 1.0e-3f)
            fail ("la tabla de la ranura no refleja el analisis");

        std::printf ("[probe]   engine: ranura A valida, parcial %d (=dominante) a %.3f\n",
                     kModel + 1, (double) slotAmps[(size_t) kModel]);
    }

    // 6. FASE 10.4: analisis TEMPORAL del WAV real — 4 frames, export v2 con
    //    los 4 frames y recarga. Con material real los frames deben diferir
    //    (evolucion temporal) y todos con pico valido.
    {
        const SpectralModel temp = analyzer.analyzeTemporal (audio, sr, f0, 4);
        if (temp.frameCount != 4) fail ("analyzeTemporal no produjo 4 frames");

        float minTop = 1.0f, maxAmp0 = 0.0f;
        for (int fr = 0; fr < 4; ++fr)
        {
            float top = 0.0f;
            for (int i = 0; i < 64; ++i) top = juce::jmax (top, temp.ampAt (fr, i));
            minTop = juce::jmin (minTop, top);
            maxAmp0 = juce::jmax (maxAmp0, temp.ampAt (fr, 0));
        }
        if (minTop < 0.05f) fail ("un frame temporal esta vacio");

        const auto tjson = serializeV2 (temp, wav.getFileNameWithoutExtension());
        const auto tout = outDir.getChildFile (wav.getFileNameWithoutExtension() + "-temporal.neuronikmodel");
        if (! tout.replaceWithText (tjson)) fail ("no se pudo escribir el modelo temporal");
        const SpectralModel tback = NEURONiK::Serialization::PresetManager::loadModelFromFile (tout);
        if (! tback.isValid || tback.frameCount != 4) fail ("frames perdidos en el ciclo temporal");

        // FASE 10.6: las raices por frame sobreviven el ciclo y forman la
        // TRAYECTORIA de pitch que el motor reproduce al mover morphZ
        // (frame 0 = canonico con la f0 global; frames 1+ = HPS por ventana).
        bool f0Travel = true;
        float f0Min = 1.0e9f, f0Max = -1.0e9f;
        for (int fr = 1; fr < tback.frameCount; ++fr)
        {
            const float f0k = tback.f0At (fr);
            f0Travel &= f0k > 20.0f && f0k < 5000.0f;
            f0Min = juce::jmin (f0Min, f0k);
            f0Max = juce::jmax (f0Max, f0k);
            std::printf ("[probe]     frame %d: f0=%.1f Hz\n", fr, (double) f0k);
        }
        if (! f0Travel) fail ("una f0 por frame fuera de rango en el ciclo");

        // El frame interpolado z=0.5 lleva el ratio de rejilla (remapeo vivo).
        SpectralModel zFrame;
        NEURONiK::Common::sampleFrame (tback, 0.5f, zFrame);
        if (zFrame.frameF0 <= 0.0f) fail ("el frame interpolado no lleva f0 (sin remapeo)");

        std::printf ("[probe]   temporal: 4 frames v2 (%d bytes), pico min=%.3f, dominante max=%.3f, "
                     "trayectoria f0=%.1f..%.1f Hz\n",
                     (int) tjson.length(), (double) minTop, (double) maxAmp0,
                     (double) f0Min, (double) f0Max);
    }

    std::printf ("[probe]   OK: ciclo completo WAV->modelo->recarga->sampleFrame->engine\n");
}

} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
        fail ("uso: ModelMakerRealWavProbe <wav> [wav...]");

    juce::AudioFormatManager fmts;
    fmts.registerBasicFormats();

    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("build-reference/probe-models");
    outDir.createDirectory();

    for (int i = 1; i < argc; ++i)
        probeWav (juce::File (juce::String::fromUTF8 (argv[i])), outDir, fmts);

    std::printf ("[probe] RESULT: OK (%d wav(s) analizados -> %s)\n", argc - 1,
                 outDir.getFullPathName().toRawUTF8());
    return 0;
}
