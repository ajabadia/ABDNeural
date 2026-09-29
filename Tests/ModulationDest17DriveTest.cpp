/*
  ==============================================================================

    ModulationDest17DriveTest.cpp
    Created: 29 Sep 2026
    Description: EL DESTINO 17 DE LA MATRIZ MUEVE EL DRIVE DEL HUECO 1, Y SE
                 DEMUESTRA ESCUCHANDO.

                 El destino 17 es "Saturation" y su `parameterId` es `fx1Param1`.
                 Eso ya lo comprueba `NEURONiK_ModulationContractTest`, que
                 compara la tabla de C++ contra el JSON de ABDSharedAssets.

                 PERO ESO NO ES SUFICIENTE, y este test existe porque no lo era.
                 `parameterId` es una DECLARACION: dice a que mando se supone que
                 apunta. Que el motor lo mueva de verdad es otra cosa, y hay al
                 menos tres formas de que no lo mueva sin que nada se rompa:

                   - el Kind es correcto pero el `busParam` se queda en -1, y la
                     escritura se salta en silencio;
                   - escribe al hueco equivocado (al 3 en vez del 0) y suena
                     otra cosa;
                   - escribe al mando equivocado del hueco (params[1] en vez de
                     params[0]) y vuelve a sonar otra cosa.

                 Las tres compilan, los `static_assert` de la tabla pasan, el
                 contrato compartido sigue verde, y el boton de saturacion no
                 hace NADA. Un destino que se puede seleccionar, que la pagina
                 ofrece como valido, y que no suena.

                 ASI QUE ESTE TEST NO COMPRUEBA LA TABLA: CONDUCE EL MOTOR DE
                 VERDAD Y ESCUCHA. Y no puede ser un aserto sobre la tabla,
                 porque la version barata pasaba en verde mientras el destino no
                 movia una sola muestra.

                 ── POR QUE NO HAY NINGUN ASERTO "BIT A BIT" ──────────────────

                 Porque el motor NO es reproducible entre instancias. Medido con
                 una sonda que renderiza dos veces LO MISMO en el mismo proceso:
                 en 2 de cada 10 ejecuciones las dos corridas divergen desde la
                 muestra ~64, con una diferencia maxima de 6.3e-2 sobre una senal
                 de rms 5.7e-2. O sea que la reproduccion es tanto mejor como
                 peor que el efecto que se quiere medir (una modulacion de verdad
                 mueve el rms un factor de 5, pero las muestras cambian un 10 %).

                 Es un bug del motor y esta anotado aqui para que no se pierda:
                 dos instancias nuevas con los mismos parametros y el mismo
                 material tienen que dar el mismo audio, y no lo dan. Pero
                 mientras no se arregle, un aserto exacto aqui seria una moneda
                 al aire: pasaria unas veces y caeria otras sin que hubiera
                 cambiado nada. Por eso TODAS las medidas de este test son
                 estadisticas (rms y factor de cresta sobre la cola) y todas las
                 tolerancias van escritas con el margen que dejan.

  ==============================================================================
*/

#include <cmath>
#include <cstdio>
#include <vector>

#include "State/ParameterDefinitions.h"
#include "State/ModDestinationTable.h"
#include "CoreModules/NeuronikEngine.h"
#include "DspEffects/adapters/BasicAdapters.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 64;

