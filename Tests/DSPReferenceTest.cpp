#include "../Source/DSP/CoreModules/NeuronikEngine.h"
#include "../Source/DSP/Runtime/DspEngineFacade.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    // ─── Fase 1: paridad de la frontera ───
    // La ruta JUCE directa (renderNextBlock con MidiBuffer) y la ruta fachada
    // (punteros crudos + Runtime::Event) deben producir EXACTAMENTE el mismo
    // audio: mismas llamadas internas, mismos datos. El DSP con parámetros por
    // defecto es determinista (semillas constantes, entropy=0), así que la
    // comparación es muestra a muestra, sin tolerancia.
    struct NotePlan
    {
        int note = 60;
        float velocity = 100.0f / 127.0f;
        int sustainBlocks = 8;
        int releaseBlocks = 12;
    };

    bool renderDirectRoute(const NotePlan& plan, double sampleRate, int blockSize,
                           std::vector<float>& outLeft, std::vector<float>& outRight)
    {
        NEURONiK::DSP::NeuronikEngine engine;
        engine.prepare(sampleRate, blockSize);

        juce::AudioBuffer<float> buffer(2, blockSize);
        juce::MidiBuffer midi;

        // Frontera dsp::AudioBuffer <-> juce::AudioBuffer (motor sin JUCE):
        // vista zero-copy de los mismos canales (patron DspEngineFacade::process).
        dsp::AudioBuffer<float> dspBufferView (buffer.getArrayOfWritePointers(),
                                               buffer.getNumChannels(),
                                               buffer.getNumSamples());

        midi.addEvent(juce::MidiMessage::noteOn(1, plan.note, plan.velocity), 0);
        // IMPORTANTE: limpiar a traves de la VISTA, no del buffer JUCE. El motor
        // suma en el buffer (addFrom) y escribe via la vista: si limpiamos con
        // buffer.clear(), el flag isClear del buffer JUCE queda true tras el
        // primer ciclo y los clear() siguientes son NO-OP (las escrituras por la
        // vista no lo invalidan) => el bloque anterior se suma al siguiente.
        // Limpiando por la vista, el flag y la memoria viven en el mismo objeto.
        dspBufferView.clear();
        engine.renderNextBlock(dspBufferView, midi);

        for (int b = 0; b < plan.sustainBlocks + plan.releaseBlocks; ++b)
        {
            midi.clear();
            if (b == plan.sustainBlocks - 1)
                midi.addEvent(juce::MidiMessage::noteOff(1, plan.note, plan.velocity), 0);
            dspBufferView.clear();
            engine.renderNextBlock(dspBufferView, midi);
        }

        outLeft.assign(buffer.getReadPointer(0), buffer.getReadPointer(0) + buffer.getNumSamples());
        outRight.assign(buffer.getReadPointer(1), buffer.getReadPointer(1) + buffer.getNumSamples());
        return true;
    }

    bool renderFacadeRoute(const NotePlan& plan, double sampleRate, int blockSize,
                           std::vector<float>& outLeft, std::vector<float>& outRight)
    {
        NEURONiK::DSP::NeuronikEngine engine;
        NEURONiK::DSP::Runtime::DspEngineFacade facade(engine);
        facade.prepare(sampleRate, blockSize);

        juce::AudioBuffer<float> buffer(2, blockSize);
        float* left = buffer.getWritePointer(0);
        float* right = buffer.getWritePointer(1);

        const NEURONiK::DSP::Runtime::Event noteOn {
            NEURONiK::DSP::Runtime::EventType::NoteOn,
            1,
            plan.note,
            8192,
            plan.velocity,
            0
        };
        const NEURONiK::DSP::Runtime::Event noteOff {
            NEURONiK::DSP::Runtime::EventType::NoteOff,
            1,
            plan.note,
            8192,
            plan.velocity,
            0
        };

        facade.process(left, right, blockSize, &noteOn, 1);
        for (int b = 0; b < plan.sustainBlocks + plan.releaseBlocks; ++b)
        {
            if (b == plan.sustainBlocks - 1)
                facade.process(left, right, blockSize, &noteOff, 1);
            else
                facade.process(left, right, blockSize, nullptr, 0);
        }

        outLeft.assign(left, left + blockSize);
        outRight.assign(right, right + blockSize);
        return true;
    }

    bool buffersEqual(const std::vector<float>& a, const std::vector<float>& b,
                      const char* channelName, std::string& problem)
    {
        if (a.size() != b.size())
        {
            problem = std::string(channelName) + ": size mismatch";
            return false;
        }
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (a[i] != b[i])  // bit-exacto a propósito; sin tolerancia
            {
                problem = channelName + std::string(": first mismatch at sample ")
                          + std::to_string(i) + " (direct=" + std::to_string(a[i])
                          + ", facade=" + std::to_string(b[i]) + ")";
                return false;
            }
        }
        return true;
    }

    bool hasAudio(const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;
        double energy = 0.0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float value = data[sample];
                peak = std::max(peak, std::abs(value));
                energy += static_cast<double>(value) * static_cast<double>(value);
            }
        }

        const double rms = std::sqrt(energy / static_cast<double>(buffer.getNumChannels() * buffer.getNumSamples()));
        std::cout << "NEURONiK DSP reference: peak=" << peak << " rms=" << rms << '\n';
        return peak > 1.0e-5f && rms > 1.0e-6;
    }

}

