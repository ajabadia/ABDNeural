/*
  ==============================================================================

    ModulationMatrixTest.cpp
    Created: 28 Sep 2026
    Description: LA TABLA DE LA MATRIZ DE MODULACION de NEURONiK contra su
                 propio motor — y sobre todo contra sus PROPIAS promesses.

                 La tabla de fuentes (getModSources) ofrecia ocho: Off, LFO 1,
                 LFO 2, Pitch Bend, Mod Wheel, Aftertouch, ENV 1 y ENV 2. Tres
                 de ellas —Pitch Bend, Mod Wheel y Aftertouch— eran un
                 `0.0f, // TODO` en el array `sources` de
                 NeuronikEngine::applyModulation. O sea que se podian SELECCIONAR
                 en el desplegable, la pagina las ofrecia como routing valido, y
                 no modulaban NADA.

                 Es la clase de fallo mas incomoda que hay en un sintetizador,
                 porque nada se rompe: el preset carga, la ruta aparece, y lo
                 unico que falla es el sonido que nadie espera oyer. Un TODO en
                 un array de fuentes es una promesa que el motor no cumple.

                 ASI QUE ESTE TEST NO COMPRUEBA LA TABLA: COMPRUEBA EL SONIDO DE
                 LA RUTA. Conduce el motor de verdad (prepare + un bloque con un
                 gesto MIDI + renderNextBlock) y lee lo que la ruta entrega, que
                 es lo unico que el oido puede contradecir. Un aserto sobre el
                 tamano de una tabla pasaria igual con las tres fuentes muertas:
                 por eso la parte de abajo mueve el gesto MIDI y mira el
                 resultado, y un `0.0f` con un TODO no la puede pasar.

                 Fija ademas la POLITICA que el switch de destinos esconde: los
                 destinos que preguntan por la fuente y, cuando es una
                 envolvente, REEMPLAZAN el factor en vez de sumar encima
                 (sintesis de reemplazo). Esa politica esta en
                 getModDestinationTable (perNote/replaces) y se comprueba aqui
                 contra el switch, para que cuando el switch se sustituye por
                 la tabla no se pierda por el camino.

  ==============================================================================
*/

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "State/ParameterDefinitions.h"
#include "CoreModules/NeuronikEngine.h"

namespace {

int passed = 0;
int failed = 0;

void check (bool condition, const char* name)
{
    if (condition) { ++passed; std::printf ("  [PASS] %s\n", name); }
    else           { ++failed; std::printf ("  [FAIL] %s\n", name); }
}

/** Aplica `gesture` al motor con UNA ruta(source -> 2, amount 1) y devuelve lo
    que esa ruta entrega al destino 2. El destino 2 es modInharmonicity: una
    suma normal, sin la politica de reemplazo de las envolventes, asi que lo que
    sale es exactamente el valor de la fuente. */
float probeRoute (int sourceIndex, const dsp::MidiMessage& gesture)
{
    NEURONiK::DSP::NeuronikEngine engine;
    engine.prepare (48000.0, 64);

    NEURONiK::DSP::GlobalParams params;
    params.modMatrix[0].source = sourceIndex;
    params.modMatrix[0].destination = 2;
    params.modMatrix[0].amount = 1.0f;
    engine.setGlobalParams (params);
    engine.updateParameters();   // setGlobalParams deja la ruta en pending

    dsp::AudioBuffer<float> buffer (2, 64);
    dsp::MidiBuffer midi;
    midi.addEvent (gesture, 0);
    engine.renderNextBlock (buffer, midi);

    float values[64] = { 0.0f };
    engine.getModulationValues (values, 64);
    return values[2];
}

/** Lo mismo, pero con DOS gestos en dos bloques: sirve para comprobar que la
    fuente no se QUEDA ENCERRADA en el ultimo valor (una rueda que se va y
    vuelve a cero tiene que volver a modular cero). */
void probeRouteTwice (int sourceIndex,
                      const dsp::MidiMessage& first,
                      const dsp::MidiMessage& second,
                      float& outFirst, float& outSecond)
{
    NEURONiK::DSP::NeuronikEngine engine;
    engine.prepare (48000.0, 64);

    NEURONiK::DSP::GlobalParams params;
    params.modMatrix[0].source = sourceIndex;
    params.modMatrix[0].destination = 2;
    params.modMatrix[0].amount = 1.0f;
    engine.setGlobalParams (params);
    engine.updateParameters();

    dsp::AudioBuffer<float> buffer (2, 64);
    dsp::MidiBuffer midi;
    float values[64] = { 0.0f };

    midi.addEvent (first, 0);
    engine.renderNextBlock (buffer, midi);
    engine.getModulationValues (values, 64);
    outFirst = values[2];

    midi.clear();
    midi.addEvent (second, 0);
    engine.renderNextBlock (buffer, midi);
    engine.getModulationValues (values, 64);
    outSecond = values[2];
}

bool near (float a, float b, float tolerance = 1.0e-3f) { return std::fabs (a - b) <= tolerance; }

} // namespace

