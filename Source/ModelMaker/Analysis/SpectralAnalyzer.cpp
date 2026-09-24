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
// Maximo local +-1 bin: el esbozo del pico NO re-ventanea (no toca fftData);
// el bin queda en la misma cordillera del espectro crudo, ya anclado al
// maximo local. Llamar con magnitudeSpectrum ya publicado.
int SpectralAnalyzer::refineSpectralPeak (int center) const
{
    const int numBins = fftSize / 2 + 1;
    int b = juce::jlimit (2, numBins - 2, center);
    float m = magnitudeSpectrum[static_cast<size_t> (b)];
    if (b > 1 && magnitudeSpectrum[static_cast<size_t> (b - 1)] > m)
    {
        m = magnitudeSpectrum[static_cast<size_t> (b - 1)];
        --b;
    }
    if (b < numBins - 1 && magnitudeSpectrum[static_cast<size_t> (b + 1)] > m)
        ++b;
    return b;
}

// Nucleo comun de detectPitch y detectPitchFromSpectrum (HPS refinado).
// Consume magnitudeSpectrum: detectPitch la publica tras el FFT;
// analyzeTemporal la publica por frame antes de llamar (fase 10.6).
float SpectralAnalyzer::detectPitchImpl (double sampleRate) const
{
    const int numBins = fftSize / 2 + 1;
    const float binWidth = static_cast<float> (sampleRate) / static_cast<float> (fftSize);
    const int minBin = juce::jlimit (2, numBins - 1, static_cast<int> (50.0f / binWidth));
    const int maxBin = juce::jlimit (2, numBins - 1, static_cast<int> (2000.0f / binWidth));

    // 1. ANCLA: pico crudo en rango musical + esbozo +-1 bin
    int rawBin = 0;
    float rawMax = -1.0f;
    for (int i = minBin; i <= maxBin; ++i)
        if (magnitudeSpectrum[static_cast<size_t> (i)] > rawMax)
        {
            rawMax = magnitudeSpectrum[static_cast<size_t> (i)];
            rawBin = i;
        }
    if (rawBin == 0)
        return 130.81f; // Default C3 si no hay nada (como antes)

    const int anchor = refineSpectralPeak (rawBin);

    // 2. HPS clasico (producto k=2..4 sobre el MISMO espectro)
    std::vector<float> hps (static_cast<size_t> (numBins), 0.0f);
    for (int i = 0; i < numBins; ++i)
        hps[static_cast<size_t> (i)] = magnitudeSpectrum[static_cast<size_t> (i)];
    for (int down = 2; down <= 4; ++down)
    {
        for (int i = 0; i < numBins / down; ++i)
            hps[static_cast<size_t> (i)] *= magnitudeSpectrum[static_cast<size_t> (i * down)];
        for (int i = numBins / down; i < numBins; ++i)
            hps[static_cast<size_t> (i)] = 0.0f;
    }

    // 3. Pico del producto acotado a la ventana del ancla
    const int wLo = juce::jlimit (minBin, maxBin, anchor / 2 - 1);
    const int wHi = juce::jlimit (wLo, maxBin, anchor * 2 + 1);
    int prodBin = wLo;
    float prodMax = -1.0f;
    for (int i = wLo; i <= wHi; ++i)
        if (hps[static_cast<size_t> (i)] > prodMax)
        {
            prodMax = hps[static_cast<size_t> (i)];
            prodBin = i;
        }

    // 4. Candidato: el producto SOLO puede degradar el ancla UNA octava
    //    (candidato a +-60 cents de ancla/2, con soporte espectral
    //    >= -40 dB del ancla). Nunca hacia arriba: dentro de la ventana,
    //    el pico del producto puede ser un armónico/crestas de fuga por
    //    encima de un ancla ya correcta (SWEP1 head 123.8 -> 188; RRISE
    //    291 -> 167) y secuestrarla. El unico caso que justifica bajar:
    //    el pico crudo es el 2o armonico de una fundamental mas debil.
    int base = anchor;
    const float anchorHz = static_cast<float> (anchor) * binWidth;
    const float prodHz = static_cast<float> (prodBin) * binWidth;
    const float centsToHalf = 1200.0f * std::log2 (prodHz / (0.5f * anchorHz));
    const bool octaveBelow = std::abs (centsToHalf) <= 60.0f;
    const bool hasSupport = magnitudeSpectrum[static_cast<size_t> (prodBin)]
                                >= 0.01f * magnitudeSpectrum[static_cast<size_t> (anchor)];
    if (octaveBelow && hasSupport)
        base = prodBin;

    // 5. Escalera de octava (f0/2, f0, 2f0, 3f0): impares vacios frente a
    //    2f0 = la fundamental real esta UNA octava arriba del candidato.
    auto bandPeak = [&] (float centerHz, float halfHz, float& refinedHz) -> float
    {
        int lo = static_cast<int> (std::floor ((centerHz - halfHz) / binWidth));
        int hi = static_cast<int> (std::ceil ((centerHz + halfHz) / binWidth));
        lo = juce::jlimit (1, numBins - 2, lo);
        hi = juce::jlimit (lo, numBins - 2, hi);
        if (hi <= lo) { refinedHz = centerHz; return 0.0f; }

        int peakBin = lo;
        float peakMag = -1.0f;
        for (int b = lo; b <= hi; ++b)
            if (magnitudeSpectrum[static_cast<size_t> (b)] > peakMag)
            {
                peakMag = magnitudeSpectrum[static_cast<size_t> (b)];
                peakBin = b;
            }

        if (peakBin > lo && peakBin < hi)
        {
            const float yl = magnitudeSpectrum[static_cast<size_t> (peakBin - 1)];
            const float y0 = magnitudeSpectrum[static_cast<size_t> (peakBin)];
            const float yr = magnitudeSpectrum[static_cast<size_t> (peakBin + 1)];
            const float den = yl - 2.0f * y0 + yr;
            const float d = std::abs (den) > 1.0e-9f
                                ? juce::jlimit (-0.5f, 0.5f, 0.5f * (yl - yr) / den)
                                : 0.0f;
            refinedHz = (static_cast<float> (peakBin) + d) * binWidth;
        }
        else
        {
            refinedHz = static_cast<float> (peakBin) * binWidth;
        }
        return peakMag;
    };

    const float f0Hz = static_cast<float> (base) * binWidth;
    float rOdd = 0.0f, r2 = 0.0f, r3 = 0.0f, r4 = 0.0f;
    const float mOdd = bandPeak (1.0f * f0Hz, 0.25f * f0Hz, rOdd);
    const float m2 = bandPeak (2.0f * f0Hz, 0.25f * f0Hz, r2);
    const float m3 = bandPeak (3.0f * f0Hz, 0.25f * f0Hz, r3);
    if (m2 > 1.0e-6f && mOdd < 0.15f * m2 && m3 < 0.15f * m2)
        base /= 2; // una octava por debajo: fundamental debil detectada
    else
    {
        // Fundamental debil con sub-octava fuerte (CZ-SWEP1): 2f0 domina
        // TODO su entorno de la serie corta (sobre f0 y sobre 3f0, y al
        // menos el doble que f0) y la serie muere tras el 3er parcial
        // (m4 al suelo): la fundamental real es 2f0 — la nota del patch.
        // Puertas verificadas contra el material CZ: BASS1/HAMOG/PAD1 con
        // f0 dominante no entran; RRISE con 3f0 dominante tampoco.
        const float m4 = bandPeak (4.0f * f0Hz, 0.25f * f0Hz, r4);
        if (m2 > mOdd && m2 > m3 && m2 > 2.0f * mOdd && m4 < 0.25f * m3)
            base *= 2; // la nota vive una octava arriba del candidato
    }

    // 6. Refinado sub-bin (parabola sobre el ESPECTRO en el bin base; mas
    //    preciso que la parabola del producto cuando el candidato es el
    //    ancla de un material con fundamental debil)
    float frequency = static_cast<float> (base) * binWidth;
    if (base > 1 && base < numBins - 1)
    {
        const float yl = magnitudeSpectrum[static_cast<size_t> (base - 1)];
        const float y0 = magnitudeSpectrum[static_cast<size_t> (base)];
        const float yr = magnitudeSpectrum[static_cast<size_t> (base + 1)];
        const float den = yl - 2.0f * y0 + yr;
        if (std::abs (den) > 1.0e-9f)
        {
            const float p = juce::jlimit (-0.5f, 0.5f, 0.5f * (yl - yr) / den);
            frequency = (static_cast<float> (base) + p) * binWidth;
        }
    }
    return frequency;
}


