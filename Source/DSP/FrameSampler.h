#pragma once

#include "../Common/SpectralModel.h"

namespace NEURONiK::Common {

/**
 * @brief Interpola el frame z (0..1 mapeado a [0..frameCount-1]) en `out`.
 *
 * `out` sale como snapshot estático (frameCount=1): es lo que el morfeo
 * bilineal existente consume hoy. Real-time safe (sin asignaciones).
 * Coste O(128) por llamada — la llamada es UNA por sub-bloque de 32 muestras
 * y por slot (4 slots => ~16k floats/s, despreciable).
 *
 * La frecuencia del parcial i es baseFreq*n_i + offset_i: interpolar el
 * offset linealmente ES interpolar la frecuencia linealmente. Un offset
 * absoluto grande (parcial que "salta" de armónico vecino) barredorea el
 * espectro — el diseño (TEMPORAL_MODELS_PLAN §2.2) manda interpolación por
 * camino corto envolviendo por el espaciado armónico. Sin f0 aqui, la guardia
 * conservadora es medio espaciado armónico UNITARIO (0.5*n sobre el offset)
 * cuando el fichero no declara frameSpanHz; si lo declara, la envoltura usa
 * 0.5*span (dimensión Hz real). El escritor (serialización v2) acota
 * |offset| < 0.5*span: una sola envoltura basta y los extremos z=0/z=1 se
 * copian BIT-EXACTOS (ramas tempranas).
 */
/** Copia el frame k de src a out como snapshot estático (bit-exacto). */
inline void copyFrame (const SpectralModel& src, int k, SpectralModel& out) noexcept
{
    const float* a = src.ampsOf (k);
    const float* o = src.offsetsOf (k);
    for (int i = 0; i < 64; ++i)
    {
        out.amplitudes[(size_t) i]       = a[i];
        out.frequencyOffsets[(size_t) i] = o[i];
    }
}

inline void sampleFrame (const SpectralModel& src, float z, SpectralModel& out) noexcept
{
    const int n = src.frameCount;
    out.frameCount = 1;
    out.isValid = src.isValid;

    if (n <= 1)
    {
        out.amplitudes = src.amplitudes;             // ruta v1: copia trivial
        out.frequencyOffsets = src.frequencyOffsets;
        out.frameF0 = 0.0f;                          // estatico: sin remapeo
        return;
    }

    // Extremos: copia directa del frame (bit-exacto, sin redondeo de lerp).
    // En los extremos el snapshot ES el frame: sin remapeo (frameF0 = 0).
    if (z <= 0.0f) { copyFrame (src, 0, out); out.frameF0 = 0.0f; return; }
    if (z >= 1.0f) { copyFrame (src, n - 1, out); out.frameF0 = 0.0f; return; }

    const float t = juce::jlimit (0.0f, 1.0f, z) * (float) (n - 1);
    const int i0 = (int) t;
    const float frac = t - (float) i0;

    const float* a0   = src.ampsOf (i0);
    const float* a1   = src.ampsOf (i0 + 1);
    const float* off0 = src.offsetsOf (i0);
    const float* off1 = src.offsetsOf (i0 + 1);

    // FASE 10.6: la raiz por frame se interpola en dominio LOG (musical:
    // el camino D4->E5 de un barrido es lineal en semitonos, no en Hz) y
    // viaja como ratio (f0_interpolada / f0_del_frame_0) para que el motor
    // remapee la rejilla con UNA multiplicacion por parcial.
    const float f0a = src.f0At (i0);
    const float f0b = src.f0At (i0 + 1);
    float ratio = 1.0f;
    if (f0a > 0.0f && f0b > 0.0f)
        ratio = std::exp2 ((std::log2 (f0b) - std::log2 (f0a)) * frac);
    out.frameF0 = f0b > 0.0f ? f0a * ratio : 0.0f;

    const float span = src.frameSpanHz > 0.0f ? src.frameSpanHz : 0.0f;

    for (int i = 0; i < 64; ++i)
    {
        out.amplitudes[(size_t) i] = a0[i] + (a1[i] - a0[i]) * frac;

        // Camino corto: envuelve el delta por el espaciado (span real en Hz;
        // fallback armónico unitario n*f0 si el fichero no lo declara). Con el
        // clamp del escritor (|offset| < span/2) una envoltura basta.
        const float fallback = (float) (i + 1);
        const float gap      = span > 0.0f ? span : fallback;
        const float d        = off1[i] - off0[i];
        const float wrapped  = (d >  0.5f * gap) ? d - gap
                             : (d < -0.5f * gap) ? d + gap
                                                 : d;
        out.frequencyOffsets[(size_t) i] = off0[i] + wrapped * frac;
    }
}


} // namespace NEURONiK::Common
