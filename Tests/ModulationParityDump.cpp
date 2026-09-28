/*
  ==============================================================================

    ModulationParityDump.cpp
    Created: 28 Sep 2026
    Description: LA RED DE PARIDAD DE LA MATRIZ DE MODULACION, como
                 HERRAMIENTA y no como test.

                 Rinde 41 escenarios de modulacion con audio real (un modelo de
                 dos frames, nota sostenida, filtro audible) y saca un hash de
                 cada uno: el audio de los dos canales y el vector de
                 modulaciones que la pagina dibuja, byte a byte.

                 Para que sirve: cuando se toca el motor —la tabla de destinos,
                 las fuentes, las envolventes— se ejecuta ANTES del cambio, se
                 guarda la salida, y se ejecuta DESPUES. Si las dos salidas
                 coinciden, el cambio no ha movido ni un ULP. Ese es el unico
                 metodo que cubre lo que ninguna aserto cubre: que la ruta siga
                 moviendo EXACTAMENTE lo mismo.

                 ── POR QUE NO ESTA EN CTEST ───────────────────────────────────
                 1. El hash es de bytes CRUDOS, y los bytes de un float no son
                    los mismos en Debug que en Release. Un golden asi solo vale
                    para la configuracion con la que se capturo, y un test que
                    falla en la mitad de las configuraciones es peor que no
                    tener test.
                 2. Cuesta alrededor de un minuto en Debug (41 motores, uno por
                     escenario). Meter eso en la suite levanta el coste de cada
                     build de un segundo a un minuto.

                 El guard que SI va en ctest es ModulationMatrixTest.cpp, que es
                 barato y no depende de la configuracion. Este fichero es la red
                 debajo: la que se tira cuando el guard no puede ver el problema
                 porque el problema es "suena un pelo distinto".

                 Uso:  NEURONiK_ModulationParityDump  > antes.txt
                       <cambia el motor>
                       NEURONiK_ModulationParityDump  > despues.txt
                       diff antes.txt despues.txt

  ==============================================================================
*/

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "CoreModules/NeuronikEngine.h"
#include "Common/SpectralModel.h"

namespace {

using NEURONiK::Common::SpectralModel;

constexpr int kBlockSize = 64;
constexpr int kBlocks = 4;
constexpr int kNumDestinations = 31;

/** Dos frames: frame 0 seno puro, frame 1 con el segundo parcial. Con morphX y
    morphY a 0 el morfeo bilineal se queda en el slot A, asi que el audio es
    estable entre renders. */
SpectralModel parityModel()
{
    SpectralModel m;
    m.amplitudes.fill (0.0f);
    m.frequencyOffsets.fill (0.0f);
    m.amplitudes[0] = 1.0f;
    m.isValid = true;
    m.extraAmps[0].fill (0.0f);
    m.extraOffsets[0].fill (0.0f);
    m.extraAmps[0][1] = 0.5f;
    m.frameCount = 2;
    return m;
}

/** FNV-1a de 64 sobre los bytes CRUDOS: lo que se compara es el patron de bits
    de la muestra, no una aproximacion con tolerancia. */
struct Hasher
{
    std::uint64_t state = 1469598103934665603ULL;

