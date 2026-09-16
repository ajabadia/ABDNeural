#include "../Source/DSP/CoreModules/NeuronikEngine.h"
#include "../Source/DSP/Runtime/DspEngineFacade.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
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

    std::cout << "NEURONiK DSP reference test passed\n";
    return EXIT_SUCCESS;
}
