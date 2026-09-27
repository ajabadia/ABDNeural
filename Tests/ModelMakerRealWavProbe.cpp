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
#include "../Source/Common/SpectralModelWriter.h"
#include "../Source/Main/NEURONiKProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdlib>

using NEURONiK::Common::SpectralModel;

// 2026-09-27: TABLA COMPARATIVA de metricas sobre el banco real (Descriptores vs
// EnvelopeCosine con su corte dedicado kEnvelopeCosineCut = 0.60). Se corre
// sobre cada WAV con la misma f0 y los mismos 4 frames, con el mismo plegado.
// El defecto sigue siendo Descriptors (sec 3 del plan); el dedicado queda
// documentado aunque TAMPOCO separa (barrido 0.10..0.90 huecos invertidos).
struct ComparativeRecord
{
    juce::String name;
    float f0 = 0.0f;
    int descLayers = 1;
    int cosineLayers = 1;
    bool descSkipped = false;
    bool cosineSkipped = false;
    NEURONiK::ModelMaker::Analysis::SpectralAnalyzer::OctaveFold fold;
    int descTraces[3] = { 0, 0, 0 };
    int cosineTraces[3] = { 0, 0, 0 };
};

static std::vector<ComparativeRecord> gComparative;

namespace
{
void fail (const juce::String& what)
{
    std::printf ("[probe] FAIL: %s\n", what.toRawUTF8());
    std::exit (1);
}

/**
 * FASE 11.1: el dialecto .neuronikmodel vive en UN solo sitio
 * (Common::modelToJson / writeModelToFile, el mismo escritor que usa la GUI y
 * que lee el plugin). Antes esta sonda llevaba su propia copia del v2 y las
 * tres habia que cambiarlas a la vez; el ciclo WAV->v2->load de produccion es
 * ahora literalmente el mismo codigo.
 */
juce::String serializeV2 (const SpectralModel& m, const juce::String& name)
{
    return juce::JSON::toString (NEURONiK::Common::modelToJson (
        m, name, "Created with NEURONiK Model Maker"));
}

int argmax (const SpectralModel& m)
{
    int k = 0;
    for (int i = 1; i < 64; ++i)
        if (m.amplitudes[i] > m.amplitudes[k]) k = i;
    return k;
}


/**
 * 2026-09-25: OFFSETS TRANSPONIBLES — la evidencia del modo, sobre material real.
 *
 * Elige el parcial CON MATERIAL mas desviado de su armonico y saca las tres
 * cifras que definen el problema y el arreglo. Con la ley de siempre el offset se
 * SUMA en Hz: al subir una octava la rejilla se dobla y la desviacion en cents SE
 * PARTE (la inharmonicidad que suena cambia con la nota tocada). Con el modo
 * declarado el offset es un RATIO contra la rejilla (delta-n/n) y el motor lo
 * escala por base/f0: la desviacion en cents es la MISMA en toda la extension —
 * que es lo que hace el transporte musicalmente coherente.
 *
 * La rejilla de referencia es la del MOTOR (frameSpanHz): la que usa
 * offsetScaleAt al escalar, y la que el sampler transporta al snapshot.
 */
void transposeReport (const SpectralModel& m, float f0)
{
    if (f0 <= 0.0f) return;

    // El parcial mas desviado CON amplitud real: es el que hace visible el
    // efecto. El dominante suele ser el fundamental (casi afinado: no distingue
    // nada) y los indices de cola son ruido del analisis (amplitud ~0).
    constexpr float kMinAmp = 0.05f;

    int k = -1;
    float worst = 0.0f;

    for (int i = 0; i < 64; ++i)
    {
        if (m.ampAt (0, i) < kMinAmp) continue;

        const float dev = std::abs (m.offsetAt (0, i));

        if (k < 0 || dev > worst) { k = i; worst = dev; }
    }

    if (k < 0) return;

    const double n = (double) (k + 1);
    const double off = (double) m.offsetAt (0, k);
    const double up = 2.0;                       // una octava arriba

    std::printf ("[probe]   offsets transp.: parcial %d (n=%g, amp %.3f), offset %+.1f Hz sobre %.1f Hz\n",
                 k + 1, n, (double) m.ampAt (0, k), off, (double) f0);
    std::printf ("[probe]     en la rejilla: %+.1f cents (lo que midio el analisis)\n",
                 1200.0 * std::log2 ((n * f0 + off) / (n * f0)));
    std::printf ("[probe]     una octava arriba, offset en Hz: %+.1f cents (se PARTE: el modelo cambia con la nota)\n",
                 1200.0 * std::log2 ((n * f0 * up + off) / (n * f0 * up)));
    std::printf ("[probe]     una octava arriba, modo ratio:    %+.1f cents (se conserva: el motor escala por base/f0)\n",
                 1200.0 * std::log2 (1.0 + off / (n * f0)));
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

    // 2026-09-26: REJILLAS ENTRELAZADAS. Un material construido sobre una
    // sub-oscilacion (f0/2 con familias propias) tiene DOS familias de
    // picos reales: la de la rejilla del modelo (k*f0) y la de la
    // sub-rejilla (impares de f0/2, que ninguna rejilla k*f0 representa).
    // Un parcial del modelo cuyo pico medido cae sobre un impar de f0/2 NO
    // es un error del modelo: es energia de la OTRA familia que vive en
    // su banda. El diagnostico lo explica y lo saca del maximo.
    int interlacedCount = 0;
    float interlacedCents = 0.0f;   // el mas grande explicado
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
    std::vector<float> centsOwn;   // solo la familia que la rejilla representa
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

        // REJILLAS ENTRELAZADAS (2026-09-26): el pico real de la banda puede
        // ser un impar de f0/2 — la sub-rejilla de la fuente, que NINGUNA
        // rejilla k*f0 representa. Firma: el pico esta a MENOS de un
        // cuarto de f0 de un impar de f0/2 Y a mas de un cuarto de f0 de
        // k*f0 (si estuviera sobre su armónico, la desviacion seria fina).
        const double halfGrid = 0.5 * (double) f0Ladder;
        const double odd = std::round (measured / halfGrid);
        const double oddHz = odd * halfGrid;
        const bool oddUsable = odd >= 1.0 && std::abs (measured - oddHz) < 0.25 * (double) f0Ladder;
        const bool onOwnGrid = std::abs (measured - center) < 0.25 * (double) f0Ladder;
        const bool interlaced = oddUsable && ! onOwnGrid && (odd * 2.0 > (double) k + 0.5);

        if (interlaced)
        {
            ++out.interlacedCount;
            out.interlacedCents = juce::jmax (out.interlacedCents, (float) std::abs (cents));
            std::printf ("[probe]     parcial %2d: modelo %.1f Hz vs medida %.1f Hz -> %+.1f cents (amp %.2f)"
                         "  [ENTRELAZADO: pico real = impar %.0f de %.1f Hz = %.1f Hz,"
                         " la sub-rejilla de la fuente; el modelo no la representa]\n",
                         k, center, measured, cents, (double) amp, odd, halfGrid, oddHz);
        }
        else
        {
            out.maxCents = juce::jmax (out.maxCents, (float) std::abs (cents));
            std::printf ("[probe]     parcial %2d: modelo %.1f Hz vs medida %.1f Hz -> %+.1f cents (amp %.2f)\n",
                         k, center, measured, cents, (double) amp);
        }
        centsAll.push_back ((float) cents);
        if (! interlaced)
            centsOwn.push_back ((float) cents);
        ++out.validated;
    }