// ── LAS VENTANAS DEL RENDER, DERIVADAS Y NO ESCRITAS A MANO ────────────────
//
// kCola son las muestras que se descartan por delante de lo que se mide: el
// retardo por defecto del hueco 3, con la rampa del `drive` de margen. kBloques
// es el largo del render, y son varias vueltas al retardo para que el eco
// suene DESPUES de que la modulacion haya asentado. Con una sola vuelta el
// destino 18 queda mudo por falta de tiempo, que es el falso negativo mas caro:
// el test pasa en verde sin haber medido nada.
//
// LAS DOS SALEN DE `fxDefaultSlotDelaySeconds`, QUE NO ES LO MISMO QUE
// `GlobalParams::delayTime`. Ese campo vale 0.3 tambien, asi que elegir el
// equivocado daba el mismo numero y el test pasaba igual: son dos 0,3 en sitios
// distintos. Pero `delayTime` es lo que lee el HOST del APVTS, y este test
// conduce el motor sin host, de modo que el retardo que suena es el que va por
// el bus del hueco 3. Derivar del otro daria una cola calculada de un retardo
// que puede no ser el que suena.
//
// Antes `kCola` valia `64 * 32` = 2048 muestras = 42,7 ms, con un comentario
// que decia "el ultimo medio segundo". Las dos cosas eran falsas: el numero no
// llegaba a lo que el comentario prometia, y lo que se midia eran 3,16 s.
constexpr double kRetardoPorDefecto = NEURONiK::DSP::fxDefaultSlotDelaySeconds (2);
constexpr int    kRampaDelDriveMs   = static_cast<int> (abd::dsp::adapters::SaturationFx::kDriveRampSeconds * 1000.0);
constexpr double kSegundosDeCola    = kRetardoPorDefecto + kRampaDelDriveMs / 1000.0;
constexpr int    kCola              = static_cast<int> (kSegundosDeCola * kSampleRate);
constexpr int    kBloques           = static_cast<int> ((4.0 * kRetardoPorDefecto + kSegundosDeCola) * kSampleRate) / kBlockSize;

int passed = 0;
int failed = 0;

void check (bool condition, const char* name)
{
    if (condition) { ++passed; std::printf ("  [PASS] %s\n", name); }
    else           { ++failed; std::printf ("  [FAIL] %s\n", name); }
}

// ── LOS DESTINOS, POR NOMBRE Y NO POR NUMERO ────────────────────────────────
//
// `constexpr int kDestinoDrive = 17` es un 17 suelto: si una fila de
// `kModDestinationTable` se mueve, el 17 pasa a ser otro destino y este test
// sigue midiendo el 17 en verde. Atarlo por nombre ata el test a lo que DICE
// medir. Los dos `static_assert` no cuestan nada en runtime, y si la etiqueta
// cambia el fallo sale alli, que es donde se puede ver.
constexpr std::size_t kDestinoDrive   = 17;
constexpr std::size_t kDestinoRetardo = 18;

static_assert (NEURONiK::State::modDestinationLabelIs (kDestinoDrive, "Saturation"),
               "el destino 17 es 'Saturation': si la etiqueta cambio, este test mide otro");
static_assert (NEURONiK::State::modDestinationLabelIs (kDestinoRetardo, "Delay Time"),
               "el destino 18 es 'Delay Time': si la etiqueta cambio, este test mide otro");

// LA FUENTE 4 NO SE PUEDE ATAR POR NOMBRE, y por eso se queda con el numero y
// escrito el motivo, que es lo que hace falta para que el 4 no parezca casual.
//
// Las ocho fuentes viven en un array de literales dentro de `applyModulation()`,
// en este orden: Off, LFO 1, LFO 2, Pitch Bend, Mod Wheel, Aftertouch, ENV 1,
// ENV 2. No hay tabla con nombres que consultar. La unica con nombres es
// `getModSources()`, en ParameterDefinitions.h, y es `inline juce::StringArray`:
// llamarla obligaria a enlazar `juce_audio_processors` en este target, que hoy
// enlaza sin el a proposito (ver el `target_link_libraries`). Por un numero que
// el motor lee de un array que el mismo motor escribe, no vale anadir un modulo
// entero a un exe de test.
//
// O sea: los DESTINOS si van atados por nombre porque su tabla es constexpr y sin
// JUCE, y la FUENTE no, y la diferencia esta en si hay algo que consultar. Si
// algun dia las fuentes salen de una tabla como las de los destinos, esta linea
// se cambia por un `static_assert` igual que los de arriba.
constexpr int kFuenteModWheel = 4;

/** El drive con el que arranca el hueco 1 en las tres corridas. */
constexpr float kDriveBajo = 0.10f;