NEURONiK::Common::SpectralModel SpectralAnalyzer::analyze(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency)
{
    NEURONiK::Common::SpectralModel model;

    const int numSamples = audio.getNumSamples();
    if (numSamples <= 0)
        return model;
    // GUARDIA de desviacion de pitch: el modelo estatico solo describe
    // material cuasi-monotonico. Si el pitch por ventana se desvia (barrido,
    // octava inestable), el modelo saldria des-afinado en silencio; el
    // llamador consulta lastPitchGuardCents() y decide (GUI: aviso y
    // bloqueo de export; sonda: reporte).
    measurePitchDeviation (audio, sampleRate, rootFrequency);


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

    // Guardia de maximo local: si el maximo de la ventana no lo es del
    // espectro (un vecino inmediato lo supera), la energia es la falda de
    // OTRO parcial (sub-octava/serie vecina) y no de este hueco -> 0, en
    // vez de un parcial fantasma con offset saturado (CZ-SWEP1).
    if (bestMag > 0.0f)
    {
        const float left = magnitudeSpectrum[static_cast<size_t>(bestBin - 1)];
        const float right = magnitudeSpectrum[static_cast<size_t>(bestBin + 1)];
        if (left > bestMag || right > bestMag)
        {
            bestMag = 0.0f;
            bestBin = nominalBin;
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
    // INDICADOR (2026-09-24): cada lectura lo recalcula; sin material, n/d.
    gridResidCents = -1.0f;
    gridObsCount = 0;

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

    // 2. Perform FFT & publish the magnitude spectrum: el HPS refinado
    //    consume ESTE buffer (igual que detectPitchFromSpectrum).
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    const int numBins = fftSize / 2 + 1;
    for (int i = 0; i < numBins; ++i)
        magnitudeSpectrum[static_cast<size_t>(i)] = fftData[static_cast<size_t>(i)];

    const float seed = detectPitchImpl(sampleRate);

    // AJUSTE DE REJILLA PREVIO (2026-09-24): la lectura no se queda con la
    // cabecera — la semilla del HPS ancla las bandas y el ajuste por minimos
    // cuadrados de TODAS las ventanas activas afina la raiz con sus picos.
    return fitGridLeastSquares(audio, sampleRate, seed);
}

// 2026-09-24: AJUSTE DE REJILLA POR MINIMOS CUADRADOS. Observaciones de
// TODAS las ventanas activas (el mismo reparto que analyze: hasta maxFrames
// tramos repartidos por el fichero, silencio fuera) contra la rejilla
// k*raiz: cada armonico k aporta su pico sub-bin p_i y el modelo
// p_i = k_i*f0 se ajusta a traves del origen ->
//     f0* = SUM(k_i * p_i) / SUM(k_i * k_i)
// sin pesos: cada pico pesa lo mismo, que es lo que dice "sobre los picos de
// todas las ventanas". Dos pasadas (bandas de la semilla, luego bandas del
// f0 ajustado) y guardia de media octava contra la semilla: el LS REFINA la
// lectura del HPS, no la sustituye por otra cosa (un barrido no tiene UNA
// rejilla y la semilla sigue siendo la respuesta historica del llamador).
float SpectralAnalyzer::fitGridLeastSquares (const juce::AudioBuffer<float>& audio,
                                             double sampleRate, float seedHz)
{
    if (seedHz <= 0.0f)
        return seedHz;

    const int numSamples = audio.getNumSamples();
    if (numSamples <= 0)
        return seedHz;

    constexpr int numBins = fftSize / 2 + 1;
    const float* left = audio.getReadPointer (0);
    const float* right = (audio.getNumChannels() > 1) ? audio.getReadPointer (1) : nullptr;
    const int frameCount = (numSamples <= fftSize)
                               ? 1
                               : juce::jlimit (2, maxFrames, numSamples / fftSize);

    struct Observation { int harmonic; float peakHz; };
    std::vector<Observation> observations;
    observations.reserve (static_cast<size_t> (frameCount * 16));

    auto collect = [&] (float root)
    {
        observations.clear();
        if (root <= 0.0f)
            return;

        for (int f = 0; f < frameCount; ++f)
        {
            const int start = (frameCount == 1)
                                  ? 0
                                  : (numSamples - fftSize) * f / (frameCount - 1);
            const int count = juce::jmin (fftSize, numSamples - start);
            if (count <= 0)
                continue;

            std::fill (fftData.begin(), fftData.end (), 0.0f);
            double energy = 0.0;
            for (int i = 0; i < count; ++i)
            {
                float sample = left[start + i];
                if (right != nullptr) sample = (sample + right[start + i]) * 0.5f;
                fftData[static_cast<size_t> (i)] = sample;
                energy += static_cast<double> (sample) * sample;
            }
            if (std::sqrt (energy / static_cast<double> (count)) < static_cast<double> (kFrameRmsFloor))
                continue; // silencio: sin picos que aportar

            transformCurrentFrame (count);
            for (int b = 0; b < numBins; ++b)
                magnitudeSpectrum[static_cast<size_t> (b)] = fftData[static_cast<size_t> (b)];

            // Picos de ESTA ventana en la rejilla k*root: mismas reglas que
            // el analisis (banda de media espaciado que nunca invade al
            // armonico vecino, maximo local, suelo relativo kPartialFloor
            // contra el maximo de la ventana).
            std::array<PartialMeasurement, 64> measured;
            float windowMax = 0.0f;
            for (int k = 0; k < 64; ++k)
            {
                measured[static_cast<size_t> (k)] =
                    measurePartial (root * static_cast<float> (k + 1), root, sampleRate);
                windowMax = std::max (windowMax, measured[static_cast<size_t> (k)].amplitude);
            }
            if (windowMax <= 0.0f)
                continue;

            for (int k = 0; k < 64; ++k)
            {
                const auto& m = measured[static_cast<size_t> (k)];
                if (m.amplitude <= 0.0f || m.amplitude < windowMax * kPartialFloor)
                    continue;
                observations.push_back ({ k + 1, root * static_cast<float> (k + 1) + m.offsetHz });
            }
        }
    };

    auto solve = [] (const std::vector<Observation>& obs) -> float
    {
        double num = 0.0;
        double den = 0.0;
        for (const auto& o : obs)
        {
            num += static_cast<double> (o.harmonic) * static_cast<double> (o.peakHz);
            den += static_cast<double> (o.harmonic) * static_cast<double> (o.harmonic);
        }
        return den > 0.0 ? static_cast<float> (num / den) : 0.0f;
    };

    // INDICADOR (2026-09-24): residuo RMS en cents de los picos contra la
    // rejilla k*f0 devuelta, mas el numero de observaciones que la sostienen.
    // La UI del ModelMaker lo muestra como feedback inmediato del material.
    auto residualCents = [] (const std::vector<Observation>& obs, float f0) -> float
    {
        if (obs.empty () || f0 <= 0.0f)
            return -1.0f;
        double acc = 0.0;
        for (const auto& o : obs)
        {
            const double r = 1200.0 * std::log2 (
                (double) o.peakHz / ((double) o.harmonic * (double) f0));
            acc += r * r;
        }
        return (float) std::sqrt (acc / (double) obs.size ());
    };
    auto publish = [&] (const std::vector<Observation>& obs, float f0)
    {
        gridObsCount   = static_cast<int> (obs.size ());
        gridResidCents = residualCents (obs, f0);
    };

    // Pasada 1: bandas ancladas en la semilla.
    collect (seedHz);
    if (static_cast<int> (observations.size ()) < kMinLsObservations)
    {
        publish (observations, seedHz);
        return seedHz; // material pobre: la semilla sigue siendo lo mejor
    }
    const float firstFit = solve (observations);
    if (firstFit <= 0.0f)
    {
        publish (observations, seedHz);
        return seedHz;
    }

    // Pasada 2: bandas recolocadas con el f0 ya ajustado (si la semilla
    // estaba medio paso fuera, los armonicos altos vuelven a su sitio).
    collect (firstFit);
    const float fitted = (static_cast<int> (observations.size ()) >= kMinLsObservations)
                             ? solve (observations)
                             : firstFit;
    if (fitted <= 0.0f)
    {
        publish (observations, seedHz);
        return seedHz;
    }

    // Guardia: el LS solo REFINA la semilla (media octava). A mas, la
    // semilla esta mal o el material no tiene UNA rejilla (barrido): manda
    // la lectura de cabecera, que es lo que siempre hiso el llamador.
    const float devCents = std::abs (1200.0f * std::log2 (fitted / seedHz));
    if (devCents > 600.0f)
    {
        publish (observations, seedHz); // sin UNA rejilla: el residuo
        return seedHz;                   // delata el desajuste
    }

    publish (observations, fitted);
    return fitted;
}
// 2026-09-24: AJUSTE DE REJILLA POR FRAME (analyzeTemporal). El mismo
// minimos cuadrados que fitGridLeastSquares pero sobre el espectro YA
// publicado del frame: las observaciones son los picos sub-bin de LOS
// parciales de ESTA ventana (analyzeTemporal copia frameSpectrum en
// magnitudeSpectrum antes de llamar), no las ventanas del fichero. Dos
// pasadas (bandas de la semilla HPS, luego bandas del f0 ajustado) y las
// mismas guardias: menos de kMinLsObservaciones picos validos, solver
// nulo o a mas de media octava de la semilla -> la semilla sigue siendo
// la raiz del frame. Asi cada frame de un barrido (CZ-RRISE) sale del
// MISMO estimador que detectPitch, con la precision sub-bin promediada
// sobre todos los armonicos del frame en vez de la del pico del
// producto HPS. Frame 0 y frameSpanHz NO pasan por aqui: siguen siendo
// la f0 del llamador (canonico legado + contrato de los tests).
float SpectralAnalyzer::fitGridFromSpectrum (double sampleRate, float seedHz)
{
    if (seedHz <= 0.0f)
        return seedHz;

    struct Observation { int harmonic; float peakHz; };
    std::vector<Observation> observations;
    observations.reserve (64);

    auto collect = [&] (float root)
    {
        observations.clear();
        if (root <= 0.0f)
            return;

        std::array<PartialMeasurement, 64> measured;
        float windowMax = 0.0f;
        for (int k = 0; k < 64; ++k)
        {
            measured[static_cast<size_t> (k)] =
                measurePartial (root * static_cast<float> (k + 1), root, sampleRate);
            windowMax = std::max (windowMax, measured[static_cast<size_t> (k)].amplitude);
        }
        if (windowMax <= 0.0f)
            return;

        for (int k = 0; k < 64; ++k)
        {
            const auto& m = measured[static_cast<size_t> (k)];
            if (m.amplitude <= 0.0f || m.amplitude < windowMax * kPartialFloor)
                continue;
            observations.push_back ({ k + 1, root * static_cast<float> (k + 1) + m.offsetHz });
        }
    };

    auto solve = [] (const std::vector<Observation>& obs) -> float
    {
        double num = 0.0;
        double den = 0.0;
        for (const auto& o : obs)
        {
            num += static_cast<double> (o.harmonic) * static_cast<double> (o.peakHz);
            den += static_cast<double> (o.harmonic) * static_cast<double> (o.harmonic);
        }
        return den > 0.0 ? static_cast<float> (num / den) : 0.0f;
    };

    collect (seedHz);
    if (static_cast<int> (observations.size ()) < kMinLsObservations)
        return seedHz; // frame con menos de 4 picos: manda el HPS del frame
    const float firstFit = solve (observations);
    if (firstFit <= 0.0f)
        return seedHz;

    collect (firstFit);
    const float fitted = (static_cast<int> (observations.size ()) >= kMinLsObservations)
                             ? solve (observations)
                             : firstFit;
    if (fitted <= 0.0f)
        return seedHz;

    const float devCents = std::abs (1200.0f * std::log2 (fitted / seedHz));
    if (devCents > 600.0f)
        return seedHz; // el LS solo REFINA la semilla del frame

    return fitted;
}



// 2026-09-23: GUARDIA de desviacion de pitch (analisis estatico). Mismas
// ventanas que analyze(), pitch por ventana con el HPS refinado sobre el
// espectro de cada una. Con la octava plegada (x2^k a [1, 2) contra la
// mediana), la desviacion maxima en cents dice si el material es
// cuasi-monotonico. lastPitchGuardCents(): 0 = de pie; > 0 = disparada.
void SpectralAnalyzer::measurePitchDeviation (const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency)
{
    lastGuardCents = 0.0f;
    if (rootFrequency <= 0.0f)
        return;
    const int numSamples = audio.getNumSamples();
    if (numSamples < fftSize)
        return; // una sola ventana: nada que comparar

    const int numBins = fftSize / 2 + 1;
    const float* left = audio.getReadPointer (0);
    const float* right = (audio.getNumChannels() > 1) ? audio.getReadPointer (1) : nullptr;

    const int frameCount = juce::jlimit (2, maxFrames, numSamples / fftSize);
    std::vector<float> detected;

    for (int f = 0; f < frameCount; ++f)
    {
        const int start = (numSamples - fftSize) * f / (frameCount - 1);
        std::fill (fftData.begin(), fftData.end(), 0.0f);

        double energy = 0.0;
        for (int i = 0; i < fftSize; ++i)
        {
            float sample = left[start + i];
            if (right != nullptr) sample = (sample + right[start + i]) * 0.5f;
            fftData[(size_t) i] = sample;
            energy += (double) sample * sample;
        }
        if (std::sqrt (energy / (double) fftSize) < (double) kFrameRmsFloor)
            continue; // silencio: sin pitch que medir

        transformCurrentFrame (fftSize);
        for (int b = 0; b < numBins; ++b)
            magnitudeSpectrum[(size_t) b] = fftData[(size_t) b];
        detected.push_back (detectPitchImpl (sampleRate));
    }

    if (detected.size() < 2)
        return;

    // Plegado CONTRA LA RAIZ del analisis (leccion de PAD1): los saltos de
    // octava del estimador por ventana (fundamental debil en unas ventanas
    // y no en otras: 62 vs 124 en BASS1/SWEP1) colapsan a 0; solo los
    // desplazamientos intra-octava reales (barridos, modulaciones, quinta)
    // cuentan como desviacion. Un error de octava PERSISTENTE es cosa del
    // estimador (regla de fundamental debil) y del check de sub-octava de
    // la sonda, no de esta guardia.
    float maxDev = 0.0f;
    for (const float f0k : detected)
    {
        if (f0k <= 0.0f)
            continue;
        // Plegado en cents sobre el intervalo (-600, +600]: la equivalencia
        // de octava correcta. Un pliegue por ratio con bordes en 1.0 tiene
        // un acantilado EN la raiz (una deteccion a -0.2 cents del ancla se
        // duplicaria a ~1200): aqui el ruido del estimador queda a su valor
        // real y solo una raiz MUSICALMENTE distinta (quinta = -498 plegada)
        // supera el umbral.
        float dev = 1200.0f * std::log2 (f0k / rootFrequency);
        while (dev > 600.0f)   dev -= 1200.0f;
        while (dev <= -600.0f) dev += 1200.0f;
        maxDev = std::max (maxDev, std::abs (dev));
    }
    lastGuardCents = (maxDev > pitchGuardCents) ? maxDev : 0.0f;
}

// FASE 10.6: HPS refinado POR VENTANA. El llamador (analyzeTemporal) acaba
// de publicar el espectro del frame en magnitudeSpectrum; el nucleo es el
// mismo detectPitchImpl. No re-ventanea: fftData queda como el llamador lo
// dejo.
float SpectralAnalyzer::detectPitchFromSpectrum (double sampleRate)
{
    return detectPitchImpl(sampleRate);
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
    // FASE 10.6: raiz detectada POR VENTANA (frame 0 usa la f0 global del
    // llamador: el canonico/legado no cambia).
    std::vector<float> frameF0s(static_cast<size_t>(nFrames), rootFrequency);
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

        // FASE 10.6: f0 POR VENTANA (HPS sobre el espectro ya llenado). El
        // frame se mide contra SU rejilla: un barrido de pitch (CZ-RRISE)
        // produce frames cuyas raices siguen la evolucion del fichero.
        // Frame 0 conserva la f0 global (canonico legado, ratio 1.0 en z=0).
        // 2026-09-24: los frames 1+ salen del MISMO estimador que
        // detectPitch: el HPS de la ventana es la SEMILLA y el minimos
        // cuadrados sobre los picos del frame afina SU raiz.
        if (f > 0)
        {
            const float seed = detectPitchFromSpectrum(sampleRate);
            frameF0s[static_cast<size_t>(f)] = fitGridFromSpectrum(sampleRate, seed);
        }
        const float f0Frame = frameF0s[static_cast<size_t>(f)];

        float frameMax = 0.0f;
        for (int k = 0; k < 64; ++k)
        {
            const float targetFreq = f0Frame * static_cast<float>(k + 1);
            perFrame[static_cast<size_t>(f)][static_cast<size_t>(k)] =
                measurePartial(targetFreq, f0Frame, sampleRate);
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
    // FASE 10.6: la raiz de cada frame viaja en el modelo (el frame 0 es la
    // canonica: en z=0 el snapshot ES el frame canonico y el ratio es 1.0).
    for (int f = 1; f < nFrames; ++f)
        model.setF0At (f, frameF0s[static_cast<size_t>(f)]);
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
