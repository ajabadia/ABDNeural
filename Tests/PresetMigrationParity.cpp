/*
  ==============================================================================

    PresetMigrationParity.cpp
    Created: 28 Sep 2026
    Description: MIGRAR UN PRESET NO PUEDE CAMBIAR NI UNA MUESTRA.

                 insertEnvModRoutes mete en un preset anterior a las fuentes ENV
                 dos rutas —ENV 1 -> Osc Level y ENV 2 -> Filter Cutoff, las dos
                 con amount 1.0— y su comentario promete que el preset "suena
                 igual". Es una promesa, y aqui las promesas no se comprueban:
                 PresetRoundTripTest mira el ARBOL (que tras la migracion pone
                 6, 1, 1.0 en la ranura 1), no el SONIDO. Un arbol correcto con
                 un factor mal cableado pasa ese test y cambia el preset de
                 cada usuario que lo abra.

                 El motivo por el que la promesa es cierta: los dos destinos
                 preguntan por la fuente y, cuando es su envolvente, ASIGNAN el
                 amount al factor de routing (envAssign en la tabla de
                 destinos). El factor de una voz nace en 1.0 y lo que hace la
                 migracion es escribir 1.0 explicitamente: el numero que ya
                 estaba ahi. Por eso el resultado tiene que ser identico
                 BIT A BIT, y no "parecido": no hay interpolacion, no hay
                 redondeo, hay el mismo 1.0 en el mismo sitio.

                 ASI QUE ESTE TEST RINDE LAS DOS VERSIONES y compara las
                 muestras una a una. Si algun dia el amount deja de ser 1.0, si
                 envAssign pasa a sumar, o si el reset de la voz deja de
                 sembrar 1.0, aqui se ve en la cuenta de muestras distintas, no
                 en un preset que suena raro en la cabeza de alguien.

  ==============================================================================
*/

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Serialization/PresetMigration.h"
#include "State/ParameterDefinitions.h"
#include "CoreModules/NeuronikEngine.h"
#include "Common/SpectralModel.h"