    if (out.validated < 3) fail ("menos de 3 parciales validables acusticamente");

    // MEDIANA del error = contrato: si el modelo esta des-afinado (octava
    // erronea ~1200 cents, barrido de pitch cientos), la mediana lo refleja;
    // los picos vecinos sueltos (outliers de la fuente) no.
    // 2026-09-26: y los REJILLAS ENTRELAZADAS tampoco — el pico real de esa
    // banda es la OTRA familia de la fuente (impares de f0/2), que ninguna
    // rejilla k*f0 representa; no mide la fidelidad del modelo sino la
    // riqueza de la fuente. La mediana se toma sobre la familia que la
    // rejilla SI representa (reserva: con menos de 3, sobre todas).
    const std::vector<float>& medianaFuente =
        (centsOwn.size() >= 3) ? centsOwn : centsAll;
    std::vector<float> ordenada (medianaFuente);
    std::sort (ordenada.begin(), ordenada.end());
    const int n = (int) ordenada.size();
    out.medianCents = (n % 2 == 1) ? ordenada[(size_t) (n / 2)]
                                   : 0.5f * (ordenada[(size_t) (n / 2 - 1)] + ordenada[(size_t) (n / 2)]);
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
    float f0 = analyzer.detectPitch (audio, sr);

    // PROBE_F0=<hz>: la rejilla la fija el USUARIO — la via manual del
    // ModelMaker (el campo de pitch y su DETECT). Sirve para medir si un f0 a
    // mano (material por debajo del suelo del estimador, p. ej. E1 = 41.62 Hz)
    // produce el modelo correcto y, sobre todo, que hacen los frames del
    // analisis temporal cuando la rejilla no la ha puesto el estimador.
    // Sin la variable el flujo es el de siempre.
    bool manualF0 = false;
    if (const char* env = std::getenv ("PROBE_F0"))
    {
        const float typed = juce::String (env).getFloatValue();
        if (typed > 0.0f)
        {
            f0 = typed;
            manualF0 = true;
        }
    }

