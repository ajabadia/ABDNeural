#pragma once

#include "../ISynthesisEngine.h"
#include "DspEvent.h"

namespace NEURONiK::DSP::Runtime
{
    /**
     * Transitional boundary between hosts and the current JUCE-backed engine.
     *
     * The implementation still uses JUCE internally. Hosts outside JUCE only
     * need to provide interleaved-independent stereo channel pointers and
     * Runtime::Event values.
     */
    class DspEngineFacade
    {
    public:
        explicit DspEngineFacade(NEURONiK::DSP::ISynthesisEngine& engine) noexcept;

        void prepare(double sampleRate, int samplesPerBlock);

        void process(float* left,
                     float* right,
                     int numSamples,
                     const Event* events,
                     int eventCount);

        void reset();

        /**
         * Releases every sounding voice. Used as a panic and when the MIDI channel
         * changes, so no note is left hanging when the input source disappears.
         */
        void allNotesOff();

        int getNumActiveVoices() const;

    private:
        NEURONiK::DSP::ISynthesisEngine& engine;
    };
}
