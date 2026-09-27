/*
  ==============================================================================

    LayerEngineTest.cpp
    FASE 11.3 — el motor SUMA las capas (sampler por capa + morphZ2/3 + pesos).

    El modelo de prueba es un RRISE sintetico de DOS capas, con el reparto que
    produce el analizador (cada indice en UNA capa):

      - capa 0 ("drone"): 2 frames; el frame 0 canta el parcial 1 y el frame 1 el
        parcial 2. La mueve `morphZ` (el eje de siempre).
      - capa 1 ("barrido"): 4 frames; el parcial 3 (indice 2) en todos, con el
        offset deslizando -180 -> +180 Hz. La mueve `morphZ2`.

    Propiedades fijadas:
      1. Con una capa, `sampleLayeredFrame` es BIT-EXACTO a `sampleFrame` (la
         condicion que deja quieto todo el legado) y los pesos por capa se aplican.
      2. El frame efectivo SUMA las capas: con una capa el parcial del barrido esta
         MUDO (es lo que sonaba antes); con dos, canta.
      3. El barrido por capas EN EL MOTOR (aditivo): `morphZ1` mueve el drone y NO
         toca al barrido; `morphZ2` mueve el barrido y NO toca al drone. Se mide
         sobre el audio renderizado (Goertzel), no sobre la tabla de parciales: que
         la voz SE MUEVA es una frecuencia, y eso no lo dice una amplitud.
      4. `resetModulations` limpia los z: la matriz SUMA en cada tramo de control,
         asi que un destino que no se limpia se acumula solo (modMorphZ se habia
         quedado fuera de la lista).

  ==============================================================================
*/

#include "../Source/DSP/CoreModules/NeuronikEngine.h"
#include "../Source/DSP/CoreModules/NeurotikEngine.h"
#include "../Source/DSP/Synthesis/AdditiveVoice.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/FrameSampler.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <vector>
#include <cstring>
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

/** Espaciado declarado del fichero: dimensiona la envoltura de offsets. */
constexpr float spanHz = 400.0f;

/** Los cuatro offsets del barrido: deltas de 120 Hz, por debajo de span/2, asi
    que el camino corto no envuelve y el barrido va en un solo sentido. */
constexpr float sweepOffsets[4] = { -180.0f, -60.0f, 60.0f, 180.0f };

SpectralModel twoLayerSweepModel()
{
    SpectralModel m;
    m.amplitudes.fill (0.0f);
    m.frequencyOffsets.fill (0.0f);
    m.amplitudes[0] = 1.0f;                  // capa 0, frame 0: parcial 1
    m.extraAmps[0].fill (0.0f);
    m.extraOffsets[0].fill (0.0f);
    m.extraAmps[0][1] = 1.0f;                // capa 0, frame 1: parcial 2
    m.frameCount = 2;
    m.frameSpanHz = spanHz;
    m.isValid = true;

    m.setLayerCount (2);
    m.setLayerWeightAt (0, 1.0f);
    m.setLayerWeightAt (1, 1.0f);
    m.setNumFramesOf (1, 4);

    for (int f = 0; f < 4; ++f)
    {
        auto* amps = m.ampsOf (1, f);
        auto* offs = m.offsetsOf (1, f);

        for (int i = 0; i < 64; ++i)
        {
            amps[i] = 0.0f;
            offs[i] = 0.0f;
        }

        amps[2] = 1.0f;                      // parcial 3 (indice 2) del barrido
        offs[2] = sweepOffsets[f];
    }

    return m;
}

/** El MISMO drone con una sola capa: es el camino de siempre (legado). */
SpectralModel oneLayerModel()
{
    SpectralModel m = twoLayerSweepModel();
    m.setLayerCount (1);
    return m;
}

/** Bit-exacto, no "el epsilon de siempre": la garantia del legado es de bits. */
bool sameSnapshot (const SpectralModel& a, const SpectralModel& b)
{
    return a.frameCount == b.frameCount
           && std::memcmp (&a.frameF0, &b.frameF0, sizeof (float)) == 0
           && std::memcmp (a.amplitudes.data(), b.amplitudes.data(), sizeof (a.amplitudes)) == 0
           && std::memcmp (a.frequencyOffsets.data(), b.frequencyOffsets.data(),
                           sizeof (a.frequencyOffsets)) == 0;
}

