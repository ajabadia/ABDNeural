/*
  ==============================================================================

    AudioBufferParityTest.cpp
    dsp::AudioBuffer (ABDSharedCode::DspCore, DspCore/DspCore.h) contra
    juce::AudioBuffer, que es su original.

    Por que vive aqui y no en DspCore: el modulo compartido es JUCE-free por
    contrato, asi que la comparacion contra el original solo puede estar en un
    consumidor con JUCE (misma razon por la que viven aqui MidiPortTest y
    DspReverbParityTest). El alias corto `dsp::` de los shims queda cubierto por
    MidiPortTest.

    Que fija:

      1. PARIDAD DE DATOS. La MISMA secuencia de operaciones (setSize con sus
         banderas, clear, setSample/addSample, applyGain, applyGainRamp,
         addFrom/copyFrom y sus variantes con rampa, reverse, getMagnitude,
         getRMSLevel) deja en los dos tipos exactamente las mismas muestras bit a
         bit, la misma magnitud/RMS y la misma bandera hasBeenCleared(). El port
         es una copia literal de JUCE: si alguien "mejora" una operacion, salta
         aqui.

      2. PARIDAD DE ESTRUCTURA DE PUNTEROS, que es el guard de la rama que estuvo
         MUERTA. `numElementsInArray` tomaba el array POR VALOR y devolvia
         sizeof(Type*)/sizeof(Type) (= 1 en sus tres consumidores), de modo que la
         comprobacion `numChannels < 32` de AudioBuffer se evaluaba como
         `numChannels < 1`: la rama de `preallocatedChannelSpace` era
         inalcanzable, el ctor de memoria externa pagaba un malloc por buffer y el
         constructor/asignacion de movimiento nunca copiaban los punteros al hueco
         propio. Aqui se recorre de 1 a 40 canales preguntando DONDE vive el array
         de punteros (dentro del objeto o en el heap) y se exige la misma frontera
         que juce::AudioBuffer; con el helper roto la frontera del port era 0.

      3. MEMORIA EXTERNA, MOVIMIENTO Y COPIA. Para un buffer que referencia
         canales del que llama: (a) tras moverse (constructor y asignacion) sigue
         escribiendo en esos canales y su array de punteros vive en el DESTINO,
         no en el origen; (b) el constructor de copia COMPARTE la memoria externa
         (documentado en JUCE) mientras que makeCopyOf() se queda con memoria
         propia. Los cuatro comportamientos se comparan con juce::AudioBuffer.

    Uso: NEURONiK_AudioBufferParityTest  (ctest lo ejecuta; no toma argumentos)

  ==============================================================================
*/

#include "DspCore/DspCore.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <utility>
#include <vector>