int main()
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    NEURONiK::DSP::NeuronikEngine engine;
    NEURONiK::DSP::Runtime::DspEngineFacade facade(engine);
    facade.prepare(sampleRate, blockSize);

    juce::AudioBuffer<float> buffer(2, blockSize);
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    const NEURONiK::DSP::Runtime::Event noteOn {
        NEURONiK::DSP::Runtime::EventType::NoteOn,
        1,
        60,
        8192,
        100.0f / 127.0f,
        0
    };

    facade.process(left, right, blockSize, &noteOn, 1);

    if (!hasAudio(buffer))
    {
        std::cerr << "NEURONiK DSP reference test failed: rendered silence\n";
        return EXIT_FAILURE;
    }

    if (facade.getNumActiveVoices() <= 0)
    {
        std::cerr << "NEURONiK DSP reference test failed: no active voice\n";
        return EXIT_FAILURE;
    }

    // allNotesOff is the panic used when the MIDI channel changes. It releases the
    // voices instead of resetting them, so the tail has to finish on its own.
    facade.allNotesOff();

    if (facade.getNumActiveVoices() <= 0)
    {
        std::cerr << "NEURONiK DSP reference test failed: allNotesOff killed the voices "
                     "instead of releasing them\n";
        return EXIT_FAILURE;
    }

    // The envelope is decayed multiplicatively (1/e per release time) and only goes
    // idle below 1e-4, so the 500 ms default release needs about 4.6 s to finish.
    // Render until it does, with a generous ceiling.
    const int maxBlocks = static_cast<int>(sampleRate * 8.0 / blockSize);
    int renderedBlocks = 0;

    while (renderedBlocks < maxBlocks && facade.getNumActiveVoices() != 0)
    {
        buffer.clear();
        facade.process(left, right, blockSize, nullptr, 0);
        ++renderedBlocks;
    }

    if (facade.getNumActiveVoices() != 0)
    {
        std::cerr << "NEURONiK DSP reference test failed: " << facade.getNumActiveVoices()
                  << " voice(s) still active after " << (renderedBlocks * blockSize / sampleRate)
                  << " s of release\n";
        return EXIT_FAILURE;
    }

    std::cout << "NEURONiK allNotesOff: release finished after "
              << (renderedBlocks * blockSize / sampleRate) << " s of rendering\n";

    // ─── Fase 1: la ruta fachada no cambia el resultado sonoro ───
    {
        const NotePlan plan;
        std::vector<float> directL, directR, facadeL, facadeR;
        renderDirectRoute(plan, sampleRate, blockSize, directL, directR);
        renderFacadeRoute(plan, sampleRate, blockSize, facadeL, facadeR);

        std::string problem;
        if (!buffersEqual(directL, facadeL, "L", problem)
            || !buffersEqual(directR, facadeR, "R", problem))
        {
            std::cerr << "NEURONiK DSP reference test failed: facade route differs\n"
                      << "  " << problem << '\n';
            return EXIT_FAILURE;
        }

        float peak = 0.0f;
        for (size_t i = 0; i < directL.size(); ++i)
            peak = std::max({ peak, std::abs(directL[i]), std::abs(directR[i]) });
        if (peak <= 1.0e-5f)
        {
            std::cerr << "NEURONiK DSP reference test failed: parity render is silence "
                         "(the comparison proves nothing)\n";
            return EXIT_FAILURE;
        }

        std::cout << "NEURONiK facade parity: bit-exact across " << directL.size()
                  << " samples/channel (peak=" << peak << ")\n";
    }

    std::cout << "NEURONiK DSP reference test passed\n";
    return EXIT_SUCCESS;
}