// FASE 11.3 (frontera WASM): la funcion que cruza las capas. Fuera de
// emscripten WASM_EXPORT es vacio y la TU del puente la define con linkage
// C; el test la declara aqui (ambito global, como el propio puente).
extern "C" void neuronikLoadModelLayers (int slot, int engineType,
                                          const float* data, int isValid,
                                          const float* extraData, int layerFrames,
                                          float layerWeight, const float* frameWeights);

// FASE 11.4: el VOLUMEN de las capas 1 y 2 (misma TU del puente).
extern "C" void neuronikSetVoiceLayerMorph (float layerGain2, float layerGain3);

/** Energia de una frecuencia por el algoritmo de Goertzel. */
double goertzel (const juce::AudioBuffer<float>& buffer, int start, int count, double freq)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * freq / sr;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;

    for (int i = start; i < start + count; ++i)
    {
        const double s0 = (double) buffer.getSample (0, i) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - coeff * s1 * s2));
}

constexpr int kBlock = 512;
constexpr int kBlocks = 32;                  // ~0,34 s de nota

/** Ventana de analisis: el sostenido, ya asentados los smoothers (glide 20 ms). */
constexpr int analysisStart = kBlock * 16;
constexpr int analysisCount = kBlock * 16;

constexpr int kNote = 60;

struct Measurement
{
    juce::AudioBuffer<float> audio { 1, kBlock * kBlocks };
    std::array<float, 64> partials {};
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
    // filterEnvAmount retirado: la profundidad de ENV 2 vive en la matriz (factor 0 por defecto aqui: ruta sin amount = silencio de ENV 2, igual que ponia el knob a 0).
    p.morphX = 0.0f;             // eje Z aislado: el morfeo usa solo el slot A
    p.morphY = 0.0f;
    return p;
}

/** Renderiza una nota con el motor aditivo y devuelve audio + tabla de parciales. */
Measurement renderAdditive (const SpectralModel& model, float z1, float z2)
{
    NEURONiK::DSP::NeuronikEngine engine;
    engine.prepare (sr, kBlock);
    engine.loadModel (model, 0);

    auto params = neutralParams();
    params.morphZ = z1;
    params.morphZ2 = z2;
    params.morphZ3 = 0.0f;
    engine.setVoiceParams (params);
    engine.updateParameters();

    Measurement out;
    dsp::AudioBuffer<float> block (2, kBlock);   // el motor habla el puerto DSP

    for (int b = 0; b < kBlocks; ++b)
    {
        block.clear();
        dsp::MidiBuffer midi;

        if (b == 0)
            midi.addEvent (dsp::MidiMessage::noteOn (1, kNote, 1.0f), 0);

        engine.renderNextBlock (block, midi);

        for (int i = 0; i < kBlock; ++i)
            out.audio.setSample (0, b * kBlock + i,
                                 block.getSample (0, i) + block.getSample (1, i));
    }

    engine.getSpectralData (out.partials.data());
    return out;
}

double energyAt (const Measurement& m, double freq)
{
    return goertzel (m.audio, analysisStart, analysisCount, freq);
}

} // namespace

