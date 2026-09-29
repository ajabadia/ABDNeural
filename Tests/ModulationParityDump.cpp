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

                 ── POR QUE CADA ESCENARIO SE RINDE DOS VECES ─────────────────
                 Porque el motor puede no ser reproducible entre instancias. En
                 `ModulationDest17DriveTest.cpp` esta medido y documentado: 2 de
                 cada 10 ejecuciones divergen desde la muestra ~64, con una
                 diferencia maxima de 6.3e-2 sobre una senal de rms 5.7e-2. El
                 mismo binario, los mismos parametros, la misma llamada.

                 Con un solo render por escenario, uno de cada cinco entraria en
                 la comparacion con un hash que no lo identifica, y entonces
                 "ha cambiado" y "ha salido el valor raro" se verian IGUALES. Y
                 el golden, en vez de avisar, se ensuciaria: la primera vez que
                 saliera un valor raro marcaria la referencia como vieja, y a
                 partir de ahi cualquier cambio real pasaria por la misma
                 salida que un descuido. Un golden que se invalida solo acaba
                 Habituando a re-aceptarlo sin mirar, que es justo lo contrario
                 de lo que se le pidio.

                 Asi que cada escenario se rinde DOS VECES en el mismo proceso.
                 Los dos renders que coinciden son de fiar y se comparan contra
                 la referencia. Los que no coinciden se marcan NO REPRODUCIBLE,
                 se listan aparte y NO entran en la referencia: su hash no lo
                 identifica hoy y meterlo contaminaria el fichero para siempre.
                 El resumen dice cuantos de los 41 entraron y cuantos quedaron
                 fuera, en vez de imprimir un 41 redondo que esconde el
                 descarte.

                 OJO CON LO QUE SE MIDE AQUI Y LO QUE SE MIDIO ALLI. En 27
                 tiradas seguidas de esta herramienta en Release, el motor se ha
                 mostrado reproducible las 27: ni un escenario ha salido
                 distinto dos veces. O sea, que el filtro no se ha ejercitado
                 solo. Se ha ejercitado a proposito, perturbando un ULP del
                 hash de un escenario entre las dos pasadas, y el filtro ha
                 hecho lo que tiene que hacer. La razon de que en Release no
                 se vea y en el otro test si, no esta diagnosticada.

                 ── POR QUE NO ESTA EN CTEST ───────────────────────────────────
                 1. El hash es de bytes CRUDOS, y los bytes de un float no son
                    los mismos en Debug que en Release. Una referencia solo vale
                    para la configuracion con la que se capturo, y un test que
                    falla en la mitad de las configuraciones es peor que no
                    tener test. Por eso el fichero de referencia lleva el nombre
                    de la configuracion, y la herramienta avisa si se le pasa
                    una ruta que no lo lleva.
                 2. Cuesta alrededor de un minuto en Debug (41 motores, dos
                    renders cada uno). Meter eso en la suite levanta el coste de
                    cada build de un segundo a un minuto.
                 3. Y lo decisive: lo que este informe AVISA no puede ser un
                    fallo de la build. Si el motor cambia a proposito, la
                    referencia tiene que cambiar con el, y un `add_test` que
                    falla cada vez que la referencia envejece enseña a toda la
                    gente a regenerarla sin leer. Un aviso que se lee gana a un
                    rojo que nadie mira.

                 El guard que SI va en ctest es ModulationMatrixTest.cpp, que es
                 barato y no depende de la configuracion. Este fichero es la red
                 debajo: la que se tira cuando el guard no puede ver el problema
                 porque el problema es "suena un pelo distinto".

                 ── USO ────────────────────────────────────────────────────────
                   NEURONiK_ModulationParityDump [ficheroDeReferencia]

                 Sin argumento, la referencia por defecto es la de ESTA
                 configuracion (la inyecta CMake como NEURONiK_PARITY_CONFIG) y
                 se escribe si no existe todavia, avisando en claro de que se
                 acaba de crear.

                 Códigos de salida:
                   0  todo comparado; las diferencias salen como AVISO, sin romper
                   1  dos escenarios con el MISMO hash: la red es ciega

  ==============================================================================
*/

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "ParityHarness.h"

#ifndef NEURONiK_PARITY_CONFIG
// Solo si alguien compila el fichero a mano, fuera de CMake. Se avisa abajo.
#define NEURONiK_PARITY_CONFIG "Desconocida"
#endif

namespace {

namespace Parity = NEURONiK::Tests::Parity;

constexpr int kBlocks = 4;
constexpr int kNumDestinations = 31;

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
    Parity::prepare (engine, params);

