/*
  ==============================================================================

    LayerClusteringTest.cpp
    Created: 25 Sep 2026
    Description: FASE 11.2 — clustering de parciales por FORMA DE ENVOLVENTE,
                 con la PUERTA DE PLEGADO DE OCTAVA (plan 10.3) delante.

                 La bateria de no-regresion del algoritmo, en dos mitades:

                 A. TRAZAS SINTETICAS (el modulo puro, sin audio): se fabrica la
                    matriz parcial x frame a mano y se comprueba el reparto.
                      1. Drone solo => 1 capa (y una traza plana escalada x5
                         tampoco inventa capas: la forma es la misma).
                      2. Drone + voz (crestas en frames distintos) => 2 capas con
                         los indices EXACTOS; la voz es UNA capa aunque cada
                         armonico suene en ventanas distintas (la migracion la
                         lleva el emparejador por indice, no el clustering).
                      3. MEDICION NEGATIVA: con las trazas REALES del plan
                         (seccion 2, CZ-RRISE: drone n1 y voz n7/n15) el coseno
                         literal del diseno daria 0.79 entre el drone y n7 (>= el
                         corte 0.55: los fusionaria); la metrica de descriptores
                         los separa. Queda pinneado para que nadie vuelva al
                         coseno sin medirlo.
                      4. Robustez: ruido +-15 % y escalas muy distintas (x20) no
                         mueven el reparto.
                      5. Guardia de degeneracion: una capa de UN parcial, o con
                         < 10 % de la energia, se reabsorbe (=> 1 capa).
                      6. Clamp: cuatro grupos separados => nunca mas de 3 capas.
                      7. Determinismo y bordes: dos llamadas identicas; un solo
                         frame (modelo estatico), 0 trazas y silencio => 1 capa.

                 B. AUDIO DE PUNTA A PUNTA (SpectralAnalyzer::analyzeTemporal):
                      8. Drone constante (200/400/600 Hz) + voz que migra por los
                         armonicos altos (un bump por ventana) => modelo con 2
                         capas: la RAIZ es el drone (lo que oye un lector v2
                         viejo), cada capa escribe SOLO sus indices, la cresta de
                         la voz sube con el tiempo y los pesos temporales son la
                         envolvente de cada capa (pico 1).
                      9. La puerta de rejilla: el mismo material con un barrido de
                         pitch fuerte encima NO se parte (el modelo honesto es el
                         de una capa con f0 por frame) y la marca lo dice.
                     10. El camino de una capa sigue intacto: un seno con 8
                         frames y un barrido puro dan 1 capa y el modelo de
                         siempre (sin capas declaradas).

                 C. LA PUERTA DE PLEGADO DE OCTAVA (plan, seccion 10.3): la
                    dispersion de las f0 por ventana DESPUES de plegar la
                    octava es el discriminador mono-rejilla / bi-rejilla.
                     11. La medida pura, sin audio: ventanas alrededor de la
                         rejilla => mono-rejilla; una inestabilidad de octava
                         del ESTIMADOR (1200 cents crudos, el caso medido en
                         PAD1/SWEP1) plegada => 0 => MONO-rejilla igual; una
                         quinta o una cuarta sobreviven al plegado (498.04)
                         => BI-rejilla; y sin datos no hay veredicto (no se
                         exenta nada por falta de evidencia).
                     12. El barrido de pitch del caso 9: bi-rejilla declarada
                         (la dispersion plegada pasa del corte) y el modelo se
                         queda en una capa.
                     13. Un sub una octava por debajo del drone: el material
                         sigue siendo mono-rejilla y las capas se quedan.

                 D. EL PRE-FILTRO DE PLEGADO (plan 10.4): la puerta de plegado
                    de octava (C) entra ANTES del clustering — el material
                    bi-rejilla NO paga su coste. La f0 de las ventanas se mide
                    en una pre-pasada, la puerta decide, y la rejilla comun y el
                    clustering solo corren si la puerta la declara COMUN.
                     14. Bi-rejilla (el barrido): la puerta exime al material, el
                         clustering NO corre y el modelo es el de una capa con
                         f0 por frame (el analisis temporal SI ocurre).
                     15. Mono-rejilla (drone + voz): la puerta no exime y el
                         clustering SI corre (las 2 capas siguen).
                     16. Sin rejilla que seguir (REJILLA FIJA): la f0 por ventana
                         es la del llamador, plegado 0 => no se exime.

  ==============================================================================
*/

#include "../Source/ModelMaker/Analysis/SpectralAnalyzer.h"
#include "../Source/ModelMaker/Analysis/LayerClustering.h"
#include "../Source/Common/SpectralModel.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>
#include <vector>

using NEURONiK::Common::SpectralModel;
using NEURONiK::ModelMaker::Analysis::LayerClustering;
using NEURONiK::ModelMaker::Analysis::clusterTraces;
using NEURONiK::ModelMaker::Analysis::envelopeCosine;
using NEURONiK::ModelMaker::Analysis::LayerMetric;
using NEURONiK::ModelMaker::Analysis::SpectralAnalyzer;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("  [%s] %s\n", ok ? "OK" : "FAIL", what.toRawUTF8());
    if (! ok) ++failures;
}

void note (const juce::String& text)
{
    std::printf ("  [..] %s\n", text.toRawUTF8());
}

constexpr double sr = 44100.0;
constexpr int fftSize = 8192;   // debe coincidir con SpectralAnalyzer::fftSize
constexpr float gridHz = 200.0f;