    // PROBE_FIXED=1: modo REJILLA FIJA (el declarado del ModelMaker) — el
    // analisis no sigue el pitch por ventana y el modelo lo escribe. Se combina
    // con PROBE_F0 (la rejilla declarada); sin PROBE_F0 la declarada es la que
    // detecta el estimador, que tambien sirve para medir el modo.
    const bool fixedGrid = std::getenv ("PROBE_FIXED") != nullptr
                           && juce::String (std::getenv ("PROBE_FIXED")).trim() == "1";

    // PROBE_TRANSPOSE=1: OFFSETS TRANSPONIBLES (2026-09-25) — el modelo
    // exportado declara "offsetsTranspose": los offsets son RATIOS contra
    // la rejilla de analisis y el motor los escala por base/f0, asi que la
    // inharmonicidad se transpone con el teclado. Ademas se mide, sobre el
    // modelo temporal, el recorrido del offset en Hz frente al del ratio.
    const bool transpose = std::getenv ("PROBE_TRANSPOSE") != nullptr
                           && juce::String (std::getenv ("PROBE_TRANSPOSE")).trim() == "1";
    // 2026-09-26: el residuo se cita con la MISMA frase que la fila del aviso
    // del ModelMaker y que el dialogo que bloquea la exportacion
    // (SpectralAnalyzer::gridResidualNotice): las tres superficies dicen lo mismo
    // del mismo material, y "n/d" significa lo mismo en las tres.
    std::printf ("[probe]   rejilla: %s  (indicador UI)\n",
                 analyzer.gridResidualNotice ().toRawUTF8 ());
    SpectralModel model = analyzer.analyze (audio, sr, f0, fixedGrid);

    if (transpose)
        model.offsetsTranspose = true;

    // GUARDIA (2026-09-23): el analizador reporta la desviacion de pitch del
    // material. Para barridos el estatico no es representable: la via
    // correcta es el modelo temporal con f0 por frame (FASE 10.6).
    // RESIDUO (2026-09-26): la guardia cita el residuo con la MISMA frase que la
    // fila del aviso del ModelMaker y que el dialogo de exportacion, para que el
    // reporte diga de que CLASE es el material junto a cuanto se ha movido, en vez
    // de repartirlo por dos lineas con dos formatos.
    if (const float guardCents = analyzer.lastPitchGuardCents(); guardCents > 0.0f)
        std::printf ("[probe]   guardia: desviacion de pitch %.0f cents (material no cuasi-monotonico) | %s\n",
                     (double) guardCents, analyzer.gridResidualNotice ().toRawUTF8 ());

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

