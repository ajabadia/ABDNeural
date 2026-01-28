/*
  ==============================================================================

    SpectralAnalyzer.cpp
    Created: 27 Jan 2026
    Description: Implementation of spectral analysis logic.

  ==============================================================================
*/

#include "SpectralAnalyzer.h"

namespace NEURONiK::ModelMaker::Analysis {

SpectralAnalyzer::SpectralAnalyzer()
{
    fftData.resize(static_cast<size_t>(fftSize * 2), 0.0f);
    magnitudeSpectrum.resize(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
}

NEURONiK::Common::SpectralModel SpectralAnalyzer::analyze(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency)
{
    NEURONiK::Common::SpectralModel model;
    
    // 1. Prepare Audio (Mono Mix + Windowing)
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    
    int numSamples = juce::jmin(audio.getNumSamples(), fftSize);
    const float* left = audio.getReadPointer(0);
    const float* right = (audio.getNumChannels() > 1) ? audio.getReadPointer(1) : nullptr;
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = left[i];
        if (right != nullptr) sample = (sample + right[i]) * 0.5f;
        
        fftData[static_cast<size_t>(i)] = sample;
    }

    window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(numSamples));

    // 2. Perform FFT
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    // 3. Extract Harmonics (1 to 64)
    float maxAmp = 0.0f;

    for (int k = 0; k < 64; ++k)
    {
        float harmonicFactor = static_cast<float>(k + 1);
        float targetFreq = rootFrequency * harmonicFactor;
        
        // Find magnitude at this frequency
        float magnitude = getMagnitudeForFrequency(targetFreq, sampleRate);
        
        model.amplitudes[static_cast<size_t>(k)] = magnitude;
        model.frequencyOffsets[static_cast<size_t>(k)] = 0.0f; // TODO: Calculate precise offset if bin is not exact match

        if (magnitude > maxAmp) maxAmp = magnitude;
    }

    // 4. Normalize
    if (maxAmp > 0.00001f)
    {
        float scaler = 1.0f / maxAmp;
        for (float& amp : model.amplitudes)
        {
            amp *= scaler;
        }
    }

    return model;
}

float SpectralAnalyzer::detectPitch(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    if (audio.getNumSamples() == 0) return 0.0f;

    // 1. Prepare Audio (Mono Mix + Windowing)
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    int numSamples = juce::jmin(audio.getNumSamples(), fftSize);
    
    for (int i = 0; i < numSamples; ++i)
    {
        float s = audio.getSample(0, i);
        if (audio.getNumChannels() > 1) s = (s + audio.getSample(1, i)) * 0.5f;
        fftData[static_cast<size_t>(i)] = s;
    }
    
    // Apply window to the filled buffer
    window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(numSamples));

    // 2. Perform FFT & Compute Magnitude Spectrum
    fft.performFrequencyOnlyForwardTransform(fftData.data());
    
    int numBins = fftSize / 2 + 1;
    std::vector<float> hps(static_cast<size_t>(numBins), 0.0f);
    
    for (int i = 0; i < numBins; ++i)
        hps[static_cast<size_t>(i)] = fftData[static_cast<size_t>(i)];

    // 3. Harmonic Product Spectrum (Downsample & Multiply)
    // We'll use up to 4 harmonics
    for (int down = 2; down <= 4; ++down)
    {
        for (int i = 0; i < numBins / down; ++i)
        {
            hps[static_cast<size_t>(i)] *= fftData[static_cast<size_t>(i * down)];
        }
        // Zero out the rest of the buffer to avoid carrying over high frequency noise
        for (int i = numBins / down; i < numBins; ++i)
        {
            hps[static_cast<size_t>(i)] = 0.0f;
        }
    }

    // 4. Find Peak in valid range (Musical range: 50Hz to 2000Hz)
    float binWidth = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    int minBin = static_cast<int>(50.0f / binWidth);
    int maxBin = static_cast<int>(2000.0f / binWidth);
    
    minBin = juce::jlimit(2, numBins - 1, minBin);
    maxBin = juce::jlimit(2, numBins - 1, maxBin);

    float maxVal = -1.0f;
    int bestBin = 0;

    for (int i = minBin; i <= maxBin; ++i)
    {
        if (hps[static_cast<size_t>(i)] > maxVal)
        {
            maxVal = hps[static_cast<size_t>(i)];
            bestBin = i;
        }
    }

    if (bestBin == 0) return 130.81f; // Default C3 if nothing found

    // 5. Refined Frequency (Parabolic Interpolation)
    float frequency = static_cast<float>(bestBin) * binWidth;
    
    if (bestBin > 0 && bestBin < numBins - 1)
    {
        float alpha = hps[static_cast<size_t>(bestBin - 1)];
        float beta = hps[static_cast<size_t>(bestBin)];
        float gamma = hps[static_cast<size_t>(bestBin + 1)];
        
        float p = 0.5f * (alpha - gamma) / (alpha - 2.0f * beta + gamma);
        frequency = (static_cast<float>(bestBin) + p) * binWidth;
    }

    return frequency;
}

float SpectralAnalyzer::getMagnitudeForFrequency(float frequency, double sampleRate)
{
    // Convert frequency to bin index
    float binSize = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    float fpBin = frequency / binSize; // Fractional bin
    
    // Linear Interpolation between bins for smoother response
    int index1 = static_cast<int>(fpBin);
    int index2 = index1 + 1;
    
    if (index2 >= fftSize / 2) return 0.0f;

    float frac = fpBin - static_cast<float>(index1);
    
    // Note: performFrequencyOnlyForwardTransform puts magnitude in the first half of the buffer
    // But juce::dsp::FFT usually works in-place or copies. 
    // performFrequencyOnlyForwardTransform doc: "The array provided must contain at least 2 * getSize() elements..."
    // "On return, the first getSize() / 2 + 1 elements... contain the magnitudes"
    
    float val1 = fftData[static_cast<size_t>(index1)];
    float val2 = fftData[static_cast<size_t>(index2)];
    
    return val1 * (1.0f - frac) + val2 * frac;
}

} // namespace NEURONiK::ModelMaker::Analysis
