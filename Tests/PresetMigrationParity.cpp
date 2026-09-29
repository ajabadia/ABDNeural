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
#include "Serialization/PresetMigrationFx.h"
#include "State/ParameterDefinitions.h"
#include "State/ModMatrixFromState.h"
#include "ParityHarness.h"

namespace {

using namespace NEURONiK;
namespace Parity = NEURONiK::Tests::Parity;

int passed = 0;
int failed = 0;

void check (bool condition, const char* name)
{
    if (condition) { ++passed; std::printf ("  [PASS] %s\n", name); }
    else           { ++failed; std::printf ("  [FAIL] %s\n", name); }
}

constexpr int kBlocks = 6;

/** Un PARAM del arbol de preset, como los que escribe PresetManager. */
juce::ValueTree param (const char* id, double value)
{
    juce::ValueTree child ("PARAM");
    child.setProperty ("id", juce::String (id), nullptr);
    child.setProperty ("value", value, nullptr);
    return child;
}

// --- La MIGRACION DEL BUS, que necesita un processor con el layout real ----
// `migrateFlatFxToSlotBus` decide que filas aplicar mirando QUE IDS DECLARA EL
// PLUGIN (la puerta), asi que el test necesita un AudioProcessor de verdad con
// el layout de verdad. No se puede pasar un nullptr ni una lista inventada: con
// una lista inventada la puerta no significaria nada y el test estaria
// comprobando una migracion que el producto no ejecuta.
//
// El layout sale de `createParameterLayout()`, el MISMO que usa el plugin, asi
// que el test no puede quedarse viejo solo: si anaden `fx2Type` al layout, esta
// fila del coro se arma sola y el test lo ve.
class LayoutProbe : public juce::AudioProcessor
{
public:
    LayoutProbe()
        : juce::AudioProcessor (BusesProperties()
                                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                    .withInput ("Input", juce::AudioChannelSet::stereo(), true))
    {
        apvts_ = std::make_unique<juce::AudioProcessorValueTreeState> (
            *this, nullptr, juce::Identifier ("NEURONiK_MigrationFxProbe"),
            State::createParameterLayout());
    }

