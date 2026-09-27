/*
  ==============================================================================

    MemoryBudgetTest.cpp
    Created: 26 Sep 2026
    Description: La huella REAL del modelo de 25 KB.

                 FASE 11.1 llevo SpectralModel de ~8 KB a ~25 KB (3 capas x 16
                 frames). El presupuesto que manda no es el modelo suelto ni el
                 WASM: es CUANTAS COPIAS del modelo lleva cada voz. Resonator y
                 ResonatorBank guardan 4 slots (A-D) + 4 caches de frame = OCHO
                 modelos por voz. Y la reserva es PEREZOSA: el motor nace con
                 las voces de su limite (16 la aditiva, 8 la neurotik) y crece
                 solo si el limite sube —antes eran 32 fijas para cualquier
                 polifonia—; bajar la polifonia devuelve SOLO las ociosas (las
                 que suenan conservan su cola, ver BaseEngine::reclaimIdleVoices).
                 De ahi sale un motor de 1,6-3,2 MB en su default,
                 6,4 MB al maximo, mas la cola de comandos del procesador
                 (32 modelos). La seccion 3 mide esa reserva: cuantas voces tiene
                 el motor y cuanto ocupan.

                 Esta prueba MIDE —contador propio de bytes vivos sobre
                 operator new/delete, que es por donde pasan std::vector y los
                 make_unique de las voces— y FIJA el presupuesto por slot, por
                 voz, por motor y por procesador. Los numeros exactos se
                 imprimen: un fallo dice cuanto se ha ido, no solo que se fue.

                 Aviso de medida: el contador cuenta `new`/`delete`, no `malloc`
                 (juce::HeapBlock no entra y tampoco hace falta: no lleva
                 modelos). Este ejecutable es el UNICO usuario de ese override.

  ==============================================================================
*/

#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/CoreModules/Resonator.h"
#include "../Source/DSP/CoreModules/ResonatorBank.h"
#include "../Source/DSP/Synthesis/AdditiveVoice.h"
#include "../Source/DSP/Synthesis/NeurotikVoice.h"
#include "../Source/DSP/CoreModules/NeuronikEngine.h"
#include "../Source/DSP/CoreModules/NeurotikEngine.h"
#include "../Source/Main/NEURONiKProcessor.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>

// ============================================================================
// Contador de bytes vivos
// ============================================================================
//
// Cabecera de 16 bytes delante de cada bloque: guarda el tamano (para poder
// descontarlo en delete) y conserva la alineacion de 16 que da malloc en x64
// (los buffers SIMD de las voces son alignas(16), pero viven DENTRO del objeto,
// no en un new propio). No se instrumentan los `operator new` alineados: si el
// compilador emitiera uno, tiraria del par por defecto de la CRT y ese bloque
// simplemente no se cuenta — ningun tipo del motor es over-aligned.

namespace
{
    std::atomic<std::size_t> liveBytes { 0 };
    std::atomic<std::size_t> peakBytes { 0 };
    std::atomic<std::size_t> liveBlocks { 0 };

    constexpr std::size_t kAllocHeader = 16;
}

static void trackAllocation (std::size_t size) noexcept
{
    const std::size_t live = liveBytes.fetch_add (size) + size;

    std::size_t peak = peakBytes.load();

    while (live > peak && ! peakBytes.compare_exchange_weak (peak, live))
    {}

    liveBlocks.fetch_add (1);
}

static void trackDeallocation (std::size_t size) noexcept
{
    liveBytes.fetch_sub (size);
    liveBlocks.fetch_sub (1);
}

void* operator new (std::size_t size)
{
    void* raw = std::malloc (size + kAllocHeader);

    if (raw == nullptr)
        throw std::bad_alloc();

    *static_cast<std::size_t*> (raw) = size;
    trackAllocation (size);

    return static_cast<char*> (raw) + kAllocHeader;
}

void* operator new[] (std::size_t size)
{
    return operator new (size);
}

void operator delete (void* pointer) noexcept
{
    if (pointer == nullptr)
        return;

    char* raw = static_cast<char*> (pointer) - kAllocHeader;

    trackDeallocation (*reinterpret_cast<std::size_t*> (raw));
    std::free (raw);
}

void operator delete[] (void* pointer) noexcept
{
    operator delete (pointer);
}

void operator delete (void* pointer, std::size_t) noexcept
{
    operator delete (pointer);
}

