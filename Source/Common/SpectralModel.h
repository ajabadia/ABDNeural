/*
  ==============================================================================

    SpectralModel.h
    Created: 27 Jan 2026
    Description: Shared definition of the Spectral Model structure.
                 Used by both NEURONiK Plugin and Model Maker.

  ==============================================================================
*/

#pragma once

#include <array>
#include <juce_core/juce_core.h>

namespace NEURONiK::Common {

/**
 * @struct SpectralModel
 * @brief Holds a snapshot of 64 partials, including amplitudes and frequency offsets.
 * This structure defines a specific timbre that the Resonator can synthesize.
 *
 * FASE 10 (2026-09-22): el modelo es TEMPORAL. `amplitudes`/`frequencyOffsets`
 * siguen siendo EL frame canonico (indice 0) en layout 1D — todo el codigo v1
 * (bridge WASM con memcpy de 128 floats, ModelMaker, cargador de presets)
 * compila sin cambios y lee el frame 0. Los frames 1..N-1 de la evolucion
 * temporal viven en extraAmps/extraOffsets; frameCount==1 significa modelo
 * estatico (todo el legado) y el motor no toca nada de lo nuevo.
 */
struct SpectralModel
{
    static constexpr int kMaxFrames = 16;

    // Frame 0 (canonico) — layout v1 EXACTO. Compat: bridge WASM, ModelMaker,
    // cargador de presets y cualquier consumidor externo.
    std::array<float, 64> amplitudes;
    std::array<float, 64> frequencyOffsets;

    // Frames 1..kMaxFrames-1 (evolucion temporal; solo validos si
    // frameCount > k). frameCount==1 => estatico, aqui no hay nada util.
    std::array<std::array<float, 64>, kMaxFrames - 1> extraAmps;
    std::array<std::array<float, 64>, kMaxFrames - 1> extraOffsets;

    int  frameCount = 1;        // 1..kMaxFrames; 1 = modelo estatico (legado)
    // FASE 10: espaciado entre parciales del analisis (f0 del frame), en Hz.
    // 0 = desconocido (modelos sinteticos): el sampler usa el espaciado
    // armonico unitario n*f0. Dimensiona la envoltura de offsets (camino corto).
    float frameSpanHz = 0.0f;
    bool isValid = false;

    int numFrames() const noexcept { return frameCount; }

    /** @brief Amplitud del parcial i en el frame k (0..frameCount-1), sin copia. */
    float ampAt (int frame, int partial) const noexcept
    {
        return frame <= 0 ? amplitudes[(size_t) partial]
                          : extraAmps[(size_t) frame - 1][(size_t) partial];
    }

    /** @brief Offset de frecuencia del parcial i en el frame k. */
    float offsetAt (int frame, int partial) const noexcept
    {
        return frame <= 0 ? frequencyOffsets[(size_t) partial]
                          : extraOffsets[(size_t) frame - 1][(size_t) partial];
    }

    float* ampsOf (int frame) noexcept
    {
        return frame <= 0 ? amplitudes.data() : extraAmps[(size_t) frame - 1].data();
    }

    float* offsetsOf (int frame) noexcept
    {
        return frame <= 0 ? frequencyOffsets.data() : extraOffsets[(size_t) frame - 1].data();
    }

    const float* ampsOf (int frame) const noexcept
    {
        return frame <= 0 ? amplitudes.data() : extraAmps[(size_t) frame - 1].data();
    }

    const float* offsetsOf (int frame) const noexcept
    {
        return frame <= 0 ? frequencyOffsets.data() : extraOffsets[(size_t) frame - 1].data();
    }
};

} // namespace NEURONiK::Common