/**
    UNA RUEDA DE MODULO TIENE 7 BITS, y 0.5 no es un valor que exista: el CC 1 va
    de 0 a 127 y el motor divide entre 127. La corrida que gira el drive A MANO
    tiene que usar el valor REAL que produce el CC (64/127 = 0.5039), no el 0.60
    de cuentas. Con el 0.60 de cuentas la referencia queda 0.4 % por debajo del
    drive que hace la ruta, la comparacion "suena igual" no puede pasar nunca, y
    el test acaba aceptando cualquier cosa con tal de que se moviera algo. */
uint8_t ccDeRueda (float ruedaMod)  { return static_cast<uint8_t> (ruedaMod * 127.0f + 0.5f); }
float   valorDeRueda (float ruedaMod) { return ccDeRueda (ruedaMod) / 127.0f; }

/**
    Renderiza el motor entero y devuelve las muestras del canal izquierdo.

    `rutaDestino` < 0 significa "sin ruta". La rueda se mueve con un gesto MIDI
    real (CC 1) porque es el camino por el que llega en el producto, y asi el
    test no depende de un `setModWheel` que podria no existir.

    LA MEZCLA DEL HUECO 1 VA A 1 a proposito: con `mix = 0` el slot devuelve la
    seca BIT A BIT, o sea que el drive se puede mover todo lo que se quiera y no
    se oye NADA. Un test de saturacion con el hueco en seco no mide la
    saturacion. */
std::vector<float> renderizar (int rutaDestino, float ruedaMod, float driveBase)
{
    NEURONiK::DSP::NeuronikEngine engine;
    engine.prepare (kSampleRate, kBlockSize);

    NEURONiK::DSP::GlobalParams params;

    // LA CADENA CON LOS DEFAULTS DEL PRODUCTO, y no con los del struct (2026-09-29).
    // Los cuatro huecos se rellenan por su bus, y un `GlobalParams` recien
    // construido tiene el `mix` de cada hueco a CERO: los cuatro en silencio. Sin
    // esta fila el retardo no suena, y el destino 18 --que es su tiempo-- no
    // tendria nada que mover, que es como se ve un destino que ha muerto sin que
    // su propio test se entere. Los defaults salen de `FxCatalogue.h` porque son
    // los mismos que declara el APVTS: aqui se prueba el camino, no la cadena.
    for (int slot = 0; slot < NEURONiK::DSP::kFxBusSlots; ++slot)
    {
        params.fx[slot].gain = 1.0f;
        params.fx[slot].mix  = NEURONiK::DSP::fxDefaultSlotMix (slot);

        for (int i = 0; i < NEURONiK::DSP::kFxBusParams; ++i)
            params.fx[slot].params[i] = NEURONiK::DSP::fxDefaultSlotParam (slot, i);
    }

    params.fx[0].params[0] = driveBase;
    params.fx[0].gain = 1.0f;
    params.fx[0].mix  = 1.0f;

    if (rutaDestino >= 0)
    {
        params.modMatrix[0].source      = kFuenteModWheel;
        params.modMatrix[0].destination = rutaDestino;
        params.modMatrix[0].amount      = 1.0f;
    }

    engine.setGlobalParams (params);
    engine.updateParameters();   // setGlobalParams deja la ruta en pending

    std::vector<float> muestras (static_cast<std::size_t> (kBloques * kBlockSize), 0.0f);

    dsp::AudioBuffer<float> buffer (2, kBlockSize);
    dsp::MidiBuffer midi;

    // Una nota mantenida: sin voz no hay senal que saturar y el test mediria
    // silencio con estadisticas.
    midi.addEvent (dsp::MidiMessage::noteOn (1, 60, uint8_t (127)), 0);
    midi.addEvent (dsp::MidiMessage::controllerEvent (1, 1, ccDeRueda (ruedaMod)), 0);
    engine.renderNextBlock (buffer, midi);

    for (int bloque = 1; bloque < kBloques; ++bloque)
    {
        midi.clear();
        engine.renderNextBlock (buffer, midi);

        for (int i = 0; i < kBlockSize; ++i)
            muestras[static_cast<std::size_t> (bloque * kBlockSize + i)] = buffer.getSample (0, i);
    }

    return muestras;
}