int main()
{
    std::printf ("=== NEURONiK modulation matrix contract ===\n");

    const auto& sources = NEURONiK::State::getModSources();
    const auto& destinations = NEURONiK::State::getModDestinationTable();

    // ── La tabla: lo que la pagina promete ────────────────────────────────
    check (sources.size() == 8, "la tabla ofrece 8 fuentes");

    if (sources.size() == 8)
    {
        check (sources[0] == "Off",            "indice 0 = Off");
        check (sources[1] == "LFO 1",          "indice 1 = LFO 1");
        check (sources[2] == "LFO 2",          "indice 2 = LFO 2");
        check (sources[3] == "Pitch Bend",     "indice 3 = Pitch Bend");
        check (sources[4] == "Mod Wheel",      "indice 4 = Mod Wheel");
        check (sources[5] == "Aftertouch",     "indice 5 = Aftertouch");
        check (sources[6] == "ENV 1",          "indice 6 = ENV 1");
        check (sources[7] == "ENV 2",          "indice 7 = ENV 2");
    }

    // ── Lo que el motor ENTREGA de cada fuente ────────────────────────────
    // Esto es lo que muerde. Con las tres fuentes en `0.0f` los tres bloques de
    // assertions siguientes no pueden pasar.
    check (near (probeRoute (3, dsp::MidiMessage::pitchWheel (1, 16383)), 1.0f),
           "Pitch Bend: rueda arriba entrega +1");
    check (near (probeRoute (3, dsp::MidiMessage::pitchWheel (1, 1)), -1.0f),
           "Pitch Bend: rueda abajo entrega -1 (tiene signo, no es un 0..1)");
    check (near (probeRoute (3, dsp::MidiMessage::pitchWheel (1, 8192)), 0.0f),
           "Pitch Bend: al centro entrega 0");

    check (near (probeRoute (4, dsp::MidiMessage::controllerEvent (1, 1, 127)), 1.0f),
           "Mod Wheel: CC 1 al tope entrega 1");
    check (near (probeRoute (4, dsp::MidiMessage::controllerEvent (1, 1, 0)), 0.0f),
           "Mod Wheel: CC 1 abajo entrega 0");

    check (near (probeRoute (5, dsp::MidiMessage::channelPressureChange (1, 127)), 1.0f),
           "Aftertouch: presion de canal al tope entrega 1");
    check (near (probeRoute (5, dsp::MidiMessage::channelPressureChange (1, 0)), 0.0f),
           "Aftertouch: sin presion entrega 0");

    // La rueda y la presion son CONTINUAS: vuelven a cero cuando vuelven a
    // cero. Un `modWheelSource_` que se guardara una vez y no se actualizara
    // pasaria el bloque de arriba y fallaria este.
    {
        float first = 0.0f, second = 0.0f;
        probeRouteTwice (4, dsp::MidiMessage::controllerEvent (1, 1, 127),
                              dsp::MidiMessage::controllerEvent (1, 1, 0),
                         first, second);
        check (near (first, 1.0f) && near (second, 0.0f),
               "Mod Wheel: soltar la rueda devuelve la ruta a cero (no se enclava)");
    }
    {
        float first = 0.0f, second = 0.0f;
        probeRouteTwice (3, dsp::MidiMessage::pitchWheel (1, 16383),
                              dsp::MidiMessage::pitchWheel (1, 8192),
                         first, second);
        check (near (first, 1.0f) && near (second, 0.0f),
               "Pitch Bend: soltar la rueda devuelve la ruta a cero");
    }

    // El indice 0 (Off) no es una fuente muerta: es la ausencia de fuente. Su
    // ruta tiene que dar cero CON gesto MIDI de por medio, porque cualquier
    // valor ahi seria una fuente mal rotulada.
    check (near (probeRoute (0, dsp::MidiMessage::pitchWheel (1, 16383)), 0.0f),
           "Off modula cero aunque muevas la rueda");

    // ── La tabla constexpr llego ZEROS a la ejecucion, o no llego ───────────
    // Los static_assert del motor comprueban la tabla en TIEMPO DE COMPILACION.
    // El fallo de MSVC que motivo todo esto es precisamente que el valor
    // correcto se ve al compilar y llega a cero en el binario, asi que un
    // aserto de compilacion no lo puede ver: hace falta leer la tabla cuando el
    // programa ya esta corriendo.
    //
    // Este bloque lee los 31 `label` y los 31 `parameterId` del constexpr tal
    // cual estan en memoria. Si MSVC vuelve a callarse, aqui hay un nullptr y
    // el test revienta con un mensaje que lo dice, en vez de dejar que el motor
    // se coma las rutas.
    {
        const auto& constexprTable = NEURONiK::State::kModDestinationTable;
        const auto count = NEURONiK::State::kModDestinationCount;

        std::size_t nullLabels = 0;
        std::size_t mismatchedWithVector = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            if (constexprTable[i].label == nullptr)
                ++nullLabels;
            else if (i < destinations.size()
                     && std::string (constexprTable[i].label) != destinations[i].label)
                ++mismatchedWithVector;
        }

        char message[160];
        std::snprintf (message, sizeof (message),
                       "la tabla constexpr llego entera a memoria (%zu etiquetas, %zu nulas)",
                       count, nullLabels);
        check (nullLabels == 0, message);

        std::snprintf (message, sizeof (message),
                       "la tabla constexpr y getModDestinationTable() dicen lo mismo (%zu discrepancias)",
                       mismatchedWithVector);
        check (mismatchedWithVector == 0, message);

        // Y que el vector no se haya quedado corto al construirse desde el
        // constexpr, que seria la otra forma de que las dos tablas se separen.
        check (destinations.size() == count,
               "getModDestinationTable() tiene una fila por cada entrada del constexpr");
    }

    // ── La politica de reemplazo, que el switch escondia ──────────────────
    // Estos son los destinos cuyo caso en NeuronikEngine::applyModulation
    // PREGUNTA por la fuente y, si es una envolvente, REEMPLAZA el factor en
    // vez de sumar. Son los que hacen que un preset nuevo suene (ENV 1 -> VCA,
    // ENV 2 -> cutoff) y los que se perderian si el switch se sustituyera por
    // la tabla sin llevar la politica con el.
    // Los que reemplazan se LEEN de la tabla, no de una lista escrita aqui. La
    // lista vivia en dos sitios —este array y el contrato JSON de ABDSharedAssets—
    // y nada obligaba a que los dos dijeran lo mismo: se podia anadir un
    // destino con regla de reemplazo, anadirlo al JSON, y este array se
    // quedaba intacto sin que nadie se enterara. Ahora hay un sitio, y
    // ModulationContract (Tests/ModulationContractTest.cpp) lo compara con el
    // JSON campo a campo.
    std::vector<int> replacing;
    std::vector<int> perNote;

    for (std::size_t i = 0; i < destinations.size(); ++i)
    {
        if (destinations[i].replaces) replacing.push_back ((int) i);
        if (destinations[i].perNote)   perNote.push_back ((int) i);
    }

    const std::vector<int> expectedReplacing { 1, 10, 12, 13, 14, 15, 16 };
    check (replacing == expectedReplacing,
           "los siete destinos que reemplazan son los que declara la tabla");
    // perNote y replaces van juntos en este contrato: un destino o pregunta por
    // la fuente de la voz o no pregunta. Si algun dia se separan, este aserto
    // avisa en vez de dejarlos separarse en silencio.
    check (perNote == expectedReplacing,
           "perNote y replaces coinciden: los siete van de la mano");

    // El destino 12 no conduce parametro pero si modula: su parametro se
    // retiro y la PROFUNDIDAD vive en la ruta. parameterId null NO significa
    // inactivo, y confundirlos haria que la UI lo ocultase.
    if (destinations.size() > 12)
    {
        check (destinations[12].parameterId == nullptr,
               "el destino 12 (Filter Env Amt) no conduce parametro");
        check (destinations[12].label != std::string(),
               "pero tiene etiqueta: no es el 'Off' inerte");
    }
    if (!destinations.empty())
        check (destinations[0].parameterId == nullptr,
               "el destino 0 es el 'Off' inerte");

    // Los destinos que SI conducen parametro deben declarar uno real.
    bool everyDrivenHasId = true;
    for (const auto& destination : destinations)
    {
        if (destination.parameterId != nullptr && destination.parameterId[0] == '\0')
            everyDrivenHasId = false;
    }
    check (everyDrivenHasId, "ningun destino declara un parameterId vacio");

    std::printf ("\n=== Results: %d passed, %d failed ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