namespace {

using NEURONiK::Common::SpectralModel;
using namespace NEURONiK;

int passed = 0;
int failed = 0;

void check (bool condition, const char* name)
{
    if (condition) { ++passed; std::printf ("  [PASS] %s\n", name); }
    else           { ++failed; std::printf ("  [FAIL] %s\n", name); }
}

constexpr int kBlockSize = 64;
constexpr int kBlocks = 6;

/** Un PARAM del arbol de preset, como los que escribe PresetManager. */
juce::ValueTree param (const char* id, double value)
{
    juce::ValueTree child ("PARAM");
    child.setProperty ("id", juce::String (id), nullptr);
    child.setProperty ("value", value, nullptr);
    return child;
}

/**
 * Un preset ANTERIOR a las fuentes ENV: la ranura 1 ya la ocupa el usuario
 * (LFO 1 -> Cutoff) y las ranuras 2 y 3 estan libres. La 4 se usa para que la
 * migracion tenga que SKIPEARLA: si se colara en la 4 seria una ruta que el
 * usuario no pidio, y el arbol lo delataria pero el sonido no.
 */
juce::ValueTree legacyPreset()
{
    juce::ValueTree state ("STATE");
    state.appendChild (param (State::IDs::mod1Source,      1.0), nullptr); // LFO 1
    state.appendChild (param (State::IDs::mod1Destination, 10.0), nullptr); // Filter Cutoff
    state.appendChild (param (State::IDs::mod1Amount,      0.4), nullptr);
    state.appendChild (param (State::IDs::mod2Source,      0.0), nullptr); // Off
    state.appendChild (param (State::IDs::mod2Destination,  0.0), nullptr); // Off
    state.appendChild (param (State::IDs::mod2Amount,      0.0), nullptr);
    state.appendChild (param (State::IDs::mod3Source,      0.0), nullptr);
    state.appendChild (param (State::IDs::mod3Destination,  0.0), nullptr);
    state.appendChild (param (State::IDs::mod3Amount,      0.0), nullptr);
    state.appendChild (param (State::IDs::mod4Source,      2.0), nullptr); // LFO 2, ocupada
    state.appendChild (param (State::IDs::mod4Destination,  3.0), nullptr); // Roughness
    state.appendChild (param (State::IDs::mod4Amount,      0.3), nullptr);
    return state;
}

/** El arbol de preset traducido a lo que el motor recibe de verdad. Es la
    unica traduccion que hace NEURONiKProcessor, aqui escrita a mano para no
    arrastrar un AudioProcessor entero a un test de audio. */
DSP::GlobalParams toGlobalParams (const juce::ValueTree& state)
{
    DSP::GlobalParams params;

    const auto read = [&state] (const char* id) -> float
    {
        return (float) (double) state.getChildWithProperty ("id", juce::String (id))
                            .getProperty ("value");
    };

    for (int slot = 1; slot <= 4; ++slot)
    {
        // El nombre del id se guarda en un String CON NOMBRE: el puntero de
        // toRawUTF() caduca en cuanto muere el temporal que lo creo, y un id
        // leido de un puntero colgando compara cualquier cosa.
        const juce::String source      = juce::String ("mod") + juce::String (slot) + "Source";
        const juce::String destination = juce::String ("mod") + juce::String (slot) + "Destination";
        const juce::String amount      = juce::String ("mod") + juce::String (slot) + "Amount";

        params.modMatrix[slot - 1].source      = (int) read (source.toRawUTF8());
        params.modMatrix[slot - 1].destination = (int) read (destination.toRawUTF8());
        params.modMatrix[slot - 1].amount      = read (amount.toRawUTF8());
    }

    return params;
}

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

/** Todas las muestras de los dos canales, en orden. */
std::vector<float> render (const DSP::GlobalParams& params)
{
    DSP::NeuronikEngine engine;
    engine.setPolyphony (2);
    engine.prepare (44100.0, kBlockSize);
    engine.loadModel (parityModel(), 0);

    DSP::Synthesis::AdditiveVoice::Params voice;
    voice.attack = 1.0f;
    voice.decay = 1000.0f;
    voice.sustain = 0.7f;
    voice.release = 10.0f;
    voice.morphX = 0.0f;
    voice.morphY = 0.0f;
    // El filtro, ABAJO del todo (300 Hz), por el mismo motivo que en
    // ModulationParityDump: con el cutoff por defecto el jlimit de AdditiveVoice
    // satura y la envolvente del filtro no mueve el corte, con lo que la mitad
    // de esta comprobacion seria ciega. Ademas esta ruta (LFO 1 -> Cutoff, la
    // que el usuario ya tenia) se suma al factor que migra ENV 2, asi que es
    // justo el caso donde se_notaria_ un factor mal puesto.
    voice.filterCutoff = 300.0f;
    voice.filterRes = 0.5f;
    voice.fAttack = 1.0f;
    voice.fDecay = 200.0f;
    voice.fSustain = 0.5f;
    voice.fRelease = 10.0f;

    engine.setVoiceParams (voice);
    engine.setGlobalParams (params);
    engine.updateParameters();

    std::vector<float> samples;
    samples.reserve ((std::size_t) kBlocks * kBlockSize * 2);

    dsp::AudioBuffer<float> buffer (2, kBlockSize);
    for (int block = 0; block < kBlocks; ++block)
    {
        buffer.clear();
        dsp::MidiBuffer midi;
        if (block == 0)
            midi.addEvent (dsp::MidiMessage::noteOn (1, 60, 1.0f), 0);
        engine.renderNextBlock (buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                samples.push_back (buffer.getSample (channel, i));
    }

    return samples;
}

/** Comparacion BIT A BIT: el mismo patron de bits, no "muy parecido". */
int countDiffering (const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size())
        return -1;

    int different = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::memcmp (&a[i], &b[i], sizeof (float)) != 0)
            ++different;
    return different;
}

