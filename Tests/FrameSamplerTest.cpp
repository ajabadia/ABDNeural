/*
  ==============================================================================

    FrameSamplerTest.cpp
    Prueba de la FASE 10 (modelos temporales): sampleFrame + morphZ de motor.

    Propiedades fijadas:
      1. Modelo estatico (frameCount=1): sampleFrame es IDENTIDAD exacta y
         morphZ no cambia la salida (el knob es inertel — paridad por diseño).
      2. Modelo de 2 frames: z=0 reproduce el frame 0 BIT-EXACTO, z=1 el
         frame 1, z=0.5 el punto medio aritmetico.
      3. El motor (NeuronikEngine real, 44100/128) con morphZ=0 y morphZ=1
         rinde el espectro del frame correspondiente (partials table).
      4. La envoltura de offsets: medio espaciado unitario.

  ==============================================================================
*/

#include "../Source/DSP/CoreModules/NeuronikEngine.h"
#include "../Source/DSP/CoreModules/NeurotikEngine.h"
#include "../Source/Common/SpectralModel.h"
#include "../Source/DSP/FrameSampler.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <iostream>

using NEURONiK::Common::SpectralModel;

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  [ok]   " : "  [FALLO] ") << what << std::endl;
    if (! ok) ++failures;
}

SpectralModel staticSine()
{
    SpectralModel m;
    m.amplitudes.fill (0.0f);
    m.frequencyOffsets.fill (0.0f);
    m.amplitudes[0] = 1.0f;              // seno puro
    m.isValid = true;
    return m;
}

/** Modelo de 2 frames: frame 0 = seno, frame 1 = seno con el parcial 2 a 0.5. */
SpectralModel twoFrameModel()
{
    SpectralModel m = staticSine();
    m.extraAmps[0].fill (0.0f);
    m.extraOffsets[0].fill (0.0f);
    m.extraAmps[0][1] = 0.5f;            // frame 1: aparece el 2o parcial
    m.frameCount = 2;
    return m;
}

/** noteOn + N bloques con la nota sostenida => el smoother de morphZ (glide
 * de 20 ms por diseno) se asienta; lee la tabla de parciales ya estable. */
std::array<float, 64> partialsOf (NEURONiK::DSP::ISynthesisEngine& engine, int note = 60)
{
    for (int b = 0; b < 12; ++b)
    {
        dsp::AudioBuffer<float> buffer (2, 128);
        buffer.clear();
        dsp::MidiBuffer midi;
        if (b == 0)
            midi.addEvent (dsp::MidiMessage::noteOn (1, note, 1.0f), 0);
        engine.renderNextBlock (buffer, midi);
    }

    std::array<float, 64> out {};
    engine.getSpectralData (out.data());
    return out;
}

/** noteOff + tail para liberar la voz antes de la siguiente medida. */
void releaseVoices (NEURONiK::DSP::ISynthesisEngine& engine, int note = 60)
{
    dsp::AudioBuffer<float> buffer (2, 128);
    buffer.clear();
    dsp::MidiBuffer midi;
    midi.addEvent (dsp::MidiMessage::noteOff (1, note, 0.0f), 0);
    for (int b = 0; b < 8; ++b)
    {
        dsp::MidiBuffer empty;
        engine.renderNextBlock (buffer, empty);
    }
}

} // namespace