    void feed (const void* data, std::size_t bytes)
    {
        const auto* p = static_cast<const unsigned char*> (data);
        for (std::size_t i = 0; i < bytes; ++i)
        {
            state ^= p[i];
            state *= 1099511628211ULL;
        }
    }
};

std::uint64_t renderHash (const NEURONiK::DSP::GlobalParams& params,
                          const dsp::MidiMessage& gesture)
{
    NEURONiK::DSP::NeuronikEngine engine;
    // DOS voces, no dieciseis. La reserva es perezosa y prepare() crea las que
    // pida activeVoiceLimit: con 41 escenarios por delante, pagar 16 voces para
    // tocar una nota multiplica el tiempo por ocho sin cambiar ni un bit del
    // audio (con una sola nota sonando las otras quince solo anadirian
    // silencio).
    engine.setPolyphony (2);
    engine.prepare (44100.0, kBlockSize);
    engine.loadModel (parityModel(), 0);

    NEURONiK::DSP::Synthesis::AdditiveVoice::Params voice;
    voice.attack = 1.0f;
    voice.decay = 1000.0f;
    voice.sustain = 0.7f;
    voice.release = 10.0f;
    voice.morphX = 0.0f;
    voice.morphY = 0.0f;
    // EL FILTRO, ABAJO DEL TODO. Con el cutoff por defecto (20000 Hz) el
    // `jlimit(20, 20000, ...)` de AdditiveVoice satura siempre y la envolvente
    // del filtro no mueve NADA: los seis destinos de ENV 2 rendian audio
    // identico y la red era ciega justo en la rama que mas importa, la de
    // reemplazo. Con el cutoff a 300 Hz la envolvente SI mueve el corte y cada
    // destino se distingue de los otros.
    voice.filterCutoff = 300.0f;
    voice.filterRes = 0.5f;
    voice.fAttack = 1.0f;
    voice.fDecay = 200.0f;
    voice.fSustain = 0.5f;
    voice.fRelease = 10.0f;

    engine.setVoiceParams (voice);
    engine.setGlobalParams (params);
    engine.updateParameters();

    Hasher hasher;
    dsp::AudioBuffer<float> buffer (2, kBlockSize);

    for (int block = 0; block < kBlocks; ++block)
    {
        buffer.clear();
        dsp::MidiBuffer midi;
        if (block == 0)
        {
            midi.addEvent (dsp::MidiMessage::noteOn (1, 60, 1.0f), 0);
            midi.addEvent (gesture, 0);
        }
        engine.renderNextBlock (buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            hasher.feed (buffer.getReadPointer (channel),
                         sizeof (float) * (std::size_t) buffer.getNumSamples());

        float values[64] = { 0.0f };
        engine.getModulationValues (values, 64);
        hasher.feed (values, sizeof (values));
    }

    return hasher.state;
}

} // namespace

int main()
{
    struct Scenario
    {
        std::string name;   // std::string, NO char[]: un char local guardado en
                            // un vector cuelga del mismo hueco de pila y miente
        std::uint64_t hash;
    };

    std::vector<Scenario> scenarios;

    // Los 31 destinos, uno por uno, modulados con Pitch Bend al maximo (que
    // tiene signo) y amount 0.7.
    for (int destination = 0; destination < kNumDestinations; ++destination)
    {
        NEURONiK::DSP::GlobalParams params;
        params.modMatrix[0].source = 3;
        params.modMatrix[0].destination = destination;
        params.modMatrix[0].amount = 0.7f;

        char name[64];
        std::snprintf (name, sizeof (name), "destino %02d con Pitch Bend", destination);
        scenarios.push_back ({ name, renderHash (params, dsp::MidiMessage::pitchWheel (1, 16383)) });
    }

    // Los siete destinos que PREGUNTAN por la fuente: con ENV 1 (destino 1) y
    // ENV 2 (10, 12-16) mandan el amount, no el valor de la fuente.
    {
        const int envDestinations[] = { 1, 10, 12, 13, 14, 15, 16 };
        for (auto destination : envDestinations)
        {
            NEURONiK::DSP::GlobalParams params;
            params.modMatrix[0].source = destination == 1 ? 6 : 7;
            params.modMatrix[0].destination = destination;
            params.modMatrix[0].amount = 0.7f;

            char name[64];
            std::snprintf (name, sizeof (name), "destino %02d con ENV (reemplaza)", destination);
            scenarios.push_back ({ name, renderHash (params, dsp::MidiMessage::pitchWheel (1, 8192)) });
        }
    }

    // Cuatro rutas a la vez, que es como se usa de verdad: el orden de aplicacion
    // importa cuando dos rutas caen en la misma voz.
    {
        NEURONiK::DSP::GlobalParams params;
        params.modMatrix[0] = { 3, 2,  0.5f };   // Pitch Bend -> Inharmonicity
        params.modMatrix[1] = { 4, 10, -0.3f };  // Mod Wheel   -> Filter Cutoff
        params.modMatrix[2] = { 1, 4,  0.25f };  // LFO 1       -> Morph X
        params.modMatrix[3] = { 5, 26, 0.6f };   // Aftertouch  -> Res Bank Res
        scenarios.push_back ({ "cuatro rutas simultaneas",
                               renderHash (params, dsp::MidiMessage::pitchWheel (1, 16383)) });
    }
    {
        NEURONiK::DSP::GlobalParams params;
        params.modMatrix[0] = { 6, 1,  0.4f };   // ENV 1 -> Osc Level     (reemplaza)
        params.modMatrix[1] = { 7, 10, 0.8f };   // ENV 2 -> Filter Cutoff (reemplaza)
        params.modMatrix[2] = { 7, 13, 0.3f };   // ENV 2 -> Flt Attack    (reemplaza)
        params.modMatrix[3] = { 3, 20, 0.5f };   // Pitch Bend -> Odd/Even Bal
        scenarios.push_back ({ "ENV 1 y ENV 2 mas Pitch Bend",
                               renderHash (params, dsp::MidiMessage::pitchWheel (1, 1)) });
    }
    {
        NEURONiK::DSP::GlobalParams params;
        params.modMatrix[0] = { 3, 17, 0.5f };   // Pitch Bend -> Saturation (global)
        params.modMatrix[1] = { 3, 18, 0.5f };   // Pitch Bend -> Delay Time   (global)
        params.modMatrix[2] = { 3, 19, 0.5f };   // Pitch Bend -> Delay FB     (global)
        params.modMatrix[3] = { 3, 11, 0.5f };   // Pitch Bend -> Filter Res
        scenarios.push_back ({ "los tres globales de FX",
                               renderHash (params, dsp::MidiMessage::pitchWheel (1, 16383)) });
    }

    std::printf ("=== Paridad de la matriz de modulacion: %zu escenarios ===\n", scenarios.size());
    for (const auto& scenario : scenarios)
        std::printf ("  %-38s 0x%016llx\n", scenario.name.c_str(),
                     (unsigned long long) scenario.hash);

    // Si dos escenarios dieran el mismo hash, la red no los distingue y es
    // menos red de la que parece. No es un fallo del motor: es el aviso de que
    // este escenario ya no mira donde mira el anterior.
    std::vector<std::uint64_t> distinct;
    for (const auto& scenario : scenarios)
        if (std::find (distinct.begin(), distinct.end(), scenario.hash) == distinct.end())
            distinct.push_back (scenario.hash);

    if (distinct.size() != scenarios.size())
    {
        std::printf ("\n  AVISO: %zu de %zu escenarios comparten hash con otro.\n"
                     "  La red es mas ciega de lo que parece: revisa si el\n"
                     "  escenario sigue moviendo el destino que dice mover.\n",
                     scenarios.size() - distinct.size(), scenarios.size());
        return 1;
    }

    std::printf ("\n  %zu hashes distintos: cada escenario mira algo distinto.\n", distinct.size());
    return 0;
}
