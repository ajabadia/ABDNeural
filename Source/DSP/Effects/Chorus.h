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

        rateSmoother.reset(sampleRate, 0.02);
        depthSmoother.reset(sampleRate, 0.02);
        mixSmoother.reset(sampleRate, 0.02);
    }

    void setParameters(float rateHz, float depth, float mix) noexcept
    {
        rateSmoother.setTargetValue(rateHz);
        depthSmoother.setTargetValue(depth);
        mixSmoother.setTargetValue(mix);
    }

    void setMix(float mix) noexcept
    {
        mixSmoother.setTargetValue(mix);
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const int bufferSize = delayBuffer.getNumSamples();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float currentRate = rateSmoother.getNextValue();
            float currentDepth = depthSmoother.getNextValue();
            float currentMix = mixSmoother.getNextValue();

            float phaseInc = juce::MathConstants<float>::twoPi * currentRate / static_cast<float>(currentSampleRate);

            // Modulation: LFO between 5ms and 30ms
            float mod = (std::sin(phase) + 1.0f) * 0.5f; // 0 to 1
            float delaySamples = (0.005f + mod * 0.025f * currentDepth) * static_cast<float>(currentSampleRate);

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
                float output = (inputSample * (1.0f - currentMix * 0.5f)) + (delayedSample * currentMix * 0.5f);
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
        rateSmoother.setCurrentAndTargetValue(rateSmoother.getTargetValue());
        depthSmoother.setCurrentAndTargetValue(depthSmoother.getTargetValue());
        mixSmoother.setCurrentAndTargetValue(mixSmoother.getTargetValue());
    }

private:
    juce::AudioBuffer<float> delayBuffer;
    int writePos = 0;
    float phase = 0.0f;
    double currentSampleRate = 44100.0;

    juce::LinearSmoothedValue<float> rateSmoother { 1.0f };
    juce::LinearSmoothedValue<float> depthSmoother { 0.2f };
    juce::LinearSmoothedValue<float> mixSmoother { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Chorus)
};

} // namespace NEURONiK::DSP::Effects
