/*
  ==============================================================================

    Chorus.h
    Created: 26 Jan 2026
    Description: Simple stereo chorus effect using modulated delay lines.

  ==============================================================================
 */

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace NEURONiK::DSP::Effects {

class Chorus
{
public:
    Chorus() : delayBuffer(2, 4096)
    {
        delayBuffer.clear();
    }

    void prepare(double sampleRate)
    {
        currentSampleRate = sampleRate;
        delayBuffer.setSize(2, static_cast<int>(sampleRate * 0.1)); // 100ms max delay
        delayBuffer.clear();
        phase = 0.0f;
    }

    void setParameters(float rateHz, float depth, float mix) noexcept
    {
        rate = rateHz;
        depthValue = depth;
        mixValue = mix;
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const int bufferSize = delayBuffer.getNumSamples();

        float phaseInc = juce::MathConstants<float>::twoPi * rate / static_cast<float>(currentSampleRate);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Modulation: LFO between 5ms and 30ms
            float mod = (std::sin(phase) + 1.0f) * 0.5f; // 0 to 1
            float delaySamples = (0.005f + mod * 0.025f * depthValue) * static_cast<float>(currentSampleRate);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                float inputSample = buffer.getReadPointer(channel)[sample];

                // Write input to delay buffer
                delayBuffer.setSample(channel % 2, writePos, inputSample);

                // Read modulated position
                float readPos = static_cast<float>(writePos) - delaySamples;
                if (readPos < 0) readPos += static_cast<float>(bufferSize);

                int index1 = static_cast<int>(readPos);
                int index2 = (index1 + 1) % bufferSize;
                float fraction = readPos - static_cast<float>(index1);

                float delayedSample = (1.0f - fraction) * delayBuffer.getSample(channel % 2, index1) +
                                      fraction * delayBuffer.getSample(channel % 2, index2);

                // Mix
                float output = (inputSample * (1.0f - mixValue * 0.5f)) + (delayedSample * mixValue * 0.5f);
                buffer.getWritePointer(channel)[sample] = output;
            }

            phase += phaseInc;
            if (phase >= juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;

            if (++writePos >= bufferSize) writePos = 0;
        }
    }

    void reset()
    {
        delayBuffer.clear();
    }

private:
    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    float phase = 0.0f;
    float rate = 1.0f;
    float depthValue = 0.2f;
    float mixValue = 0.0f;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Chorus)
};

} // namespace NEURONiK::DSP::Effects
