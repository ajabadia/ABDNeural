/*
  ==============================================================================

    WasmParityTest.cpp
    Description: Fase 5 (1/2) — referencia NATIVA para la paridad WASM<->nativo.

    Ejecuta 5 escenarios deterministas sobre la frontera de la Fase 1
    (DspEngineFacade + Runtime::Event + GlobalParams) — exactamente la misma
    ruta que consume el puente WASM — y vuelca las muestras del canal
    izquierdo a JSON. Tests/neuronik_wasm_parity.mjs instancia el módulo WASM
    real y compara muestra a muestra contra este volcado.

    MATRIZ (validación de sample rate y tamaño de bloque): el volcado cubre
    3 sample rates x 3 tamaños de bloque. La duración de cada escenario es la
    MISMA en todos los casos — el número de bloques se escala sobre una
    referencia de 128 muestras — para que la comparación sea de contenido
    musical y no de número de llamadas. Dos consecuencias, las dos buscadas:
      - WASM vs nativo se compara en cada caso (presupuesto 0 ulps).
      - El propio test comprueba, en nativo, que la salida NO depende del
        tamaño de bloque: si algún día un módulo empieza a suavizar por bloque,
        esto lo caza aquí antes que en el DAW del usuario.

    El DSP es determinista (siembra fija de LFO/voces del 2026-09-17,
    entropyAmount=0), así que la misma secuencia produce la misma salida en
    cualquier pasada y máquina. Los escenarios están duplicados 1:1 en el
    test JS: si cambias uno aquí, cambia el otro.

    Uso: NEURONiK_WasmParityTest.exe [ruta/salida.json]

  ==============================================================================
*/

// El volcado se escribe con stdio (varios MB, con formato "%.9g" que tiene que
// coincidir con lo que parsea Tests/neuronik_wasm_parity.mjs), asi que el uso de
// fopen/fprintf es deliberado. Este generador es host-only: no entra en el
// modulo WASM.
#if defined (_MSC_VER) && ! defined (_CRT_SECURE_NO_WARNINGS)
 #define _CRT_SECURE_NO_WARNINGS
#endif

#include "../Source/DSP/Runtime/DspEngineFacade.h"
#include "../Source/DSP/DspTypes.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/CoreModules/NeuronikEngine.h"
#include "../Source/DSP/CoreModules/NeurotikEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace NEURONiK::DSP;

namespace
{
    /** Tamaño de bloque de referencia: los escenarios declaran sus bloques con
     *  este valor y cada caso los reescala proporcionalmente (misma duración). */
    constexpr int kReferenceBlockSize = 128;

    struct Case
    {
        double sampleRate;
        int blockSize;
    };

    /** 3 sample rates reales x 3 tamaños de bloque reales: 64 (sub-bloque),
     *  128 (el de la suite) y 512 (el buffer grande del driver). 96 kHz está a
     *  propósito: es donde un error de "muestras por segundo" se nota.
     *  Requisito: kReferenceBlockSize % blockSize == 0. */
    const std::vector<Case>& parityCases()
    {
        static const std::vector<Case> cases
        {
            { 44100.0,  64 }, { 44100.0, 128 }, { 44100.0, 512 },
            { 48000.0,  64 }, { 48000.0, 128 }, { 48000.0, 512 },
            { 96000.0,  64 }, { 96000.0, 128 }, { 96000.0, 512 }
        };
        return cases;
    }

    struct BlockEvent
    {
        int block;
        Runtime::Event event;
    };

    struct Scenario
    {
        const char* name;
        int engineType;                     // 0 = Neuronik, 1 = Neurotik
        GlobalParams params;
        std::vector<BlockEvent> events;
        int blocks;
        int panicAtBlock = -1;              // facade->allNotesOff() antes de renderizar este bloque
        bool loadModel = false;             // carga el modelo espectral E en el slot 0
    };

