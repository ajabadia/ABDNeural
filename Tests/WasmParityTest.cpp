/*
  ==============================================================================

    WasmParityTest.cpp
    Description: Fase 5 (1/2) — referencia NATIVA para la paridad WASM<->nativo.

    Ejecuta 4 escenarios deterministas sobre la frontera de la Fase 1
    (DspEngineFacade + Runtime::Event + GlobalParams) — exactamente la misma
    ruta que consume el puente WASM — y vuelca las muestras del canal
    izquierdo a JSON. Tests/neuronik_wasm_parity.mjs instancia el módulo WASM
    real y compara muestra a muestra contra este volcado.

    El DSP es determinista (siembra fija de LFO/voces del 2026-09-17,
    entropyAmount=0), así que la misma secuencia produce la misma salida en
    cualquier pasada y máquina. Los escenarios están duplicados 1:1 en el
    test JS: si cambias uno aquí, cambia el otro.

    Uso: NEURONiK_WasmParityTest.exe [ruta/salida.json]

  ==============================================================================
*/

#include "../Source/DSP/Runtime/DspEngineFacade.h"
#include "../Source/DSP/DspTypes.h"
#include "../Source/DSP/CoreModules/NeuronikEngine.h"
#include "../Source/DSP/CoreModules/NeurotikEngine.h"

#include <cstdio>
#include <cmath>
#include <memory>
#include <vector>
#include <string>

using namespace NEURONiK::DSP;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;

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
    };

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

    /** Los 4 escenarios, duplicados 1:1 en Tests/neuronik_wasm_parity.mjs. */
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

        return s;
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

    std::fprintf (out, "{\n  \"sampleRate\": %d,\n  \"blockSize\": %d,\n", (int) kSampleRate, kBlockSize);
    std::fprintf (out, "  \"scenarios\": [\n");

    const auto scenarios = buildScenarios();
    bool firstScenario = true;

    for (const auto& sc : scenarios)
    {
        auto engine = sc.engineType == 1
            ? std::unique_ptr<ISynthesisEngine> (std::make_unique<NeurotikEngine>())
            : std::unique_ptr<ISynthesisEngine> (std::make_unique<NeuronikEngine>());

        Runtime::DspEngineFacade facade (*engine);
        facade.prepare (kSampleRate, kBlockSize);
        facade.setGlobalParams (sc.params);

        std::vector<float> samples;
        samples.reserve ((size_t) sc.blocks * kBlockSize);
        float peak = 0.0f;

        std::vector<float> left (kBlockSize), right (kBlockSize);

        for (int b = 0; b < sc.blocks; ++b)
        {
            if (b == sc.panicAtBlock)
                facade.allNotesOff();

            std::vector<Runtime::Event> blockEvents;
            for (const auto& be : sc.events)
                if (be.block == b)
                    blockEvents.push_back (be.event);

            facade.process (left.data(), right.data(), kBlockSize,
                            blockEvents.empty() ? nullptr : blockEvents.data(),
                            (int) blockEvents.size());

            for (int i = 0; i < kBlockSize; ++i)
            {
                const float v = left[(size_t) i];
                if (! std::isfinite (v))
                {
                    std::printf ("[parity-native] ERROR: muestra no finita en '%s' bloque %d\n", sc.name, b);
                    fclose (out);
                    return 1;
                }
                samples.push_back (v);
                peak = std::max (peak, std::abs (v));
            }
        }

        if (! firstScenario)
            std::fprintf (out, ",\n");
        firstScenario = false;

        std::fprintf (out, "    {\"name\": \"%s\", \"blocks\": %d, \"peak\": %.9g, \"samples\": [",
                      sc.name, sc.blocks, (double) peak);
        for (size_t i = 0; i < samples.size(); ++i)
            std::fprintf (out, "%s%.9g", i == 0 ? "" : ",", (double) samples[i]);
        std::fprintf (out, "]}");

        std::printf ("[parity-native] %-20s bloques=%3d  peak=%.6f  muestras=%zu\n",
                     sc.name, sc.blocks, (double) peak, samples.size());
    }

    std::fprintf (out, "\n  ]\n}\n");
    fclose (out);

    std::printf ("[parity-native] OK: referencia escrita en %s\n", outPath);
    return 0;
}