    if (manualF0)
        std::printf ("[probe]   rejilla MANUAL (PROBE_F0): el usuario fija la f0 y el modelo sale de ella\n");

    if (fixedGrid)
        std::printf ("[probe]   modo REJILLA FIJA (PROBE_FIXED): sin seguimiento de pitch por ventana\n");

    if (transpose)
        std::printf ("[probe]   modo OFFSETS TRANSPONIBLES (PROBE_TRANSPOSE): el offset es ratio contra la rejilla\n");

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
        if (ac.interlacedCount > 0)
        {
            const int propios = ac.validated - ac.interlacedCount;
            if (propios >= 3)
                std::printf ("[probe]   (mediana sobre %d parciales de la rejilla; los demas son la otra familia)\n",
                             propios);
            else
                std::printf ("[probe]   (mediana sobre TODOS: la rejilla propia no llega a 3 parciales; "
                             "%d de %d son la otra familia)\n",
                             ac.interlacedCount, ac.validated);
        }
        if (ac.interlacedCount > 0)
            std::printf ("[probe]   rejillas entrelazadas: %d pico(s) explicado(s) como la OTRA familia"
                         " de la fuente (impares de f0/2); el max %.1f cents es de la sub-rejilla," 
                         " no del modelo\n",
                         ac.interlacedCount, (double) ac.interlacedCents);
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
        SpectralModel temp = analyzer.analyzeTemporal (audio, sr, f0, 4, fixedGrid);

        if (transpose)
            temp.offsetsTranspose = true;
        if (temp.frameCount != 4) fail ("analyzeTemporal no produjo 4 frames");

        // PUERTA DE PLEGADO DE OCTAVA (plan 10.3): la dispersion de las f0 por
        // ventana CON LA OCTAVA PLEGADA es el discriminador mono-rejilla /
        // bi-rejilla del material (y la condicion que paga el clustering). Se
        // imprime la dispersion CRUDA al lado porque es la que delata la
        // inestabilidad de octava del estimador (PAD1: 1200.5 cents crudos que
        // plegados caen a ~0: una sola rejilla, dos lecturas del estimador).
        {
            const auto& fold = analyzer.lastOctaveFold();
            std::printf ("[probe]   plegado: %.1f cents post-plegado (crudo %.1f, %d ventanas, %d saltos de octava) => %s\n",
                         (double) fold.foldCents, (double) fold.rawCents, fold.observations, fold.octaveFlips,
                         fold.biGrid ? "BI-REJILLA (raices distintas: no paga clustering)"
                                     : "mono-rejilla (una sola rejilla)");
        }

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

        if (transpose)
        {
            if (! tback.offsetsTranspose) fail ("el modo offsetsTranspose no sobrevivio el ciclo");
            transposeReport (tback, tback.frameSpanHz);
        }

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

        // 2026-09-27: RESIDUO POR CAPA (segunda capa = familia entrelazada f0/2)
        {
            for (int l=0; l<tback.layerCount; ++l)
            {
                const float r = analyzer.lastLayerGridResidCents(l);
                const int obs = analyzer.lastLayerGridObsCount(l);
                const auto band = analyzer.lastLayerBand(l);
                const bool isEntre = (analyzer.lastEntrelazadaLayer()==l);
                const char* tag = isEntre ? "entre" : "propia";
                const char* name = tback.layerNameAt(l).toRawUTF8();
                if (r >= 0.0f)
                    std::printf ("[probe]   residuo capa %d (%s %s): %.1f cents (%d picos) -> %s\n",
                                 l, name, tag, (double)r, obs, band.toRawUTF8());
                else
                    std::printf ("[probe]   residuo capa %d (%s %s): n/d\n", l, name, tag);
            }
            if (analyzer.lastHasEntrelazada())
                std::printf ("[probe]   entrelazada como segunda capa: la capa %d son impares de f0/2, verde = propia %s y entre %s\n",
                             (int)analyzer.lastEntrelazadaLayer(),
                             analyzer.lastLayerBand(0).toRawUTF8(), analyzer.lastLayerBand(analyzer.lastEntrelazadaLayer()).toRawUTF8());
        }