    /** Genera el modelo del escenario E. SOLO constantes exactas (potencias de
     *  dos) y multiplicación por 2^-7: idénticas bits en float32 y en double,
     *  así que MSVC y V8 construyen el mismo modelo sin doble redondeo posible
     *  (una división 1/(1+a*i) en f32 vs f64-then-round PODRIA diferir en 1 ulp). */
    void fillTestModel (NEURONiK::Common::SpectralModel& model)
    {
        for (int i = 0; i < 64; ++i)
        {
            const int k = i % 4;
            model.amplitudes[(size_t) i] = k == 0 ? 1.0f : k == 1 ? 0.5f : k == 2 ? 0.25f : 0.125f;
            model.frequencyOffsets[(size_t) i] = (float) i * 0.0078125f;   // 2^-7, exacto
        }
        model.isValid = true;
    }

    Runtime::Event noteOn (int note, float velocity)
    {
        Runtime::Event e;
        e.type = Runtime::EventType::NoteOn;
        e.channel = 1;
        e.note = note;
        e.value14 = 8192;
        e.value = velocity;
        e.sampleOffset = 0;
        return e;
    }

    /** Los 5 escenarios, duplicados 1:1 en Tests/neuronik_wasm_parity.mjs. */
    std::vector<Scenario> buildScenarios()
    {
        std::vector<Scenario> s;

        // A — Neuronik por defecto, una nota, 32 bloques (~0.085 s): camino
        //     caliente del motor (osciladores, resonador, envolvente).
        {
            Scenario a;
            a.name = "A_neuronik_default";
            a.engineType = 0;
            a.params = GlobalParams {};
            a.events.push_back ({ 0, noteOn (60, 100.0f / 127.0f) });
            a.blocks = 32;
            s.push_back (a);
        }

        // B — Neurotik por defecto, una nota, 32 bloques: el segundo motor
        //     (ruta NeurotikVoice) también cruza la frontera.
        {
            Scenario b;
            b.name = "B_neurotik_default";
            b.engineType = 1;
            b.params = GlobalParams {};
            b.events.push_back ({ 0, noteOn (48, 1.0f) });
            b.blocks = 32;
            s.push_back (b);
        }

        // C — efectos activos + pánico: chorus/reverb mezclados, master bajo,
        //     nota, allNotesOff en el bloque 16 y drenaje del release.
        {
            Scenario c;
            c.name = "C_fx_panico";
            c.engineType = 0;
            c.params = GlobalParams {};
            c.params.masterLevel   = 0.5f;
            c.params.chorusRate    = 0.8f;
            c.params.chorusMix     = 0.35f;
            c.params.reverbSize    = 0.7f;
            c.params.reverbDamping = 0.6f;
            c.params.reverbMix     = 0.25f;
            c.events.push_back ({ 0, noteOn (64, 100.0f / 127.0f) });
            c.blocks = 48;
            c.panicAtBlock = 16;
            s.push_back (c);
        }

        // D — matriz de modulación activa: LFO -> morphX (dest 4), la ruta
        //     de modMatrix que consume el motor cada bloque.
        {
            Scenario d;
            d.name = "D_modmatrix";
            d.engineType = 0;
            d.params = GlobalParams {};
            d.params.modMatrix[0] = { 1, 4, 0.5f };
            d.events.push_back ({ 0, noteOn (60, 100.0f / 127.0f) });
            d.blocks = 24;
            s.push_back (d);
        }

        // E — modelo espectral cargado en el slot 0 (el timbre que los presets
        //     publican por el puente, camino neuronikLoadModel): el Resonator
        //     debe morphing entre los MISMOOS parciales en WASM y nativo.
        {
            Scenario e;
            e.name = "E_modelo_espectral";
            e.engineType = 0;
            e.params = GlobalParams {};
            e.loadModel = true;
            e.events.push_back ({ 0, noteOn (69, 1.0f) });
            e.blocks = 24;
            s.push_back (e);
        }

        return s;
    }

