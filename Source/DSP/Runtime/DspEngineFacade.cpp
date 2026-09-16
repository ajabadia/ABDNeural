#include "DspEngineFacade.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace NEURONiK::DSP::Runtime
{
    namespace
    {
        juce::MidiMessage toMidiMessage(const Event& event)
        {
            const int channel = juce::jlimit(1, 16, event.channel);

            switch (event.type)
            {
                case EventType::NoteOn:
                    return juce::MidiMessage::noteOn(channel,
                                                     juce::jlimit(0, 127, event.note),
                                                     juce::jlimit(0.0f, 1.0f, event.value));

                case EventType::NoteOff:
                    return juce::MidiMessage::noteOff(channel,
                                                      juce::jlimit(0, 127, event.note),
                                                      juce::jlimit(0.0f, 1.0f, event.value));

                case EventType::PitchBend:
                    return juce::MidiMessage::pitchWheel(channel, juce::jlimit(0, 16383, event.value14));

                case EventType::ChannelPressure:
                    return juce::MidiMessage::channelPressureChange(channel,
                                                              juce::jlimit(0, 127,
                                                                           static_cast<int>(event.value * 127.0f)));

                case EventType::PolyAftertouch:
                    return juce::MidiMessage::aftertouchChange(channel,
                                                               juce::jlimit(0, 127, event.note),
                                                               juce::jlimit(0, 127,
                                                                            static_cast<int>(event.value * 127.0f)));

                case EventType::Timbre:
                    return juce::MidiMessage::controllerEvent(channel,
                                                               74,
                                                               juce::jlimit(0, 127,
                                                                            static_cast<int>(event.value * 127.0f)));
            }

            return {};
        }
    }

    DspEngineFacade::DspEngineFacade(NEURONiK::DSP::ISynthesisEngine& engineToWrap) noexcept
        : engine(engineToWrap)
    {
    }

    void DspEngineFacade::prepare(double sampleRate, int samplesPerBlock)
    {
        engine.prepare(sampleRate, samplesPerBlock);
    }

    void DspEngineFacade::process(float* left,
                                  float* right,
                                  int numSamples,
                                  const Event* events,
                                  int eventCount)
    {
        if (left == nullptr || right == nullptr || numSamples <= 0)
            return;

        juce::AudioBuffer<float> buffer;
        float* channels[] = { left, right };
        buffer.setDataToReferTo(channels, 2, numSamples);
        buffer.clear();

        juce::MidiBuffer midi;
        if (events != nullptr && eventCount > 0)
        {
            for (int i = 0; i < eventCount; ++i)
            {
                const auto& event = events[i];
                midi.addEvent(toMidiMessage(event), juce::jlimit(0, numSamples - 1, event.sampleOffset));
            }
        }

        engine.renderNextBlock(buffer, midi);
    }

    void DspEngineFacade::reset()
    {
        engine.reset();
    }

    void DspEngineFacade::allNotesOff()
    {
        engine.allNotesOff();
    }

    int DspEngineFacade::getNumActiveVoices() const
    {
        return engine.getNumActiveVoices();
    }
}