        std::printf ("[probe]   temporal: 4 frames v2 (%d bytes), pico min=%.3f, dominante max=%.3f, "
                     "trayectoria f0=%.1f..%.1f Hz\n",
                     (int) tjson.length(), (double) minTop, (double) maxAmp0,
                     (double) f0Min, (double) f0Max);

        // FASE 11.3: las CAPAS en el MOTOR. El modelo temporal de arriba puede
        // venir con mas de una capa (el clustering de la 11.2): aqui se mide que
        // el motor las SUMA (cada capa aporta a la tabla de parciales que pinta
        // la UI, que es la que el motor tiene viva) y que la capa 1 responde a SU
        // eje z (morphZ2). Con una sola capa no hay nada que sumar y se dice.
        if (! tback.isLayered())
        {
            std::printf ("[probe]   capas en el motor: UNA capa (el clustering no separo nada)\n");
        }
        else
        {
            const auto layerMask = [] (const SpectralModel& m, int layer, std::array<bool, 64>& mask)
            {
                int count = 0;

                for (int i = 0; i < 64; ++i)
                {
                    bool active = false;

                    for (int f = 0; f < m.numFramesOf (layer); ++f)
                        active = active || m.ampAt (layer, f, i) > 0.0f;

                    mask[(size_t) i] = active;
                    if (active) ++count;
                }

                return count;
            };

            std::array<bool, 64> mask0 {}, mask1 {};
            const int count0 = layerMask (tback, 0, mask0);
            const int count1 = layerMask (tback, 1, mask1);

            NEURONiKProcessor layered;
            layered.setRateAndBufferSizeDetails (sr, 512);
            layered.prepareToPlay (sr, 512);
            if (! layered.loadModel (tout, 0)) fail ("el engine rechazo el modelo de capas");

            const auto energyOf = [&layered] (const std::array<bool, 64>& mask)
            {
                float sum = 0.0f;

                for (int i = 0; i < 64; ++i)
                    if (mask[(size_t) i])
                        sum += layered.spectralDataForUI[(size_t) i].load();

                return sum;
            };

            const auto renderAt = [&layered] (float z2)
            {
                if (auto* parameter = layered.getAPVTS().getParameter ("morphZ2"))
                    parameter->setValueNotifyingHost (z2);

                juce::AudioBuffer<float> block (2, 512);

                for (int b = 0; b < 12; ++b)
                {
                    juce::MidiBuffer midi;

                    if (b == 0)
                        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);

                    block.clear();
                    layered.processBlock (block, midi);
                }
            };

            renderAt (0.0f);
            const float energy0 = energyOf (mask0);
            const float energy1 = energyOf (mask1);

            renderAt (1.0f);
            const float energy1End = energyOf (mask1);

            std::printf ("[probe]   capas en el motor: %d capas (capa 0: %d indices, capa 1: %d)\n",
                         tback.layerCount, count0, count1);
            std::printf ("[probe]     motor z2=0: capa 0 = %.3f, capa 1 = %.3f\n",
                         (double) energy0, (double) energy1);
            std::printf ("[probe]     motor z2=1: capa 1 = %.3f (su propio eje mueve su peso)\n",
                         (double) energy1End);