/** Matriz traza x frame row-major, como la que consume clusterTraces. */
struct TraceSet
{
    std::vector<float> data;
    int traces = 0;
    int frames = 0;

    TraceSet (int t, int f) : data ((size_t) (t * f), 0.0f), traces (t), frames (f) {}

    void set (int trace, int frame, float value)
    {
        data[(size_t) trace * (size_t) frames + (size_t) frame] = value;
    }

    const float* trace (int index) const { return data.data() + (size_t) index * (size_t) frames; }

    LayerClustering cluster() const { return clusterTraces (data, traces, frames); }

    LayerClustering cluster (LayerMetric metric) const
    {
        return clusterTraces (data, traces, frames, LayerClustering::kActivityFloor, metric);
    }

    juce::String layers() const
    {
        juce::String s;
        const auto r = cluster();

        for (int t = 0; t < traces; ++t)
            s << (t == 0 ? "" : " ") << r.layerOfTrace[(size_t) t];

        return s;
    }
};

/** Traza plana: el drone (activa en TODOS los frames, entropia 1). */
void flat (TraceSet& ts, int trace, float level)
{
    for (int f = 0; f < ts.frames; ++f)
        ts.set (trace, f, level);
}

/** Cresta: el parcial suena `len` frames desde `start` (el 2o a `tail`). */
void bump (TraceSet& ts, int trace, int start, int len, float amp, float tail = 0.6f)
{
    for (int i = 0; i < len && start + i < ts.frames; ++i)
        ts.set (trace, start + i, amp * (i == 0 ? 1.0f : tail));
}

/** Seno (fase integrada) sumado a la pista, opcionalmente con envolvente Hann
    de su propia longitud (un "bump" que empieza y acaba en cero: no se cuela en
    la ventana vecina mas alla del suelo de deteccion). */
void addTone (juce::AudioBuffer<float>& audio, float freq, float amp, int start, int count, bool hann)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * (double) freq / sr;
    const double denom = (double) juce::jmax (1, count - 1);
    double phase = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const int index = start + i;

        if (index < 0 || index >= audio.getNumSamples())
            continue;

        float env = 1.0f;

        if (hann)
            env = (float) (0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * (double) i / denom)));

        audio.addSample (0, index, (float) (amp * env) * (float) std::sin (phase));
        phase += w;

        if (phase > 2.0 * juce::MathConstants<double>::pi)
            phase -= 2.0 * juce::MathConstants<double>::pi;
    }
}

/** Material de la fase: drone constante en 200/400/600 Hz + voz que sube por los
    armonicos 7..13 (un bump de dos ventanas por armonico, la migracion del
    CZ-RRISE: cada armonico sube, canta y muere).

    @param extraSweepHz  si > 0, un barrido de pitch FUERTE de extraSweepHz a
                         3*extraSweepHz por todo el fichero (prueba de la puerta). */
juce::AudioBuffer<float> makeLayeredMaterial (int frames, float extraSweepHz)
{
    juce::AudioBuffer<float> audio (1, frames * fftSize);
    audio.clear();

    for (int k = 1; k <= 3; ++k)
        addTone (audio, 200.0f * (float) k, 0.5f, 0, audio.getNumSamples(), false);

    for (int j = 0; j < frames - 1; ++j)
        addTone (audio, 200.0f * (float) (7 + j), 0.25f, j * fftSize, 2 * fftSize, true);

    if (extraSweepHz > 0.0f)
    {
        const int n = audio.getNumSamples();
        const double w0 = 2.0 * juce::MathConstants<double>::pi * (double) extraSweepHz / sr;
        const double w1 = 3.0 * w0;
        double phase = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double t = (double) i / (double) n;
            phase += w0 + (w1 - w0) * t;
            audio.addSample (0, i, (float) (1.0f * std::sin (phase)));
        }
    }

    return audio;
}
} // namespace