void operator delete[] (void* pointer, std::size_t) noexcept
{
    operator delete (pointer);
}

// ============================================================================
// Presupuesto
// ============================================================================
//
// Los numeros son el TECHO, no la medida: la medida se imprime al lado. El
// margen es corto a proposito (una decena de KB, no un MB): la talla del modelo
// es determinista —no hay asignaciones de tamano variable—, asi que un campo
// nuevo se ve enseguida. La cola de comandos del procesador (32 x modelo) y el
// estado del motor entran por separado.

namespace
{
    /** Un modelo espectral (1 slot). Medido 25032 B; el techo deja ~1,5 KB:
        caza una capa extra (+8 KB), un kMaxFrames mayor o un array nuevo de 64. */
    constexpr std::size_t kModelBudget = 26 * 1024;

    /** Copias del modelo dentro de UNA voz (Resonator/ResonatorBank). */
    constexpr int kModelCopiesPerVoice = 8;   // 4 slots x (models + frameCache)

    /** Una voz completa. Medido ~203 KB (206-209 KB en el heap con su jitter);
        el techo caza un slot mas (+25 KB) o una copia mas del modelo. */
    constexpr std::size_t kVoiceBudget = 224 * 1024;

    /** Un motor PREPARADO con su limite POR DEFECTO (16 la aditiva, 8 la
        neurotik), que es lo que reserva la perezosa al nacer. */
    constexpr std::size_t kEngineBudgetHalf = 4 * 1024 * 1024;

    /** El mismo motor con la polifonia AL MAXIMO (32 voces). Medido 6,46 MB
        (aditivo) y 6,37 MB (neurotik) cuando reservaba 32 SIEMPRE; con la
        perezosa el techo sigue cubriendo el peor caso. El techo caza voces de
        mas (~200 KB cada una) o una lista que crezca sin tope. */
    constexpr std::size_t kEngineBudget = 7 * 1024 * 1024 + 512 * 1024;

    /** El procesador completo (cola de 32 modelos + UN motor + managers).
        Medido 7,20 MB con el motor aditivo; el techo caza que el procesador
        pase a sostener dos motores (~13,6 MB) o que la cola se duplique. */
    constexpr std::size_t kProcessorBudget = 8 * 1024 * 1024 + 512 * 1024;
}

// Coherencia del propio presupuesto: la suma de las partes no puede pasarse del
// techo de la siguiente. Si alguien sube un techo suelto, salta aqui.
static_assert ((std::size_t) kModelCopiesPerVoice * kModelBudget < kVoiceBudget,
               "el modelo x8 ya no cabe en el techo de una voz");
static_assert (kEngineBudget + 32 * kModelBudget < kProcessorBudget,
               "un motor + la cola de comandos ya no caben en el techo del procesador");

// Guardias de compilacion: no dependen de ejecutar la prueba. Si el modelo o las
// voces engordan, el compilador para el build antes que el heap.
static_assert (sizeof (NEURONiK::Common::SpectralModel) < kModelBudget,
               "SpectralModel se ha ido de presupuesto: mira cuantas copias lleva cada voz");
static_assert (sizeof (NEURONiK::DSP::Synthesis::AdditiveVoice) < kVoiceBudget,
               "AdditiveVoice se ha ido de presupuesto (modelo x8)");
static_assert (sizeof (NEURONiK::DSP::Synthesis::NeurotikVoice) < kVoiceBudget,
               "NeurotikVoice se ha ido de presupuesto (modelo x8)");
static_assert (sizeof (NEURONiKProcessor) < 128 * 1024,
               "NEURONiKProcessor se ha ido de talla: no lo crees en la pila (Fase 11.1)");

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  [ok]   " << description << '\n';
            return;
        }

        std::cout << "  [FAIL] " << description << '\n';
        ++failures;
    }

    juce::String bytes (std::size_t value)
    {
        if (value >= 1024 * 1024)
            return juce::String (value / (1024.0 * 1024.0), 2) + " MB";

        return juce::String (value / 1024.0, 1) + " KB (" + juce::String ((int) value) + " B)";
    }

    /** Heap vivo en este instante, segun el contador de arriba. */
    std::size_t liveNow()
    {
        return liveBytes.load();
    }

} // namespace