    Hasher hasher;
    dsp::AudioBuffer<float> buffer (2, Parity::kBlockSize);

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

//==============================================================================
/** Un escenario, con las DOS tiradas que ha hecho falta para saber si su hash
    significa algo. */
struct Scenario
{
    std::string name;         // std::string, NO char[]: un char local guardado en
                              // un vector cuelga del mismo hueco de pila y miente
    std::uint64_t hash = 0;         // la primera tirada
    std::uint64_t hashRepetido = 0; // la segunda
    bool reproducible = false;      // los dos coinciden
};

/**
    Rinde el escenario dos veces y decide si su hash es de fiar.

    @see La cabecera del fichero, seccion "POR QUE CADA ESCENARIO SE RINDE DOS
          VECES", para el porque de la medida y para lo que se ha medido.
*/
Scenario renderTwice (const std::string& name, const NEURONiK::DSP::GlobalParams& params,
                      const dsp::MidiMessage& gesture)
{
    Scenario scenario;
    scenario.name         = name;
    scenario.hash         = renderHash (params, gesture);
    scenario.hashRepetido = renderHash (params, gesture);

    scenario.reproducible = scenario.hash == scenario.hashRepetido;

    return scenario;
}

//==============================================================================
/** La ruta sin separadores, en minusculas, para poder preguntar si menciona la
    configuracion. `Tests/MiRef-Release.txt` y `release` tienen que casar. */
std::string pathTokens (const std::string& text)
{
    std::string out;

    for (char c : text)
    {
        if (c == '/' || c == '\\' || c == ' ' || c == '\t' || c == '_' || c == '-')
            continue;

        out += static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    }

    return out;
}

bool pathMentionsConfig (const std::string& path)
{
    const std::string config = pathTokens (NEURONiK_PARITY_CONFIG);

    if (config.empty())
        return false;

    return pathTokens (path).find (config) != std::string::npos;
}

/** Quita los espacios de los dos extremos. */
std::string trim (const std::string& text)
{
    std::size_t first = 0;
    while (first < text.size() && std::isspace (static_cast<unsigned char> (text[first])))
        ++first;

    std::size_t last = text.size();
    while (last > first && std::isspace (static_cast<unsigned char> (text[last - 1])))
        --last;

    return text.substr (first, last - first);
}

/** `0x` + 16 hex + dos espacios + nombre: lo que se escribe y lo que se relee,
    y legible con un `cat` sin la herramienta.

    El nombre va SIN sangria a proposito, y esa es una distincion que no es
    estetica. La impresion por pantalla mete los escenarios en una columna con
    `%-38s`, o sea con el nombre precedido de espacios; si se copiase de ahi, el
    nombre guardado en la referencia llevaria la sangria dentro y al releerlo
    casaria con `"  destino 17..."` mientras que el de esta ejecucion seria
    `"destino 17..."`. Los 41 escenarios salen entonces como NUEVOS y como
    DESAPARECIDOS a la vez, que es lo que paso la primera vez que se ejecuto
    esto de verdad. Un nombre con espacios a la izquierda no se ve comparandose
    a si mismo en un fichero, y por eso el fallo no se ve leyendo el codigo. */
std::string lineAsWritten (const Scenario& scenario)
{
    char buffer[32];
    std::snprintf (buffer, sizeof (buffer), "0x%016llx  ",
                   (unsigned long long) scenario.hash);

    return std::string (buffer) + trim (scenario.name);
}

std::string hashAsText (std::uint64_t hash)
{
    char buffer[32];
    std::snprintf (buffer, sizeof (buffer), "0x%016llx", (unsigned long long) hash);

    return buffer;
}

/** Lee la referencia. Devuelve false si el fichero no se pudo abrir, que es lo
    que distingue "no hay todavia" de "esta ahi y se ha leido". */
bool readReference (const std::string& path,
                    std::vector<std::pair<std::string, std::uint64_t>>& out)
{
    std::ifstream in (path, std::ios::binary);

    if (! in)
        return false;

    out.clear();

    std::string line;
    while (std::getline (in, line))
    {
        // El fichero lleva su propia cabecera en comentarios, para que un `cat`
        // a pelo enseñe que es y de cuando es. Los `#` no son datos.
        if (line.empty() || line[0] == '#')
            continue;

        if (line.compare (0, 2, "0x") != 0)
            continue;

        if (line.size() < 18)
            continue;

        // El nombre se recorta por los dos lados. Sin esto, los dos espacios
        // separadores del formato se colarian en el nombre y la comparacion
        // por nombre no casaria ni consigo misma.
        const std::string name = trim (line.substr (18));

        out.emplace_back (name, std::stoull (line.substr (2, 16), nullptr, 16));
    }

    return true;
}

} // namespace

