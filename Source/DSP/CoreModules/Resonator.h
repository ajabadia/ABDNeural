/*
  ==============================================================================

    Resonator.h
    Created: 21 Jan 2026
    Description: Additive synthesis core with 64 partials.
                 Now supports 2D spectral model morphing.

  ==============================================================================
*/

#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include "Oscillator.h"
#include "SpectralModel.h"
#include "../FrameSampler.h"   // FASE 11.3: LayerMorphZ + sampler por capa

namespace NEURONiK::DSP::Core {

using NEURONiK::Common::SpectralModel;

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

class Resonator {
public:
    Resonator() noexcept;
    ~Resonator() = default;

    void setSampleRate(double sr) noexcept;
    void setBaseFrequency(float hz) noexcept;

    void loadModel(const SpectralModel& model, int slot) noexcept;

    /**
     * FASE 10: eje temporal. z en 0..1 mapea [frame0..frameN-1] de CADA slot
     * (modelos estaticos no aportan: su z no tiene efecto). El refresco del
     * cache es O(128) por slot y SOLO cuando z cambia (latch), llamado desde
     * updateHarmonicsFromModels.
     */
    void setMorphZ (float z) noexcept { setLayerMorphZ (0, z); }

    /**
     * FASE 11.4: el VOLUMEN de UNA capa (0..1) en el frame efectivo.
     * Es otro parametro de voz del slot —como su z (11.3)— pero la capa a
     * 0.0 queda CALLADA (y su z deja de importar); a 1.0 suena entera.
     * Con el reparto del analizador (cada indice pertenece a UNA capa)
     * el gesto es lineal sobre esa capa, no un barrido de frames: MANDA
     * EL Z de la capa, y la ganancia la sube y baja.
     */
    void setLayerGain (int layer, float gain) noexcept
    {
        constexpr int kMax = NEURONiK::Common::SpectralModel::kMaxLayers;
        layerGains[(size_t) juce::jlimit (0, kMax - 1, layer)] =
            juce::jlimit (0.0f, 1.0f, gain);
    }


    /**
     * FASE 11.3: el eje temporal de UNA capa (0 = `morphZ` de siempre, 1..2 =
     * `morphZ2`/`morphZ3`). Solo tiene efecto si el modelo cargado TIENE esa
     * capa: con layerCount == 1 los z de arriba quedan inertes y el sonido es
     * el de siempre, bit a bit.
     */
    void setLayerMorphZ (int layer, float z) noexcept
    {
        layerZ[(size_t) juce::jlimit (0, SpectralModel::kMaxLayers - 1, layer)] =
            juce::jlimit (0.0f, 1.0f, z);
    }

    // --- Real-time safe processing ---
    void updateHarmonicsFromModels(float morphX, float morphY) noexcept;
    const SpectralModel& frameForSlot (int slot) noexcept;
    void setStretching(float amount) noexcept;
    void setEntropy(float amount) noexcept;
    void setParity(float amount) noexcept;
    void setShift(float amount) noexcept;
    void setRollOff(float amount) noexcept;
    void setUnison(float detune, float spread) noexcept;

    /**
     * Renderiza UNA muestra del bloque.
     * @param sampleIdx indice dentro del bloque actual (0..numSamples-1). Solo lo
     *                  usa la ruta de entropia, que lee su jitter por indice; la
     *                  ruta SIMD (por defecto) no depende de el.
     */
    float processSample(int sampleIdx) noexcept;
    void reset() noexcept;

    /**
     * Pre-reserva los buffers de jitter. NO es real-time safe (asigna memoria):
     * se llama UNA vez desde prepare(), de modo que prepareEntropy nunca reserva
     * dentro del hilo de audio (regla ZERO ALLOCATIONS del proyecto).
     * @param maxBlockSize tamano maximo de bloque que se va a renderizar.
     */
    void prepareJitterBuffers(int maxBlockSize);

    /**
     * Rellena el jitter del bloque. Real-time safe: sin asignaciones ni locks.
     * Si el host entrega un bloque mayor que lo reservado, el jitter se recicla
     * por modulo (determinista) en lugar de reservar aqui.
     */
    void prepareEntropy(int numSamples) noexcept;

    const std::array<float, 64>& getPartialAmplitudes() const noexcept { return partialAmplitudes; }
    const std::array<SpectralModel, 4>& getModels() const noexcept { return models; }

private:
    std::array<Oscillator, 128> partials;
    std::array<float, 64> partialAmplitudes; // 64 for visualization (main engine)
    
    // Four models for 2D morphing (A, B, C, D)
    std::array<SpectralModel, 4> models;

    // FASE 10: cache de frames muestreados (lo que realmente morfean los
    // bucles). frameCacheValid=false obliga a refrescar (loadModel/z nuevo).
    // FASE 11.3: el cache es del frame EFECTIVO del slot (la SUMA de sus capas,
    // cada una con su propio z).
    std::array<SpectralModel, 4> frameCache;
    NEURONiK::Common::LayerMorphZ layerZ = NEURONiK::Common::restLayerMorphZ();
    NEURONiK::Common::LayerGains layerGains = NEURONiK::Common::restLayerGains();
    NEURONiK::Common::LayerMorphZ lastLayerZ { { -1.0f, -1.0f, -1.0f } };
    NEURONiK::Common::LayerMorphZ lastConsumedZ { { -1.0f, -1.0f, -1.0f } };
    NEURONiK::Common::LayerGains lastLayerGains { -1.0f, -1.0f, -1.0f };
    bool  frameCacheValid = false;

    float baseFrequency = 440.0f;
    double sampleRate = 48000.0;
    
    float stretchingAmount = 0.0f;
    float entropyAmount = 0.0f;
    float parityAmount = 0.5f;
    float shiftAmount = 1.0f;
    float rollOffAmount = 1.0f;
    float unisonDetune = 0.01f;
    float unisonSpread = 0.5f;


    // State for optimization
    float lastMorphX = -1.0f;
    float lastMorphY = -1.0f;
    float lastBaseFreq = -1.0f;
    float lastStrecth = -1.0f;
    float lastParity = -1.0f;
    float lastShift = -1.0f;
    float lastRollOff = -1.0f;
    float lastUnisonDetune = -1.0f;
    bool modelChanged = true;

    // Fast random seed
    uint32_t randomSeed = 1234567;

    // SIMD Buffers
    alignas(16) float currentPhases[128] = {0};
    alignas(16) float phaseIncrements[128] = {0};
    alignas(16) float amplitudes_v[128] = {0};

    std::array<float, 64> lnTable;

    // Entropy Buffers (for block processing). Se reservan en prepareJitterBuffers().
    std::vector<float> ampJitterBuffer;
    std::vector<float> phaseJitterBuffer;
    int jitterLength = 0;
};

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

} // namespace NEURONiK::DSP::Core
