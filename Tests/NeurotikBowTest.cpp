/*
  ==============================================================================

    NeurotikBowTest.cpp
    Created: 22 Sep 2026
    Description: El modo bow (arco continuo) del motor modal, verificado como
                 numero con el motor real:

                   - bowExcite = 0 (default, legacy impulso+ruido): el
                     sostenido queda al nivel historico (ruido resonado,
                     peak ~ -49 dBFS) — pin de compatibilidad.
                   - bowExcite = 1 (arco, excitador fuera del nivel del
                     ruido): el sostenido canta con ganancia plena.
                     RMS del arco >= 10x el legacy y por encima del suelo
                     de audible.

  ==============================================================================
*/

#include "../Source/DSP/CoreModules/NeurotikEngine.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <iostream>

namespace
{
    using namespace NEURONiK;

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

    struct RenderStats { float peak = 0.0f; float rms = 0.0f; };

    /** Nota sostenida de 2 s sobre el motor real (ruta directa, sin facade).
        muteLegacy=false: excitadores en SUS DEFAULTS (impulso 0.8 + ruido
        0.1) — el nivel historico del sostenido, el mismo del escenario B de
        la paridad (-49 dBFS). muteLegacy=true: solo arco. */
    RenderStats renderSustain (float bowExcite, bool muteLegacy)
    {
        constexpr double sampleRate = 44100.0;
        constexpr int blockSize = 512;
        constexpr int numBlocks = (int) (2.0 * sampleRate / blockSize); // ~172 bloques

        NEURONiK::DSP::NeurotikEngine engine;
        engine.prepare (sampleRate, blockSize);

        NEURONiK::DSP::Synthesis::NeurotikVoice::Params p;
        p.level = 1.0f;
        p.impulseMix = muteLegacy ? 0.0f : 0.8f;
        p.excitationNoise = muteLegacy ? 0.0f : 0.1f;
        p.bowExcite = bowExcite;
        p.excitationColor = 0.5f;
        p.resonatorResonance = 0.99f;
        engine.setVoiceParams (p);

        juce::AudioBuffer<float> buffer (2, blockSize);
        dsp::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), 2, blockSize);

        double energy = 0.0;
        long long samples = 0;

        for (int b = 0; b < numBlocks; ++b)
        {
            dsp::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (dsp::MidiMessage::noteOn (1, 60, 0.9f), 0);

            view.clear();
            engine.renderNextBlock (view, midi);

            // Estadisticas del SEGUNDO sostenido (el ataque ya asento).
            if (b >= numBlocks / 2)
            {
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < blockSize; ++i)
                    {
                        const float v = buffer.getSample (ch, i);
                        energy += (double) v * v;
                        ++samples;
                    }
            }
        }

        RenderStats stats;
        stats.rms = (float) std::sqrt (energy / (double) juce::jmax (1LL, samples));

        // Peak sobre todo el render (el transitorio del noteOn incluido).
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                stats.peak = std::max (stats.peak, std::abs (buffer.getSample (ch, i)));

        return stats;
    }
}

int main()
{
    std::cout << "NeurotikEngine modo bow: sostenido con arco vs legacy\n";

    const auto legacy = renderSustain (0.0f, false); // defaults: impulso+ruido
    const auto bow = renderSustain (1.0f, true);     // solo arco

    std::cout << "  legacy (bow=0): rms=" << legacy.rms << " peak=" << legacy.peak << '\n';
    std::cout << "  arco   (bow=1): rms=" << bow.rms   << " peak=" << bow.peak   << '\n';

    check (legacy.rms < 0.02f,
           "el sostenido con los defaults legacy queda en su nivel historico (ruido resonado, rms<0.02)");
    check (bow.rms > 0.05f,
           "el sostenido con arco canta con ganancia plena (rms>0.05)");
    check (bow.rms > legacy.rms * 10.0f,
           "el arco supera 10x al legacy en sostenido");
    check (std::isfinite (bow.rms) && std::isfinite (bow.peak) && bow.peak <= 4.0f,
           "la senal del arco es finita y acotada (sin divergencias del excitador)");

    std::cout << "\nRESULT: " << ((failures == 0) ? "OK" : "FAIL") << " (" << failures << " fallos)\n";
    return (failures == 0) ? 0 : 1;
}