/** La cola: lo que queda DESPUES de esperar a que el retardo y el drive esten
    asentados. Lo que se descarta por delante son `kCola` muestras, y eso sale de
    `delayTime`, no de un numero escrito aqui. */
std::vector<float> cola (const std::vector<float>& todo)
{
    return std::vector<float> (todo.begin() + static_cast<std::ptrdiff_t> (kCola), todo.end());
}

double rms (const std::vector<float>& x)
{
    if (x.empty()) return 0.0;
    double suma = 0.0;
    for (float v : x) suma += static_cast<double> (v) * v;
    return std::sqrt (suma / static_cast<double> (x.size()));
}

double pico (const std::vector<float>& x)
{
    double mayor = 0.0;
    for (float v : x) mayor = std::max (mayor, std::fabs (static_cast<double> (v)));
    return mayor;
}

/** Pico sobre RMS.

    MAS drive = MAS saturacion, y la saturacion APLANA: la misma senal entra y
    sale mas pegada a la curva de `atan(x · drive)`, o sea con menos factor de
    cresta. Es la medida que distingue "mas drive" de "otro mando", y la que hace
    falta porque el RMS NO: `atan(x · drive)` no esta normalizado a 1, asi que
    subir el drive SUBE el nivel en vez de bajarlo. Un test que mirase el rms
    esperando que bajara al comprimir estaria describiendo un motor que no es
    este. */
double cresta (const std::vector<float>& x)
{
    const double r = rms (x);
    return r > 0.0 ? pico (x) / r : 0.0;
}

/** Cuanto se parecen dos medidas. 0 = iguales. */
double relativa (double a, double b)
{
    const double escala = std::max (std::fabs (a), std::fabs (b));
    return escala > 0.0 ? std::fabs (a - b) / escala : 1.0;
}

} // namespace

