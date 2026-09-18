#include "DspCore.h"
#include "DspEngineFacade.h"

#include "../ISynthesisEngine.h"

namespace NEURONiK::DSP::Runtime
{
    namespace
    {
        // Event -> dsp::MidiMessage. Sin JUCE: la fachada sigue siendo una
        // frontera sin juce_*. El note on/off usa la factoria float (misma
        // cuantizacion de velocity que antes, dsp::roundToInt) y el resto usa
        // el truncado explicito de siempre.
        dsp::MidiMessage toMidiMessage(const Event& event)
        {
            const int channel = dsp::jlimit(1, 16, event.channel);

            switch (event.type)
            {
                case EventType::NoteOn:
                    return dsp::MidiMessage::noteOn(channel,
                                                    dsp::jlimit(0, 127, event.note),
                                                    dsp::jlimit(0.0f, 1.0f, event.value));

                case EventType::NoteOff:
                    return dsp::MidiMessage::noteOff(channel,
                                                     dsp::jlimit(0, 127, event.note),
                                                     dsp::jlimit(0.0f, 1.0f, event.value));

                case EventType::PitchBend:
                    return dsp::MidiMessage::pitchWheel(channel, dsp::jlimit(0, 16383, event.value14));

                case EventType::ChannelPressure:
                    return dsp::MidiMessage::channelPressureChange(channel,
                                                              dsp::jlimit(0, 127,
                                                                           static_cast<int>(event.value * 127.0f)));

                case EventType::PolyAftertouch:
                    return dsp::MidiMessage::aftertouchChange(channel,
                                                              dsp::jlimit(0, 127, event.note),
                                                              dsp::jlimit(0, 127,
                                                                           static_cast<int>(event.value * 127.0f)));

                case EventType::Timbre:
                    return dsp::MidiMessage::controllerEvent(channel,
                                                              74,
                                                              dsp::jlimit(0, 127,
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

        dsp::AudioBuffer<float> buffer;
        float* channels[] = { left, right };
        buffer.setDataToReferTo(channels, 2, numSamples);
        buffer.clear();

        midiBuffer.clear();
        if (events != nullptr && eventCount > 0)
        {
            for (int i = 0; i < eventCount; ++i)
            {
                const auto& event = events[i];
                midiBuffer.addEvent(toMidiMessage(event), dsp::jlimit(0, numSamples - 1, event.sampleOffset));
            }
        }

        engine.renderNextBlock(buffer, midiBuffer);
    }

    void DspEngineFacade::reset()
    {
        engine.reset();
    }

    void DspEngineFacade::setGlobalParams(const NEURONiK::DSP::GlobalParams& params)
    {
        engine.setGlobalParams(params);
    }

    void DspEngineFacade::setPolyphony(int numVoices)
    {
        engine.setPolyphony(numVoices);
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