    // Los cuatro puros que JUCE deja al plugin. `getStateInformation` y
    // `setStateInformation` NO se escriben a mano: este probe existe solo para
    // que la migracion pueda preguntar al PROCESSOR por su layout real, y un
    // guardado aqui no solo no hace falta, sino que daria al test un segundo
    // sitio donde leer y ocultar una regresion.
    const juce::String getName() const override { return "NEURONiK MigrationFx Probe"; }
    double getTailLengthSeconds() const override { return 0.0; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // El resto de puros de JUCE, que no tienen nada que ver con la migracion y
    // se contestan con la respuesta honesta: este probe no habla MIDI, no trae
    // programas y no tiene editor. Poner aqui un "0" o un "1" de convenience
    // seria mentir sobre el probe para que compile.
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 0; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts_;
};

/** El id viejo de la saturacion, que ya no esta en el layout. Vive aqui y no
    en `State::IDs::` porque ALLI no puede estar: un id que el plugin ya no
    declara no pertenece a la lista de ids que declara. */
constexpr const char* kLegacySaturation = "fxSaturation";

bool near (double a, double b, double tolerance = 1e-6)
{
    return std::fabs (a - b) <= tolerance;
}

double readParam (const juce::ValueTree& state, const char* id, double fallback = 0.0)
{
    return (double) state.getChildWithProperty ("id", id).getProperty ("value", fallback);
}

bool hasParam (const juce::ValueTree& state, const char* id)
{
    return state.getChildWithProperty ("id", id).isValid();
}

void setParam (juce::ValueTree& state, const char* id, double value)
{
    state.getChildWithProperty ("id", id).setProperty ("value", value, nullptr);
}

/** La fila que el motor dara en el hueco 1, y de ella su PRIMER mando: el
    drive. Se pide al CATALOGO y no a la clase del efecto a proposito, por dos
    razones. La primera es que el test no debe saber en que header vive cada
    motor: si anaden un efecto nuevo, el include aqui se queda viejo y el
    test no compila, cuando lo que tiene que comprobar --que el drive migrado
    coincide con el drive de la fila-- no ha cambiado. La segunda es mas
    importante: el CATALOGO es lo unico que ve la migracion, asi que comparar
    contra el catalogo es comparar contra la misma fuente de verdad. Contra la
    clase, el test podria dar verde con una fila que el motor nunca usa. */
const abd::dsp::FxParamSpec& saturationDriveSpec() noexcept
{
    return NEURONiK::DSP::fxNeuronikEffectAt (NEURONiK::DSP::fxDefaultTypeForSlot (0))
                .effect->params[0];
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
/**
 * El preset de un arbol a los numeros que recibe el motor, con la MISMA
 * traduccion que usa el producto (State/ModMatrixFromState.h).
 *
 * Lo unico que cambia respecto al producto es de donde se lee: aqui el
 * ValueTree del preset guardado, en el procesador los atomicos vivos del APVTS.
 * Los nombres de los campos y el orden en que se leen son los mismos codigo,
 * y esa es la parte que no puede separarse sin que el test deje de comprobar
 * lo que dice comprobar. Antes esta funcion vivia aqui copiada: un rename de
 * `mod3Amount` rompia el producto en silencio y este test seguia en verde.
 */
DSP::GlobalParams toGlobalParams (const juce::ValueTree& state)
{
    const auto routes = State::readModMatrix (
        [&state] (int slot, State::ModField field) -> float
        {
            return (float) (double) state.getChildWithProperty (
                "id", State::modMatrixParameterId (slot, field)).getProperty ("value");
        });

    DSP::GlobalParams params;
    for (int slot = 0; slot < State::kModMatrixSlots; ++slot)
        params.modMatrix[slot] = routes[slot];

    return params;
}

/**
 * Todas las muestras de los dos canales, en orden. El rig sale de
 * ParityHarness.h, el MISMO que usa ModulationParityDump: esta comprobacion
 * depende de la ruta del usuario (LFO 1 -> Cutoff) sumando sobre el factor que
 * migra ENV 2, y eso solo se nota con el corte a media pista.
 */
std::vector<float> render (const DSP::GlobalParams& params)
{
    DSP::NeuronikEngine engine;
    Parity::prepare (engine, params);

    std::vector<float> samples;
    samples.reserve ((std::size_t) kBlocks * Parity::kBlockSize * 2);

    dsp::AudioBuffer<float> buffer (2, Parity::kBlockSize);
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

/** Comparacion BIT A BIT: el mismo patron de bits, no "muy parecido".

    Compara hasta donde llegan las dos. Que el TAMANO cuadre es un aserto aparte,
    del llamante: antes esta funcion devolvia -1 si no cuadraba y salia "-1 de
    768 muestras distintas", que parece un recuento y no lo es. */
std::size_t countDiffering (const std::vector<float>& a, const std::vector<float>& b)
{
    std::size_t different = 0;
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
        if (std::memcmp (&a[i], &b[i], sizeof (float)) != 0)
            ++different;
    return different;
}

/** La primera muestra en la que se separan, para que el fallo diga DONDE. */
void reportFirstDifference (const std::vector<float>& a, const std::vector<float>& b)
{
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
        if (std::memcmp (&a[i], &b[i], sizeof (float)) != 0)
        {
            std::printf ("         primera diferencia en la muestra %zu: %.9g vs %.9g\n",
                         i, (double) a[i], (double) b[i]);
            return;
        }
}

} // namespace

int main()
{
    std::printf ("=== Paridad de la migracion de presets: el audio no puede cambiar ===\n");

    // El probe del layout, que la migracion del bus usa como PUERTA. Se crea
    // una vez y vive toda la.main: el constructor monta el APVTS real, que es
    // lo que decide que ids existen.
    LayoutProbe processor;
    const auto processorIds = Serialization::currentParameterIds (processor);

    // ── LA TRADUCCION, EN ABSOLUTO ────────────────────────────────────────
    // Lo de mas abajo de esta linea compara el preset migrado CONTRA el que no
    // esta migrado. Es una comparacion diferencial, y por eso es ciega a una
    // parte entera de la traduccion: si esta lee el campo equivocado, el preset
    // de antes y el de despues se estropean LOS DOS por igual y la comparacion
    // sale igual. Se comprobo: hacer que `destination` lea `amount` deja este
    // bloque diferencial en verde, porque estropea los dos lados a la vez y se
    // cancela.
    //
    // O sea que el test de mas abajo comprueba LA MIGRACION, no la traduccion.
    // Para la traduccion hace falta un aserto que mire el resultado de cabeza:
    // estos doce numeros tienen que acabar en estas cuatro rutas. Con el
    // sabotaje de arriba, ESTE bloque se pone rojo, que es lo que el
    // diferencial no puede hacer.
    {
        auto known = juce::ValueTree ("STATE");
        // Ranura 1: LFO 1 (1) -> Inharmonicity (2), amount -0.5. El amount va
        // NEGATIVO a proposito: una traduccion que se comiera el signo pasaria
        // un test con amounts solo positivos, y el signo es la mitad de lo que
        // significa que la matriz sea bipolar.
        known.appendChild (param (State::IDs::mod1Source,      1.0), nullptr);
        known.appendChild (param (State::IDs::mod1Destination, 2.0), nullptr);
        known.appendChild (param (State::IDs::mod1Amount,     -0.5), nullptr);
        // Ranura 2: destino 31, que NO existe en la tabla. La traduccion no
        // filtra ni recorta a proposito —eso lo hace el motor, que descarta lo
        // que no cabe en kModDestinations—, asi que el 31 tiene que llegar
        // tal cual. Si alguien anade un clamp aqui, este aserto lo delata.
        known.appendChild (param (State::IDs::mod2Source,      7.0), nullptr);
        known.appendChild (param (State::IDs::mod2Destination, 31.0), nullptr);
        known.appendChild (param (State::IDs::mod2Amount,      0.25), nullptr);
        // Ranura 3 ausente del todo: un preset guardado puede no traerla, y lo
        // que falte tiene que salir en cero, no en basura de pila.
        known.appendChild (param (State::IDs::mod4Source,      3.0), nullptr);
        known.appendChild (param (State::IDs::mod4Destination, 10.0), nullptr);
        known.appendChild (param (State::IDs::mod4Amount,      1.0), nullptr);

        const auto routes = State::readModMatrix (
            [&known] (int slot, State::ModField field) -> float
            {
                return (float) (double) known.getChildWithProperty (
                    "id", State::modMatrixParameterId (slot, field)).getProperty ("value");
            });

        struct Expected { int source; int destination; float amount; };
        const Expected expected[4] =
        {
            {  1,  2, -0.50f },   // ranura 1, con el signo intacto
            {  7, 31,  0.25f },   // ranura 2, destino fuera de tabla sin recortar
            {  0,  0,  0.00f },   // ranura 3, ausente del preset
            {  3, 10,  1.00f },   // ranura 4
        };

        for (int slot = 0; slot < 4; ++slot)
        {
            char message[160];
            std::snprintf (message, sizeof (message),
                           "la traduccion pone la ranura %d en %d -> %d @ %.2f",
                           slot + 1, expected[slot].source, expected[slot].destination,
                           (double) expected[slot].amount);
            check (routes[slot].source == expected[slot].source
                   && routes[slot].destination == expected[slot].destination
                   && routes[slot].amount == expected[slot].amount, message);
        }
    }

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

    // El tamano, por su cuenta. Sin esto un desajuste se contaria como
    // "muchas muestras iguales" en vez de decir que no se pueden comparar.
    check (samplesBefore.size() == samplesAfter.size(),
           "las dos versiones rinden el mismo numero de muestras");

    const auto different = countDiffering (samplesBefore, samplesAfter);
    if (different != 0)
        reportFirstDifference (samplesBefore, samplesAfter);

    char message[160];
    std::snprintf (message, sizeof (message),
                   "migrar un preset pre-ENV no cambia ni una muestra (%zu de %zu)",
                   different, samplesBefore.size());
    check (different == 0, message);

    // Una red que dice "todo igual" porque las dos versiones eran silencio no
    // vale para nada: sin audio no hay paridad que comprobar.
    float peak = 0.0f;
    for (float sample : samplesBefore)
        peak = std::max (peak, std::fabs (sample));
    check (peak > 0.01f, "el preset rendido tiene audio (la comparacion mira algo)");

    // ==========================================================================
    // LA MIGRACION DE LOS DOCE IDS PLANOS AL BUS DEL HUECO
    // ==========================================================================
    std::printf ("\n--- Los doce ids planos -> el bus por hueco ---\n");

    {
        // Un preset ANTERIOR al rack: los doce ids planos, con la saturacion a
        // media pista. Los otros van en sus defaults, que es lo que traia un
        // preset de la epoca.
        auto legacy = juce::ValueTree ("STATE");
        legacy.appendChild (param (kLegacySaturation, 0.5), nullptr);
        legacy.appendChild (param (State::IDs::fxChorusRate, 0.3), nullptr);
        legacy.appendChild (param (State::IDs::fxChorusDepth, 0.2), nullptr);
        legacy.appendChild (param (State::IDs::fxChorusMix, 0.4), nullptr);
        legacy.appendChild (param (State::IDs::fxDelayTime, 0.5), nullptr);
        legacy.appendChild (param (State::IDs::fxDelayFeedback, 0.4), nullptr);
        legacy.appendChild (param (State::IDs::fxDelaySync, 0.0), nullptr);
        legacy.appendChild (param (State::IDs::fxDelayDivision, 2.0), nullptr);
        legacy.appendChild (param (State::IDs::fxReverbSize, 0.5), nullptr);
        legacy.appendChild (param (State::IDs::fxReverbDamping, 0.5), nullptr);
        legacy.appendChild (param (State::IDs::fxReverbWidth, 1.0), nullptr);
        legacy.appendChild (param (State::IDs::fxReverbMix, 0.0), nullptr);
        legacy.appendChild (param (State::IDs::masterBPM, 120.0), nullptr);

        const int written = Serialization::migrateFlatFxToSlotBus (legacy, processor);

        // CON EL LAYOUT DE HOY SOLO SE MIGRARIA EL HUECO 1. Los otros tres
        //(ids planos, todos) tienen que seguir en el arbol: sus huecos los
        // manejan todavia con mandos planos, y borrarlos dejaria esos tres
        // huecos mudos al abrir un preset viejo. Esta es la asercion que
        // documenta que la tabla de doce filas tiene puerta de verdad.
        check (hasParam (legacy, kLegacySaturation),
               "el id viejo de la saturacion NO se borra aqui (lo borra el limpiado)");
        check (hasParam (legacy, State::IDs::fxChorusMix),
               "el coro no se toca: su hueco aun lo maneja con mandos planos");
        check (hasParam (legacy, State::IDs::fxDelayTime),
               "el retardo no se toca: su hueco aun lo maneja con mandos planos");
        check (hasParam (legacy, State::IDs::fxReverbSize),
               "la reverb no se toca: su hueco aun lo maneja con mandos planos");

        // Y el hueco 1, que si se migra, escribe sus cuatro ids de bus.
        check (written == 4, "el hueco 1 escribe sus cuatro ids del bus");
        check (hasParam (legacy, State::IDs::fx1Type), "el bus del hueco 1 lleva su tipo");
        check (hasParam (legacy, State::IDs::fx1Gain), "el bus del hueco 1 lleva su ganancia");
        check (hasParam (legacy, State::IDs::fx1Mix), "el bus del hueco 1 lleva su mezcla");
        check (hasParam (legacy, State::IDs::fx1Param1), "el bus del hueco 1 lleva su mando 1");

        // LA CONVERSION, Y AQUI ESTA EL CONTROL NEGATIVO DE VERDAD.
        //
        // Un preset guarda el NORMALIZADO del APVTS viejo y el bus guarda el
        // normalizado de la FILA, y no son el mismo numero. Si la migracion
        // copiara el valor, el drive seria 0.5; con el viaje correcto, 0.5 de
        // amount son 3.0 de drive fisico (`1 + 4*0.5`), y la fila lo normaliza
        // con su sesgo 0.5 sobre el rango 1..8.
        const float expectedDrive = abd::dsp::fxNormalise (saturationDriveSpec(),
                                                           1.0f + 4.0f * 0.5f);

        check (near (readParam (legacy, State::IDs::fx1Param1), expectedDrive),
               "el mando 1 del bus es el DRIVE normalizado, no el amount copiado");

        // Y la asercion que la de arriba no puede ver: si el valor migrado
        // fuera un 0.5 (la copia) en vez del 0.449..., esta comparacion
        // tambien fallaria, pero al reves: esta mira que el numero se ha
        // MOVIDO de sitio, que es el sintoma real de una copia.
        check (! near (readParam (legacy, State::IDs::fx1Param1), 0.5f),
               "el mando migrado NO es una copia del valor guardado");

        // La mezcla: el envoltorio viejo era INSERTO en cuanto el amount era
        // > 0, asi que amount 0.5 tiene que dar mix 1.
        check (near (readParam (legacy, State::IDs::fx1Mix), 1.0f),
               "amount > 0 migra a la mezcla en inserto (mix = 1)");

        // La ganancia, que el preset viejo no tenia: a unidad, explicito.
        check (near (readParam (legacy, State::IDs::fx1Gain), 1.0f),
               "la ganancia del hueco se escribe a unidad");

        // El tipo sale de la MISMA tabla que el motor. Si se escribiera otro
        // numero, el preset migrado abriria con el drive de la saturacion sobre
        // la fila de otro efecto.
        check (near (readParam (legacy, State::IDs::fx1Type),
                     (double) NEURONiK::DSP::fxDefaultTypeForSlot (0)),
               "el tipo del hueco sale de fxDefaultTypeForSlot, la misma tabla del motor");
    }

    // --- EL BYPASS: amount 0 tiene que dar la seca, no un drive bajito -------
    {
        auto off = juce::ValueTree ("STATE");
        off.appendChild (param (kLegacySaturation, 0.0), nullptr);
        Serialization::migrateFlatFxToSlotBus (off, processor);

        check (near (readParam (off, State::IDs::fx1Mix), 0.0f),
               "amount 0 migra a la mezcla a cero (el bypass que devolvia la seca)");

        // Y el drive sigue siendo 1.0 (que es el `1 + 4*0`), no 0. La fila lo
        // normaliza al 0 de su rango, que es lo que hacia la puerta
        // `drive > 1.001` del envoltorio viejo.
        const float expectedOff = abd::dsp::fxNormalise (saturationDriveSpec(), 1.0f);
        check (near (readParam (off, State::IDs::fx1Param1), expectedOff),
               "amount 0 deja el drive en 1.0, el minimo de la fila");
    }

    // --- IDEMPOTENCIA: migrar dos veces no puede pisar lo que el usuario eligio
    {
        auto once = juce::ValueTree ("STATE");
        once.appendChild (param (kLegacySaturation, 0.5), nullptr);
        const int first = Serialization::migrateFlatFxToSlotBus (once, processor);

        // El usuario cambia de efecto DESDE el host, que es lo que hace
        // `parameterChanged` -> `setFxSlotType`.
        setParam (once, State::IDs::fx1Type, 1.0f);

        const int second = Serialization::migrateFlatFxToSlotBus (once, processor);

        check (first == 4, "la primera migracion escribe los cuatro ids");
        check (second == 0, "la segunda migracion no escribe NADA (es idempotente)");
        check (near (readParam (once, State::IDs::fx1Type), 1.0),
               "el efecto que el usuario eligio no lo pisa una segunda migracion");
    }

    // --- UN PRESET NUEVO NO SE TOCA ---------------------------------------
    {
        auto fresh = juce::ValueTree ("STATE");
        fresh.appendChild (param (State::IDs::fx1Type, 2.0), nullptr);
        fresh.appendChild (param (State::IDs::fx1Mix, 0.7), nullptr);
        const int written = Serialization::migrateFlatFxToSlotBus (fresh, processor);

        check (written == 0, "un preset que ya esta en el bus no se escribe");
        check (near (readParam (fresh, State::IDs::fx1Mix), 0.7),
               "la mezcla de un preset nuevo se queda como estaba");
    }

    // --- LA LISTA CONGELADA DE LOS DOCE -----------------------------------
    {
        const auto retired = Serialization::retiredFlatFxParameterIds();
        check (retired.size() == 12, "la lista congelada de ids planos tiene doce");

        // Los doce tienen que ser REALES: o un id que existe en el layout, o
        // uno que la migracion viene a sustituir. Un nombre inventado aqui
        // haria que un preset con ese id perdiera el mando en silencio.
        int found = 0;
        for (const auto& id : retired)
            if (processorIds.contains (id))
                ++found;

        check (found == 11,
               "once de los doce ids siguen vivos en el layout (solo `fxSaturation` se retiro)");
    }

    std::printf ("\n=== %d pasan, %d fallan ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