    /** Distancia en ulps entre dos float32 (bitwise). Mismo mapa monótono que
     *  el test JS: para negativos se invierte el orden de los enteros. */
    int64_t ulpDistance (float a, float b)
    {
        int32_t ia = 0, ib = 0;
        std::memcpy (&ia, &a, sizeof (ia));
        std::memcpy (&ib, &b, sizeof (ib));

        const int64_t ta = ia < 0 ? 0x80000000LL - (int64_t) ia : (int64_t) ia;
        const int64_t tb = ib < 0 ? 0x80000000LL - (int64_t) ib : (int64_t) ib;

        return ta > tb ? ta - tb : tb - ta;
    }
}

int main (int argc, char** argv)
{
    const char* outPath = argc > 1 ? argv[1] : "build-wasm/parity-native.json";

    FILE* out = fopen (outPath, "wb");
    if (out == nullptr)
    {
        std::printf ("[parity-native] ERROR: no se pudo abrir '%s' para escritura\n", outPath);
        return 1;
    }

    const auto scenarios = buildScenarios();
    const auto& cases = parityCases();

    // Muestras de cada caso, guardadas para poder comprobar después que la
    // salida NO depende del tamaño de bloque (comparando los casos del mismo
    // sample rate entre sí). 9 casos x 5 escenarios x 6144 muestras = 1.1 MB.
    std::vector<std::vector<std::vector<float>>> samplesByCase (cases.size());

    std::fprintf (out, "{\n  \"referenceBlockSize\": %d,\n  \"cases\": [\n", kReferenceBlockSize);

    bool firstCase = true;

    for (size_t c = 0; c < cases.size(); ++c)
    {
        const Case& tc = cases[c];

        auto& caseSamples = samplesByCase[c];
        caseSamples.resize (scenarios.size());

        std::fprintf (out,
                      "%s    {\n      \"sampleRate\": %d,\n      \"blockSize\": %d,\n      \"scenarios\": [\n",
                      firstCase ? "" : ",\n", (int) tc.sampleRate, tc.blockSize);
        firstCase = false;

        bool firstScenario = true;

        for (size_t si = 0; si < scenarios.size(); ++si)
        {
            const auto& sc = scenarios[si];

            // Misma duración que la referencia, granularidad distinta. Se multiplica
            // ANTES de dividir: con bloque 512 el factor es < 1, así que la división
            // entera directa daría 0 bloques (y el caso saldría vacío).
            static_assert ((kReferenceBlockSize == 128) || (kReferenceBlockSize == 256),
                           "reescala la matriz de bloques si cambias la referencia");

            const int scaledBlocks = sc.blocks * kReferenceBlockSize;
            const int blocks       = scaledBlocks / tc.blockSize;
            const int panicAtBlock = sc.panicAtBlock < 0
                                   ? -1
                                   : sc.panicAtBlock * kReferenceBlockSize / tc.blockSize;

            if (blocks * tc.blockSize != scaledBlocks)
            {
                std::printf ("[parity-native] ERROR: el bloque %d no escala exacto desde %d (%d muestras)\n",
                             tc.blockSize, kReferenceBlockSize, scaledBlocks);
                fclose (out);
                return 1;
            }

            auto engine = sc.engineType == 1
                ? std::unique_ptr<ISynthesisEngine> (std::make_unique<NeurotikEngine>())
                : std::unique_ptr<ISynthesisEngine> (std::make_unique<NeuronikEngine>());

            Runtime::DspEngineFacade facade (*engine);
            facade.prepare (tc.sampleRate, tc.blockSize);
            facade.setGlobalParams (sc.params);

            if (sc.loadModel)
            {
                NEURONiK::Common::SpectralModel model;
                fillTestModel (model);
                engine->loadModel (model, 0);
            }

            std::vector<float>& samples = caseSamples[si];
            samples.reserve ((size_t) blocks * (size_t) tc.blockSize);
            float peak = 0.0f;

            std::vector<float> left ((size_t) tc.blockSize), right ((size_t) tc.blockSize);

            for (int b = 0; b < blocks; ++b)
            {
                if (b == panicAtBlock)
                    facade.allNotesOff();

                std::vector<Runtime::Event> blockEvents;
                for (const auto& be : sc.events)
                    if (be.block == b)
                        blockEvents.push_back (be.event);

                facade.process (left.data(), right.data(), tc.blockSize,
                                blockEvents.empty() ? nullptr : blockEvents.data(),
                                (int) blockEvents.size());

                for (int i = 0; i < tc.blockSize; ++i)
                {
                    const float v = left[(size_t) i];
                    if (! std::isfinite (v))
                    {
                        std::printf ("[parity-native] ERROR: muestra no finita en '%s' (%d Hz, bloque %d) bloque %d\n",
                                     sc.name, (int) tc.sampleRate, tc.blockSize, b);
                        fclose (out);
                        return 1;
                    }
                    samples.push_back (v);
                    peak = std::max (peak, std::abs (v));
                }
            }

            std::fprintf (out,
                          "%s        {\"name\": \"%s\", \"blocks\": %d, \"panicAtBlock\": %d, "
                          "\"peak\": %.9g, \"samples\": [",
                          firstScenario ? "" : ",\n", sc.name, blocks, panicAtBlock, (double) peak);
            firstScenario = false;

            for (size_t i = 0; i < samples.size(); ++i)
                std::fprintf (out, "%s%.9g", i == 0 ? "" : ",", (double) samples[i]);

            std::fprintf (out, "]}");

            std::printf ("[parity-native] %6d Hz / bloque %3d  %-20s bloques=%3d  peak=%.6f  muestras=%zu\n",
                         (int) tc.sampleRate, tc.blockSize, sc.name, blocks, (double) peak, samples.size());
        }

        std::fprintf (out, "\n      ]\n    }");
    }

    // ------------------------------------------------------------------------
    // Independencia del tamaño de bloque (solo nativo, sin WASM de por medio).
    //
    // La duración es idéntica en los tres tamaños de bloque, así que un DSP
    // puramente por muestra tiene que dar la MISMA señal en los tres (0 ulps).
    // Estado 2026-09-19 (CERRADO): los CINCO escenarios son bit-exactos en las nueve
    // celdas de la matriz, o sea que la salida del motor ya no depende del buffer del
    // host. Las dos rutas que sí dependían, y cómo quedaron:
    //
    //   - Modulación (ARREGLADA 2026-09-19) — `BaseEngine::applyGlobalFX` llamaba a
    //     `lfo.processBlock (numSamples)` una vez por bloque y aplicaba la matriz con
    //     ese único valor: la modulación era una escalera de paso = blockSize (128 en
    //     la web, 512 en un host de 512). Ahora `renderVoicesWithControlRate()` avanza
    //     los LFOs, aplica la matriz y renderiza las voces en tramos de
    //     `BaseEngine::kControlBlockSize` (64) muestras, con el resto encadenado entre
    //     bloques del host. Escenario D: la primera diferencia con el volcado anterior
    //     cae en la muestra 64, que es exactamente la primera frontera de la rejilla.
    //
    //   - Reverb (ARREGLADA 2026-09-19) — `Effects/Reverb.h::processBlock`
    //     consumía UN paso de sus smoothers de 20 ms por bloque (un
    //     `getNextValue()` fuera del bucle de muestras), así que la rampa duraba
    //     882 BLOQUES: ~2,5 s con bloque 128 y ~10 s con 512 a 44,1 kHz, o sea
    //     segundos de subida y dependientes del host. Ahora la consume y la
    //     aplica por MUESTRA (una llamada de una muestra a dsp::Reverb, que solo
    //     expone API por bloque), y el escenario C da 0/12288 diferencias en las
    //     nueve celdas. Efecto audible del arreglo: el wet de la reverb llega a
    //     su valor en 20 ms en vez de arrastrarse (al acabar el render anterior su
    //     wet seguia al ~5% del objetivo: 48 de los 960 pasos de la rampa a 48 kHz).
    //
    // El núcleo (osciladores, resonador, envolvente, filtros, voz aditiva y
    // voz Neurotik) es bit-exacto en los tres tamaños: A, B y E dan 0/4096
    // diferencias a los tres sample rates.
    // ------------------------------------------------------------------------
    std::vector<std::string> dependentScenarios;

    std::fprintf (out, "\n  ],\n  \"blockSizeInvariance\": [\n");

    bool firstRate = true;

    for (size_t base = 0; base < cases.size(); ++base)
    {
        if (cases[base].blockSize != kReferenceBlockSize)
            continue;                                   // se compara contra la referencia

        std::fprintf (out, "%s    {\"sampleRate\": %d, \"scenarios\": [\n",
                      firstRate ? "" : ",\n", (int) cases[base].sampleRate);
        firstRate = false;

        bool firstScenario = true;

        for (size_t si = 0; si < scenarios.size(); ++si)
        {
            size_t   compared  = 0;
            size_t   differing = 0;
            int64_t  maxUlp    = 0;
            double   maxAbs    = 0.0;

            for (size_t other = 0; other < cases.size(); ++other)
            {
                if (other == base || cases[other].sampleRate != cases[base].sampleRate)
                    continue;

                const auto& a = samplesByCase[base][si];
                const auto& b = samplesByCase[other][si];

                if (a.size() != b.size())
                {
                    std::printf ("[parity-native] ERROR: %s tiene %zu muestras a %d Hz y %zu con bloque %d\n",
                                 scenarios[si].name, a.size(), (int) cases[base].sampleRate,
                                 b.size(), cases[other].blockSize);
                    fclose (out);
                    return 1;
                }

                compared += a.size();

                for (size_t i = 0; i < a.size(); ++i)
                {
                    const double d = std::abs ((double) a[i] - (double) b[i]);
                    if (d != 0.0)
                        ++differing;
                    if (d > maxAbs)
                        maxAbs = d;

                    const int64_t u = ulpDistance (a[i], b[i]);
                    if (u > maxUlp)
                        maxUlp = u;
                }
            }

            if (differing > 0
                && std::find (dependentScenarios.begin(), dependentScenarios.end(),
                              std::string (scenarios[si].name)) == dependentScenarios.end())
                dependentScenarios.push_back (scenarios[si].name);

            std::fprintf (out,
                          "%s        {\"name\": \"%s\", \"comparedSamples\": %zu, "
                          "\"differingSamples\": %zu, \"maxUlp\": %lld, \"maxAbsDiff\": %.9g}",
                          firstScenario ? "" : ",\n", scenarios[si].name,
                          compared, differing, (long long) maxUlp, maxAbs);
            firstScenario = false;

            std::printf ("[parity-native] %6d Hz  %-20s bloque %d vs 64/512: %s (%zu/%zu muestras, maxAbs=%.3e)\n",
                         (int) cases[base].sampleRate, scenarios[si].name, kReferenceBlockSize,
                         differing == 0 ? "bit-exacto   " : "DEPENDE      ",
                         differing, compared, maxAbs);
        }

        std::fprintf (out, "\n      ]}");
    }

    std::fprintf (out, "\n  ],\n  \"blockSizeDependentScenarios\": [");
    for (size_t i = 0; i < dependentScenarios.size(); ++i)
        std::fprintf (out, "%s\"%s\"", i == 0 ? "" : ", ", dependentScenarios[i].c_str());
    std::fprintf (out, "]\n}\n");

    fclose (out);

    std::printf ("[parity-native] OK: referencia escrita en %s\n", outPath);

    if (! dependentScenarios.empty())
        std::printf ("[parity-native] AVISO: %zu escenario(s) dependen del tamaño de bloque. "
                     "No es un fallo de este test, pero SI una regresion: el motor tenia dos "
                     "rutas por bloque (modulacion y rampa de la reverb) y las dos se "
                     "arreglaron el 2026-09-19 (ver el comentario de arriba), asi que esto "
                     "significa que algo ha vuelto a introducir una. El render web (cuanto "
                     "fijo de 128) no coincidiria con el nativo en un host de 512.\n",
                     dependentScenarios.size());

    return 0;
}