/** La primera muestra en la que se separan, para que el fallo diga DONDE. */
void reportFirstDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
    {
        if (std::memcmp (&a[i], &b[i], sizeof (float)) == 0)
            continue;

        std::printf ("         primera diferencia en la muestra %zu: %.9g vs %.9g\n",
                     i, (double) a[i], (double) b[i]);
        return;
    }
}

} // namespace

int main()
{
    std::printf ("=== Paridad de la migracion de presets: el audio no puede cambiar ===\n");

    auto before = legacyPreset();
    auto after = legacyPreset();

    const int inserted = Serialization::insertEnvModRoutes (after);
    check (inserted == 2, "la migracion inserta las dos rutas ENV");

    // Lo que el ARBOL dice, que es lo que ya cubria PresetRoundTripTest: la
    // ranura 1 la tiene el usuario, asi que las rutas nuevas tienen que caer
    // en la 2 y la 3, y la 4 sigue siendo la del usuario.
    const auto route = [&after] (int slot)
    {
        const juce::String prefix = juce::String ("mod") + juce::String (slot);
        const auto read = [&after, &prefix] (const char* suffix)
        {
            return (int) (double) after.getChildWithProperty ("id", prefix + suffix)
                                .getProperty ("value");
        };
        return std::string (prefix.toUTF8()) + " = "
             + std::to_string (read ("Source")) + " -> "
             + std::to_string (read ("Destination"))
             + " @ " + juce::String (read ("Amount")).toStdString();
    };

    // IDEMPOTENCIA: migrar un preset ya migrado no puede anadir nada. Sin esta
    // pregunta, la migracion metia una segunda ENV 1 -> Osc Level en cuanto
    // encontraba dos ranuras libres, y un preset recien creado trae ENV 1 y ENV 2
    // en la 1 y la 2 con la 3 y la 4 vacias: abrirlo y migrarlo duplicaba la
    // primera ruta sin tocar la original. El limite `slot <= 2` tapaba el caso
    // por accidente, no por diseno.
    auto twice = legacyPreset();
    check (Serialization::insertEnvModRoutes (twice) == 2,
           "la primera migracion inserta las dos rutas");
    check (Serialization::insertEnvModRoutes (twice) == 0,
           "migrar dos veces no duplica nada (la migracion es idempotente)");

    check (route (1) == "mod1 = 1 -> 10 @ 0", "la ranura 1 sigue siendo del usuario");
    check (route (4) == "mod4 = 2 -> 3 @ 0", "la ranura 4 no se toca");
    // Y las dos nuevas caen en la 2 y la 3. Este par es el que caza el defecto
    // que motivo: con la ranura 1 ocupada, un recorrido de solo las dos
    // primeras insertaba ENV 1 y se comia ENV 2 sin decir nada.
    check (route (2) == "mod2 = 6 -> 1 @ 1", "la ENV 1 -> Osc Level cae en la ranura 2");
    check (route (3) == "mod3 = 7 -> 10 @ 1", "la ENV 2 -> Filter Cutoff cae en la ranura 3");

    // Y AHORA LO QUE NO CUBRIA NINGUN TEST: el sonido.
    const auto samplesBefore = render (toGlobalParams (before));
    const auto samplesAfter  = render (toGlobalParams (after));

    const int different = countDiffering (samplesBefore, samplesAfter);
    if (different != 0)
        reportFirstDifference (samplesBefore, samplesAfter);

    char message[160];
    std::snprintf (message, sizeof (message),
                   "migrar un preset pre-ENV no cambia ni una muestra (%d de %zu)",
                   different < 0 ? -1 : different, samplesBefore.size());
    check (different == 0, message);

    // Una red que dice "todo igual" porque las dos versiones eran silencio no
    // vale para nada: sin audio no hay paridad que comprobar.
    float peak = 0.0f;
    for (float sample : samplesBefore)
        peak = std::max (peak, std::fabs (sample));
    check (peak > 0.01f, "el preset rendido tiene audio (la comparacion mira algo)");

    std::printf ("\n=== %d pasan, %d fallan ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