int main (int argc, char* argv[])
{
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
        scenarios.push_back (renderTwice (name, params, dsp::MidiMessage::pitchWheel (1, 16383)));
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
            scenarios.push_back (renderTwice (name, params, dsp::MidiMessage::pitchWheel (1, 8192)));
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
        scenarios.push_back (renderTwice ("cuatro rutas simultaneas",
                                          params, dsp::MidiMessage::pitchWheel (1, 16383)));
    }
    {
        NEURONiK::DSP::GlobalParams params;
        params.modMatrix[0] = { 6, 1,  0.4f };   // ENV 1 -> Osc Level     (reemplaza)
        params.modMatrix[1] = { 7, 10, 0.8f };   // ENV 2 -> Filter Cutoff (reemplaza)
        params.modMatrix[2] = { 7, 13, 0.3f };   // ENV 2 -> Flt Attack    (reemplaza)
        params.modMatrix[3] = { 3, 20, 0.5f };   // Pitch Bend -> Odd/Even Bal
        scenarios.push_back (renderTwice ("ENV 1 y ENV 2 mas Pitch Bend",
                                          params, dsp::MidiMessage::pitchWheel (1, 1)));
    }
    {
        NEURONiK::DSP::GlobalParams params;
        params.modMatrix[0] = { 3, 17, 0.5f };   // Pitch Bend -> Saturation (global)
        params.modMatrix[1] = { 3, 18, 0.5f };   // Pitch Bend -> Delay Time   (global)
        params.modMatrix[2] = { 3, 19, 0.5f };   // Pitch Bend -> Delay FB     (global)
        params.modMatrix[3] = { 3, 11, 0.5f };   // Pitch Bend -> Filter Res
        scenarios.push_back (renderTwice ("los tres globales de FX",
                                          params, dsp::MidiMessage::pitchWheel (1, 16383)));
    }

    // ── QUIEN ES REPRODUCIBLE Y QUIEN NO ────────────────────────────────────
    std::vector<const Scenario*> reproducibles;
    std::vector<const Scenario*> filtrados;

    for (const auto& scenario : scenarios)
    {
        if (scenario.reproducible)
            reproducibles.push_back (&scenario);
        else
            filtrados.push_back (&scenario);
    }

    std::printf ("=== Paridad de la matriz de modulacion: %zu escenarios ===\n", scenarios.size());

    for (const auto& scenario : scenarios)
    {
        if (scenario.reproducible)
        {
            std::printf ("  %-38s %s\n", scenario.name.c_str(),
                         hashAsText (scenario.hash).c_str());
        }
        else
        {
            // Las dos tiradas del lado, para que se vea que NO es que el
            // escenario sea raro: es que el motor no ha dado lo mismo dos veces.
            std::printf ("  %-38s NO REPRODUCIBLE  %s / %s\n", scenario.name.c_str(),
                         hashAsText (scenario.hash).c_str(),
                         hashAsText (scenario.hashRepetido).c_str());
        }
    }

    if (! filtrados.empty())
    {
        std::printf ("\n  %zu escenario(s) NO reproducibles, y por tanto NO comparados:\n",
                     filtrados.size());

        for (const auto* scenario : filtrados)
            std::printf ("    - %s\n", scenario->name.c_str());

        std::printf ("  Sus dos renders dentro del MISMO proceso no han coincidido. El hash de\n"
                     "  uno no lo identifica, asi que no entra ni en la comparacion ni en la\n"
                     "  referencia: meterlo contaminaria el fichero. No es un fallo de estos\n"
                     "  escenarios, y por eso NO se cuentan como desaparecidos de la\n"
                     "  referencia mas abajo: siguen ahi, solo que hoy no son comparables.\n");
    }

    // ── EL UNICO FALLO DURO: LA RED ES CIEGA ────────────────────────────────
    //
    // Se comprueba SOLO sobre los reproducibles, y no por orden sino porque el
    // hash de uno no reproducible puede coincidir con el de otro por casualidad,
    // y eso no es ceguera de la red sino ruido del motor.
    std::vector<std::uint64_t> distinct;
    for (const auto* scenario : reproducibles)
        if (std::find (distinct.begin(), distinct.end(), scenario->hash) == distinct.end())
            distinct.push_back (scenario->hash);

    const std::size_t ciegos = reproducibles.size() - distinct.size();

    if (ciegos > 0)
    {
        std::printf ("\n  %zu de %zu escenarios COMPARADOS comparten hash con otro.\n"
                     "  La red es mas ciega de lo que parece: revisa si el\n"
                     "  escenario sigue moviendo el destino que dice mover.\n",
                     ciegos, reproducibles.size());
    }

    // ── LA REFERENCIA ───────────────────────────────────────────────────────
    //
    // Sin argumento se usa la de ESTA configuracion, y el nombre de la
    // configuracion lo mete CMake. La razon de que el hash sea de bytes crudos
    // es la de siempre: un Debug y un Release con el mismo codigo dan floats
    // distintos, y un solo fichero para los dos no compara nada, parece que
    // compara.
    const std::string rutaPorDefecto =
        std::string ("Tests/ModulationParity-") + NEURONiK_PARITY_CONFIG + ".txt";

    const std::string ruta = argc > 1 ? std::string (argv[1]) : rutaPorDefecto;

    std::printf ("\n  referencia        : %s\n", ruta.c_str());
    std::printf ("  configuracion     : %s\n", NEURONiK_PARITY_CONFIG);

    // El aviso va aqui y NO solo en la rama de "la referencia existe", porque
    // el caso peligroso es el contrario: crear un fichero de referencia nuevo
    // con un nombre que no dice de que configuracion es, y que otro dia lo
    // compare contra un Debug sin que ninguna de las dos tiradas haya dicho
    // nada. Comprobarlo solo al comparar es comprobarlo tarde.
    if (! pathMentionsConfig (ruta))
        std::printf ("  AVISO: el nombre de '%s' no menciona la configuracion (%s).\n"
                     "  El hash es de bytes crudos: si ese fichero lo comparte otra\n"
                     "  configuracion, la comparacion no significa nada. Si acabas de\n"
                     "  crearlo, renombralo a algo con '%s' dentro.\n",
                     ruta.c_str(), NEURONiK_PARITY_CONFIG, NEURONiK_PARITY_CONFIG);

    std::vector<std::pair<std::string, std::uint64_t>> referencia;
    const bool existe = readReference (ruta, referencia);

    int diferencias = 0;
    int nuevos = 0;
    int desaparecidos = 0;

    if (! existe)
    {
        // Se escribe con lo que hay, y se dice en claro que se acaba de crear.
        // Un fichero recien creado que no se anuncia se confunde con uno viejo,
        // que es justo la confusion que este informe viene a quitar.
        std::ofstream out (ruta, std::ios::binary | std::ios::trunc);

        if (! out)
        {
            std::printf ("  AVISO: no se pudo escribir la referencia en %s. Esta ejecucion\n"
                         "  no ha comparado con nada.\n", ruta.c_str());
        }
        else
        {
            out << "# Referencia de la paridad de la matriz de NEURONiK.\n"
                << "# Configuracion: " << NEURONiK_PARITY_CONFIG << "\n"
                << "#\n"
                << "# Solo escenarios REPRODUCIBLES. El motor puede no ser determinista entre\n"
                << "# instancias, y un hash que sale distinto dos veces no identifica nada:\n"
                << "# si entrara, marcaria la referencia como vieja y a partir de ahi todo\n"
                << "# cambio real pasaria por el mismo aviso que un descuido.\n"
                << "#\n"
                << "# Formato: 0x<16 hex>  <nombre del escenario>\n"
                << "#\n"
                << "# COMO SE USA. Antes de tocar el motor, se ejecuta y se guarda esta salida.\n"
                << "# Despues del cambio, se ejecuta otra vez y se lee el resumen. Si hay\n"
                << "# diferencias, la pregunta no es 'fallo?' sino 'este cambio es el que\n"
                << "# queria?': si lo era, este fichero se regenera; si no lo era, el cambio\n"
                << "# es un bug.\n\n";

            for (const auto* scenario : reproducibles)
                out << lineAsWritten (*scenario) << '\n';

            std::printf ("  AVISO: no habia referencia en %s; se acaba de ESCRIBIR con los %zu\n"
                         "  escenarios reproducibles de esta ejecucion. Esta tirada aun no\n"
                         "  compara con nada: la siguiente, si.\n",
                         ruta.c_str(), reproducibles.size());
        }
    }
    else
    {
        // ── COMPARAR POR NOMBRE, Y POR QUE NO POR POSICION ───────────────────
        // Los escenarios se filtran, y comparar el puesto 12 con el puesto 12
        // de un fichero donde el 7 se descarto compara dos escenarios
        // distintos. Ademas, un escenario puede salir y luego no, y por
        // puesto se compararia con el vecino.
        for (const auto* scenario : reproducibles)
        {
            const auto encontrado = std::find_if (
                referencia.begin(), referencia.end(),
                [scenario] (const std::pair<std::string, std::uint64_t>& fila)
                {
                    return fila.first == trim (scenario->name);
                });

            if (encontrado == referencia.end())
            {
                std::printf ("  AVISO: '%s' no estaba en la referencia (escenario nuevo).\n",
                             scenario->name.c_str());
                ++nuevos;
                continue;
            }

            if (encontrado->second != scenario->hash)
            {
                std::printf ("  AVISO: '%s' cambio: %s en la referencia, %s ahora.\n",
                             scenario->name.c_str(),
                             hashAsText (encontrado->second).c_str(),
                             hashAsText (scenario->hash).c_str());
                ++diferencias;
            }
        }

        // Y aqui lo IMPORTANTE, que es lo que la primera version hacia mal: un
        // escenario que hoy se ha DESCARTADO por no reproducible sigue estando
        // en la referencia, y no ha desaparecido. Contarlo como desaparecido
        // producia "AVISO: 'destino 17' estaba en la referencia y no ha salido
        // hoy", que es mentira: ha salido, pero su hash no es comparable, y esa
        // es una situacion distinta que merece su propia linea.
        for (const auto* scenario : filtrados)
        {
            const auto estaba = std::find_if (
                referencia.begin(), referencia.end(),
                [scenario] (const std::pair<std::string, std::uint64_t>& fila)
                {
                    return fila.first == trim (scenario->name);
                });

            if (estaba != referencia.end())
                std::printf ("  AVISO: '%s' esta en la referencia pero hoy NO se ha comparado:\n"
                             "  sus dos renders no han coincidido. No es que haya cambiado ni que\n"
                             "  falte; es que hoy su hash no es de fiar.\n", scenario->name.c_str());
        }

        for (const auto& fila : referencia)
        {
            const bool sigueVivo = std::find_if (
                reproducibles.begin(), reproducibles.end(),
                [&fila] (const Scenario* scenario) { return fila.first == trim (scenario->name); })
                != reproducibles.end();

            const bool estabaFiltrado = std::find_if (
                filtrados.begin(), filtrados.end(),
                [&fila] (const Scenario* scenario) { return fila.first == trim (scenario->name); })
                != filtrados.end();

            if (! sigueVivo && ! estabaFiltrado)
            {
                std::printf ("  AVISO: '%s' estaba en la referencia y no ha salido hoy.\n",
                             fila.first.c_str());
                ++desaparecidos;
            }
        }

        if (diferencias == 0 && nuevos == 0 && desaparecidos == 0)
            std::printf ("  sin cambios: los %zu escenarios comparados coinciden con la referencia.\n",
                         reproducibles.size());
    }

    // ── EL RESUMEN, CON LOS DESCARTES A LA VISTA ────────────────────────────
    //
    // El numero que se imprime primero es el de COMPARADOS, no el de
    // escenarios, y los dos van juntos. Un "41 de 41" cuando dos se han
    // descartado seria una mentira por redondeo, y es la clase de mentira que
    // hace que un informe deje de leerse.
    std::printf ("\n  RESUMEN: %zu escenarios, %zu comparados contra la referencia"
                 " (doble render coincidente), %zu filtrados por no reproducibles.\n",
                 scenarios.size(), reproducibles.size(), filtrados.size());

    if (filtrados.empty())
        std::printf ("  Esta tirada ha reproducido los %zu: no hay nada filtrado.\n", scenarios.size());

    if (ciegos == 0)
        std::printf ("  %zu hashes distintos: cada escenario comparado mira algo distinto.\n",
                     distinct.size());

    if (diferencias > 0 || nuevos > 0 || desaparecidos > 0)
        std::printf ("  AVISO: %d diferencias, %d escenarios nuevos, %d desaparecidos.\n"
                     "  Esto AVISA, no rompe: el motor cambio a proposito, o el cambio no es lo\n"
                     "  que se queria. Que la herramienta lo decida seria decidir por quien\n"
                     "  lee.\n", diferencias, nuevos, desaparecidos);

    if (std::string (NEURONiK_PARITY_CONFIG) == "Desconocida")
        std::printf ("  AVISO: compilado sin NEURONiK_PARITY_CONFIG (fuera de CMake), asi que la\n"
                     "  referencia por defecto no lleva el nombre de la configuracion. El hash\n"
                     "  es de bytes crudos: no lo compares con el de otro binario.\n");

    if (ciegos > 0)
        return 1;

    return 0;
}