int main()
{
    // Sin ScopedJuceInitialiser_GUI: este test enlaza solo audio_basics/core/dsp
    // (el analizador no toca JUCE GUI) y el inicializador vive en gui_basics.
    NEURONiK::ModelMaker::Analysis::SpectralAnalyzer analyzer;

    // ======================================================================
    // A. TRAZAS SINTETICAS
    // ======================================================================
    std::printf ("\nA. Trazas sinteticas\n");

    // ---------- 1. Drone solo ----------
    {
        TraceSet ts (4, 8);
        flat (ts, 0, 1.0f);
        flat (ts, 1, 0.9f);
        flat (ts, 2, 1.1f);
        flat (ts, 3, 5.0f);   // el mismo drone, 5x mas fuerte: la FORMA es la misma

        const auto r = ts.cluster();
        check (r.layerCount == 1, "un drone solo (cuatro trazas planas) es 1 capa");
        note ("descriptores: soporte " + juce::String (r.supportFraction[0], 2)
              + " entropia " + juce::String (r.entropy[0], 2));
        check (r.active[0] && r.active[3], "las cuatro trazas planas son activas");
    }

    // ---------- 2. Drone + voz ----------
    {
        TraceSet ts (8, 8);
        flat (ts, 0, 1.0f);
        flat (ts, 1, 0.95f);
        flat (ts, 2, 1.05f);

        // La voz: un armonico por posicion, cada uno en DOS frames (soporte 0.25).
        for (int j = 0; j < 5; ++j)
            bump (ts, 3 + j, j, 2, 0.8f);

        const auto r = ts.cluster();
        check (r.layerCount == 2, "drone + voz son 2 capas");

        bool droneOk = true, voiceOk = true;

        for (int t = 0; t < 3; ++t)  droneOk = droneOk && r.layerOfTrace[(size_t) t] == 0;
        for (int t = 3; t < 8; ++t)  voiceOk = voiceOk && r.layerOfTrace[(size_t) t] == 1;

        check (droneOk, "la capa 0 (raiz) es el drone: indices 0-2, reparto " + ts.layers());
        check (voiceOk, "la capa 1 es la voz entera: los cinco armonicos juntos, no cada uno aparte");
        check (r.layerTraces[0] == 3 && r.layerTraces[1] == 5,
               "el recuento por capa es 3 + 5 parciales");
        note ("descriptor drone (soporte, entropia) = (" + juce::String (r.supportFraction[0], 2)
              + ", " + juce::String (r.entropy[0], 2) + ") / voz = ("
              + juce::String (r.supportFraction[3], 2) + ", " + juce::String (r.entropy[3], 2) + ")");
    }

    // ---------- 3. Medicion negativa: el coseno del diseno NO decide ----------
    {
        // Trazas REALES de la seccion 2 del plan (CZ-RRISE, 9 ventanas): el drone
        // (n1) y dos armonicos de la voz (n7 y n15). El drone lleva DOS parciales
        // (n3 = n1/2, mismo soporte y misma forma) porque una capa de UN solo
        // indice es degenerada por diseno (plan 3.7, y el caso 5 lo pinnea): con
        // el drone de un unico parcial el reparto honesto es 1 capa.
        const float drone[9] = { 307, 133, 177, 312, 145, 165, 308, 156, 154 };
        const float n3[9]    = { 154,  67,  89, 156,  73,  83, 154,  78,  77 };
        const float n7[9]    = { 461,   0, 119,  98,   0,  59,  75,   0,   0 };
        const float n15[9]   = {   0,   0,   0,  13,  31,  78, 323, 505, 289 };

        const float cosDroneVoice = envelopeCosine (drone, n7, 9);
        check (cosDroneVoice >= LayerClustering::kAffinityCut,
               "el coseno literal del plan daria " + juce::String (cosDroneVoice, 2)
                   + " >= " + juce::String (LayerClustering::kAffinityCut, 2)
                   + " entre drone y n7 (los FUSIONARIA)");

        TraceSet ts (4, 9);
        for (int f = 0; f < 9; ++f)
        {
            ts.set (0, f, drone[f]);
            ts.set (1, f, n3[f]);
            ts.set (2, f, n7[f]);
            ts.set (3, f, n15[f]);
        }

        const auto r = ts.cluster();
        check (r.layerCount == 2, "las trazas reales del plan dan 2 capas con la metrica de descriptores");
        check (r.layerOfTrace[0] == 0 && r.layerOfTrace[1] == 0,
               "la raiz sigue siendo el drone n1+n3 (la capa mas persistente)");
        check (r.layerOfTrace[2] == 1 && r.layerOfTrace[3] == 1,
               "n7 y n15 (soportes disjuntos, coseno 0.09) caen en la MISMA capa de voz");
        note ("n7: soporte " + juce::String (r.supportFraction[2], 2) + " entropia " + juce::String (r.entropy[2], 2)
              + " | n15: soporte " + juce::String (r.supportFraction[3], 2) + " entropia " + juce::String (r.entropy[3], 2)
              + " | energia drone " + juce::String (r.layerEnergy[0], 0)
              + " vs voz " + juce::String (r.layerEnergy[1], 0));
    }

    // ---------- 4. Robustez (ruido y escalas) ----------
    {
        TraceSet ts (6, 9);
        flat (ts, 0, 1.0f);
        flat (ts, 1, 1.0f);
        bump (ts, 2, 1, 4, 1.0f);
        bump (ts, 3, 4, 4, 1.0f);
        bump (ts, 4, 1, 4, 1.0f);
        bump (ts, 5, 4, 4, 1.0f);

        // Ruido determinista de +-15 % (no aleatorio: mismo test en cada maquina).
        for (int t = 0; t < ts.traces; ++t)
            for (int f = 0; f < ts.frames; ++f)
            {
                const float jitter = 1.0f + 0.15f * (float) std::sin ((double) (7 * t + 13 * f));
                ts.set (t, f, ts.data[(size_t) t * (size_t) ts.frames + (size_t) f] * jitter);
            }

        const auto base = ts.cluster();
        check (base.layerCount == 2, "con +-15 % de ruido siguen siendo 2 capas");

        const auto mapOf = [] (const LayerClustering& r)
        {
            juce::String s;

            for (int t = 0; t < (int) r.layerOfTrace.size(); ++t)
                s << (t == 0 ? "" : " ") << r.layerOfTrace[(size_t) t];

            return s;
        };

        // Y los mismos datos con la voz 20x mas fuerte (y 5x: el ruido del
        // analizador no cambia el reparto, la FORMA si).
        for (int t = 2; t < ts.traces; ++t)
            for (int f = 0; f < ts.frames; ++f)
                ts.set (t, f, ts.data[(size_t) t * (size_t) ts.frames + (size_t) f] * 20.0f);

        const auto loud = ts.cluster();
        const bool same = (loud.layerCount == base.layerCount && loud.layerOfTrace == base.layerOfTrace);
        check (same, "escalar la voz x20 no mueve ni un indice de capa");

        if (! same)
            note ("base " + juce::String (base.layerCount) + " [" + mapOf (base)
                  + "] vs loud " + juce::String (loud.layerCount) + " [" + mapOf (loud) + "]");
    }

    // ---------- 5. Guardia de degeneracion ----------
    {
        TraceSet thin (4, 8);   // drone de 3 + UNA traza suelta
        flat (thin, 0, 1.0f);
        flat (thin, 1, 1.0f);
        flat (thin, 2, 1.0f);
        bump (thin, 3, 2, 3, 0.9f);

        const auto rThin = thin.cluster();
        check (rThin.layerCount == 1, "una 'capa' de UN parcial se reabsorbe (1 capa)");

        TraceSet weak (5, 8);   // drone fuerte + dos trazas de energia minima
        flat (weak, 0, 1.0f);
        flat (weak, 1, 1.0f);
        flat (weak, 2, 1.0f);
        bump (weak, 3, 0, 4, 0.02f);
        bump (weak, 4, 4, 4, 0.02f);

        const auto rWeak = weak.cluster();
        check (rWeak.layerCount == 1,
               "una capa con < 10 % de la energia se reabsorbe (1 capa); energia "
                   + juce::String (rWeak.layerEnergy[0], 3));
    }

    // ---------- 6. Clamp a 3 capas ----------
    {
        TraceSet ts (8, 8);   // cuatro grupos, separados en (soporte, entropia)

        for (int t = 0; t < 2; ++t) flat (ts, t, 1.0f);                    // (1.00, 1.00)
        for (int t = 2; t < 4; ++t) bump (ts, t, t, 2, 1.0f);              // (0.25, ~0.97)
        for (int t = 4; t < 6; ++t)                                        // (1.00, ~0.02)
            for (int f = 0; f < 8; ++f)
                ts.set (t, f, f == 0 ? 1.0f : 0.0012f);
        for (int t = 6; t < 8; ++t) bump (ts, t, t, 2, 1.0f, 0.004f);      // (0.25, ~0.05)

        const auto r = ts.cluster();
        check (r.layerCount <= LayerClustering::kMaxLayers,
               "cuatro grupos separados nunca dan mas de " + juce::String (LayerClustering::kMaxLayers)
                   + " capas (dio " + juce::String (r.layerCount) + ")");
        note ("clamp: " + juce::String (r.layerCount) + " capas con "
              + juce::String (r.layerTraces[0]) + "/" + juce::String (r.layerTraces[1]) + "/"
              + juce::String (r.layerTraces[2]) + " parciales");
    }

    // ---------- 7. Determinismo y bordes ----------
    {
        TraceSet ts (6, 8);
        flat (ts, 0, 1.0f);
        flat (ts, 1, 1.0f);
        for (int j = 0; j < 4; ++j)
            bump (ts, 2 + j, j + 1, 2, 0.7f);

        const auto a = ts.cluster();
        const auto b = ts.cluster();
        check (a.layerCount == b.layerCount && a.layerOfTrace == b.layerOfTrace
                   && a.layerEnergy == b.layerEnergy,
               "dos llamadas con la misma entrada dan lo mismo (determinista)");

        TraceSet oneFrame (4, 1);
        for (int t = 0; t < 4; ++t) oneFrame.set (t, 0, 1.0f + (float) t);
        check (oneFrame.cluster().layerCount == 1,
               "un solo frame no tiene eje temporal: 1 capa (el modelo estatico no se parte)");

        std::vector<float> silence ((size_t) (4 * 8), 0.0f);
        check (clusterTraces (silence, 4, 8).layerCount == 1, "silencio (todo a cero): 1 capa");
        check (clusterTraces (silence.data(), 0, 8).layerCount == 1, "sin trazas: 1 capa");
    }

    // ======================================================================
    // B. AUDIO DE PUNTA A PUNTA
    // ======================================================================
    std::printf ("\nB. Audio de punta a punta (analyzeTemporal)\n");

    // ---------- 8. Drone + voz migrando ----------
    {
        constexpr int frames = 8;
        auto audio = makeLayeredMaterial (frames, 0.0f);

        const auto model = analyzer.analyzeTemporal (audio, sr, gridHz, frames);

        check (analyzer.lastLayerCount() == 2,
               "drone + voz que migra => 2 capas (el analizador lo ve)");
        check (model.layerCount == 2 && model.isLayered(), "el modelo sale con 2 capas");
        check (model.numFramesOf (0) == frames && model.numFramesOf (1) == frames,
               "las dos capas llevan la pelicula de 8 frames");

        bool droneIdx = true, voiceIdx = true;

        for (int k = 0; k < 3; ++k)
            droneIdx = droneIdx && analyzer.lastLayerOfTrace (k) == 0;

        for (int k = 6; k <= 12; ++k)
            voiceIdx = voiceIdx && analyzer.lastLayerOfTrace (k) == 1;

        juce::String map;
        for (int k = 0; k < 16; ++k)
            map << (k == 0 ? "" : " ") << analyzer.lastLayerOfTrace (k);

        check (droneIdx, "los armonicos 1-3 (el drone) son la capa 0, la RAIZ");
        check (voiceIdx, "los armonicos 7-13 (la voz) son la capa 1; mapa " + map);
        check (analyzer.lastLayerOfTrace (3) == 0 && analyzer.lastLayerOfTrace (4) == 0,
               "los indices sin voz (n4/n5: silencio medido) se quedan en la capa 0");

        // La raiz es el drone: un lector v2 viejo (sin capas) oye eso, y las
        // amplitudes de la capa de voz NO estan duplicadas en la raiz.
        const float rootA = model.ampAt (0, 4, 0);
        const float rootVoice = model.ampAt (0, 4, 6);
        const float voiceA = model.ampAt (1, 0, 6);

        check (rootA > 0.3f, "la raiz lleva el drone (n1 = " + juce::String (rootA, 3) + ")");
        check (rootVoice < 1.0e-6f, "el armonico de la voz NO esta en la raiz (n7 = " + juce::String (rootVoice, 6) + ")");
        check (voiceA > 0.02f, "el armonico de la voz si esta en la capa 1 (n7 = " + juce::String (voiceA, 3) + ")");

        // El drone no se mueve en el tiempo; la cresta de la voz SUBE.
        float flatMin = 1.0e9f, flatMax = 0.0f;
        for (int f = 0; f < frames; ++f)
        {
            flatMin = juce::jmin (flatMin, model.ampAt (0, f, 0));
            flatMax = juce::jmax (flatMax, model.ampAt (0, f, 0));
        }
        check (flatMax / juce::jmax (1.0e-6f, flatMin) < 1.2f,
               "la capa del drone es plana en el tiempo (max/min = "
                   + juce::String (flatMax / juce::jmax (1.0e-6f, flatMin), 3) + ")");

        int lastPeakFrame = -1;
        bool migrating = true;
        juce::String peaks;

        for (int k = 6; k <= 12; ++k)
        {
            int peakFrame = 0;
            float peakAmp = -1.0f;

            for (int f = 0; f < frames; ++f)
                if (model.ampAt (1, f, k) > peakAmp)
                {
                    peakAmp = model.ampAt (1, f, k);
                    peakFrame = f;
                }

            peaks << (k == 6 ? "" : " ") << peakFrame;
            migrating = migrating && peakFrame >= lastPeakFrame;
            lastPeakFrame = peakFrame;
        }

        check (migrating, "la cresta de la capa de voz SUBE por los armonicos: frames de pico " + peaks);

        // Pesos temporales: envolvente por capa, 0..1 con el pico en 1.
        bool weightsOk = true;
        juce::String weightReport;

        for (int l = 0; l < 2; ++l)
        {
            float peak = 0.0f, low = 1.0e9f;

            for (int f = 0; f < frames; ++f)
            {
                const float w = model.frameWeightAt (l, f);
                peak = juce::jmax (peak, w);
                low = juce::jmin (low, w);
                weightsOk = weightsOk && w >= 0.0f && w <= 1.0f;
            }

            weightsOk = weightsOk && std::abs (peak - 1.0f) < 1.0e-4f;
            weightReport << (l == 0 ? "" : " | ") << "capa " << l << " pico " << juce::String (peak, 3)
                         << " min " << juce::String (low, 3);
        }

        check (weightsOk, "los pesos temporales son la envolvente de cada capa (pico 1, 0..1): " + weightReport);

        // La rejilla de capas: una sola, la del llamador (en todas las capas).
        check (std::abs (model.frameSpanHz - gridHz) < 0.01f
                   && std::abs (model.f0At (1, 3) - gridHz) < 0.01f
                   && std::abs (model.f0At (0, 3) - gridHz) < 0.01f,
               "todas las capas comparten la rejilla comun (" + juce::String (gridHz, 0) + " Hz)");
        check (model.layerNameAt (0) == "capa 1" && model.layerWeightAt (1) > 0.99f,
               "nombres y mezcla de capa quedan declarados (mezcla fiel = 1)");
    }

    // ---------- 9. La puerta de rejilla ----------
    {
        constexpr int frames = 8;
        auto audio = makeLayeredMaterial (frames, 200.0f);   // + barrido 200 -> 600 Hz fuerte

        const auto model = analyzer.analyzeTemporal (audio, sr, gridHz, frames);

        check (analyzer.lastLayerCount() == 1 && model.layerCount == 1,
               "con un barrido de pitch fuerte encima NO se parte en capas");
        note (juce::String ("PRE-FILTRO de plegado: la puerta exime al material ANTES del clustering (")
              + (analyzer.lastClusteringSkipped() ? "clustering SALTADO" : "clustering corrido") + ")");
        check (! model.isLayered(), "el modelo es el de una capa (el camino de siempre)");
    }

    // ---------- 10. El camino de una capa sigue intacto ----------
    {
        constexpr int frames = 8;
        juce::AudioBuffer<float> audio (1, frames * fftSize);
        audio.clear();
        addTone (audio, 300.0f, 0.6f, 0, audio.getNumSamples(), false);
        addTone (audio, 600.0f, 0.2f, 0, audio.getNumSamples(), false);

        const auto model = analyzer.analyzeTemporal (audio, sr, 300.0f, frames);

        check (analyzer.lastLayerCount() == 1 && ! model.isLayered(),
               "un timbre estatico (una sola rejilla) sigue siendo 1 capa");
        check (model.frameCount == frames && model.amplitudes[0] > 0.5f,
               "y conserva el volcado de siempre (frame canonico + extras)");
        check (! analyzer.lastLayerGridRejected(), "sin barrido la puerta no se dispara");
    }

    // ======================================================================
    // C. LA PUERTA DE PLEGADO DE OCTAVA (plan, seccion 10.3)
    // ======================================================================
    std::printf ("\nC. La puerta de plegado de octava\n");

    // ---------- 11. La medida pura (sin audio) ----------
    {
        // 11a. Cuatro ventanas alrededor de la rejilla: sin saltos de octava, la
        //      dispersion cruda Y la plegada son el mismo numero (el -3 Hz de la
        //      ventana 3, el ruido del estimador a su valor real).
        const auto stable = SpectralAnalyzer::measureOctaveFold ({ 200.0f, 203.0f, 197.0f, 201.0f }, 200.0f);
        check (stable.observations == 4 && stable.octaveFlips == 0 && ! stable.biGrid,
               "cuatro ventanas alrededor de la rejilla: mono-rejilla y sin saltos de octava");
        check (std::abs (stable.foldCents - 26.2f) < 0.5f
                   && std::abs (stable.rawCents - stable.foldCents) < 1.0e-6f,
               "sin saltos, crudo y plegado son el MISMO numero (" + juce::String (stable.foldCents, 1) + " cents)");

        // 11b. LA INESTABILIDAD DE OCTAVA DEL ESTIMADOR (medida en PAD1 y en
        //      SWEP1: 1200.5 cents crudos): 1200 crudos que el plegado colapsa a
        //      0. Es el caso que la puerta NO debe castigar —el material tiene
        //      UNA rejilla y lo que se mueve es la respuesta del estimador—, y
        //      es tambien la razon de ser del plegado.
        const std::vector<float> flipping { 62.0f, 124.0f, 62.0f, 124.0f };
        const auto pad = SpectralAnalyzer::measureOctaveFold (flipping, 62.0f);
        check (! pad.biGrid && pad.foldCents < 1.0e-3f,
               "una octava de salto del estimador NO es una rejilla distinta: plegada, mono-rejilla");
        check (std::abs (pad.rawCents - 1200.0f) < 0.5f && pad.octaveFlips == 2,
               "y la cruda lo dice en voz alta: " + juce::String (pad.rawCents, 1) + " cents crudos, "
                   + juce::String (pad.octaveFlips) + " de 4 ventanas saltadas");

        const auto padUp = SpectralAnalyzer::measureOctaveFold (flipping, 124.0f);
        check (! padUp.biGrid && padUp.foldCents < 1.0e-3f && padUp.octaveFlips == 2,
               "el veredicto no depende de la octava de la referencia (contra 124 Hz: plegado "
                   + juce::String (padUp.foldCents, 3) + " cents)");

        // 11c. Una quinta (701.96 cents) sobrevive al plegado como -498.04, y una
        //      cuarta da EL MISMO -498.04: la puerta ve "raiz distinta", no
        //      "hacia donde". Es la evidencia de bi-rejilla del plan.
        const auto fifth = SpectralAnalyzer::measureOctaveFold ({ 200.0f, 300.0f }, 200.0f);
        const auto fourth = SpectralAnalyzer::measureOctaveFold ({ 200.0f, 200.0f * 4.0f / 3.0f }, 200.0f);
        check (fifth.biGrid && fourth.biGrid, "una quinta y una cuarta plegadas siguen separadas: bi-rejilla");
        check (std::abs (fifth.foldCents - 498.04f) < 0.2f
                   && std::abs (fourth.foldCents - fifth.foldCents) < 0.05f,
               "las dos dan la MISMA dispersion plegada (" + juce::String (fifth.foldCents, 2)
                   + " cents): el plegado ve raiz distinta, no direccion");
        check (std::abs (fifth.rawCents - 701.96f) < 0.2f,
               "la cruda si distingue la quinta (" + juce::String (fifth.rawCents, 2)
                   + " cents): por eso hay que plegar antes de comparar");

        // 11d. La frontera: el corte (kOctaveFoldCents = 100) separa las familias.
        const auto below = SpectralAnalyzer::measureOctaveFold ({ 200.0f, 200.0f * std::pow (2.0f, 80.0f / 1200.0f) }, 200.0f);
        const auto above = SpectralAnalyzer::measureOctaveFold ({ 200.0f, 200.0f * std::pow (2.0f, 120.0f / 1200.0f) }, 200.0f);
        check (! below.biGrid, "80 cents por debajo del corte: una sola rejilla");
        check (above.biGrid, "120 cents por encima del corte: bi-rejilla");

        // 11e. Sin datos NO hay veredicto: no se exenta nada por falta de
        //      evidencia (ni con silencio, ni con f0 no positivas, ni sin
        //      referencia con la que plegar).
        const auto none = SpectralAnalyzer::measureOctaveFold (std::vector<float> {}, 200.0f);
        const auto zeros = SpectralAnalyzer::measureOctaveFold ({ 0.0f, -3.0f }, 200.0f);
        const auto noRef = SpectralAnalyzer::measureOctaveFold ({ 200.0f }, 0.0f);
        check (none.foldCents == -1.0f && none.observations == 0 && ! none.biGrid
                   && zeros.foldCents == -1.0f && zeros.observations == 0 && ! zeros.biGrid,
               "sin ventanas (o sin f0 positiva) no hay medida: foldCents -1 y sin veredicto");
        check (noRef.foldCents == -1.0f && ! noRef.biGrid, "sin rejilla de referencia no hay plegado posible");

        // 11f. La referencia solo importa por su OCTAVA: f0 en tres octavas
        //      distintas son UNA rejilla. Y con la rejilla declarada (modo
        //      rejilla fija) todas las ventanas SON la rejilla: plegado 0.
        bool sameOctave = true;
        for (const float ref : { 200.0f, 100.0f, 400.0f })
        {
            const auto r = SpectralAnalyzer::measureOctaveFold ({ 200.0f, 100.0f, 400.0f }, ref);
            sameOctave = sameOctave && ! r.biGrid && r.foldCents < 1.0e-3f && r.observations == 3;
        }
        check (sameOctave, "f0 en tres octavas (200/100/400) es UNA rejilla contra cualquier referencia");

        const auto pinned = SpectralAnalyzer::measureOctaveFold ({ 200.0f, 200.0f, 200.0f, 200.0f }, 200.0f);
        check (pinned.foldCents == 0.0f && pinned.rawCents == 0.0f && pinned.observations == 4,
               "con la rejilla declarada (rejilla fija) todas las ventanas son la rejilla: plegado 0");

        check (std::abs (SpectralAnalyzer::foldOctaveCents (400.0f, 100.0f)) < 1.0e-4f
                   && SpectralAnalyzer::foldOctaveCents (0.0f, 100.0f) == 0.0f
                   && SpectralAnalyzer::foldOctaveCents (300.0f, 0.0f) == 0.0f,
               "foldOctaveCents: las octavas exactas van a 0 y sin frecuencias utiles tambien (no propaga NaN)");
    }

    // ---------- 12. El barrido de pitch: bi-rejilla declarada ----------
    {
        constexpr int frames = 8;
        auto audio = makeLayeredMaterial (frames, 200.0f);   // + barrido 200 -> 600 Hz fuerte

        const auto model = analyzer.analyzeTemporal (audio, sr, gridHz, frames);
        const auto fold = analyzer.lastOctaveFold();

        check (fold.biGrid,
               "el material que barre el pitch es BI-rejilla: dispersion plegada " + juce::String (fold.foldCents, 0)
                   + " cents (cruda " + juce::String (fold.rawCents, 0) + ", " + juce::String (fold.observations) + " ventanas)");
        check (fold.rawCents >= fold.foldCents,
               "la cruda nunca es menor que la plegada: el plegado es lo que hace comparable la evidencia");
        check (analyzer.lastLayerCount() == 1 && ! model.isLayered(),
               "y el modelo se queda en una capa: el material no comparte la rejilla del llamador");
        note (juce::String ("el PRE-FILTRO lo declara lastClusteringSkipped(): ")
              + (analyzer.lastClusteringSkipped() ? "SALTADO (bi-rejilla no paga clustering)" : "corrido"));
    }

    // ---------- 13. Un sub una octava por debajo: sigue siendo mono-rejilla ----------
    {
        constexpr int frames = 8;
        auto audio = makeLayeredMaterial (frames, 0.0f);
        addTone (audio, 100.0f, 0.6f, 0, audio.getNumSamples(), false);   // el sub, una octava bajo la rejilla

        const auto model = analyzer.analyzeTemporal (audio, sr, gridHz, frames);
        const auto fold = analyzer.lastOctaveFold();

        check (! fold.biGrid,
               "un sub una octava por debajo NO vuelve bi-rejilla al material (plegado "
                   + juce::String (fold.foldCents, 0) + " cents, crudo " + juce::String (fold.rawCents, 0)
                   + ", " + juce::String (fold.octaveFlips) + " saltos de octava)");
        check (analyzer.lastLayerCount() == 2 && model.layerCount == 2,
               "y las capas se quedan: el sub no rompe la separacion del drone y la voz");
    }

    // ======================================================================
    // D. EL PRE-FILTRO DE PLEGADO (plan 10.4)
    // ======================================================================
    std::printf ("\nD. El pre-filtro de plegado (plan 10.4)\n");

    // ---------- 14. Bi-rejilla: la puerta exime ANTES, el clustering no corre ----------
    {
        constexpr int frames = 8;
        auto audio = makeLayeredMaterial (frames, 200.0f);   // barrido fuerte: bi-rejilla

        const auto model = analyzer.analyzeTemporal (audio, sr, gridHz, frames);
        const auto fold = analyzer.lastOctaveFold();

        check (fold.biGrid,
               "el material del barrido es bi-rejilla (plegado " + juce::String (fold.foldCents, 0) + " cents)");
        check (analyzer.lastClusteringSkipped(),
               "y el PRE-FILTRO lo exime: la puerta decide ANTES y el clustering NO corre");
        check (analyzer.lastLayerGridRejected() && analyzer.lastLayerCount() == 1 && model.layerCount == 1,
               "las capas quedan descartadas y el modelo es de una capa");

        // El PRE-FILTRO quita el CLUSTERING, no el ANALISIS: la f0 por frame
        // sigue el barrido (la representacion honesta de 10.6).
        bool tracked = false;
        for (int f = 1; f < frames; ++f)
            tracked = tracked || std::abs (model.f0At (f) - gridHz) > 1.0f;
        check (! model.isLayered() && model.frameCount == frames && tracked,
               "el modelo es el de una capa con f0 por frame (el analisis SI ocurre: f0 final "
                   + juce::String (model.f0At (frames - 1), 1) + " Hz)");
    }

    // ---------- 15. Mono-rejilla: el pre-filtro NO salta el clustering ----------
    {
        constexpr int frames = 8;
        auto audio = makeLayeredMaterial (frames, 0.0f);   // drone + voz, sin barrido

        const auto model = analyzer.analyzeTemporal (audio, sr, gridHz, frames);

        check (! analyzer.lastClusteringSkipped() && ! analyzer.lastOctaveFold().biGrid,
               "mono-rejilla: la puerta NO exime y el clustering SI corre");
        check (analyzer.lastLayerCount() == 2 && model.isLayered(),
               "y sigue dando las 2 capas del drone y la voz (la puerta no sobre-exime)");
    }

    // ---------- 16. Sin rejilla que seguir (REJILLA FIJA): plegado 0, no se exime ----------
    {
        constexpr int frames = 8;
        // El MISMO material bi-rejilla, pero con la rejilla FIJA declarada: no hay
        // f0 por ventana que seguir (todas son la del llamador), asi que el plegado
        // es 0 —mono-rejilla— y el pre-filtro NO se dispara. La puerta la decide la
        // POLITICA de f0, no el audio: es lo que la hace un pre-filtro del ANALISIS
        // y no del material.
        auto audio = makeLayeredMaterial (frames, 200.0f);

        analyzer.analyzeTemporal (audio, sr, gridHz, frames, true);   // fixedGrid

        check (! analyzer.lastClusteringSkipped() && ! analyzer.lastOctaveFold().biGrid,
               "con REJILLA FIJA la f0 por ventana es la del llamador: plegado 0 y el pre-filtro no exime");
        check (analyzer.lastOctaveFold().foldCents == 0.0f
                   && analyzer.lastOctaveFold().observations == frames,
               "las " + juce::String (analyzer.lastOctaveFold().observations) + " ventanas son la rejilla declarada (plegado exacto 0)");
    }

    // ======================================================================
    // E. LA METRICA, SELECCIONABLE (2026-09-26)
    // ======================================================================
    // El plan (seccion 3.3) pide el coseno por envolvente; el modulo decide por
    // descriptores. Aqui se corren LAS DOS sobre los mismos datos para fijar
    // donde coinciden y donde no: el coseno vale en las trazas sinteticas del
    // criterio de aceptacion y falla en las trazas reales del plan.
    std::printf ("\nE. Metrica seleccionable (LayerMetric)\n");

    // ---------- 17. Las dos metricas, sobre las trazas sinteticas ----------
    {
        TraceSet ts (8, 8);
        flat (ts, 0, 1.0f);
        flat (ts, 1, 0.95f);
        flat (ts, 2, 1.05f);

        for (int j = 0; j < 5; ++j)
            bump (ts, 3 + j, j, 2, 0.8f);

        const auto d = ts.cluster();                              // defecto
        const auto c = ts.cluster (LayerMetric::EnvelopeCosine);   // el plan

        const float cosDV = envelopeCosine (ts.trace (0), ts.trace (3), 8);

        check (d.layerCount == 2, "sinteticas + descriptores: 2 capas (el caso de aceptacion)");
        check (c.layerCount == 1,
               "sinteticas + coseno del plan: 1 capa (el coseno no separa NI AQUI)");
        check (cosDV < LayerClustering::kAffinityCut,
               "coseno drone-voz = " + juce::String (cosDV, 2) + " < corte "
                   + juce::String (LayerClustering::kAffinityCut, 2)
                   + ": ninguna pareja llega al corte");
        note ("sin NINGUNA fusion en el corte, el clamp a 3 y la guardia de "
              "degeneracion colapsan lo que el coseno no separo: por eso sale 1 capa");
    }

    // ---------- 18. Las dos metricas, sobre las trazas REALES del plan ----------
    {
        // Mismas trazas que la seccion 3 (CZ-RRISE, 9 ventanas): drone n1 + n3 y
        // voz n7/n15. Es el caso donde la eleccion se ve.
        const float drone[9] = { 307, 133, 177, 312, 145, 165, 308, 156, 154 };
        const float n3[9]    = { 154,  67,  89, 156,  73,  83, 154,  78,  77 };
        const float n7[9]    = { 461,   0, 119,  98,   0,  59,  75,   0,   0 };
        const float n15[9]   = {   0,   0,   0,  13,  31,  78, 323, 505, 289 };

        TraceSet ts (4, 9);
        for (int f = 0; f < 9; ++f)
        {
            ts.set (0, f, drone[f]);
            ts.set (1, f, n3[f]);
            ts.set (2, f, n7[f]);
            ts.set (3, f, n15[f]);
        }

        const auto d = ts.cluster();
        const auto c = ts.cluster (LayerMetric::EnvelopeCosine);

        check (d.layerCount == 2, "reales + descriptores: 2 capas (drone / voz)");
        check (c.layerCount == 1,
               "reales + coseno: 1 capa (aqui por lo contrario: fusiona de mas)");
        note ("reales: coseno(n1,n7) = "
              + juce::String (envelopeCosine (ts.trace (0), ts.trace (2), 9), 2)
              + " >= corte " + juce::String (LayerClustering::kAffinityCut, 2)
              + " => fusiona dos capas distintas; en las sinteticas ni fusiona ni separa");
    }

    // ---------- 19. La metrica llega al analizador de punta a punta ----------
    {
        constexpr int frames = 8;
        auto audio = makeLayeredMaterial (frames, 0.0f);

        analyzer.analyzeTemporal (audio, sr, gridHz, frames, false,
                                  LayerMetric::EnvelopeCosine);

        check (analyzer.lastLayerMetric() == LayerMetric::EnvelopeCosine,
               "analyzeTemporal publica la metrica pedida");
        check (analyzer.lastLayerCount() == 1,
               "el audio del caso de aceptacion con el coseno: 1 capa (con descriptores, 2)");
        check (analyzer.lastLayerMetric() == LayerMetric::EnvelopeCosine,
               "la metrica no se pierde por el camino (no la reescribe el analizador)");
    }

    std::printf ("\n%s (%d fallos)\n", failures == 0 ? "RESULT: OK" : "RESULT: FAIL", failures);
    return failures == 0 ? 0 : 1;
}