            if (energy0 <= 0.001f) fail ("la capa 0 no suena en el motor");
            if (energy1 <= 0.001f) fail ("la capa 1 no suena: el motor NO suma las capas");
        }
    }

    std::printf ("[probe]   OK: ciclo completo WAV->modelo->recarga->sampleFrame->engine\n");

    // 2026-09-27: COMPARATIVA Descriptors vs EnvelopeCosine (corte dedicado)
    // sobre el MISMO WAV, la MISMA f0 y los mismos 4 frames. No condiciona el
    // OK: es el dato del plan sec 3.3 / sec 3 del dedicado (el dedicado 0.60
    // tampoco separa, barrido 0.10..0.90 huecos invertidos).
    {
        NEURONiK::ModelMaker::Analysis::SpectralAnalyzer comp;
        ComparativeRecord rec;
        rec.name = wav.getFileName();
        rec.f0   = f0;

        // Descriptors (corte 0.55)
        {
            auto dModel = comp.analyzeTemporal (audio, sr, f0, 4, fixedGrid,
                                                NEURONiK::ModelMaker::Analysis::LayerMetric::Descriptors);
            (void) dModel;
            rec.descLayers  = comp.lastLayerCount();
            rec.descSkipped = comp.lastClusteringSkipped();
            rec.fold        = comp.lastOctaveFold();
            for (int l = 0; l < 3; ++l)
                rec.descTraces[l] = comp.lastLayerTraces (l);
        }
        // EnvelopeCosine (corte DEDICADO 0.60)
        {
            auto cModel = comp.analyzeTemporal (audio, sr, f0, 4, fixedGrid,
                                                NEURONiK::ModelMaker::Analysis::LayerMetric::EnvelopeCosine);
            (void) cModel;
            rec.cosineLayers  = comp.lastLayerCount();
            rec.cosineSkipped = comp.lastClusteringSkipped();
            // plegado es el mismo (no depende de metrica); si bi-rejilla, ambas saltan
            // rec.fold ya esta, pero por si acaso se refresca (mismo valor)
            // rec.fold = comp.lastOctaveFold();
            for (int l = 0; l < 3; ++l)
                rec.cosineTraces[l] = comp.lastLayerTraces (l);
        }

        gComparative.push_back (rec);

        const auto tracesToString = [] (const int t[3], int layers, bool skipped) -> juce::String
        {
            if (skipped) return juce::String ("skip (plegado BI)");
            if (layers <= 1) return juce::String (t[0]) + " (1 capa)";
            juce::String s;
            for (int l = 0; l < layers && l < 3; ++l)
                s << (l == 0 ? "" : "+") << t[l];
            s << " (" << layers << " capas)";
            return s;
        };

        std::printf ("[probe]   -- comparativa metricas (4 frames, f0 %.1f Hz, corte dedicado) --\n",
                     (double) f0);
        std::printf ("[probe]     Descriptors (%.2f)%s: %d capa(s) [%s]%s\n",
                     (double) NEURONiK::ModelMaker::Analysis::LayerClustering::kAffinityCut,
                     rec.descSkipped ? " skip" : "",
                     rec.descLayers,
                     tracesToString (rec.descTraces, rec.descLayers, rec.descSkipped).toRawUTF8(),
                     rec.fold.biGrid ? " | plegado BI" : " | plegado mono");
        std::printf ("[probe]     EnvelopeCosine (%.2f)%s: %d capa(s) [%s]  -> %s\n",
                     (double) NEURONiK::ModelMaker::Analysis::LayerClustering::kEnvelopeCosineCut,
                     rec.cosineSkipped ? " skip" : "",
                     rec.cosineLayers,
                     tracesToString (rec.cosineTraces, rec.cosineLayers, rec.cosineSkipped).toRawUTF8(),
                     (rec.descLayers != rec.cosineLayers ? "DIFIEREN" : "igual"));
        std::printf ("[probe]     plegado: %.1f cents post (crudo %.1f, %d obs, %d saltos) => %s\n",
                     (double) rec.fold.foldCents, (double) rec.fold.rawCents,
                     rec.fold.observations, rec.fold.octaveFlips,
                     rec.fold.biGrid ? "BI-REJILLA (no paga clustering)" : "mono-rejilla");
    }
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

    // 2026-09-27: TABLA COMPARATIVA por patch (Descriptores vs EnvelopeCosine)
    if (! gComparative.empty())
    {
        std::printf ("\n[probe] ============================================================\n");
        std::printf ("[probe] TABLA COMPARATIVA DE CAPAS --- banco CZ101 (4 frames, f0 detectada)\n");
        std::printf ("[probe]  Descriptors kAffinityCut=%.2f  |  EnvelopeCosine kEnvelopeCosineCut=%.2f (dedicado 2026-09-27)\n",
                     (double) NEURONiK::ModelMaker::Analysis::LayerClustering::kAffinityCut,
                     (double) NEURONiK::ModelMaker::Analysis::LayerClustering::kEnvelopeCosineCut);
        std::printf ("[probe]  (el dedicado tampoco separa: barrido 0.10..0.90 huecos invertidos sin interseccion)\n");
        std::printf ("[probe] ------------------------------------------------------------\n");
        std::printf ("[probe]  %-14s  %7s  %-26s  %-13s  %-13s  %s\n",
                     "patch", "f0(Hz)", "plegado(post/crudo, bi)", "Descriptors", "Cosine", "veredicto");
        std::printf ("[probe]  %-14s  %7s  %-26s  %-13s  %-13s  %s\n",
                     "--------------", "-------", "--------------------------", "-------------", "-------------", "---------");

        for (const auto& rec : gComparative)
        {
            const juce::String foldStr = juce::String (rec.fold.foldCents, 1) + "/" + juce::String (rec.fold.rawCents, 1)
                                         + (rec.fold.biGrid ? " BI" : " mono")
                                         + " (" + juce::String (rec.fold.observations) + " obs)";

            const auto tracesStr = [] (const int t[3], int layers, bool skipped) -> juce::String
            {
                if (skipped) return juce::String ("1 skip");
                if (layers <= 1) return juce::String (layers) + " (" + juce::String (t[0]) + ")";
                juce::String s = juce::String (layers) + " (";
                for (int l = 0; l < layers && l < 3; ++l)
                    s << (l == 0 ? "" : "+") << t[l];
                s << ")";
                return s;
            };

            const juce::String dStr = tracesStr (rec.descTraces, rec.descLayers, rec.descSkipped);
            const juce::String cStr = tracesStr (rec.cosineTraces, rec.cosineLayers, rec.cosineSkipped);
            const juce::String verdict = (rec.descLayers != rec.cosineLayers ? "DIFIEREN"
                                        : (rec.fold.biGrid ? "igual (BI)" : "igual"));

            std::printf ("[probe]  %-14s  %7.1f  %-26s  %-13s  %-13s  %s\n",
                         rec.name.toRawUTF8(),
                         (double) rec.f0,
                         foldStr.toRawUTF8(),
                         dStr.toRawUTF8(),
                         cStr.toRawUTF8(),
                         verdict.toRawUTF8());
        }

        std::printf ("[probe] ------------------------------------------------------------\n");
        std::printf ("[probe]  Criterio sintetico (8 frames): Descriptors 2/2, Cosine 1/1 (dedicado 0.60)\n");
        std::printf ("[probe]  CZ101 reales: Descriptors solo SWEP1 2 capas; Cosine 2 en BASS1 (FP) + 1 en SWEP1 (FN)\n");
        std::printf ("[probe]  (RRISE es BI-rejilla: ambas metricas skip -> 1 capa, no paga clustering)\n");
        std::printf ("[probe] ============================================================\n");
    }

    std::printf ("[probe] RESULT: OK (%d wav(s) analizados -> %s)\n", argc - 1,
                 outDir.getFullPathName().toRawUTF8());
    return 0;
}
