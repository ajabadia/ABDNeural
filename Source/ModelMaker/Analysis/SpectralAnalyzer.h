/*
  ==============================================================================

    SpectralAnalyzer.h
    Created: 27 Jan 2026
    Description: Engine for extracting 64 harmonic partials from a waveform using FFT.

  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../../../Source/Common/SpectralModel.h"

namespace NEURONiK::ModelMaker::Analysis {

class SpectralAnalyzer
{
public:
    SpectralAnalyzer();
    ~SpectralAnalyzer() = default;

    /**
     * Performs analysis on a provided audio buffer.
     * @param audio Audio buffer (mono or stereo - will mix to mono).
     * @param sampleRate The sample rate of the audio.
     * @param rootFrequency The fundamental frequency (F0) to search harmonics for.
     */
    NEURONiK::Common::SpectralModel analyze(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency);

private:
    // FFT Configuration
    static constexpr int fftOrder = 13; // 2^13 = 8192 points
    static constexpr int fftSize = 1 << fftOrder;
    
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris };
    
    std::vector<float> fftData;
    std::vector<float> magnitudeSpectrum;

    // Helpers
    float getMagnitudeForFrequency(float frequency, double sampleRate);
};

} // namespace NEURONiK::ModelMaker::Analysis
