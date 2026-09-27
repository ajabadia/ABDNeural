/*
  ==============================================================================

    TransposableOffsetsTest.cpp
    2026-09-25 — OFFSETS TRANSPONIBLES (modo delta-n/n): la inharmonicidad se
    transpone con el teclado.

    Los offsets del modelo viven en Hz y el motor los SUMA a la rejilla n*base,
    asi que la desviacion en cents de un parcial (offset/(n*f0)) CAMBIA con la
    nota: la misma inharmonicidad suena a otra cosa al transportar. El modo
    `offsetsTranspose` declara los offsets como RATIOS medidos contra la rejilla
    de analisis (`offsetRootHz`, normalmente frameSpanHz) y el motor los escala
    por base/f0 al renderizar:

        freq_n = n*base + offset * (base / f0)      =>  cents = INVARIANTE

    Propiedades que se fijan aqui:

      1. LEGADO BIT A BIT. Sin el modo el factor es 1.0 exacto, y con el modo
         TAMBIEN lo es cuando la nota renderizada ES la rejilla de analisis: el
         modo no toca el sonido del material, solo como se transporta. Se
         comprueba sobre el AUDIO renderizado, no sobre una tabla.
      2. INVARIANCIA MUSICAL. Una octava arriba el parcial sigue a la MISMA
         desviacion en cents (el offset se dobla en Hz); SIN el modo se queda en
         el offset absoluto y la desviacion se parte. Medido con Goertzel sobre
         el audio del motor real, no leyendo la tabla de parciales.
      3. El modo y su rejilla VIAJAN al snapshot (el motor lee el snapshot
         muestreado, no el modelo fuente): sin eso el motor no sabria que escalar.
      4. El formato: "offsetsTranspose" SOLO aparece declarado (misma regla que
         "gridFixed"), y no arrastra claves ajenas.

    El lector (PresetManager) se cubre en ModelMakerRoundTripTest, que ya enlaza
    la serializacion de produccion.

  ==============================================================================
*/

#include "../Source/DSP/CoreModules/NeuronikEngine.h"
#include "../Source/DSP/Synthesis/AdditiveVoice.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/Common/SpectralModelWriter.h"
#include "../Source/DSP/FrameSampler.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <iostream>

using NEURONiK::Common::SpectralModel;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  [ok]   " : "  [FALLO] ") << what << std::endl;
    if (! ok) ++failures;
}

constexpr double sr = 48000.0;

/** El parcial 3 (indice 2) es el unico activo: una sola sinusoide que medir. */
constexpr int kIndex = 2;
constexpr int kHarmonic = kIndex + 1;
constexpr float kOffsetHz = 30.0f;

constexpr int kBlock = 512;
constexpr int kBlocks = 32;                  // ~0,34 s de nota

/** Ventana de analisis: el sostenido, ya asentados los smoothers (glide 20 ms). */
constexpr int analysisStart = kBlock * 16;
constexpr int analysisCount = kBlock * 16;

constexpr int kNote = 60;

double baseOf (int note) { return dsp::MidiMessage::getMidiNoteInHertz (note); }

/** Desviacion en cents de `freq` respecto de `harmonicRef` (n*base). */
double centsFrom (double freq, double harmonicRef)
{
    return 1200.0 * std::log2 (freq / harmonicRef);
}

struct Measurement
{
    juce::AudioBuffer<float> audio { 1, kBlock * kBlocks };
};

NEURONiK::DSP::Synthesis::AdditiveVoice::Params neutralParams()
{
    NEURONiK::DSP::Synthesis::AdditiveVoice::Params p;
    p.oscLevel = 1.0f;
    p.attack = 1.0f;
    p.decay = 1000.0f;
    p.sustain = 0.7f;
    p.release = 10.0f;
    p.filterCutoff = 20000.0f;   // el filtro no colorea la medida
    p.filterRes = 0.0f;
    // filterEnvAmount retirado (2026-09-26): la profundidad de ENV 2 vive en la matriz.
    p.morphX = 0.0f;             // eje Z aislado: el morfeo usa solo el slot A
    p.morphY = 0.0f;
    // Una sola sinusoide: ni unisonio (otra frecuencia) ni stretching (otra rejilla).
    p.unisonDetune = 0.0f;
    p.inharmonicity = 0.0f;
    p.resonatorShift = 1.0f;
    p.resonatorRollOff = 1.0f;
    return p;
}