namespace {

constexpr int kChannels = 2;
constexpr int kSamples  = 512;
constexpr int kExternalSamples = 8;
constexpr int kMaxSweepChannels = 40;

int gFailures = 0;

void check (bool ok, const char* what)
{
    if (! ok)
    {
        std::printf ("[FAIL] %s\n", what);
        ++gFailures;
    }
}

//==============================================================================
/** LCG: la entrada del test no depende de rand() ni del reloj. */
inline uint32_t nextLcg (uint32_t& state) noexcept
{
    state = state * 1664525u + 1013904223u;
    return state;
}

/** Valor determinista por (canal, muestra). */
float inputValue (int channel, int sample) noexcept
{
    uint32_t state = 0x9e3779b9u ^ (uint32_t) (channel * 131 + sample);

    for (int i = 0; i < 3; ++i)
        nextLcg (state);

    return ((float) (nextLcg (state) >> 8) / (float) (1 << 24) - 0.5f) * 0.8f;
}

uint32_t bitsOf (float x) noexcept
{
    uint32_t u = 0;
    std::memcpy (&u, &x, sizeof (u));
    return u;
}

/** Distancia en ulps con el mapa monotono para floats con signo. */
uint32_t ulpDistance (float a, float b) noexcept
{
    const uint32_t ia = bitsOf (a);
    const uint32_t ib = bitsOf (b);
    const uint32_t ta = (ia & 0x80000000u) ? (0x80000000u - ia) : ia;
    const uint32_t tb = (ib & 0x80000000u) ? (0x80000000u - ib) : ib;
    return ta > tb ? ta - tb : tb - ta;
}

//==============================================================================
// 1. Paridad de datos: el mismo guion para los dos tipos.

struct Snapshot
{
    int channels = 0;
    int samples = 0;
    bool cleared = false;
    float magnitude = 0.0f;
    float rms = 0.0f;
    std::vector<float> left, right;
};

template <typename Buffer>
Snapshot runScript()
{
    Buffer buffer (kChannels, kSamples);
    buffer.clear();

    for (int ch = 0; ch < kChannels; ++ch)
        for (int i = 0; i < kSamples; ++i)
            buffer.setSample (ch, i, inputValue (ch, i));

    buffer.addSample (0, 7, 0.25f);
    buffer.applyGain (0, 0, kSamples, 0.5f);
    buffer.applyGainRamp (1, 0, kSamples, 0.0f, 1.0f);

    Buffer source (kChannels, 64);

    for (int ch = 0; ch < kChannels; ++ch)
        for (int i = 0; i < 64; ++i)
            source.setSample (ch, i, inputValue (10 + ch, i));

    buffer.addFrom (0, 64, source, 0, 0, 64, 0.25f);
    buffer.copyFrom (1, 64, source, 1, 0, 64);
    buffer.addFromWithRamp (0, 128, source.getReadPointer (0), 64, 0.0f, 1.0f);
    buffer.copyFromWithRamp (1, 128, source.getReadPointer (1), 64, 1.0f, 0.0f);
    buffer.clear (1, 0, 32);   // variante por canal: muestras 0..31 del canal 1

    // setSize con contenido que se conserva y con reutilizacion de memoria.
    buffer.setSize (kChannels, kSamples, true, true, true);
    buffer.reverse (0, 0, kSamples);
    buffer.reverse (1, 0, kSamples);

    Buffer copied;
    copied.makeCopyOf (buffer);

    Snapshot s;
    s.channels = copied.getNumChannels();
    s.samples = copied.getNumSamples();
    s.left.assign (copied.getReadPointer (0), copied.getReadPointer (0) + kSamples);
    s.right.assign (copied.getReadPointer (1), copied.getReadPointer (1) + kSamples);
    s.magnitude = copied.getMagnitude (0, 0, kSamples);
    s.rms = copied.getRMSLevel (1, 0, kSamples);
    s.cleared = copied.hasBeenCleared();
    return s;
}

void compareSnapshots (const char* label, const Snapshot& port, const Snapshot& reference)
{
    check (port.channels == reference.channels, "setSize/makeCopyOf: numero de canales");
    check (port.samples == reference.samples, "setSize/makeCopyOf: numero de muestras");
    check (port.cleared == reference.cleared, "hasBeenCleared() no coincide con juce::AudioBuffer");
    check (port.left.size() == reference.left.size() && port.right.size() == reference.right.size(),
           "longitud del render distinta");

    size_t mismatches = 0;
    size_t worst = 0;
    uint32_t worstUlp = 0;

    for (size_t i = 0; i < port.left.size() && i < reference.left.size(); ++i)
    {
        const uint32_t ulp = ulpDistance (port.left[i], reference.left[i]);

        if (ulp != 0) ++mismatches;
        if (ulp > worstUlp) { worstUlp = ulp; worst = i; }
    }

    for (size_t i = 0; i < port.right.size() && i < reference.right.size(); ++i)
    {
        const uint32_t ulp = ulpDistance (port.right[i], reference.right[i]);

        if (ulp != 0) ++mismatches;
        if (ulp > worstUlp) { worstUlp = ulp; worst = i; }
    }

    const uint32_t magnitudeUlp = ulpDistance (port.magnitude, reference.magnitude);
    const uint32_t rmsUlp       = ulpDistance (port.rms, reference.rms);

    std::printf ("[info] %-28s muestras=%zu maxUlp=%-3u (peor #%zu) magnitudUlp=%u rmsUlp=%u\n",
                 label, port.left.size() + port.right.size(), worstUlp, worst, magnitudeUlp, rmsUlp);

    check (mismatches == 0, "las muestras no coinciden bit a bit con juce::AudioBuffer");
    check (magnitudeUlp == 0, "getMagnitude no coincide con juce::AudioBuffer");
    check (rmsUlp == 0, "getRMSLevel no coincide con juce::AudioBuffer");
}

//==============================================================================
// 2. Donde vive el array de punteros (la rama que estuvo muerta).

/** Direccion del array de canales dentro del propio objeto (la rama de
    `preallocatedChannelSpace`) o fuera (array reservado en el heap). */
template <typename Buffer>
bool channelArrayIsInsideObject (const Buffer& buffer)
{
    const auto arrayAddress  = reinterpret_cast<std::uintptr_t> (buffer.getArrayOfReadPointers());
    const auto objectAddress = reinterpret_cast<std::uintptr_t> (&buffer);

    return arrayAddress >= objectAddress && arrayAddress < objectAddress + sizeof (Buffer);
}

/** Almacenamiento externo con `numChannels` arrays de 8 muestras y sus punteros. */
struct ExternalStorage
{
    explicit ExternalStorage (int numChannels)
        : storage ((size_t) numChannels, std::vector<float> (kExternalSamples, 0.0f))
    {
        for (auto& channel : storage)
            pointers.push_back (channel.data());
    }

