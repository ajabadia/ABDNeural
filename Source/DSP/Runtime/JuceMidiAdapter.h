/*
  ==============================================================================

    JuceMidiAdapter.h
    Frontera MIDI juce::MidiBuffer -> dsp::MidiBuffer (Fase 1, paso 4/6).

    Este es el unico sitio donde el transporte MIDI de JUCE toca al motor:
    el motor solo entiende dsp::MidiMessage/dsp::MidiBuffer (sin JUCE). Los
    hosts JUCE (el procesador del plugin, los tests nativos) traducen aqui.

    SOLO para hosts JUCE: la build WASM no lo incluye ni lo compila (usa
    Runtime::Event + DspEngineFacade). El #error de abajo lo hace explicito
    en vez de dejar que un include accidental meta JUCE en el modulo WASM.

    Politica de traduccion (decisiva para no cambiar el audio):
      - Se copian los bytes CRUDOS tal cual: cero re-cuantizacion, la velocity
        que ya viaja en el mensaje JUCE llega identica al motor.
      - Se copia el subconjunto que el motor interpreta: mensajes de canal/voz
        de 1 a 3 bytes (status 0x80..0xEF). SysEx, meta-eventos y realtime
        (0xF0..0xFF) se descartan: el motor nunca los mira y dsp::MidiMessage
        no los modela.
      - Las posiciones dentro del bloque y el ORDEN se conservan (addEvent de
        dsp::MidiBuffer inserta ordenado igual que juce::MidiBuffer, empates
        incluidos).

  ==============================================================================
*/

#pragma once

#if defined (__EMSCRIPTEN__)
  #error "JuceMidiAdapter es la frontera JUCE: la build WASM no lo compila (usa Runtime::Event + DspEngineFacade)."
#endif

#include <juce_audio_basics/juce_audio_basics.h>

#include "../DspMidiBuffer.h"

namespace NEURONiK::DSP::Runtime
{
    /** Numero maximo de bytes que el motor interpreta (mensajes de canal y voz). */
    inline constexpr int kEngineMidiMaxBytes = 3;

    /** true si el evento pertenece al contrato del motor: status >= 0x80 y
        familia de canal (0x80..0xEF), con 1 a 3 bytes. */
    inline bool isEngineMidiEvent (const uint8_t* data, int numBytes) noexcept
    {
        if (data == nullptr || numBytes <= 0 || numBytes > kEngineMidiMaxBytes)
            return false;

        return (data[0] & 0x80) != 0 && (data[0] & 0xf0) != 0xf0;
    }

    /** Igual que isEngineMidiEvent, partiendo de un mensaje de JUCE. */
    inline bool isEngineMidiMessage (const juce::MidiMessage& message) noexcept
    {
        return isEngineMidiEvent (message.getRawData(), message.getRawDataSize());
    }

    /** Copia el subconjunto de canal/voz de un juce::MidiBuffer al dsp::MidiBuffer.

        destination se limpia y se reserva de antemano: en regimen estable la
        traduccion no asigna memoria en el hilo de audio (reutiliza el mismo
        objeto entre bloques). Devuelve el numero de eventos copiados. */
    inline int copyToDspMidiBuffer (const juce::MidiBuffer& source, dsp::MidiBuffer& destination)
    {
        destination.clear();
        destination.ensureSize (source.getNumEvents() * (dsp::MidiBuffer::kEventHeaderSize + kEngineMidiMaxBytes) + 64);

        int copied = 0;

        for (const auto metadata : source)
        {
            if (! isEngineMidiEvent (metadata.data, metadata.numBytes))
                continue;

            destination.addEvent (metadata.data, metadata.numBytes, metadata.samplePosition);
            ++copied;
        }

        return copied;
    }
}