int main()
{
    std::cout << "sampleFrame + morphZ (FASE 10)" << std::endl;

    // ---------- 1. Estatico: identidad e inercia ----------
    {
        const auto s = staticSine();
        SpectralModel out;
        NEURONiK::Common::sampleFrame (s, 0.73f, out);

        bool same = out.frameCount == 1 && out.isValid
                    && out.amplitudes[0] == 1.0f && out.amplitudes[1] == 0.0f;
        check (same, "estatico: sampleFrame es identidad exacta");
    }

    // ---------- 2. Dos frames: extremos y punto medio ----------
    {
        const auto m = twoFrameModel();
        SpectralModel out;

        NEURONiK::Common::sampleFrame (m, 0.0f, out);
        check (out.amplitudes[0] == 1.0f && out.amplitudes[1] == 0.0f,
               "z=0: frame 0 bit-exacto");

        NEURONiK::Common::sampleFrame (m, 1.0f, out);
        check (out.amplitudes[0] == 0.0f && out.amplitudes[1] == 0.5f,
               "z=1: frame 1 bit-exacto (seno apagado + parcial 2 a 0.5)");

        NEURONiK::Common::sampleFrame (m, 0.5f, out);
        check (std::abs (out.amplitudes[1] - 0.25f) < 1e-6f,
               "z=0.5: punto medio aritmetico");
    }

    // ---------- 3. Envoltura de offsets (medio espaciado unitario) ----------
    {
        SpectralModel m = twoFrameModel();
        // parcial 1 (n=1): offset frame0 = +0.4, frame1 = -0.4 -> d=-0.8 cruza
        // el medio espaciado (0.5): envuelve por +1 => +0.2 (camino corto).
        m.frequencyOffsets[0] = 0.4f;
        m.extraOffsets[0][0] = -0.4f;

        SpectralModel out;
        NEURONiK::Common::sampleFrame (m, 1.0f, out);
        check (std::abs (out.frequencyOffsets[0] - (-0.4f)) < 1e-6f,
               "z=1: extremo bit-exacto del frame almacenado (-0.4)");

        NEURONiK::Common::sampleFrame (m, 0.5f, out);
        check (std::abs (out.frequencyOffsets[0] - 0.5f) < 1e-6f,
               "z=0.5: camino corto cruza por 0.5 (no por -0.8/2)");
    }

    // ---------- 4. Motor aditivo: morphZ muda el espectro ----------
    {
        NEURONiK::DSP::NeuronikEngine engine;
        engine.prepare (44100.0, 128);
        engine.loadModel (twoFrameModel(), 0);

        NEURONiK::DSP::Synthesis::AdditiveVoice::Params p;
        // Sostenida durante la medida (el glide de morphZ necesita ~20 ms con
        // la voz viva) y muerte rapida al soltar: release 10 ms cae dentro de
        // releaseVoices (23 ms) para que la siguiente medida no herede la voz.
        p.attack = 1.0f; p.decay = 1000.0f; p.sustain = 0.7f; p.release = 10.0f;
        // Eje Z aislado: morphX=morphY=0 => el morfeo bilineal usa SOLO el slot
        // A (los otros slots llevan los modelos por defecto del engine).
        p.morphX = 0.0f; p.morphY = 0.0f;
        engine.setVoiceParams (p);

        const auto at = [&engine, &p] (float z)
        {
            p.morphZ = z;
            engine.setVoiceParams (p);
            engine.updateParameters();
            return partialsOf (engine);
        };

        const auto f0 = at (0.0f);
        releaseVoices (engine);
        const auto f1 = at (1.0f);
        releaseVoices (engine);

        check (std::abs (f0[1]) < 1e-6f, "motor z=0: parcial 2 mudo (frame 0)");
        check (f1[1] > 0.05f, "motor z=1: parcial 2 canta (frame 1)");

        // Normalizado: manda el parcial dominante de CADA frame.
        check (f0[0] > f0[1] && f1[1] > f1[0],
               "motor: la normalizacion sigue al frame dominante");
    }

    // ---------- 5. Motor modal: morphZ tambien muda el banco ----------
    {
        NEURONiK::DSP::NeurotikEngine engine;
        engine.prepare (44100.0, 128);
        engine.loadModel (twoFrameModel(), 0);

        NEURONiK::DSP::Synthesis::NeurotikVoice::Params p;
        p.impulseMix = 1.0f;                 // excitacion percusiva clasica
        p.attack = 1.0f; p.decay = 1000.0f; p.sustain = 0.7f; p.release = 10.0f;
        p.morphX = 0.0f; p.morphY = 0.0f;    // eje Z aislado (solo slot A)
        engine.setVoiceParams (p);

        const auto at = [&engine, &p] (float z)
        {
            p.morphZ = z;
            engine.setVoiceParams (p);
            engine.updateParameters();
            return partialsOf (engine);
        };

        const auto f0 = at (0.0f);
        releaseVoices (engine);
        const auto f1 = at (1.0f);
        releaseVoices (engine);

        check (std::abs (f0[1]) < 1e-6f, "banco z=0: resonador 2 mudo (frame 0)");
        check (f1[1] > 0.05f, "banco z=1: resonador 2 canta (frame 1)");
    }

    std::cout << (failures == 0 ? "RESULT: OK (0 fallos)" 
                                : "RESULT: FALLO (" + juce::String (failures) + ")")
              << std::endl;
    return failures == 0 ? 0 : 1;
}