/** Renderiza una nota con el motor aditivo real y devuelve su audio. */
Measurement renderAdditive (const SpectralModel& model, int note)
{
    NEURONiK::DSP::NeuronikEngine engine;
    engine.prepare (sr, kBlock);
    engine.loadModel (model, 0);
    engine.setVoiceParams (neutralParams());
    engine.updateParameters();

    Measurement out;
    dsp::AudioBuffer<float> block (2, kBlock);   // el motor habla el puerto DSP

    for (int b = 0; b < kBlocks; ++b)
    {
        block.clear();
        dsp::MidiBuffer midi;

        if (b == 0)
            midi.addEvent (dsp::MidiMessage::noteOn (1, note, 1.0f), 0);

        engine.renderNextBlock (block, midi);

        for (int i = 0; i < kBlock; ++i)
            out.audio.setSample (0, b * kBlock + i,
                                 block.getSample (0, i) + block.getSample (1, i));
    }

    return out;
}

/** Energia de una frecuencia por el algoritmo de Goertzel. */
double goertzel (const juce::AudioBuffer<float>& buffer, double freq)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * freq / sr;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;

    for (int i = analysisStart; i < analysisStart + analysisCount; ++i)
    {
        const double s0 = (double) buffer.getSample (0, i) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2));
}

double energyAt (const Measurement& m, double freq) { return goertzel (m.audio, freq); }

/** La frecuencia del parcial: barrido fino del pico de Goertzel. */
double measurePeak (const Measurement& m, double around, double halfSpan, double step)
{
    double bestF = around, bestE = -1.0;

    for (double f = around - halfSpan; f <= around + halfSpan; f += step)
    {
        const double e = energyAt (m, f);
        if (e > bestE) { bestE = e; bestF = f; }
    }

    return bestF;
}

/** El modelo de prueba: un parcial, un offset, la rejilla = la nota de kNote. */
SpectralModel probeModel (bool transpose)
{
    SpectralModel m;
    m.amplitudes.fill (0.0f);
    m.frequencyOffsets.fill (0.0f);
    m.amplitudes[(size_t) kIndex] = 1.0f;
    m.frequencyOffsets[(size_t) kIndex] = kOffsetHz;
    m.frameSpanHz = (float) baseOf (kNote);   // rejilla de analisis declarada
    m.offsetsTranspose = transpose;
    m.isValid = true;
    return m;
}

} // namespace

