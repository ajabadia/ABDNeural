/*
  ==============================================================================

    SpectralAnalyzer.cpp
    Created: 27 Jan 2026
    Description: Implementation of spectral analysis logic.

    Multiframe + peak-picking + sub-bin (2026-09-22). Antes: UNA ventana con
    las primeras 8192 muestras y la magnitud leida en el bin exacto del
    armonico, con frequencyOffsets a cero (TODO). Ahora:
      1. Hasta 6 ventanas repartidas por TODO el fichero; las silentes (RMS
         bajo) se descartan del promedio (con fichero entero en silencio se
         promedian todas, como antes, para no inventar contenido).
      2. Por armonico: pico local en +-2 bins (acotado para no invadir el
         armonico vecino) sobre el espectro medio.
      3. Interpolacion parabolica sub-bin (en dB) -> frequencyOffset en Hz,
         el canal del modelo que los motores morphean y que nunca se generaba.

  ==============================================================================
*/

#include "SpectralAnalyzer.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace NEURONiK::ModelMaker::Analysis {

SpectralAnalyzer::SpectralAnalyzer()
{
    fftData.resize(static_cast<size_t>(fftSize * 2), 0.0f);
    magnitudeSpectrum.resize(static_cast<size_t>(fftSize / 2 + 1), 0.0f);
}

void SpectralAnalyzer::transformCurrentFrame(int numSamples)
{
    window.multiplyWithWindowingTable(fftData.data(), static_cast<size_t>(numSamples));
    fft.performFrequencyOnlyForwardTransform(fftData.data());
}

NEURONiK::Common::SpectralModel SpectralAnalyzer::analyze(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency)
{
    NEURONiK::Common::SpectralModel model;

    const int numSamples = audio.getNumSamples();
    if (numSamples <= 0)
        return model;

    constexpr int numBins = fftSize / 2 + 1;

    std::vector<float> sumActive(static_cast<size_t>(numBins), 0.0f);
    std::vector<float> sumAll(static_cast<size_t>(numBins), 0.0f);
    int activeCount = 0;
    int allCount = 0;

    // 1. Ventanas repartidas por el fichero --------------------------------
    //    Mono mix dentro de fftData (padded a 2*fftSize para el FFT de JUCE).
    const float* left = audio.getReadPointer(0);
    const float* right = (audio.getNumChannels() > 1) ? audio.getReadPointer(1) : nullptr;

    const int frameCount = (numSamples <= fftSize)
                               ? 1
                               : juce::jlimit(2, maxFrames, numSamples / fftSize);

    for (int f = 0; f < frameCount; ++f)
    {
        const int start = (frameCount == 1)
                              ? 0
                              : (numSamples - fftSize) * f / (frameCount - 1);
        const int count = juce::jmin(fftSize, numSamples - start);
        if (count <= 0)
            continue;

        std::fill(fftData.begin(), fftData.end(), 0.0f);

        double energy = 0.0;
        for (int i = 0; i < count; ++i)
        {
            float sample = left[start + i];
            if (right != nullptr) sample = (sample + right[start + i]) * 0.5f;
            fftData[static_cast<size_t>(i)] = sample;
            energy += static_cast<double>(sample) * sample;
        }

        transformCurrentFrame(count);

        for (int b = 0; b < numBins; ++b)
        {
            const float mag = fftData[static_cast<size_t>(b)];
            sumAll[static_cast<size_t>(b)] += mag;
        }
        ++allCount;

        const double frameRms = std::sqrt(energy / static_cast<double>(count));
        if (frameRms >= static_cast<double>(kFrameRmsFloor))
        {
            for (int b = 0; b < numBins; ++b)
                sumActive[static_cast<size_t>(b)] += fftData[static_cast<size_t>(b)];
            ++activeCount;
        }
    }

    // 2. Media: solo ventanas activas si las hay (el silencio no diluye el
    //    modelo); fichero entero en silencio -> media de todas, como antes.
    {
        const bool useActive = activeCount > 0;
        const float* src = useActive ? sumActive.data() : sumAll.data();
        const float inv = 1.0f / static_cast<float>(useActive ? activeCount : allCount);
        for (int b = 0; b < numBins; ++b)
            magnitudeSpectrum[static_cast<size_t>(b)] = src[b] * inv;
    }

    // 3. Peak-picking +-2 bins + offset sub-bin por armonico ---------------
    float maxAmp = 0.0f;
    std::array<PartialMeasurement, 64> measured;

    for (int k = 0; k < 64; ++k)
    {
        const float targetFreq = rootFrequency * static_cast<float>(k + 1);
        measured[static_cast<size_t>(k)] = measurePartial(targetFreq, rootFrequency, sampleRate);
        maxAmp = std::max(maxAmp, measured[static_cast<size_t>(k)].amplitude);
    }

    // Los parciales por debajo del suelo no son de la fuente: su offset
    // seria desajuste del ruido, no inharmonicidad.
    for (int k = 0; k < 64; ++k)
    {
        if (measured[static_cast<size_t>(k)].amplitude < maxAmp * kPartialFloor)
            measured[static_cast<size_t>(k)].offsetHz = 0.0f;

        model.amplitudes[static_cast<size_t>(k)] = measured[static_cast<size_t>(k)].amplitude;
        model.frequencyOffsets[static_cast<size_t>(k)] = measured[static_cast<size_t>(k)].offsetHz;
    }

    // 4. Normalize
    if (maxAmp > 0.00001f)
    {
        const float scaler = 1.0f / maxAmp;
        for (float& amp : model.amplitudes)
            amp *= scaler;
    }

    // FASE 10.1: el espaciado armonico REAL de la fuente (f0) es la dimension
    // en Hz que la envoltura de camino corto del sampler necesita cuando este
    // modelo particione en frames temporales. Sin el, el fallback unitario
    // (n*f0 con f0=1) envuelve por la dimension equivocada.
    model.frameSpanHz = rootFrequency;

    return model;
}

