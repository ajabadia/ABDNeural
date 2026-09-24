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
     *
     * 2026-09-24: la lectura YA NO depende solo de la cabecera. El HPS del
     * primer tramo es la SEMILLA y fitGridLeastSquares() ajusta la rejilla
     * por minimos cuadrados sobre los picos de TODAS las ventanas activas
     * del fichero antes de devolver la raiz.
     */
    float detectPitch(const juce::AudioBuffer<float>& audio, double sampleRate);

    /** FASE 10.6: HPS refinado POR VENTANA. El llamador (analyzeTemporal)
        publica el espectro del frame en magnitudeSpectrum antes de llamar. */
    float detectPitchFromSpectrum(double sampleRate);

    /** 2026-09-23: GUARDIA de desviacion de pitch para el analisis ESTATICO.
        Un modelo estatico describe UNA rejilla n*f0; si el pitch del material
        se desplaza mas de pitchGuardCents (con la octava plegada) entre
        ventanas, el modelo saldria des-afinado en silencio. El llamador
        consulta lastPitchGuardCents(): 0 (de pie) o > 0 (guardia disparada).
        El analisis temporal (analyzeTemporal) sigue el pitch por ventana y
        NO paga esta guardia: es la representacion legitima de los barridos. */
    static constexpr float pitchGuardCents = 150.0f;
    void measurePitchDeviation (const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency);
    float lastPitchGuardCents() const noexcept { return lastGuardCents; }

    /** 2026-09-24: INDICADOR de rejilla para feedback inmediato en la
        UI del ModelMaker. Tras cada detectPitch(): residuo RMS en cents
        de los picos de TODAS las ventanas contra la rejilla k*f0
        devuelta y numero de observaciones que la sostienen. Bajo = el
        material ES una serie armonica; alto = offsets, ruido o barrido
        (no hay UNA rejilla). -1 = sin datos (material insuficiente). */
    float lastGridResidualCents() const noexcept { return gridResidCents; }
    int   lastGridObservations()   const noexcept { return gridObsCount; }


    /** HPS refinado (2026-09-23): esbozo del pico del espectro (+-1 bin)
        ANTES del producto de armonicos, pico del producto acotado a la
        ventana [ancla/2, ancla*2] con guardia de soporte, y escalera de
        octava musical (impares vacios frente a 2f0). Corrige la fundamental
        debil con armonico 2 dominante (CZ-SWEP1); no-op en series
        armonicas limpias. Consume magnitudeSpectrum. */
    float detectPitchImpl(double sampleRate) const;

    /** Maximo local +-1 bin alrededor de center en magnitudeSpectrum. */
    int refineSpectralPeak(int center) const;
    NEURONiK::Common::SpectralModel analyzeTemporal(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency, int frameCount);

    /** Una ventana del audio (mix mono) -> espectro acumulado en out. */
    void averageWindow(const float* left, const float* right, int start, int count, std::vector<float>& out);

private:
    float lastGuardCents = 0.0f;

    /** Estado del ultimo ajuste de rejilla (ver accessores publicos). */
    float gridResidCents = -1.0f;
    int   gridObsCount = 0;

    /** 2026-09-24: AJUSTE DE REJILLA POR MINIMOS CUADRADOS previo a la
        lectura. La semilla del HPS ancla una banda por armonico (media del
        espaciado, igual que measurePartial) y CADA ventana activa repartida
        por el fichero aporta sus picos sub-bin como observaciones
        p_i = k_i*f0; el ajuste a traves del origen del modelo p_i = k_i*f0
        es f0* = SUM(k_i*p_i) / SUM(k_i*k_i). Promedia el ruido de la
        deteccion y el offset medio de la fuente sobre decenas de
        observaciones en vez de quedarse con una sola ventana. Dos pasadas:
        la segunda recoloca las bandas con el f0 ya ajustado. Con menos de
        kMinLsObservaciones observaciones, sin fondos o a mas de media
        octava de la semilla (barridos: no hay UNA rejilla), se devuelve la
        semilla. */
    float fitGridLeastSquares (const juce::AudioBuffer<float>& audio, double sampleRate, float seedHz);

    /** 2026-09-24: mismo minimos cuadrados POR FRAME para
        analyzeTemporal. Opera sobre el espectro ya publicado del
        frame (magnitudeSpectrum): los picos sub-bin de SUS parciales
        ajustan la raiz del frame a traves del origen, con dos
        pasadas y las mismas guardias que la rejilla global (minimo
        kMinLsObservaciones, solver valido y +-600 cents de la
        semilla HPS). La semilla sigue siendo la respuesta cuando el
        frame no tiene UNA rejilla. No toca el indicador de la UI. */
    float fitGridFromSpectrum (double sampleRate, float seedHz);

    /** Observaciones minimas del ajuste para creerle mas que a la semilla. */
    static constexpr int kMinLsObservations = 4;
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