int main()
{
    std::cout << "2026-09-25: offsets transpuestos (ratio delta-n/n)" << std::endl;

    const double base = baseOf (kNote);
    const float span = (float) base;

    // ---------- 0. El factor: 1.0 exacto sin modo, base/f0 con modo ----------
    {
        check (probeModel (false).offsetScaleAt (2.0f * span) == 1.0f,
               "sin modo el factor es 1.0 EXACTO (el offset se suma en Hz, como siempre)");
        check (probeModel (true).offsetScaleAt (2.0f * span) == 2.0f,
               "con modo el factor es base/f0 (una octava = 2)");

        SpectralModel noGrid = probeModel (true);
        noGrid.frameSpanHz = 0.0f;
        check (noGrid.offsetScaleAt (2.0f * span) == 1.0f,
               "modo declarado SIN rejilla de referencia: inerte (factor 1.0, no se inventa una)");
    }

    // ---------- 1. En la nota de la rejilla el modo NO toca el sonido ----------
    {
        const auto a = renderAdditive (probeModel (false), kNote);
        const auto b = renderAdditive (probeModel (true), kNote);

        bool identical = a.audio.getNumSamples() == b.audio.getNumSamples();

        for (int i = 0; i < a.audio.getNumSamples() && identical; ++i)
            identical = a.audio.getSample (0, i) == b.audio.getSample (0, i);

        check (identical, "en la nota de la rejilla, con modo y sin modo el audio es BIT-EXACTO");
    }

    // ---------- 2. Una octava arriba: el offset SIGUE a la nota ----------------
    {
        const double base2 = baseOf (kNote + 12);
        const double scale = base2 / (double) span;

        const double musical  = kHarmonic * base2 + kOffsetHz * scale;   // modo ON
        const double absolute = kHarmonic * base2 + kOffsetHz;           // modo OFF

        const auto onM  = renderAdditive (probeModel (true),  kNote + 12);
        const auto offM = renderAdditive (probeModel (false), kNote + 12);
        const auto refM = renderAdditive (probeModel (false), kNote);

        check (energyAt (onM, musical) > 4.0 * energyAt (onM, absolute),
               "modo ON una octava arriba: el parcial esta en n*base + offset*base/f0, NO en el offset absoluto");
        check (energyAt (offM, absolute) > 4.0 * energyAt (offM, musical),
               "modo OFF una octava arriba: el parcial esta en el offset ABSOLUTO (el legado no se mueve)");

        const double centsRef   = centsFrom (measurePeak (refM, kHarmonic * base + kOffsetHz, 40.0, 0.25),
                                             kHarmonic * base);
        const double centsOn    = centsFrom (measurePeak (onM, musical, 40.0, 0.25),
                                             kHarmonic * base2);
        const double centsOff   = centsFrom (measurePeak (offM, absolute, 40.0, 0.25),
                                             kHarmonic * base2);

        std::cout << "  [medida] residuo del parcial " << kHarmonic << ": rejilla "
                  << juce::String (centsRef, 2) << " cents | una octava ON "
                  << juce::String (centsOn, 2) << " cents | una octava OFF "
                  << juce::String (centsOff, 2) << " cents" << std::endl;

        check (std::abs (centsOn - centsRef) < 6.0,
               "modo ON: la desviacion en cents se CONSERVA al transportar una octava (inharmonicidad musical)");
        check (std::abs (centsOff - centsRef) > 20.0,
               "modo OFF: la desviacion en cents NO se conserva (el problema que arregla el modo)");
    }

    // ---------- 3. El modo y su rejilla viajan al snapshot --------------------
    {
        const auto model = probeModel (true);

        SpectralModel snap;
        NEURONiK::Common::sampleFrame (model, 0.5f, snap);
        check (snap.offsetsTranspose && snap.offsetRootHz == span,
               "sampleFrame: el snapshot lleva el modo Y su rejilla de referencia");

        auto zs = NEURONiK::Common::restLayerMorphZ();
        NEURONiK::Common::sampleLayeredFrame (model, zs, snap);
        check (snap.offsetsTranspose && snap.offsetRootHz == span,
               "sampleLayeredFrame: idem por el camino de capas (el que usa el motor)");

        SpectralModel legacy;
        NEURONiK::Common::sampleFrame (probeModel (false), 0.5f, legacy);
        check (! legacy.offsetsTranspose, "sin modo el snapshot no declara nada (legado intacto)");
    }

    // ---------- 4. El formato: la clave solo cuando el modo esta declarado ----
    {
        const auto jsonOn  = juce::JSON::toString (NEURONiK::Common::modelToJson (probeModel (true),  "t", "d"));
        const auto jsonOff = juce::JSON::toString (NEURONiK::Common::modelToJson (probeModel (false), "t", "d"));

        check (jsonOn.contains ("offsetsTranspose"),
               "el escritor declara el modo cuando esta puesto");
        check (! jsonOff.contains ("offsetsTranspose"),
               "sin modo el JSON no cambia (clave ausente, los ficheros de siempre no se mueven)");
        check (! jsonOn.contains ("gridFixed") && ! jsonOn.contains ("layers"),
               "el modo no arrastra claves que no le tocan (ortogonal a gridFixed y a las capas)");
    }

    if (failures == 0)
        std::cout << "RESULT: OK" << std::endl;
    else
        std::cout << "RESULT: FALLOS = " << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
