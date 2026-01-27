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

        timeSmoother.reset(sampleRate, 0.05); // 50ms ramp for delay time to avoid pitch jumps
        feedbackSmoother.reset(sampleRate, 0.02); // 20ms ramp
    }

    void setParameters(float timeInSeconds, float feedback) noexcept
    {
        timeSmoother.setTargetValue(timeInSeconds * static_cast<float>(currentSampleRate));
        feedbackSmoother.setTargetValue(juce::jlimit(0.0f, 0.95f, feedback));
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const int bufferSize = delayBuffer.getNumSamples();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentDelay = timeSmoother.getNextValue();
            float currentFB = feedbackSmoother.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
            {
                float inputSample = buffer.getReadPointer(channel)[sample];

                // Read from delay buffer (Linear Interpolation)
                float readPos = static_cast<float>(writePos) - currentDelay;
                if (readPos < 0) readPos += static_cast<float>(bufferSize);

                int index1 = static_cast<int>(readPos);
                int index2 = (index1 + 1) % bufferSize;
                float fraction = readPos - static_cast<float>(index1);

                float delayedSample = (1.0f - fraction) * delayBuffer.getSample(channel % 2, index1) +
                                      fraction * delayBuffer.getSample(channel % 2, index2);

                // Write to delay buffer (Input + Feedback)
                delayBuffer.setSample(channel % 2, writePos, inputSample + (delayedSample * currentFB));

                // Mix
                buffer.getWritePointer(channel)[sample] += delayedSample * 0.5f;
            }

            if (++writePos >= bufferSize) writePos = 0;
        }
    }

    void reset()
    {
        delayBuffer.clear();
        timeSmoother.setCurrentAndTargetValue(timeSmoother.getTargetValue());
        feedbackSmoother.setCurrentAndTargetValue(feedbackSmoother.getTargetValue());
    }

private:
    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    double currentSampleRate = 44100.0;

    juce::LinearSmoothedValue<float> timeSmoother;
    juce::LinearSmoothedValue<float> feedbackSmoother;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Delay)
};

} // namespace NEURONiK::DSP::Effects