int main()
{
    std::printf ("=== El destino 17 mueve el drive del hueco 1 ===\n\n");

    constexpr float rueda     = 0.5f;
    const float     delivery  = valorDeRueda (rueda);
    const float     driveAlto = kDriveBajo + delivery;   // lo que SUMA la ruta

    // Tres corridas sobre el MISMO material. Lo unico que cambia es COMO LLEGA
    // ese drive:
    //
    //   A  sin ruta, drive a la mano en kDriveBajo     -> la referencia
    //   B  ruta Mod Wheel -> destino 17, drive base bajo -> MODULADO
    //   C  sin ruta, drive a la mano en `driveAlto`    -> el mismo mando, a mano

    const auto a = cola (renderizar (-1, rueda, kDriveBajo));
    const auto b = cola (renderizar (static_cast<int> (kDestinoDrive), rueda, kDriveBajo));
    const auto c = cola (renderizar (-1, rueda, driveAlto));

    std::printf ("  rueda al %.4f -> el drive suma %.4f\n", delivery, delivery);
    std::printf ("  A sin ruta, drive %.2f   rms %.6f  cresta %.4f\n", kDriveBajo, rms (a), cresta (a));
    std::printf ("  B MODULADO, drive %.2f   rms %.6f  cresta %.4f\n", driveAlto, rms (b), cresta (b));
    std::printf ("  C a mano,  drive %.2f   rms %.6f  cresta %.4f\n\n", driveAlto, rms (c), cresta (c));

    // QUE MODULAR SUENA A LO MISMO QUE GIRAR EL MANDO A MANO. Esa es la
    // asercion que importa, y no la de que "suene diferente": que suene
    // diferente solo prueba que el motor hace algo, y lo que hay que probar es
    // que el destino 17 aterriza EN EL DRIVE y no en otro sitio. Un destino que
    // escribiera en params[1] sonaria distinto de A y pasaria una comparacion
    // ingenua: seria otro fallo con el mismo aspecto.
    //
    // El margen es del 1 % porque la diferencia real entre las dos corridas es
    // del 0.01 % (solo el redondeo de los 7 bits del CC y la rampa de
    // `SaturationFx::kDriveRampSeconds`, 5 ms),
    // y porque la no reproduccion del motor (ver la cabecera) se queda en el
    // orden del 10 % muestra a muestra pero no llega al 1 % en un rms de medio
    // segundo.
    check (relativa (cresta (b), cresta (c)) < 0.01,
           "modular suena IGUAL que girar el drive a mano (mismo factor de cresta)");
    check (relativa (rms (b), rms (c)) < 0.01,
           "modular suena IGUAL que girar el drive a mano (mismo nivel)");

    // Y que se oiga de verdad, no solo que suene distinto. El umbral es 3 y no
    // el 5 que ponia antes, y el motivo es que el 5 estaba calibrado contra una
    // ventana que no era la que dice medir: la cola de entonces eran 2048
    // muestras (42,7 ms, no el "medio segundo" que decia su comentario), y el
    // factor salia 5,64 con la cola larga y 4,12 con la de ahora. O sea, el 5 no
    // venia de ninguna razon, era el numero que daba una ejecucion.
    //
    // Lo que este test tiene que separar es "mueve" de "no mueve", y eso lo dan
    // los dos controles negativos de abajo, que exigen menos del 2 % de cambio.
    // Un factor de 3 contra un 2 % son dos ordenes de magnitud de separacion:
    // cualquier cosa que no sea el motor entero roto cae al otro lado.
    check (rms (b) > rms (a) * 3.0,
           "el destino 17 cambia el sonido de verdad respecto a no modular");

    // QUE PARA EL LADO CORRECTO: mas drive aplana. Un destino que sumase al reves
    // daria la relacion contraria, y es el fallo que un "suena diferente" no ve.
    check (cresta (b) < cresta (a) * 0.95,
           "mas drive APLANA: baja el factor de cresta (es saturacion, no volumen)");

    // --- CONTROL NEGATIVO 1: LA RUTA SIN GESTO NO HACE NADA ------------------
    // El mismo destino 17, la misma cantidad, la rueda a cero. Si aqui la salida
    // se MOVIERA, el cambio de antes no lo habria hecho la modulacion sino algo
    // de la ruta (una escritura al bus, un `prepare`, un estado del motor). Un
    // test que solo mira "suena diferente" no distingue esos dos casos.
    const auto ruedaCero = cola (renderizar (static_cast<int> (kDestinoDrive), 0.0f, kDriveBajo));
    check (relativa (rms (ruedaCero), rms (a)) < 0.02,
           "destino 17 con la rueda a cero: NO cambia nada");

    // --- CONTROL NEGATIVO 2: LA MISMA RUEDA, SIN DESTINO ----------------------
    // La rueda al 50 % por si sola no puede mover el drive. Sin esto, el cambio
    // de B podria atribuirse al gesto MIDI y no a la ruta.
    const auto ruedaSinDestino = cola (renderizar (-1, rueda, kDriveBajo));
    check (relativa (rms (ruedaSinDestino), rms (a)) < 0.02,
           "la misma rueda SIN destino: NO cambia nada");

    // --- Y EL HERMANO: EL 18 ESTABA MUERTO IGUAL QUE EL 17 -------------------
    // El 18 (Delay Time) es `globalAdd` y sufria la misma muerte: escribia en
    // `currentGlobalParams` cuando los mandos ya se habian empujado a los
    // huecos. Se comprueba para que el empuje no se pueda deshacer de un solo
    // destino: si alguien lo quita, caen el 17 y el 18 juntos, que se ve mas que
    // caerse solo.
    const auto destino18     = cola (renderizar (static_cast<int> (kDestinoRetardo), rueda, kDriveBajo));
    const auto destino18Nulo = cola (renderizar (static_cast<int> (kDestinoRetardo), 0.0f, kDriveBajo));
    check (relativa (rms (destino18), rms (destino18Nulo)) > 0.05,
           "el destino 18 (Delay Time) tambien mueve el motor: el empuje no es solo del 17");

    std::printf ("\n=== %d pasan, %d fallan ===\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