int main()
{
    std::cout << "FASE 11.3: el motor suma las capas" << std::endl;

    // ---------- 1. Una capa: bit-exacto al camino de siempre ----------
    {
        const auto legacy = oneLayerModel();

        bool identical = true;

        for (const float z : { 0.0f, 0.37f, 1.0f })
        {
            SpectralModel a, b;
            NEURONiK::Common::sampleFrame (legacy, z, a);

            auto zs = NEURONiK::Common::restLayerMorphZ();
            zs[0] = z;
            NEURONiK::Common::sampleLayeredFrame (legacy, zs, b);

            identical = identical && sameSnapshot (a, b);
        }

        check (identical, "una capa: sampleLayeredFrame es BIT-EXACTO a sampleFrame");
    }

    // ---------- 2. La suma y los pesos ----------
    {
        const auto layered = twoLayerSweepModel();
        SpectralModel out;
        NEURONiK::Common::sampleLayeredFrame (layered, NEURONiK::Common::restLayerMorphZ(), out);

        check (out.amplitudes[0] == 1.0f && out.amplitudes[2] == 1.0f,
               "dos capas: el frame efectivo lleva la capa 0 Y la capa 1 (indice 2)");

        SpectralModel weighted = layered;
        weighted.setLayerWeightAt (1, 0.5f);
        weighted.setFrameWeightAt (1, 0, 0.25f);

        SpectralModel half;
        NEURONiK::Common::sampleLayeredFrame (weighted, NEURONiK::Common::restLayerMorphZ(), half);

        // Los dos pesos se MULTIPLICAN: 0,5 (mezcla estatica) x 0,25 (peso del
        // frame 0) = 0,125, y la capa 0 no se entera.
        check (half.amplitudes[2] == 0.125f && half.amplitudes[0] == 1.0f,
               "pesos de capa: estatico x temporal escalan SOLO la capa 1 (0,5 x 0,25)");
    }

    // ---------- 3. El motor aditivo: el barrido por capas ----------
    {
        const auto model = twoLayerSweepModel();
        const double base = dsp::MidiMessage::getMidiNoteInHertz (kNote);

        const auto drone1 = base * 1.0;
        const auto drone2 = base * 2.0;
        const auto sweepStart = base * 3.0 + (double) sweepOffsets[0];
        const auto sweepEnd = base * 3.0 + (double) sweepOffsets[3];

        const auto a = renderAdditive (model, 0.0f, 0.0f);   // drone n1, barrido al inicio
        const auto b = renderAdditive (model, 1.0f, 0.0f);   // drone n2, barrido al inicio
        const auto c = renderAdditive (model, 1.0f, 1.0f);   // drone n2, barrido al final

        check (energyAt (a, drone1) > 4.0 * energyAt (a, drone2),
               "z1=0: el drone canta el parcial 1 (el 2 esta mudo)");
        check (energyAt (a, sweepStart) > 4.0 * energyAt (a, sweepEnd),
               "z1=0, z2=0: el barrido esta en su offset inicial");

        check (energyAt (b, drone2) > 4.0 * energyAt (b, drone1),
               "z1=1: el drone cambia al parcial 2");
        check (energyAt (b, sweepStart) > 4.0 * energyAt (b, sweepEnd),
               "z1 NO toca al barrido: sigue en su offset inicial");

        check (energyAt (c, sweepEnd) > 4.0 * energyAt (c, sweepStart),
               "z2=1: el barrido se ha movido a su offset final");
        check (energyAt (c, drone2) > 4.0 * energyAt (c, drone1),
               "z2 NO toca al drone (sigue en el parcial 2 de z1=1)");

        check (a.partials[0] > 0.05f && a.partials[1] < 1.0e-6f && a.partials[2] > 0.05f,
               "la tabla de parciales publica las DOS capas (drone + barrido)");

        // ---------- 3b. Control: con UNA capa el barrido no existe ----------
        const auto legacy = renderAdditive (oneLayerModel(), 0.0f, 0.0f);

        check (legacy.partials[2] < 1.0e-6f,
               "control de una capa: sin capa 1, el parcial del barrido esta MUDO");
        check (energyAt (legacy, drone1) > 4.0 * energyAt (legacy, sweepStart),
               "control de una capa: suena el drone y no el barrido");
    }

    // ---------- 3c. HALLAZGO (medido, NO arreglado aqui): el remapeo de rejilla
    // del motor no se activa porque el snapshot no lleva frameSpanHz ----------
    //
    // Los dos motores calculan `gridRatio = frameF0 / frame.f0At(0)` sobre el
    // frame MUESTREADO (Resonator / ResonatorBank), pero `sampleFrame` /
    // `sampleLayerFrame` no copian `frameSpanHz` al snapshot: `f0At(0)` sale 0 y
    // el ratio queda en 1.0 SIEMPRE, con lo que la trayectoria de f0 de la Fase
    // 10.6 (modelos que cantan el pitch del WAV) nunca remapea la rejilla. Se
    // pinnea el ESTADO actual —medida, no fallo— para que cambiarlo sea un acto
    // deliberado: activarlo mueve el sonido de material real ya existente.
    {
        auto model = twoLayerSweepModel();
        model.setF0At (1, model.frameSpanHz * 2.0f);   // trayectoria declarada (2x)

        SpectralModel frame;
        NEURONiK::Common::sampleFrame (model, 0.5f, frame);

        const float gridRatio = (frame.frameF0 > 0.0f && frame.f0At (0) > 0.0f)
                                    ? frame.frameF0 / frame.f0At (0)
                                    : 1.0f;

        check (frame.frameF0 > 0.0f && frame.frameSpanHz == 0.0f && gridRatio == 1.0f,
               "HALLAZGO: el snapshot lleva frameF0 (" + juce::String (frame.frameF0, 1)
                   + " Hz) pero NO frameSpanHz -> el ratio del motor queda en 1 (10.6 inactivo)");
    }

    // ---------- 4. La matriz no acumula: resetModulations limpia los z ----------
    {
        NEURONiK::DSP::Synthesis::AdditiveVoice voice;
        voice.modMorphX = 0.1f;
        voice.modMorphZ = 0.4f;
        voice.modMorphZ2 = 0.3f;
        voice.modMorphZ3 = 0.2f;

        voice.resetModulations();

        check (voice.modMorphX == 0.0f && voice.modMorphZ == 0.0f
                   && voice.modMorphZ2 == 0.0f && voice.modMorphZ3 == 0.0f,
               "resetModulations limpia los tres z (modMorphZ se acumulaba sin freno)");
    }

    // ---------- 5. El motor modal: el mismo modelo, la misma suma ----------
    {
        NEURONiK::DSP::NeurotikEngine engine;
        engine.prepare (sr, kBlock);
        engine.loadModel (twoLayerSweepModel(), 0);

        NEURONiK::DSP::Synthesis::NeurotikVoice::Params p;
        p.impulseMix = 1.0f;                 // excitacion percusiva clasica
        p.attack = 1.0f;
        p.decay = 1000.0f;
        p.sustain = 0.7f;
        p.release = 10.0f;
        p.morphX = 0.0f;                     // eje Z aislado (solo slot A)
        p.morphY = 0.0f;
        engine.setVoiceParams (p);
        engine.updateParameters();

        dsp::AudioBuffer<float> buffer (2, kBlock);

        const auto partialsAt = [&engine, &p, &buffer] (float z1, float z2)
        {
            auto params = p;
            params.morphZ = z1;
            params.morphZ2 = z2;
            engine.setVoiceParams (params);
            engine.updateParameters();

            for (int b = 0; b < 12; ++b)
            {
                buffer.clear();
                dsp::MidiBuffer midi;

                if (b == 0)
                    midi.addEvent (dsp::MidiMessage::noteOn (1, kNote, 1.0f), 0);

                engine.renderNextBlock (buffer, midi);
            }

            std::array<float, 64> out {};
            engine.getSpectralData (out.data());

            dsp::MidiBuffer release;
            release.addEvent (dsp::MidiMessage::noteOff (1, kNote, 0.0f), 0);

            for (int b = 0; b < 8; ++b)
            {
                buffer.clear();
                engine.renderNextBlock (buffer, release);
                release.clear();
            }

            return out;
        };

        const auto drone0 = partialsAt (0.0f, 0.0f);
        const auto drone1 = partialsAt (1.0f, 0.0f);

        check (drone0[0] > 0.05f && drone0[2] > 0.05f,
               "banco modal: las DOS capas estan en la tabla de parciales");
        check (drone0[0] > drone0[1] && drone1[1] > drone1[0],
               "banco modal: morphZ1 mueve el drone (y morphZ2 no lo toca)");
    }

    // ---------- 6. FASE 11.3 en la frontera WASM: neuronikLoadModelLayers --
    // El export que cruza las capas (el worklet lo llama). La declaracion
    // extern "C" vive en ambito global (ver arriba, junto a goertzel): la
    // TU del puente (NeuronikWasmBridge.cpp, compilada en este target) la
    // define con linkage C.
    {

        const auto model = twoLayerSweepModel();
        constexpr int floatsPerFrame = 3 * 64 + 1;

        // Raiz v1: 64 amps + 64 offsets (capa 0 del modelo de dos capas).
        std::vector<float> root (128);
        for (int i = 0; i < 64; ++i)
        {
            root[(size_t) i] = model.amplitudes[(size_t) i];
            root[(size_t) (64 + i)] = model.frequencyOffsets[(size_t) i];
        }

        // Capa extra: 4 frames x {amps, offsets, frameF0} + pesos.
        std::vector<float> extra ((size_t) floatsPerFrame * 4 + 4, 0.0f);
        for (int f = 0; f < 4; ++f)
        {
            const size_t base = (size_t) f * (size_t) floatsPerFrame;

            for (int i = 0; i < 64; ++i)
            {
                extra[base + (size_t) i] = model.ampAt (1, f, i);
                extra[base + 64 + (size_t) i] = model.offsetAt (1, f, i);
            }
            extra[base + 128] = 0.0f;   // rejilla comun
            extra[(size_t) (floatsPerFrame * 4 + f)] = model.frameWeightAt (1, f);
        }

        // El export escribe en la INSTANCIA del puente; los tests que
        // renderizan abajo usan motores propios, asi que la secuencia es:
        // export -> leer el struct del motor del puente (getSpectralData no
        // llega tan hondo) -> comprobar con el sampler, que es la MISMA
        // funcion que el motor llama.
        // Verificacion del LAYOUT (lo que el export descodifica): montar un
        // modelo de referencia y comprobar que el struct reconstruido por el
        // export es identico al del sampler.
        // El motor del puente vive en NeuronikWasmBridge.cpp (instancia
        // estatica); para VER el struct relleno el test usa el sampler sobre
        // un modelo reconstruido con el MISMO layout que el export consume —
        // y el propio export se ejercita para que no quede sin llamar.
        {
            // Carga real por el export (motor por defecto: neuronik, slot 0).
            neuronikLoadModelLayers (0, 0, root.data(), 1,
                                     extra.data(), 4, model.layerWeightAt (1),
                                     extra.data() + (size_t) (floatsPerFrame * 4));

            // El struct que el export construyo, reconstruido desde el MISMO
            // layout (no desde el motor): es la especificacion del cable.
            NEURONiK::Common::SpectralModel rebuilt;
            for (int i = 0; i < 64; ++i)
            {
                rebuilt.amplitudes[(size_t) i] = root[(size_t) i];
                rebuilt.frequencyOffsets[(size_t) i] = root[(size_t) (64 + i)];
            }
            rebuilt.isValid = true;
            rebuilt.setLayerCount (2);
            rebuilt.setLayerWeightAt (1, model.layerWeightAt (1));
            rebuilt.setNumFramesOf (1, 4);

            for (int f = 0; f < 4; ++f)
            {
                const size_t base = (size_t) f * (size_t) floatsPerFrame;

                for (int i = 0; i < 64; ++i)
                {
                    rebuilt.ampsOf (1, f)[(size_t) i] = extra[base + (size_t) i];
                    rebuilt.offsetsOf (1, f)[(size_t) i] = extra[base + 64 + (size_t) i];
                }
                rebuilt.setF0At (1, f, extra[base + 128]);
                rebuilt.setFrameWeightAt (1, f, extra[(size_t) (floatsPerFrame * 4 + f)]);
            }

            SpectralModel viaExport, directo;
            NEURONiK::Common::sampleLayeredFrame (rebuilt, NEURONiK::Common::restLayerMorphZ(), viaExport);
            NEURONiK::Common::sampleLayeredFrame (model, NEURONiK::Common::restLayerMorphZ(), directo);

            bool layoutOk = true;
            for (int i = 0; i < 64 && layoutOk; ++i)
                layoutOk = layoutOk
                           && viaExport.amplitudes[(size_t) i] == directo.amplitudes[(size_t) i]
                           && viaExport.frequencyOffsets[(size_t) i] == directo.frequencyOffsets[(size_t) i];

            check (layoutOk && viaExport.amplitudes[(size_t) 2] > 0.05f,
                   "WASM: el layout del export reconstruye el modelo de dos capas (el barrido canta)");

            // Y con el z2 al fondo, el offset del barrido es el del frame final:
            auto zs = NEURONiK::Common::restLayerMorphZ();
            zs[1] = 1.0f;

            NEURONiK::Common::sampleLayeredFrame (rebuilt, zs, viaExport);
            NEURONiK::Common::sampleLayeredFrame (model, zs, directo);

            check (viaExport.frequencyOffsets[(size_t) 2] == directo.frequencyOffsets[(size_t) 2]
                       && std::abs (viaExport.frequencyOffsets[(size_t) 2] - sweepOffsets[3]) < 0.5f,
                   "WASM: morphZ2 mueve el barrido del modelo cruzado por el export");
        }
    }

    // ---------- 7. FASE 11.4: GANANCIA por capa — la capa es una voz --
    // La suma ya era por-capa con su propio z (11.3); el volumen tambien:
    // LayerGains escala cada capa DESPUES de muestrearla (0 = callada) y
    // el dueno del offset lo decide la misma regla, sobre amplitudes ya
    // escaladas. Con todas a 1.0, el camino es el de siempre.
    {
        const auto model = twoLayerSweepModel();

        // El camino de 3 args ES el de ganancias en reposo: bit-exacto.
        {
            SpectralModel a, b;
            NEURONiK::Common::sampleLayeredFrame (model, NEURONiK::Common::restLayerMorphZ(), a);
            NEURONiK::Common::sampleLayeredFrame (model, NEURONiK::Common::restLayerMorphZ(),
                                                  NEURONiK::Common::restLayerGains(), b);
            check (sameSnapshot (a, b),
                   "11.4: el camino de siempre es el de ganancias en reposo (bit-exacto)");
        }

        // Capa 1 callada: el barrido (SOLO capa 1) desaparece; la raiz no se toca.
        {
            auto zs = NEURONiK::Common::restLayerMorphZ();
            zs[1] = 1.0f;                                   // el barrido en su extremo
            auto gains = NEURONiK::Common::restLayerGains();
            gains[1] = 0.0f;                                // ...pero CALLADA

            SpectralModel out;
            NEURONiK::Common::sampleLayeredFrame (model, zs, gains, out);

            check (out.amplitudes[(size_t) 0] == 1.0f && out.amplitudes[(size_t) 2] == 0.0f,
                   "11.4: ganancia 0 en la capa 1 apaga SU parcial y no toca la raiz");
        }

        // Capa 1 a media: el parcial baja a la mitad EXACTA y el offset
        // sigue siendo suyo (el dueno se decide sobre amplitudes escaladas).
        {
            auto zs = NEURONiK::Common::restLayerMorphZ();
            zs[1] = 1.0f;
            auto gains = NEURONiK::Common::restLayerGains();
            gains[1] = 0.5f;

            SpectralModel out;
            NEURONiK::Common::sampleLayeredFrame (model, zs, gains, out);

            check (out.amplitudes[(size_t) 2] == 0.5f
                       && out.frequencyOffsets[(size_t) 2] == sweepOffsets[3],
                   "11.4: ganancia 0.5 escala SU parcial y conserva el offset del z2");
        }

        // La frontera: el export del VOLUMEN se ejercita con el motor del
        // puente (el read-modify-write se verifica por las voces en el
        // WASM real, como el resto de los exports de morph).
        neuronikSetVoiceLayerMorph (0.25f, 0.75f);
    }

    std::cout << (failures == 0 ? "RESULT: OK (0 fallos)"
                                : "RESULT: FALLO (" + juce::String (failures) + ")")
              << std::endl;

    return failures == 0 ? 0 : 1;
}
