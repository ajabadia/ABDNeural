/*
  ==============================================================================

    Delay.h
    Created: 22 Jan 2026
    Description: Stereo feedback delay with circular buffer and smoothing.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace NEURONiK::DSP::Effects {

/**
 * A simple stereo feedback delay.
 *
 * Thread-safety: processBlock is real-time safe.
 */
class Delay
{
public:
    Delay() : delayBuffer(2, 96000) // Default 2s @ 48kHz
    {
        delayBuffer.clear();
    }

    void prepare(double sampleRate, int maxDelaySamples)
    {
        currentSampleRate = sampleRate;
        delayBuffer.setSize(2, maxDelaySamples + 1024);
        delayBuffer.clear();
        writePos = 0;
    }

    void setParameters(float timeInSeconds, float feedback) noexcept
    {
        targetDelayInSamples = timeInSeconds * static_cast<float>(currentSampleRate);
        feedbackGain = juce::jlimit(0.0f, 0.95f, feedback);
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const int bufferSize = delayBuffer.getNumSamples();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Simple smoothing for delay time to avoid clicks
            currentDelayInSamples += (targetDelayInSamples - currentDelayInSamples) * 0.001f;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                float inputSample = buffer.getReadPointer(channel)[sample];

                // Read from delay buffer (Linear Interpolation for smooth time changes)
                float readPos = static_cast<float>(writePos) - currentDelayInSamples;
                if (readPos < 0) readPos += static_cast<float>(bufferSize);

                int index1 = static_cast<int>(readPos);
                int index2 = (index1 + 1) % bufferSize;
                float fraction = readPos - static_cast<float>(index1);

                float delayedSample = (1.0f - fraction) * delayBuffer.getSample(channel % 2, index1) +
                                      fraction * delayBuffer.getSample(channel % 2, index2);

                // Write to delay buffer (Input + Feedback)
                delayBuffer.setSample(channel % 2, writePos, inputSample + (delayedSample * feedbackGain));

                // Mix (Wet/Dry could be added, here it's additive for simplicity)
                buffer.getWritePointer(channel)[sample] += delayedSample * 0.5f;
            }

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
    float feedbackGain = 0.4f;
    float targetDelayInSamples = 0.0f;
    float currentDelayInSamples = 0.0f;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Delay)
};

} // namespace NEURONiK::DSP::Effects