    std::vector<std::vector<float>> storage;
    std::vector<float*> pointers;
};

template <typename Buffer>
bool externalChannelArrayIsInside (int numChannels)
{
    ExternalStorage channels (numChannels);
    Buffer buffer (channels.pointers.data(), numChannels, kExternalSamples);

    return channelArrayIsInsideObject (buffer);
}

/** Canal mas alto que todavia guarda el array de punteros DENTRO del objeto.
    En JUCE la condicion es `numChannels < 32`, o sea 31; con el helper
    devolviendo 1 la rama era inalcanzable y daba 0. */
template <typename Buffer>
int measurePreallocatedLimit()
{
    int lastInside = 0;

    for (int channels = 1; channels <= kMaxSweepChannels; ++channels)
        if (externalChannelArrayIsInside<Buffer> (channels))
            lastInside = channels;

    return lastInside;
}

//==============================================================================
// 3. Memoria externa, movimiento y copia.

/** Ctor y asignacion de movimiento de un buffer que referencia canales ajenos:
    el destino sigue escribiendo en esos canales y su array de punteros vive en
    el DESTINO (con la rama rota el destino aliasaba el array del origen). */
template <typename Buffer>
bool movedExternalBufferKeepsWorking (bool useMoveConstructor)
{
    ExternalStorage channels (2);

    std::optional<Buffer> moved;

    {
        Buffer source (channels.pointers.data(), 2, kExternalSamples);
        source.getWritePointer (1)[0] = 0.25f;

        if (useMoveConstructor)
            moved.emplace (std::move (source));
        else
            moved = std::move (source);
    }

    moved->getWritePointer (1)[0] = 0.75f;

    return moved->getNumChannels() == 2
        && moved->getNumSamples() == kExternalSamples
        && channels.storage[1][0] == 0.75f
        && channelArrayIsInsideObject (*moved);
}

/** Constructor de copia de un buffer de memoria externa: JUCE documenta que la
    copia COMPARTE esos canales (no se queda con memoria propia). */
template <typename Buffer>
bool copyOfExternalBufferSharesStorage()
{
    ExternalStorage channels (2);

    Buffer source (channels.pointers.data(), 2, kExternalSamples);
    source.clear();

    Buffer copy (source);
    copy.getWritePointer (0)[0] = 1.0f;

    return channels.storage[0][0] == 1.0f;
}

/** makeCopyOf() sobre un buffer de memoria externa: aqui SI se queda con memoria
    propia, de modo que escribir en la copia no toca los canales del que llama. */
template <typename Buffer>
bool makeCopyOfExternalBufferOwnsMemory()
{
    ExternalStorage channels (2);

    Buffer source (channels.pointers.data(), 2, kExternalSamples);
    source.clear();

    Buffer copy;
    copy.makeCopyOf (source);
    copy.getWritePointer (0)[0] = 1.0f;

    return channels.storage[0][0] == 0.0f && ! channelArrayIsInsideObject (copy);
}

} // namespace

//==============================================================================
int main()
{
    std::printf ("[info] AudioBufferParityTest: dsp::AudioBuffer contra juce::AudioBuffer\n");

    // 1. Paridad de datos.
    compareSnapshots ("guion de operaciones",
                      runScript<abd::dsp::AudioBuffer<float>>(),
                      runScript<juce::AudioBuffer<float>>());

    // 2. Estructura de punteros.
    const int portLimit = measurePreallocatedLimit<abd::dsp::AudioBuffer<float>>();
    const int juceLimit = measurePreallocatedLimit<juce::AudioBuffer<float>>();

    std::printf ("[info] frontera de preallocatedChannelSpace: dsp=%d juce=%d (JUCE: numChannels < 32)\n",
                 portLimit, juceLimit);

    check (portLimit == juceLimit,
           "la frontera de la rama preasignada no coincide con juce::AudioBuffer");
    check (portLimit == 31,
           "la rama preasignada no llega hasta los 31 canales que permite JUCE");

    // 3. Memoria externa, movimiento y copia.
    check (movedExternalBufferKeepsWorking<abd::dsp::AudioBuffer<float>> (true)
             == movedExternalBufferKeepsWorking<juce::AudioBuffer<float>> (true),
           "mover (constructor) un buffer de memoria externa difiere de juce::AudioBuffer");

    check (movedExternalBufferKeepsWorking<abd::dsp::AudioBuffer<float>> (false)
             == movedExternalBufferKeepsWorking<juce::AudioBuffer<float>> (false),
           "mover (asignacion) un buffer de memoria externa difiere de juce::AudioBuffer");

    check (movedExternalBufferKeepsWorking<abd::dsp::AudioBuffer<float>> (true),
           "tras mover, el buffer de memoria externa no escribe en los canales del que llama");

    check (copyOfExternalBufferSharesStorage<abd::dsp::AudioBuffer<float>>()
             == copyOfExternalBufferSharesStorage<juce::AudioBuffer<float>>(),
           "el constructor de copia de un buffer de memoria externa difiere de juce::AudioBuffer");

    check (makeCopyOfExternalBufferOwnsMemory<abd::dsp::AudioBuffer<float>>()
             == makeCopyOfExternalBufferOwnsMemory<juce::AudioBuffer<float>>(),
           "makeCopyOf sobre memoria externa difiere de juce::AudioBuffer");

    check (makeCopyOfExternalBufferOwnsMemory<abd::dsp::AudioBuffer<float>>(),
           "makeCopyOf no se quedo con memoria propia");

    if (gFailures == 0)
    {
        std::printf ("[OK] dsp::AudioBuffer: paridad con juce::AudioBuffer\n");
        return 0;
    }

    std::printf ("[FALLO] %d comprobacion(es)\n", gFailures);
    return 1;
}