SpectralAnalyzer::PartialMeasurement SpectralAnalyzer::measurePartial(float targetFreq, float rootFrequency, double sampleRate) const
{
    PartialMeasurement result;

    const float binWidth = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
    if (targetFreq <= 0.0f || targetFreq >= static_cast<float>(sampleRate) * 0.5f)
        return result;

    constexpr int numBins = fftSize / 2 + 1;
    const int nominalBin = static_cast<int>(std::lround(targetFreq / binWidth));
    if (nominalBin < 1 || nominalBin >= numBins - 1)
        return result;

    // Ventana de busqueda: la mitad del espaciado entre armonicos (f0 en
    // bins) — nunca invade el territorio del parcial vecino, pero SI crece
    // con n cuando hace falta: la inharmonicidad de una cuerda rigida crece
    // con n (+6n Hz aqui son 5.6 bins en el parcial 5; un cap de +-2 bins la
    // dejaba fuera de la ventana). Sin cap artificial por arriba.
    const float spacingBins = rootFrequency / binWidth;
    const int halfWin = juce::jmax(0, static_cast<int>(spacingBins * 0.5f) - 1);

    int bestBin = nominalBin;
    float bestMag = -1.0f;
    for (int b = nominalBin - halfWin; b <= nominalBin + halfWin; ++b)
    {
        if (b < 1 || b >= numBins - 1)
            continue;
        const float mag = magnitudeSpectrum[static_cast<size_t>(b)];
        if (mag > bestMag)
        {
            bestMag = mag;
            bestBin = b;
        }
    }

    result.amplitude = (bestMag > 0.0f) ? bestMag : 0.0f;

    // Interpolacion parabolica sub-bin sobre log-magnitud: la posicion real
    // del pico cae entre bins, y el desajuste n*f0 -> pico ES el
    // frequencyOffset del modelo (la inharmonicidad de la fuente). Se acota a
    // +-2 bins: mas alla, el "pico" no es este parcial sino un vecino.
    const float e = 1.0e-12f;
    const float a = std::log(magnitudeSpectrum[static_cast<size_t>(bestBin - 1)] + e);
    const float b = std::log(magnitudeSpectrum[static_cast<size_t>(bestBin)] + e);
    const float c = std::log(magnitudeSpectrum[static_cast<size_t>(bestBin + 1)] + e);
    const float denom = a - 2.0f * b + c;

    float delta = 0.0f;
    if (std::abs(denom) > 1.0e-9f)
        delta = juce::jlimit(-0.5f, 0.5f, 0.5f * (a - c) / denom);

    const float peakFreq = (static_cast<float>(bestBin) + delta) * binWidth;
    // El limite es la propia ventana de busqueda (half spacing f0): con la
    // inharmonicidad creciente el offset real supera de largo el viejo cap de
    // +-2 bins y saturaba (parcial 1 a +12 Hz se recortaba a 10.77).
    const float maxOffset = static_cast<float>(halfWin) * binWidth;
    result.offsetHz = juce::jlimit(-maxOffset, maxOffset, peakFreq - targetFreq);
    return result;
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


// ==============================================================================
// FASE 10.4: analisis temporal. Cada frame mide la MISMA rejilla n*f0 sobre
// SU ventana (emparejamiento por indice; la ventana de media banda del
// peak-picking garantiza que el parcial k nunca invade al vecino). El frame 0
// es el modelo canonico; frames 1..N-1 viven en extraAmps/extraOffsets.
// Normalizacion por el maximo GLOBAL de frames: la evolucion de nivel entre
// frames es informacion temporal (el decaimiento) y no se aplana.
// ==============================================================================
NEURONiK::Common::SpectralModel SpectralAnalyzer::analyzeTemporal(
    const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency, int frameCount)
{
    NEURONiK::Common::SpectralModel model;

    const int numSamples = audio.getNumSamples();
    if (numSamples <= 0)
        return model;

    const int nFrames = juce::jlimit(1, NEURONiK::Common::SpectralModel::kMaxFrames, frameCount);
    constexpr int numBins = fftSize / 2 + 1;

    if ((int) frameSpectrum.size() < numBins)
        frameSpectrum.resize(static_cast<size_t>(numBins), 0.0f);

    const float* left = audio.getReadPointer(0);
    const float* right = (audio.getNumChannels() > 1) ? audio.getReadPointer(1) : nullptr;

    std::vector<std::array<PartialMeasurement, 64>> perFrame(static_cast<size_t>(nFrames));
    float globalMax = 0.0f;

    // 1. Una ventana por frame, repartidas por TODO el fichero ------------
    for (int f = 0; f < nFrames; ++f)
    {
        const int start = (nFrames == 1 || numSamples <= fftSize)
                              ? 0
                              : (numSamples - fftSize) * f / (nFrames - 1);
        const int count = juce::jmin(fftSize, numSamples - start);
        if (count <= 0)
            continue;

        std::fill(frameSpectrum.begin(), frameSpectrum.end(), 0.0f);
        averageWindow(left, right, start, count, frameSpectrum);

        // measurePartial lee magnitudeSpectrum: publica el espectro del frame
        // (sin esta copia, todos los frames medirian un espectro rancio).
        for (int b = 0; b < numBins; ++b)
            magnitudeSpectrum[static_cast<size_t>(b)] = frameSpectrum[static_cast<size_t>(b)];

        float frameMax = 0.0f;
        for (int k = 0; k < 64; ++k)
        {
            const float targetFreq = rootFrequency * static_cast<float>(k + 1);
            perFrame[static_cast<size_t>(f)][static_cast<size_t>(k)] =
                measurePartial(targetFreq, rootFrequency, sampleRate);
            frameMax = std::max(frameMax, perFrame[static_cast<size_t>(f)][static_cast<size_t>(k)].amplitude);
        }
        globalMax = std::max(globalMax, frameMax);
    }

    if (globalMax <= 0.0f)
        return model;

    const float invGlobal = 1.0f / globalMax;

    // 2. Normalizacion global + suelo de ruido por frame ------------------
    for (int f = 0; f < nFrames; ++f)
    {
        auto& m = perFrame[static_cast<size_t>(f)];
        for (int k = 0; k < 64; ++k)
        {
            m[static_cast<size_t>(k)].amplitude *= invGlobal;
            if (m[static_cast<size_t>(k)].amplitude < kPartialFloor)
                m[static_cast<size_t>(k)].offsetHz = 0.0f;
        }
    }

    // 3. Volcado al modelo: frame 0 canonico + extras ----------------------
    for (int k = 0; k < 64; ++k)
    {
        model.amplitudes[static_cast<size_t>(k)] = perFrame[0][static_cast<size_t>(k)].amplitude;
        model.frequencyOffsets[static_cast<size_t>(k)] = perFrame[0][static_cast<size_t>(k)].offsetHz;
    }
    for (int f = 1; f < nFrames; ++f)
        for (int k = 0; k < 64; ++k)
        {
            model.extraAmps[static_cast<size_t>(f - 1)][static_cast<size_t>(k)] = perFrame[static_cast<size_t>(f)][static_cast<size_t>(k)].amplitude;
            model.extraOffsets[static_cast<size_t>(f - 1)][static_cast<size_t>(k)] = perFrame[static_cast<size_t>(f)][static_cast<size_t>(k)].offsetHz;
        }

    model.frameCount = nFrames;
    model.frameSpanHz = rootFrequency;
    model.isValid = true;
    return model;
}

void SpectralAnalyzer::averageWindow(const float* left, const float* right, int start, int count, std::vector<float>& out)
{
    std::fill(fftData.begin(), fftData.end(), 0.0f);

    for (int i = 0; i < count; ++i)
    {
        float sample = left[start + i];
        if (right != nullptr) sample = (sample + right[start + i]) * 0.5f;
        fftData[static_cast<size_t>(i)] = sample;
    }

    transformCurrentFrame(count);

    for (int b = 0; b < fftSize / 2 + 1; ++b)
        out[static_cast<size_t>(b)] += fftData[static_cast<size_t>(b)];
}
} // namespace NEURONiK::ModelMaker::Analysis
