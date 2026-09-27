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

// SUELO DEL ESTIMADOR (2026-09-25): 50 Hz cuantizados al bin. Fuente unica:
// el ancla de detectPitchImpl y la politica de f0 por frame de analyzeTemporal
// preguntan aqui, para que no puedan separarse.
float SpectralAnalyzer::anchorFloorHz (double sampleRate) noexcept
{
    const float binWidth = static_cast<float> (sampleRate) / static_cast<float> (fftSize);
    const int minBin = juce::jlimit (2, fftSize / 2, static_cast<int> (50.0f / binWidth));
    return static_cast<float> (minBin) * binWidth;
}

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

// 2026-09-27: REJILLAS ENTRELAZADAS — el diagnostico que la sonda tenia
// fuera (acousticCheck) ahora vive en el analizador, donde pertenece:
// measurePartial marca el pico y el residuo/LS lo excluyen. Un pico a
// <0.25*f0 de un impar de f0/2 y a >0.25*f0 de k*f0 es la otra familia;
// esa energia no mide la inharmonicidad de k*f0, sino la sub-oscilacion
// (phasor CZ). Ver el caso CZ-SWEP1: 4 de los 8 parciales top caen aqui.
bool SpectralAnalyzer::isInterlacedPeak(float peakHz, float targetFreq, float rootFrequency) noexcept
{
    if (peakHz <= 0.0f || targetFreq <= 0.0f || rootFrequency <= 0.0f)
        return false;
    const float halfGrid = rootFrequency * 0.5f;
    const float oddF = std::round(peakHz / halfGrid);
    if (oddF < 1.0f)
        return false;
    // impar de f0/2 = la sub-rejilla; par = la propia rejilla
    const int oddI = (int) std::lround(oddF);
    const bool isOdd = (oddI % 2) == 1;
    if (! isOdd)
        return false;
    const float oddHz = oddF * halfGrid;
    const bool oddUsable = std::abs(peakHz - oddHz) < 0.25f * rootFrequency;
    const bool onOwnGrid = std::abs(peakHz - targetFreq) < 0.25f * rootFrequency;
    if (! oddUsable || onOwnGrid)
        return false;
    // Guarda de k: un pico de la sub-rejilla por debajo de la fundamental
    // de ESTE parcial no es el que contamina su banda (ventana = 0.5*f0,
    // nunca llega tan lejos). La sonda usaba odd*2 > k+0.5, que es lo
    // mismo que oddF > k + 0.5 con k = round(target/root).
    const int k = (int) std::lround(targetFreq / rootFrequency);
    // Guarda documentada: oddF siempre cae en [2k-1, 2k+1] dentro de la
    // ventana (media banda del peak-picking). Un impar fuera de ese
    // intervalo no esta en la banda de k, asi que no contamina su offset.
    // Formalmente: oddF en {2k-1, 2k+1}. Dentro => interlazado, fuera => no.
    const bool inBand = oddF >= (float)(2*k - 1) - 0.5f && oddF <= (float)(2*k + 1) + 0.5f;
    return inBand;
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
    const int minBin = juce::jlimit (2, numBins - 1,
                                     static_cast<int> (anchorFloorHz (sampleRate) / binWidth));
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


NEURONiK::Common::SpectralModel SpectralAnalyzer::analyze(const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency,
                                                         bool fixedGrid)
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
    // 2026-09-27: picos interlazados (sub-rejilla) no contaminan ni offset ni
    // residuo. Su offset ya es 0 (measurePartial) y no entra al LS; aqui se
    // deja la amplitud tal cual para diagnostico, pero el offset permanece 0.
    for (int k = 0; k < 64; ++k)
    {
        if (measured[static_cast<size_t>(k)].interlaced)
            measured[static_cast<size_t>(k)].offsetHz = 0.0f;
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
    // REJILLA FIJA (2026-09-25): procedencia del modo declarado. El analisis no
    // cambia (ya usa la rejilla del llamador); el fichero dice que esa rejilla la
    // fijo el usuario.
    model.gridFixed = fixedGrid;

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
    result.peakHz = peakFreq;
    // 2026-09-27: REJILLAS ENTRELAZADAS. Si el pico cae sobre la sub-rejilla
    // (impar de f0/2) no es inharmonicidad de ESTE parcial, sino energia de
    // la otra familia (phasor CZ: la sonda lo diagnosticaba como ENTRELAZADO).
    // La medida lo marca y el offset NO se contamina: se deja a 0 y el
    // residuo/LS lo ignoran (ver fitGridLeastSquares / fitGridFromSpectrum);
    // la amplitud se conserva para diagnostico pero el modelo la pondra a 0
    // en el volcado si hace falta (el llamador decide con el flag).
    // 2026-09-27 (+capa entrelazada): peakHz se conserva aunque el flag este
    // activo, para que la capa entrelazada pueda usar la posicion real del
    // pico (impar de f0/2) con su offset correspondiente.
    result.interlaced = isInterlacedPeak(peakFreq, targetFreq, rootFrequency);
    if (result.interlaced)
    {
        result.offsetHz = 0.0f;
        return result;
    }
    const float maxOffset = static_cast<float>(halfWin) * binWidth;
    result.offsetHz = juce::jlimit(-maxOffset, maxOffset, peakFreq - targetFreq);
    return result;
}

float SpectralAnalyzer::detectPitch(const juce::AudioBuffer<float>& audio, double sampleRate)
{
    // INDICADOR (2026-09-24): cada lectura lo recalcula; sin material, n/d.
    gridResidCents = -1.0f;
    gridObsCount = 0;
    gridInterlacedCount = 0;

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

// REJILLA FIJADA A MANO (2026-09-25): el llamador (ModelMaker) ya tiene su f0
// (escrita por el usuario); aqui solo se pule y se reporta, sin HPS de por
// medio. Es la via que cubre el material por debajo del suelo del estimador.
float SpectralAnalyzer::refineGrid (const juce::AudioBuffer<float>& audio, double sampleRate, float seedHz)
{
    return fitGridLeastSquares (audio, sampleRate, seedHz);
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
    int interlacedDropped = 0; // 2026-09-27: picos de la sub-rejilla no entran al LS

    auto collect = [&] (float root)
    {
        observations.clear();
        interlacedDropped = 0;
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
                if (m.interlaced) { ++interlacedDropped; continue; }
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
        gridInterlacedCount = interlacedDropped;
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
    int interlacedDropped = 0; // 2026-09-27: sub-rejilla fuera del LS

    auto collect = [&] (float root)
    {
        observations.clear();
        interlacedDropped = 0;
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
            if (m.interlaced) { ++interlacedDropped; continue; }
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



// 2026-09-27: BATERIA DE FAMILIAS DE OCTAVA -- decision por RESIDUO GLOBAL
// El proto HPS exploraba {seed/2, seed, seed*2} sinteticas con series debiles /
// continuas para ver donde cae el minimo de residuo sobre TODAS las ventanas.
// Aqui la metrica es la misma del ajuste LS (RMS en cents sobre los picos sub-bin
// de TODAS las ventanas activas, con las mismas guardas: media banda por armonico,
// maximo local, suelo -60 dB y diagnostico de rejillas entrelazadas). El LS de cada
// candidato es exactamente fitGridLeastSquares en miniatura (collect/solve/residuo
// a 2 pasadas, con LS que solo refina <600 cents su candidato), pero sin publicar
// el indicador -- es la medida para elegir, no el ajuste ya publicado. Los candidatos
// por debajo de anchorFloorHz() se marcan invalidos (via manual por debajo del
// suelo, seccion 8 del plan). Empate a <0.5 cents => mas observaciones.
std::array<SpectralAnalyzer::OctaveCandidate, 3>
SpectralAnalyzer::evaluateOctaveCandidates (const juce::AudioBuffer<float>& audio,
                                            double sampleRate, float seedHz)
{
    std::array<OctaveCandidate, 3> out{};
    if (seedHz <= 0.0f || audio.getNumSamples() <= 0) return out;
    const float floor = anchorFloorHz (sampleRate);
    const float candidates[3] = { seedHz * 0.5f, seedHz, seedHz * 2.0f };
    for (int ci = 0; ci < 3; ++ci)
    {
        const float cand = candidates[ci];
        auto& o = out[(size_t) ci];
        o.candidateHz = cand;
        o.fittedHz    = cand;
        if (cand < floor || cand <= 0.0f) continue;
        const int numSamples = audio.getNumSamples();
        if (numSamples <= 0) continue;
        constexpr int numBins = fftSize / 2 + 1;
        const float* left = audio.getReadPointer (0);
        const float* right = (audio.getNumChannels() > 1) ? audio.getReadPointer (1) : nullptr;
        const int frameCount = (numSamples <= fftSize) ? 1 : juce::jlimit (2, maxFrames, numSamples / fftSize);
        struct Obs { int harmonic; float peakHz; };
        std::vector<Obs> observations;
        observations.reserve ((size_t) (frameCount * 16));
        int interlacedDropped = 0;
        auto collect = [&] (float root)
        {
            observations.clear();
            interlacedDropped = 0;
            if (root <= 0.0f) return;
            for (int f = 0; f < frameCount; ++f)
            {
                const int start = (frameCount == 1) ? 0 : (numSamples - fftSize) * f / (frameCount - 1);
                const int count = juce::jmin (fftSize, numSamples - start);
                if (count <= 0) continue;
                std::fill (fftData.begin(), fftData.end(), 0.0f);
                double energy = 0.0;
                for (int i = 0; i < count; ++i)
                {
                    float s = left[start + i];
                    if (right) s = (s + right[start + i]) * 0.5f;
                    fftData[(size_t) i] = s;
                    energy += (double) s * (double) s;
                }
                if (std::sqrt (energy / (double) count) < (double) kFrameRmsFloor) continue;
                transformCurrentFrame (count);
                for (int b = 0; b < numBins; ++b) magnitudeSpectrum[(size_t) b] = fftData[(size_t) b];
                std::array<PartialMeasurement, 64> measured;
                float windowMax = 0.0f;
                for (int k = 0; k < 64; ++k)
                {
                    measured[(size_t) k] = measurePartial (root * (float)(k+1), root, sampleRate);
                    windowMax = std::max (windowMax, measured[(size_t) k].amplitude);
                }
                if (windowMax <= 0.0f) continue;
                for (int k = 0; k < 64; ++k)
                {
                    const auto& m = measured[(size_t) k];
                    if (m.interlaced) { ++interlacedDropped; continue; }
                    if (m.amplitude <= 0.0f || m.amplitude < windowMax * kPartialFloor) continue;
                    observations.push_back ({ k+1, root * (float)(k+1) + m.offsetHz });
                }
            }
        };
        auto solve = [] (const std::vector<Obs>& obs) -> float
        {
            double num = 0.0, den = 0.0;
            for (auto& o : obs) { num += (double)o.harmonic * (double)o.peakHz; den += (double)o.harmonic * (double)o.harmonic; }
            return den > 0.0 ? (float)(num/den) : 0.0f;
        };
        auto resid = [] (const std::vector<Obs>& obs, float f0) -> float
        {
            if (obs.empty() || f0 <= 0.0f) return -1.0f;
            double acc = 0.0;
            for (auto& o : obs) { double r = 1200.0 * std::log2((double)o.peakHz / ((double)o.harmonic * (double)f0)); acc += r*r; }
            return (float) std::sqrt(acc / (double)obs.size());
        };
        collect (cand);
        if ((int)observations.size() < kMinLsObservations) continue;
        const float firstFit = solve (observations);
        if (firstFit <= 0.0f) continue;
        collect (firstFit);
        const float fitted = ((int)observations.size() >= kMinLsObservations) ? solve (observations) : firstFit;
        if (fitted <= 0.0f) continue;
        if (std::abs (1200.0f * std::log2 (fitted / cand)) > 600.0f) continue;
        const float rc = resid (observations, fitted);
        if (rc < 0.0f) continue;
        if ((int) observations.size() < kMinLsObservations) continue;
        o.valid = true;
        o.fittedHz = fitted;
        o.residCents = rc;
        o.observations = (int) observations.size();
        o.interlaced = interlacedDropped;
    }
    return out;
}

float SpectralAnalyzer::resolveOctaveByGlobalResidual (const juce::AudioBuffer<float>& audio,
                                                       double sampleRate, float seedHz)
{
    if (seedHz <= 0.0f || audio.getNumSamples() <= 0) return seedHz;
    auto cands = evaluateOctaveCandidates (audio, sampleRate, seedHz);
    int best = -1;
    for (int i = 0; i < 3; ++i)
        if (cands[(size_t)i].valid)
        {
            if (best < 0) { best = i; continue; }
            const float rBest = cands[(size_t)best].residCents;
            const float rThis = cands[(size_t)i].residCents;
            if (rThis + 0.5f < rBest) best = i;
            else if (std::abs(rThis - rBest) <= 0.5f)
            {
                if (cands[(size_t)i].observations > cands[(size_t)best].observations) best = i;
                else if (cands[(size_t)i].observations == cands[(size_t)best].observations)
                {
                    float dBest = std::abs(1200.0f * std::log2(cands[(size_t)best].fittedHz / seedHz));
                    float dThis = std::abs(1200.0f * std::log2(cands[(size_t)i].fittedHz / seedHz));
                    if (dThis < dBest) best = i;
                }
            }
        }
    if (best < 0) return seedHz;
    return cands[(size_t)best].fittedHz;
}


// 2026-09-26: LA FRASE del residuo de rejilla (ver la cabecera de la
// declaracion): una sola para las tres superficies del aviso de pitch, junto a la
// medida que cita. Si el texto cambia, cambia en las tres a la vez.
juce::String SpectralAnalyzer::gridResidualNotice() const
{
    if (gridResidCents < 0.0f)
        return "residuo n/d (material insuficiente)";

    return "residuo " + juce::String (gridResidCents, 1) + " cents ("
           + juce::String (gridObsCount) + " picos)";
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
//
// FASE 11.2 (2026-09-25): el MISMO espectro se lee ademas contra la rejilla
// COMUN del llamador (rootFrequency). Esa segunda lectura alimenta el
// clustering por forma de envolvente (LayerClustering.h) y, si el material
// tiene >= 2 capas, es la que produce el modelo: las capas son sub-indices de
// UNA rejilla, no de la rejilla de cada ventana. Sin capas (o sin rejilla
// comun: el material barre el pitch) el volcado es exactamente el de antes.
// ==============================================================================
NEURONiK::Common::SpectralModel SpectralAnalyzer::analyzeTemporal(
    const juce::AudioBuffer<float>& audio, double sampleRate, float rootFrequency, int frameCount,
    bool fixedGrid, LayerMetric layerMetric)
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
    /** FASE 11.2: la misma ventana medida contra la rejilla COMUN (ver arriba).
        Solo se llena si la PUERTA DE PLEGADO (10.3) declara la rejilla COMUN:
        es el PRE-FILTRO del plan 10.4 (material bi-rejilla no paga clustering). */
    std::vector<std::array<PartialMeasurement, 64>> perFrameCommon(static_cast<size_t>(nFrames));
    // FASE 10.6: raiz detectada POR VENTANA (frame 0 usa la f0 global del
    // llamador: el canonico/legado no cambia).
    std::vector<float> frameF0s(static_cast<size_t>(nFrames), rootFrequency);
    float globalMax = 0.0f;

    // 2026-09-25 (plan 10.4): LA PUERTA DE PLEGADO DE OCTAVA ES UN PRE-FILTRO
    // DEL ANALISIS TEMPORAL. El material BI-rejilla —la f0 de sus ventanas no
    // cabe en la rejilla del llamador ni con la octava plegada— no comparte una
    // rejilla, asi que el modelo de capas no es honesto y NO paga su coste. La
    // puerta decide ANTES de medir la rejilla comun y de correr el clustering,
    // no despues (tirando unas capas ya calculadas). Para decidir antes hace
    // falta la f0 de TODAS las ventanas, asi que el analisis va en dos pasadas:
    // (1a) la f0 por ventana —la evidencia de la puerta— guardando el espectro
    // de cada frame; la PUERTA; (1b) la medida, que solo toca la rejilla comun
    // si la puerta la declaro COMUN. El espectro se guarda para no repetir la
    // FFT: el pre-filtro ahorra el trabajo del clustering, no lo duplica.
    std::vector<float> frameSpectra(static_cast<size_t>(numBins) * static_cast<size_t>(nFrames), 0.0f);

    // 1a. PRE-PASADA (la evidencia de la puerta): una ventana por frame,
    //     repartidas por TODO el fichero; el espectro se guarda y la f0 POR
    //     VENTANA se estima sobre el.
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
        // (sin esta copia, todos los frames medirian un espectro rancio) y lo
        // GUARDA para la pasada de medida (1b).
        for (int b = 0; b < numBins; ++b)
        {
            magnitudeSpectrum[static_cast<size_t>(b)] = frameSpectrum[static_cast<size_t>(b)];
            frameSpectra[static_cast<size_t>(f) * static_cast<size_t>(numBins) + static_cast<size_t>(b)] =
                frameSpectrum[static_cast<size_t>(b)];
        }

        // FASE 10.6: f0 POR VENTANA (HPS sobre el espectro ya llenado). El
        // frame se mide contra SU rejilla: un barrido de pitch (CZ-RRISE)
        // produce frames cuyas raices siguen la evolucion del fichero.
        // Frame 0 conserva la f0 global (canonico legado, ratio 1.0 en z=0).
        // 2026-09-24: los frames 1+ salen del MISMO estimador que
        // detectPitch: el HPS de la ventana es la SEMILLA y el minimos
        // cuadrados sobre los picos del frame afina SU raiz.
        // LA f0 POR VENTANA SOLO SE BUSCA SI HAY REJILLA QUE SEGUIR (2026-09-25):
        // ni con el modo REJILLA FIJA declarado (el usuario fija el pitch y pide
        // que el analisis no lo siga) ni por debajo del suelo del ancla, donde el
        // HPS de la ventana no puede leer la raiz del material (medido en
        // E1 = 41.62 Hz: devuelve su 2o armonico) y una trayectoria una octava
        // arriba es peor que no seguir nada. En los dos casos el modelo sale con
        // gridFixed a true. Por encima del suelo, sin modo declarado, el camino
        // es el de siempre, bit a bit. (frameF0s ya viene inicializado con la
        // rejilla del llamador, asi que el frame se queda con ella.)
        if (f > 0 && ! fixedGrid && rootFrequency >= anchorFloorHz (sampleRate))
        {
            const float seed = detectPitchFromSpectrum(sampleRate);
            frameF0s[static_cast<size_t>(f)] = fitGridFromSpectrum(sampleRate, seed);
        }
    }

    // LA PUERTA (10.3) COMO PRE-FILTRO (10.4). Se mide SIEMPRE —tambien cuando
    // el clustering no llegue a dar capas, y tambien si el material sale vacio—
    // para que la sonda y la UI declaren mono-rejilla / bi-rejilla sin depender
    // de que existan capas que rechazar. La referencia es la rejilla del
    // llamador, que es la que usaria el modelo de capas si saliera (plegar
    // contra el material no diria nada de ESA rejilla, que es la unica que el
    // modelo puede escribir). Su veredicto decide las DOS cosas que vienen
    // despues: si se mide la rejilla comun y si corre el clustering.
    lastFold = measureOctaveFold (frameF0s, rootFrequency);
    const bool gridCommon = gridIsCommon (frameF0s, rootFrequency);
    clusteringSkipped = ! gridCommon;

    // 1b. Medida: perFrame SIEMPRE (el camino de una capa, bit a bit, con SU
    //     rejilla por frame); la rejilla COMUN solo si la puerta la declaro
    //     COMUN. El espectro se restaura del guardado: una sola FFT por frame.
    for (int f = 0; f < nFrames; ++f)
    {
        for (int b = 0; b < numBins; ++b)
            magnitudeSpectrum[static_cast<size_t>(b)] =
                frameSpectra[static_cast<size_t>(f) * static_cast<size_t>(numBins) + static_cast<size_t>(b)];

        const float f0Frame = frameF0s[static_cast<size_t>(f)];

        // FASE 11.2: la rejilla COMUN sobre el espectro del frame, ahora que la
        // puerta la declaro comun (las dos lecturas son dos barridos de
        // peak-picking sobre el mismo espectro guardado).
        if (gridCommon && rootFrequency > 0.0f)
            for (int k = 0; k < 64; ++k)
                perFrameCommon[static_cast<size_t>(f)][static_cast<size_t>(k)] =
                    measurePartial(rootFrequency * static_cast<float>(k + 1), rootFrequency, sampleRate);

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

    // 2. Normalizacion global + suelo de ruido por frame (las DOS lecturas) --
    const auto normalise = [invGlobal] (std::vector<std::array<PartialMeasurement, 64>>& table)
    {
        for (auto& m : table)
            for (int k = 0; k < 64; ++k)
            {
                m[static_cast<size_t>(k)].amplitude *= invGlobal;
                if (m[static_cast<size_t>(k)].amplitude < kPartialFloor)
                    m[static_cast<size_t>(k)].offsetHz = 0.0f;
            }
    };
    normalise (perFrame);
    if (gridCommon)
        normalise (perFrameCommon);

    // 3. FASE 11.2: capas por forma de envolvente sobre la rejilla COMUN ----
    //    El PRE-FILTRO (10.3/10.4) ya decidio: si la puerta declaro bi-rejilla,
    //    la rejilla comun no se midio y el clustering NO corre —no se paga—. Se
    //    declara el modelo honesto de una capa con f0 por frame (10.6) sin
    //    calcular unas capas que se iban a tirar (antes se corrian y se
    //    rechazaban despues de calcularlas).
    lastMetric = layerMetric;   // se publica aunque el clustering no corra
    // reset per-layer residuo (la recomputa el camino que produzca capas)
    for (int l=0;l<LayerClustering::kMaxLayers;++l){ layerGridResidCents[l] = -1.0f; layerGridObsCount[l]=0; }
    entrelazadaLayerIndex = -1;

    if (! gridCommon)
    {
        lastClustering = LayerClustering{};
        lastLayerRejected = true;
    }
    else
    {
        std::vector<float> traces(static_cast<size_t>(64 * nFrames), 0.0f);
        for (int t = 0; t < 64; ++t)
            for (int f = 0; f < nFrames; ++f)
                traces[static_cast<size_t>(t) * static_cast<size_t>(nFrames) + static_cast<size_t>(f)] =
                    perFrameCommon[static_cast<size_t>(f)][static_cast<size_t>(t)].amplitude;

        lastClustering = clusterTraces(traces, 64, nFrames,
                                       LayerClustering::kActivityFloor, layerMetric);
        lastLayerRejected = false;

        // 2026-09-27: FAMILIA ENTRELAZADA como segunda capa (sub-rejilla f0/2).
        // Usa los mismos traces y el mismo suelo que el clustering: una traza
        // es "entrelazada dominante" si en >=2 frames activos su pico cae
        // en la sub-rejilla (flag interlaced). Si hay >=2 trazas asi, se
        // produce un modelo de 2 capas (propia vs entrelazada) sobre la
        // MISMA rejilla f0: cada indice pertenece a UNA capa y su offset
        // verdadero (peakHz - k*f0) coloca SONICAMENTE la energia en el impar
        // de f0/2 (~+-0.5*f0) aunque el indice sea k. La capa entrelazada
        // declara SU f0 = f0/2 en el fichero v2.1 (extraLayers[l].f0), pero
        // el motor suma las capas indice a indice con el gridRatio de la
        // dominante (propia): el offset ya corrige la posicion, no la
        // rejilla. Si no hay familia entrelazada, cae al camino de
        // envolvente de siempre.
        std::vector<int> entreTraces;
        entreTraces.reserve(64);
        for (int t=0; t<64; ++t)
        {
            if (! lastClustering.active[(size_t)t]) continue;
            int activeCount=0, interCount=0;
            for (int f=0; f<nFrames; ++f)
            {
                const auto& m = perFrameCommon[(size_t)f][(size_t)t];
                if (m.amplitude < LayerClustering::kActivityFloor) continue;
                ++activeCount;
                if (m.interlaced) ++interCount;
            }
            if (activeCount < LayerClustering::kMinActiveFrames) continue;
            if (interCount >= 2) entreTraces.push_back(t);
        }
        const bool hasEntre = (int)entreTraces.size() >= LayerClustering::kMinLayerTraces;
        if (hasEntre)
        {
            // monta el modelo entrelazado: propia vs entrelazada
            auto layered = buildEntrelazadaModel (perFrameCommon, entreTraces, lastClustering, nFrames, rootFrequency);
            layered.gridFixed = fixedGrid;
            // residuo por capa publicado para la sonda
            // (propia capa 0 vs f0, entrelazada capa 1 vs impares de f0/2
            // con su propio LS sobre los impares).
            return layered;
        }

        if (lastClustering.layerCount > 1)
        {
            auto layered = buildLayeredModel (perFrameCommon, lastClustering, nFrames, rootFrequency);
            layered.gridFixed = fixedGrid;   // REJILLA FIJA: procedencia declarada
            // residuo por capa (envolvente) para la sonda
            for (int l=0; l<layered.layerCount; ++l)
            {
                std::vector<int> members;
                for (int t=0; t<64; ++t) if (lastClustering.layerOfTrace[(size_t)t]==l) members.push_back(t);
                layerGridResidCents[l] = layerResidualCents(perFrameCommon, members, rootFrequency, false, nFrames);
                // obs approx: cuenta picos validos de la capa
                layerGridObsCount[l] = 0;
                for (int f=0; f<nFrames; ++f)
                    for (int t: members) {
                        const auto& m = perFrameCommon[(size_t)f][(size_t)t];
                        if (m.amplitude < LayerClustering::kActivityFloor) continue;
                        if (m.interlaced) continue;
                        // windowMax check approx: skip very quiet
                        layerGridObsCount[l]++;
                    }
            }
            entrelazadaLayerIndex = -1;
            return layered;
        }
        // sin entre y sin envolvente: una sola capa, publica su residuo propio (k*f0)
        {
            std::vector<int> allActive;
            for (int t=0; t<64; ++t) if (lastClustering.active[(size_t)t]) allActive.push_back(t);
            if (! allActive.empty())
            {
                layerGridResidCents[0] = layerResidualCents(perFrameCommon, allActive, rootFrequency, false, nFrames);
                int cnt=0; for(int f=0;f<nFrames;++f) for(int t: allActive){ auto &m=perFrameCommon[(size_t)f][(size_t)t]; if(m.amplitude>=LayerClustering::kActivityFloor && !m.interlaced) ++cnt; }
                layerGridObsCount[0]=cnt;
            }
            entrelazadaLayerIndex = -1;
        }
    }

    // 4. Volcado al modelo: frame 0 canonico + extras ----------------------
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
    model.gridFixed = fixedGrid;   // REJILLA FIJA: procedencia declarada
    model.isValid = true;
    return model;
}

// PUERTA DE PLEGADO DE OCTAVA (2026-09-25, plan seccion 10.3). El plegado es
// la equivalencia de octava escrita en cents: la desviacion se reduce modulo
// 1200 al intervalo (-600, +600] — la misma convencion (y el mismo codigo) que
// measurePitchDeviation. Plegada, la evidencia de varias ventanas se puede
// COMPARAR: un salto de octava del estimador (una ventana lee f0, otra 2f0)
// colapsa a ~0 —el material sigue teniendo UNA rejilla— mientras que una raiz
// genuinamente distinta (una quinta: 701.96 -> -498.04) sobrevive. Ese es el
// discriminador mono-rejilla / bi-rejilla.
float SpectralAnalyzer::foldOctaveCents (float hz, float refHz) noexcept
{
    if (hz <= 0.0f || refHz <= 0.0f)
        return 0.0f;

    float dev = 1200.0f * std::log2 (hz / refHz);
    while (dev > 600.0f)   dev -= 1200.0f;
    while (dev <= -600.0f) dev += 1200.0f;
    return dev;
}

SpectralAnalyzer::OctaveFold SpectralAnalyzer::measureOctaveFold (const std::vector<float>& frameF0s, float refHz)
{
    OctaveFold out;

    if (refHz <= 0.0f)
        return out;   // sin referencia no hay plegado: sin veredicto

    float maxFolded = 0.0f;
    float maxRaw = 0.0f;

    for (const float f0k : frameF0s)
    {
        if (f0k <= 0.0f)
            continue;   // silencio: esta ventana no aporta evidencia

        const float raw = std::abs (1200.0f * std::log2 (f0k / refHz));
        const float folded = std::abs (foldOctaveCents (f0k, refHz));

        maxRaw = std::max (maxRaw, raw);
        maxFolded = std::max (maxFolded, folded);
        ++out.observations;

        // Media octava de desviacion CRUDA = el estimador salto de octava
        // respecto de esta referencia (el plegado lo cuenta como acuerdo).
        if (raw > 600.0f)
            ++out.octaveFlips;
    }

    if (out.observations == 0)
        return out;   // sin datos: foldCents se queda en -1 y no se exenta nada

    out.foldCents = maxFolded;
    out.rawCents = maxRaw;
    out.biGrid = maxFolded > kOctaveFoldCents;
    return out;
}

// FASE 11.2 + PUERTA DE PLEGADO: "la rejilla del llamador es COMUN a todas las
// ventanas" es exactamente el veredicto de la puerta de plegado aplicada a esa
// rejilla (basta que una ventana se salga de kOctaveFoldCents). Se mantiene como
// predicado booleano del rechazo del clustering; la medida completa
// (dispersion cruda, saltos de octava, veredicto) la publica lastOctaveFold().
bool SpectralAnalyzer::gridIsCommon (const std::vector<float>& frameF0s, float rootFrequency) const
{
    if (rootFrequency <= 0.0f)
        return false;

    return ! measureOctaveFold (frameF0s, rootFrequency).biGrid;
}

// FASE 11.2: montaje del modelo con capas. La capa 0 (la dominante) ocupa la
// raiz; los indices SIN forma temporal (activos en un solo frame) o sin
// actividad se quedan en la capa 0 con su amplitud medida, que es donde el
// camino de una capa los habria puesto.
NEURONiK::Common::SpectralModel SpectralAnalyzer::buildLayeredModel (
    const std::vector<std::array<PartialMeasurement, 64>>& frames,
    const LayerClustering& clusters, int nFrames, float rootFrequency) const
{
    NEURONiK::Common::SpectralModel model;

    model.setLayerCount (clusters.layerCount);

    for (int l = 0; l < clusters.layerCount; ++l)
    {
        model.setNumFramesOf (l, nFrames);
        model.setLayerWeightAt (l, 1.0f);   // mezcla fiel: la suma de las capas ES el modelo
        model.setLayerNameAt (l, "capa " + juce::String (l + 1));

        std::vector<float> level(static_cast<size_t>(nFrames), 0.0f);
        float peak = 0.0f;

        for (int f = 0; f < nFrames; ++f)
        {
            float* amps = model.ampsOf (l, f);
            float* offs = model.offsetsOf (l, f);
            std::fill (amps, amps + 64, 0.0f);
            std::fill (offs, offs + 64, 0.0f);

            double energy = 0.0;

            for (int k = 0; k < 64; ++k)
                if (clusters.layerOfTrace[static_cast<size_t>(k)] == l)
                {
                    amps[k] = frames[static_cast<size_t>(f)][static_cast<size_t>(k)].amplitude;
                    offs[k] = frames[static_cast<size_t>(f)][static_cast<size_t>(k)].offsetHz;
                    energy += (double) amps[k] * (double) amps[k];
                }

            level[static_cast<size_t>(f)] = (float) std::sqrt (energy);
            peak = std::max (peak, level[static_cast<size_t>(f)]);

            // Las capas son sub-indices de UNA rejilla: su f0 es la COMUN (el
            // remapeo de pitch por frame es del camino de una sola capa).
            model.setF0At (l, f, rootFrequency);
        }

        // Peso temporal w_l[f] = energia de la capa en el frame normalizada al
        // maximo de ESA capa (plan 3.6): la envolvente de la capa, 0..1 con el
        // pico en 1. El NIVEL absoluto vive en las amplitudes (la suma de las
        // capas es el modelo de una capa), asi que el motor 11.3 reconstruye
        // sumando amplitudes y usa w/layerWeight como mando, no como re-nivel.
        for (int f = 0; f < nFrames; ++f)
            model.setFrameWeightAt (l, f, peak > 0.0f ? level[static_cast<size_t>(f)] / peak : 1.0f);
    }

    model.frameSpanHz = rootFrequency;
    model.isValid = true;
    return model;
}

float SpectralAnalyzer::lastLayerGridResidCents (int layer) const noexcept
{
    if (layer<0 || layer>=LayerClustering::kMaxLayers) return -1.0f;
    return layerGridResidCents[(size_t)layer];
}
int SpectralAnalyzer::lastLayerGridObsCount (int layer) const noexcept
{
    if (layer<0 || layer>=LayerClustering::kMaxLayers) return 0;
    return layerGridObsCount[(size_t)layer];
}
juce::String SpectralAnalyzer::lastLayerBand (int layer) const
{
    const float r = lastLayerGridResidCents(layer);
    if (r < 0.0f) return "n/d";
    if (r <= GridIndicatorModel::residualGreenCents) return "verde";
    if (r <= GridIndicatorModel::residualAmberCents) return "amarillo";
    return "naranja";
}
float SpectralAnalyzer::layerResidualCents (const std::vector<std::array<PartialMeasurement,64>>& frames,
                                              const std::vector<int>& tracesOfLayer,
                                              float rootFrequency, bool isEntrelazada, int nFrames) const
{
    if (tracesOfLayer.empty() || rootFrequency <= 0.0f || nFrames <=0) return -1.0f;
    struct Obs { int harm; float peak; };
    std::vector<Obs> obs; obs.reserve((size_t)nFrames * tracesOfLayer.size());
    const float halfGrid = rootFrequency * 0.5f;
    for (int f=0; f<nFrames; ++f)
    {
        if ((int)frames.size() <= f) continue;
        float frameMax = 0.0f;
        for (int k=0;k<64;++k) frameMax = std::max(frameMax, frames[(size_t)f][(size_t)k].amplitude);
        if (frameMax <= 0.0f) continue;
        const float floor = frameMax * kPartialFloor;
        for (int t: tracesOfLayer)
        {
            const auto& m = frames[(size_t)f][(size_t)t];
            if (m.amplitude < floor) continue;
            if (m.peakHz <= 0.0f) continue;
            if (isEntrelazada)
            {
                if (! m.interlaced) continue;
                const float oddF = std::round(m.peakHz / halfGrid);
                const int odd = (int) std::lround(oddF);
                if (odd < 1 || (odd%2)==0) continue;
                const int k = t+1;
                if (oddF < (float)(2*k -1) -0.6f || oddF > (float)(2*k +1) +0.6f) continue;
                obs.push_back({odd, m.peakHz});
            }
            else
            {
                if (m.interlaced) continue;
                obs.push_back({t+1, m.peakHz});
            }
        }
    }
    if ((int)obs.size() < kMinLsObservations) return -1.0f;
    double num=0.0, den=0.0;
    for (auto& o: obs){ num += (double)o.harm * (double)o.peak; den += (double)o.harm * (double)o.harm; }
    const double fitted = den>0.0 ? num/den : 0.0;
    if (fitted <= 0.0) return -1.0f;
    double acc=0.0;
    for (auto& o: obs){ double r = 1200.0 * std::log2((double)o.peak / ((double)o.harm * fitted)); acc += r*r; }
    return (float) std::sqrt(acc / (double)obs.size());
}
NEURONiK::Common::SpectralModel SpectralAnalyzer::buildEntrelazadaModel (
        const std::vector<std::array<PartialMeasurement,64>>& frames,
        const std::vector<int>& entrelazadaTraces,
        const LayerClustering& fallbackClusters,
        int nFrames, float rootFrequency) const
{
    NEURONiK::Common::SpectralModel model;
    std::vector<int> propiaTraces; propiaTraces.reserve(64);
    for (int t=0; t<64; ++t)
    {
        const bool isEntre = std::find(entrelazadaTraces.begin(), entrelazadaTraces.end(), t) != entrelazadaTraces.end();
        if (isEntre) continue;
        if (fallbackClusters.active[(size_t)t]) propiaTraces.push_back(t);
    }
    model.setLayerCount(2);
    {
        model.setNumFramesOf(0, nFrames);
        model.setLayerWeightAt(0, 1.0f);
        model.setLayerNameAt(0, "propia");
        std::vector<float> level((size_t)nFrames, 0.0f); float peak=0.0f;
        for (int f=0; f<nFrames; ++f)
        {
            float* amps = model.ampsOf(0,f); float* offs = model.offsetsOf(0,f);
            std::fill(amps, amps+64, 0.0f); std::fill(offs, offs+64, 0.0f);
            double energy=0.0;
            for (int t: propiaTraces)
            {
                const auto& m = frames[(size_t)f][(size_t)t];
                amps[t] = m.amplitude;
                offs[t] = m.offsetHz;
                energy += (double)amps[t]*(double)amps[t];
            }
            level[(size_t)f] = (float) std::sqrt(energy); peak = std::max(peak, level[(size_t)f]);
            model.setF0At(0, f, rootFrequency);
        }
        for (int f=0; f<nFrames; ++f) model.setFrameWeightAt(0,f, peak>0.0f ? level[(size_t)f]/peak : 1.0f);
        const_cast<SpectralAnalyzer*>(this)->layerGridResidCents[0] = layerResidualCents(frames, propiaTraces, rootFrequency, false, nFrames);
        int cnt=0; for(int f=0;f<nFrames;++f) for(int t: propiaTraces){ auto &m=frames[(size_t)f][(size_t)t]; if(m.amplitude>=LayerClustering::kActivityFloor && !m.interlaced) ++cnt; }
        const_cast<SpectralAnalyzer*>(this)->layerGridObsCount[0]=cnt;
        model.frameSpanHz = rootFrequency;
    }
    {
        model.setNumFramesOf(1, nFrames);
        model.setLayerWeightAt(1, 1.0f);
        model.setLayerNameAt(1, "entrelazada");
        const float halfGrid = rootFrequency * 0.5f;
        std::vector<float> level((size_t)nFrames, 0.0f); float peak=0.0f;
        for (int f=0; f<nFrames; ++f)
        {
            float* amps = model.ampsOf(1,f); float* offs = model.offsetsOf(1,f);
            std::fill(amps, amps+64, 0.0f); std::fill(offs, offs+64, 0.0f);
            double energy=0.0;
            for (int t: entrelazadaTraces)
            {
                const auto& m = frames[(size_t)f][(size_t)t];
                amps[t] = m.amplitude;
                if (m.peakHz > 0.0f) offs[t] = m.peakHz - (float)(t+1)*rootFrequency;
                else offs[t]=0.0f;
                energy += (double)amps[t]*(double)amps[t];
            }
            level[(size_t)f]=(float)std::sqrt(energy); peak=std::max(peak, level[(size_t)f]);
            model.setF0At(1, f, halfGrid);
        }
        for (int f=0; f<nFrames;++f) model.setFrameWeightAt(1,f, peak>0.0f ? level[(size_t)f]/peak : 1.0f);
        const_cast<SpectralAnalyzer*>(this)->layerGridResidCents[1] = layerResidualCents(frames, entrelazadaTraces, rootFrequency, true, nFrames);
        int cnt=0; for(int f=0;f<nFrames;++f) for(int t: entrelazadaTraces){ auto &m=frames[(size_t)f][(size_t)t]; if(m.amplitude>=LayerClustering::kActivityFloor && m.interlaced) ++cnt; }
        const_cast<SpectralAnalyzer*>(this)->layerGridObsCount[1]=cnt;
        const_cast<SpectralAnalyzer*>(this)->entrelazadaLayerIndex = 1;
    }
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
