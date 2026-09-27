/*
  ==============================================================================

    ResonatorBank.h
    Created: 30 Jan 2026
    Description: A bank of 64 resonant filters (BPF) for spectral modeling.
                 Inspired by physical modeling and the Neurotik concept.

  ==============================================================================
*/

#pragma once

#include <array>
#include "../../Common/SpectralModel.h"
#include "../FrameSampler.h"   // FASE 11.3: LayerMorphZ + sampler por capa

namespace NEURONiK::DSP::Core {

/**
 * Specialized lightweight biquad for the ResonatorBank.
 * Optimized for speed in banks of 64. No atomics in process.
 */
struct ResonatorBiquad {
    float z1 = 0.0f, z2 = 0.0f;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    inline float processSample(float input) noexcept {
        float output = b0 * input + z1;
        z1 = (b1 * input) - (a1 * output) + z2;
        z2 = (b2 * input) - (a2 * output);
        return output;
    }

    void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }
};

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

class ResonatorBank {
public:
    ResonatorBank() noexcept;
    ~ResonatorBank() = default;

    void setSampleRate(double sr) noexcept;
    void setBaseFrequency(float hz) noexcept;
    void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) noexcept;

    /** FASE 10: eje temporal de la CAPA 0 (mismo contrato que Resonator::setMorphZ). */
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
     * FASE 11.3: el eje temporal de UNA capa. La capa 0 es `morphZ` (el de
     * siempre); las capas 1..2 son `morphZ2`/`morphZ3` y solo tienen efecto si
     * el modelo cargado TIENE esa capa: un modelo v2 puro (layerCount == 1)
     * ignora los z de arriba, que es lo que deja el legado bit-exacto.
     */
    void setLayerMorphZ (int layer, float z) noexcept
    {
        constexpr int kMaxLayers = NEURONiK::Common::SpectralModel::kMaxLayers;
        layerZ[(size_t) juce::jlimit (0, kMaxLayers - 1, layer)] = juce::jlimit (0.0f, 1.0f, z);
    }

    // --- Real-time safe processing ---
    void updateParameters(float morphX, float morphY, float resonance, float detune) noexcept;
    const NEURONiK::Common::SpectralModel& frameForSlot (int slot) noexcept;
    
    float processSample(float excitation) noexcept;
    void reset() noexcept;

    const std::array<float, 64>& getPartialAmplitudes() const noexcept { return partialAmplitudes; }

private:
    void updateFilterCoefficients(int index, float partialFreq, float q, float amp, float detuneVal) noexcept;

    std::array<ResonatorBiquad, 128> resonators;
    std::array<float, 64> partialAmplitudes;
    std::array<NEURONiK::Common::SpectralModel, 4> models;

    // FASE 10: cache de frames muestreados (mismo esquema que Resonator).
    // FASE 11.3: el cache es del frame EFECTIVO del slot, es decir de la SUMA
    // de sus capas (Common::sampleLayeredFrame), cada una con su z.
    std::array<NEURONiK::Common::SpectralModel, 4> frameCache;
    NEURONiK::Common::LayerMorphZ layerZ = NEURONiK::Common::restLayerMorphZ();
    NEURONiK::Common::LayerGains layerGains = NEURONiK::Common::restLayerGains();
    NEURONiK::Common::LayerMorphZ lastLayerZ { { -1.0f, -1.0f, -1.0f } };
    NEURONiK::Common::LayerMorphZ lastConsumedZ { { -1.0f, -1.0f, -1.0f } };
    NEURONiK::Common::LayerGains lastLayerGains { -1.0f, -1.0f, -1.0f };
    bool  frameCacheValid = false;

    float baseFrequency = 440.0f;
    double sampleRate = 48000.0;
    
    // State for optimization
    float lastMorphX = -5.0f;
    float lastMorphY = -5.0f;
    float lastRes = -5.0f;
    float lastDetune = -5.0f;
    float lastBaseFreq = -5.0f;
    bool modelChanged = true;

    // SIMD Buffers (Aligned for SSE 128-bit)
    alignas(16) float b0_v[128] = {0}, b1_v[128] = {0}, b2_v[128] = {0};
    alignas(16) float a1_v[128] = {0}, a2_v[128] = {0};
    alignas(16) float z1_v[128] = {0}, z2_v[128] = {0};
    alignas(16) float partialAmplitudes_v[128] = {0};
};

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

} // namespace NEURONiK::DSP::Core