int main()
{
    std::cout << std::unitbuf;
    std::cout << "Huella heap/pila del modelo de 25 KB\n";

    using namespace NEURONiK;

    //==============================================================================
    // 1. TALLAS DEL MODELO Y DE SUS PORTADORES
    //==============================================================================

    std::cout << "\n1. Tallas (compilacion)\n";

    const auto modelBytes = sizeof (Common::SpectralModel);
    const auto additiveBytes = sizeof (DSP::Synthesis::AdditiveVoice);
    const auto neurotikBytes = sizeof (DSP::Synthesis::NeurotikVoice);
    const auto resonatorBytes = sizeof (DSP::Core::Resonator);
    const auto bankBytes = sizeof (DSP::Core::ResonatorBank);
    const auto processorPile = sizeof (NEURONiKProcessor);

    std::cout << "  modelo espectral          " << bytes (modelBytes) << '\n';
    std::cout << "  Resonator (aditivo)       " << bytes (resonatorBytes) << '\n';
    std::cout << "  ResonatorBank (neurotik)  " << bytes (bankBytes) << '\n';
    std::cout << "  AdditiveVoice             " << bytes (additiveBytes) << '\n';
    std::cout << "  NeurotikVoice             " << bytes (neurotikBytes) << '\n';
    std::cout << "  NEURONiKProcessor (pila)  " << bytes (processorPile) << '\n';
    std::cout << "  cola de comandos (32x)    " << bytes (32 * modelBytes) << " -> heap\n";

    check (modelBytes < kModelBudget,
           "un modelo espectral cabe en " + bytes (kModelBudget) + ": mide " + bytes (modelBytes));

    check (additiveBytes < kVoiceBudget && neurotikBytes < kVoiceBudget,
           "las dos voces caben en " + bytes (kVoiceBudget)
               + " (aditiva " + bytes (additiveBytes) + ", neurotik " + bytes (neurotikBytes) + ")");

    check (processorPile < 128 * 1024,
           "el procesador cabe en 128 KB de PILA: mide " + bytes (processorPile));

    //==============================================================================
    // 2. POR VOZ: EL MODELO MANDA
    //==============================================================================

    std::cout << "\n2. El modelo por voz\n";

    const auto copiesBytes = (std::size_t) kModelCopiesPerVoice * modelBytes;

    std::cout << "  copias del modelo / voz   " << kModelCopiesPerVoice
              << " (4 slots x models+frameCache) = " << bytes (copiesBytes) << '\n';

    check (resonatorBytes >= copiesBytes && bankBytes >= copiesBytes,
           "las " + juce::String (kModelCopiesPerVoice) + " copias ESTAN en la voz: Resonator "
               + bytes (resonatorBytes) + " y ResonatorBank " + bytes (bankBytes)
               + " llevan los " + bytes (copiesBytes));

    check (copiesBytes < additiveBytes,
           "las " + juce::String (kModelCopiesPerVoice) + " copias del modelo son la parte dominante"
               + " de la voz (" + bytes (copiesBytes) + " de " + bytes (additiveBytes) + ")");

    check (copiesBytes * 32 > 4 * 1024 * 1024,
           "y al TECHO de 32 voces ya son " + bytes (copiesBytes * 32)
               + ": el modelo, no la polifonia, manda en el heap");

    //==============================================================================
    // 3. HEAP DE CADA MOTOR (32 voces PRE-ASIGNADAS, preparadas)
    //==============================================================================

    std::cout << "\n3. Reserva perezosa: el heap sigue al limite de polifonia\n";

    // La reserva de un motor recien creado (16 la aditiva, 8 la neurotik) contra el
    // techo de 32: si alguien vuelve a pre-asignar 32 fijas, `allocated16` pasa a 32
    // y el [FAIL] lo dice antes que el numero de heap.
    std::size_t additiveEngine16 = 0;
    std::size_t additiveEngine32 = 0;
    std::size_t neurotikEngine8 = 0;
    std::size_t neurotikEngine32 = 0;
    int allocated16 = 0, allocated32 = 0, allocatedAfterLower = 0, allocatedNeuro8 = 0;

    {
        // El motor tiene que seguir VIVO cuando se lee el heap: medido dentro de
        // una lambda que lo destruye al volver, el heap daria cero.
        const auto before = liveNow();
        auto engine = std::make_unique<DSP::NeuronikEngine>();
        engine->prepare (48000.0, 512);
        allocated16 = engine->getNumAllocatedVoices();
        additiveEngine16 = liveNow() - before;
    }

    {
        // Medir el techo (32) aislado: additiveEngine32 es el heap con 32 voces,
        // ANTES de bajarlo. La rama que baja a 4 se mide en otro bloque para no
        // contaminar la pendiente por voz.
        std::size_t heapAt32 = 0;
        {
            const auto before = liveNow();
            auto engine = std::make_unique<DSP::NeuronikEngine>();
            engine->prepare (48000.0, 512);
            engine->setPolyphony (32);
            allocated32 = engine->getNumAllocatedVoices();
            heapAt32 = liveNow() - before;
            // bajar devuelve solo las ociosas (0 activas -> 4): presupuesto en otra variable
            engine->setPolyphony (4);
            allocatedAfterLower = engine->getNumAllocatedVoices();
        }
        additiveEngine32 = heapAt32;
    }

    {
        const auto before = liveNow();
        auto engine = std::make_unique<DSP::NeurotikEngine>();
        engine->prepare (48000.0, 512);
        allocatedNeuro8 = engine->getNumAllocatedVoices();
        neurotikEngine8 = liveNow() - before;
    }

    {
        const auto before = liveNow();
        auto engine = std::make_unique<DSP::NeurotikEngine>();
        engine->prepare (48000.0, 512);
        engine->setPolyphony (32);
        neurotikEngine32 = liveNow() - before;
    }

    const std::size_t additiveSlope = (additiveEngine32 - additiveEngine16) / 16;

    std::cout << "  aditiva  (limite 16)      " << bytes (additiveEngine16)
              << "   voces reservadas " << allocated16 << '\n';
    std::cout << "  aditiva  (limite 32)      " << bytes (additiveEngine32)
              << "   ->  " << bytes (additiveSlope) << " / voz\n";
    std::cout << "  neurotik (limite 8)       " << bytes (neurotikEngine8)
              << "   voces reservadas " << allocatedNeuro8 << '\n';
    std::cout << "  neurotik (limite 32)      " << bytes (neurotikEngine32) << '\n';

    check (allocated16 == 16 && allocatedNeuro8 == 8,
           "un motor recien creado reserva SU limite (16 / 8), no las 32 de antes");

    check (allocated32 == 32 && allocatedAfterLower == 4,
           "subir la polifonia reserva hasta el techo y bajarla devuelve solo las ociosas ("
               + juce::String (allocatedAfterLower) + " tras bajar a 4, 0 activas)");

    check (additiveSlope >= (std::size_t) additiveBytes && additiveSlope < kVoiceBudget,
           "cada voz nueva del heap (con su jitter) pesa lo de una voz: " + bytes (additiveSlope));

    // Bajar con voces sonando: solo las ociosas vuelven al heap.
    // Hilo de mensajes con cerrojo: el test simula el gesto del menu
    // (cabecera MIDI) disparando notas y luego bajando la polifonia;
    // el hilo de audio no participa.
    {
        auto engine = std::make_unique<DSP::NeuronikEngine>();
        engine->prepare (48000.0, 512);
        engine->setPolyphony (16);

        // 3 notas -> 3 activas
        engine->handleMidiMessage (dsp::MidiMessage::noteOn  (1, 60, 0.8f));
        engine->handleMidiMessage (dsp::MidiMessage::noteOn  (1, 62, 0.8f));
        engine->handleMidiMessage (dsp::MidiMessage::noteOn  (1, 64, 0.8f));
        check (engine->getNumActiveVoices() == 3, "con 3 notas, 3 voces suenan");

        // Bajar a 8 con 3 activas: las ociosas si vuelven, las que suenan se quedan
        engine->setPolyphony (8);
        check (engine->getNumAllocatedVoices() == 8,
               "bajar a 8 con 3 activas: tamanio 8 (8 >= 3 activas, ociosas devueltas)");
        check (engine->getNumActiveVoices() == 3,
               "y siguen sonando 3 tras bajar a 8");

        // Bajar a 2 con 3 activas: no puede bajar de 3 (max(limit, activas))
        engine->setPolyphony (2);
        check (engine->getNumAllocatedVoices() == 3,
               "bajar a 2 con 3 activas: tamanio 3 (max(2, 3 activas)), nada de la cola se corta");
        check (engine->getNumActiveVoices() == 3,
               "y siguen sonando 3 tras bajar a 2 (la cola intacta)");

        // Soltar notas -> colas en release; forzamos reset para medir la rama ociosa
        engine->handleMidiMessage (dsp::MidiMessage::noteOff (1, 60, 0.0f));
        engine->handleMidiMessage (dsp::MidiMessage::noteOff (1, 62, 0.0f));
        engine->handleMidiMessage (dsp::MidiMessage::noteOff (1, 64, 0.0f));
        engine->allNotesOff();
        engine->reset();
        check (engine->getNumActiveVoices() == 0, "tras reset, 0 activas");

        engine->setPolyphony (1);
        check (engine->getNumAllocatedVoices() == 1,
               "bajar a 1 con 0 activas: tamanio 1 (todas ociosas devueltas)");
    }

    // Reserva en caliente: capacity 32 intacta, subir no reasigna
    {
        auto engine = std::make_unique<DSP::NeuronikEngine>();
        engine->prepare (48000.0, 512);
        engine->setPolyphony (16);
        engine->setPolyphony (4);
        check (engine->getNumAllocatedVoices() == 4,
               "bajar a 4 sin notas: tamanio 4 (ociosas devueltas)");
        engine->setPolyphony (32);
        check (engine->getNumAllocatedVoices() == 32,
               "reserva en caliente: bajar a 4 y subir a 32 queda en 32 (capacity 32 intacta)");
    }

    check (additiveEngine16 < additiveEngine32 && neurotikEngine8 < neurotikEngine32,
           "y el heap crece con el limite: " + bytes (additiveEngine16) + " -> " + bytes (additiveEngine32));

    check (additiveEngine16 < kEngineBudgetHalf,
           "el motor aditivo en su limite por defecto cabe en " + bytes (kEngineBudgetHalf)
               + ": mide " + bytes (additiveEngine16));

    check (additiveEngine32 < kEngineBudget && neurotikEngine32 < kEngineBudget,
           "y al maximo (32) cabe en " + bytes (kEngineBudget) + ": mide "
               + bytes (additiveEngine32) + " / " + bytes (neurotikEngine32));

    //==============================================================================
    // 4. HEAP DEL PROCESADOR COMPLETO (16 voces)
    //==============================================================================

    std::cout << "\n4. Heap del procesador (16 voces)\n";

    std::size_t processorHeap = 0;

    {
        const auto before = liveNow();
        auto processor = std::make_unique<NEURONiKProcessor>();
        processor->setPolyphony (16);
        processor->setRateAndBufferSizeDetails (48000.0, 512);
        processorHeap = liveNow() - before;
    }

    std::cout << "  procesador completo       " << bytes (processorHeap) << '\n';
    std::cout << "    de eso, cola de comandos  " << bytes (32 * modelBytes) << '\n';

    check (processorHeap < kProcessorBudget,
           "el procesador cabe en " + bytes (kProcessorBudget) + ": mide " + bytes (processorHeap));

    check (processorHeap > 32 * modelBytes,
           "y la cola de 32 modelos vive en el HEAP, no en la pila del procesador ("
               + bytes (32 * modelBytes) + ")");

    check (processorHeap > additiveEngine16,
           "el procesador arrastra su motor ("
               + bytes (processorHeap - additiveEngine16) + " por encima del motor solo)");

    std::size_t processorHeapMax = 0;

    {
        const auto before = liveNow();
        auto processor = std::make_unique<NEURONiKProcessor>();
        processor->setRateAndBufferSizeDetails (48000.0, 512);
        processor->setPolyphony (32);
        processorHeapMax = liveNow() - before;
    }

    std::cout << "  procesador (32 voces)     " << bytes (processorHeapMax) << '\n';

    check (processorHeapMax < kProcessorBudget,
           "y con la polifonia al maximo cabe en " + bytes (kProcessorBudget)
               + ": mide " + bytes (processorHeapMax));

    // El procesador sostiene UN motor a la vez (el otro solo existe mientras se
    // cambia); el presupuesto cubre el MAYOR de los dos mas la cola.
    check (processorHeap < kProcessorBudget && kProcessorBudget > neurotikEngine32 + 32 * modelBytes,
           "y el techo cubre el motor mayor (neurotik al maximo, " + bytes (neurotikEngine32)
               + ") mas la cola de comandos");

    //==============================================================================

    std::cout << "\nPico de bytes vivos en esta prueba: " << bytes (peakBytes.load())
              << " en " << (int) liveBlocks.load() << " bloques\n";
    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';

    return failures == 0 ? 0 : 1;
}
