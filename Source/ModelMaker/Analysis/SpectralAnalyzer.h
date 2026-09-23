/*
  ==============================================================================

    SpectralAnalyzer.h
    Created: 27 Jan 2026
    Description: Engine for extracting 64 harmonic partials from a waveform using FFT.

  ==============================================================================
*/

#pragma once

#include <vector>
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
     *
     * Multiframe (2026-09-22): el espectro es la MEDIA de hasta 6 ventanas
     * repartidas por TODO el fichero (antes: solo las primeras 8192 muestras,
     * con lo que un ataque o un silencio inicial definian el modelo entero).
     * Las ventanas con RMS bajo (-80 dBFS) se descartan como silencio.
     *
     * @param audio Audio buffer (mono or stereo - will mix to mono).
     * @param sampleRate The sample rate of the audio.
     * @param rootFrequency The fundamental frequency (F0) to search harmonics for.
     */
    NEURONiK::Common::SpectralModel analyze(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency);

    /**
     * Attempts to detect the fundamental frequency (F0) of the audio.
     */
    float detectPitch(const juce::AudioBuffer<float>& audio, double sampleRate);
    NEURONiK::Common::SpectralModel analyzeTemporal(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency, int frameCount);

    /** Una ventana del audio (mix mono) -> espectro acumulado en out. */
    void averageWindow(const float* left, const float* right, int start, int count, std::vector<float>& out);

private:
    // FFT Configuration
    static constexpr int fftOrder = 13; // 2^13 = 8192 points
    static constexpr int fftSize = 1 << fftOrder;

    /** Ventanas analizadas por analyze(): cubre ficheros de hasta ~1 s con 6
        disparos; mas largo no mejora el modelo (espectro estacionario). */
    static constexpr int maxFrames = 6;

    /** Umbral de actividad por ventana (RMS): debajo, silencio y fuera. */
    static constexpr float kFrameRmsFloor = 1.0e-4f;

    /** Un parcial por debajo de esta fraccion del maximo global no es tal:
        suelo de deteccion (-60 dB) para no asignar offsets al ruido. */
    static constexpr float kPartialFloor = 1.0e-3f;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris };

    std::vector<float> fftData;
    std::vector<float> magnitudeSpectrum;

    /** Espectro temporal reutilizable entre llamadas/frames. */
    std::vector<float> frameSpectrum;

    /** Medida de UN parcial: magnitud (bin del pico) y desviacion sub-bin. */
    struct PartialMeasurement
    {
        float amplitude = 0.0f;
        float offsetHz = 0.0f;
    };

    /**
     * Peak-picking (ventana = mitad del espaciado entre armonicos f0) sobre
     * el espectro medio + interpolacion parabolica
     * sub-bin (en dB) del pico local. Devuelve la magnitud del pico y el
     * desajuste del parcial real respecto a n*f0 (frequencyOffset del modelo).
     * Sin pico por encima del suelo -> amplitude medida y offset 0.
     */

    PartialMeasurement measurePartial(float targetFreq, float rootFrequency, double sampleRate) const;

    /** FFT de una ventana ya copiada en fftData: deja magnitudes 0..fftSize/2. */
    void transformCurrentFrame(int numSamples);
};

} // namespace NEURONiK::ModelMaker::Analysis
