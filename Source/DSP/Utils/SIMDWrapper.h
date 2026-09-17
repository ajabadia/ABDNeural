/*
  ==============================================================================

    SIMDWrapper.h
    Created: 31 Jan 2026
    Description: Robust portability layer for SIMD operations.

    DOS ramas con API identica:
      - Nativo: juce::dsp::SIMDRegister (SSE/NEON), codigo original intacto.
      - WASM (__EMSCRIPTEN__): fallback escalar de 4 lanes en float[4].
        JUCE 8.0.12 no define SIMDRegister bajo Emscripten (JUCE_USE_SIMD=0:
        no existe backend SIMD wasm y forzarlo a 1 choca con un #error interno
        de juce_dsp.h). El fallback escalar es exactamente la semantica de
        SIMDRegister<float> (4 lanes, fromRawArray carga 4 floats), asi que el
        resultado numerico es el mismo que el path nativo en maquina sin AVX.

    Nota determinismo: las dos ramas producen el mismo resultado muestra a
    muestra (aritmetica IEEE idempotente), por lo que la paridad bit-exacta
    del DSPReferenceTest no se ve afectada (ese test compila en nativo, que
    usa la rama JUCE).

  ==============================================================================
*/

#pragma once

#include <cmath>

#if ! defined (__EMSCRIPTEN__)

// ============================================================================
// RAMA NATIVA: juce::dsp::SIMDRegister (SSE/NEON/escalar, segun CPU)
// ============================================================================

#include <juce_dsp/juce_dsp.h>

namespace NEURONiK::DSP::Utils {

/**
 * A portable wrapper for SIMD operations.
 * Uses juce::dsp::SIMDRegister to target SSE, NEON, or Scalar depending on platform.
 */
using SIMDFloat = juce::dsp::SIMDRegister<float>;

/**
 * Loads 4 floats from a pointer into a SIMD register (unaligned).
 */
inline SIMDFloat loadUnaligned(const float* ptr) noexcept
{
    // Use factory method for safety
    return juce::dsp::SIMDRegister<float>::fromRawArray(ptr);
}

/**
 * Stores a SIMD register into a pointer (unaligned).
 */
inline void storeUnaligned(float* ptr, SIMDFloat v) noexcept
{
    v.copyToRawArray(ptr);
}

/**
 * Broadcasts a single float to all elements of the register.
 */
inline SIMDFloat setAll(float v) noexcept
{
    return juce::dsp::SIMDRegister<float>(v);
}

/**
 * Return a zero-initialized SIMD register.
 */
inline SIMDFloat setZero() noexcept
{
    return juce::dsp::SIMDRegister<float>(0.0f);
}

/**
 * Returns the sum of all elements in the register.
 */
inline float sumRegister(SIMDFloat v) noexcept
{
    float vals[4];
    v.copyToRawArray(vals);
    return vals[0] + vals[1] + vals[2] + vals[3];
}

/**
 * Selects elements from 'a' if 'mask' is true, else from 'b'.
 */
template <typename MaskType>
inline SIMDFloat simdSelect(MaskType mask, SIMDFloat a, SIMDFloat b) noexcept
{
    // In newer JUCE versions, MaskType has a select method.
    // In older ones, we use bitwise ops by casting to int register or using the Scalar fallback.
    // The scalar loop is the most portable way when JUCE versions are uncertain.
    float va[4], vb[4], vr[4];
    a.copyToRawArray(va);
    b.copyToRawArray(vb);

    // JUCE MaskType usually has operator[]
    for (int i = 0; i < 4; ++i)
        vr[i] = mask[i] ? va[i] : vb[i];

    return SIMDFloat::fromRawArray(vr);
}

/**
 * Returns the maximum of two registers.
 */
inline SIMDFloat simdMax(SIMDFloat a, SIMDFloat b) noexcept
{
    // Use static method to avoid operator > ambiguity
    return simdSelect(juce::dsp::SIMDRegister<float>::greaterThan(a, b), a, b);
}

/**
 * Returns the absolute value of each element in the register.
 */
inline SIMDFloat absRegister(SIMDFloat v) noexcept
{
    // std::abs is often not overloaded for SIMDRegister, so we use max(v, -v)
    // but we use setZero() - v to be extra safe with operators
    return simdMax(v, setZero() - v);
}

/** Mascara de comparacion por lanes (SIMDRegister<uint32> en JUCE). */
using SIMDMask = juce::dsp::SIMDRegister<unsigned int>;

/** Comparacion por lanes (>=), misma semantica que SIMDRegister. */
inline SIMDMask simdGreaterThanOrEqual(SIMDFloat a, SIMDFloat b) noexcept
{
    return juce::dsp::SIMDRegister<float>::greaterThanOrEqual(a, b);
}

} // namespace NEURONiK::DSP::Utils

#else // __EMSCRIPTEN__

// ============================================================================
// RAMA WASM: fallback escalar de 4 lanes. API identica a la rama nativa.
// ============================================================================

namespace NEURONiK::DSP::Utils {

/** 4 lanes escalares: semantica exacta de SIMDRegister<float> en WASM. */
struct SIMDFloat
{
    float lanes[4] { 0.0f, 0.0f, 0.0f, 0.0f };

    SIMDFloat() noexcept = default;

    explicit SIMDFloat (float v) noexcept
    {
        lanes[0] = lanes[1] = lanes[2] = lanes[3] = v;
    }

    static SIMDFloat fromRawArray (const float* ptr) noexcept
    {
        SIMDFloat r;
        r.lanes[0] = ptr[0];
        r.lanes[1] = ptr[1];
        r.lanes[2] = ptr[2];
        r.lanes[3] = ptr[3];
        return r;
    }

    void copyToRawArray (float* ptr) const noexcept
    {
        ptr[0] = lanes[0];
        ptr[1] = lanes[1];
        ptr[2] = lanes[2];
        ptr[3] = lanes[3];
    }

    float operator[] (int i) const noexcept { return lanes[i]; }
};

inline SIMDFloat operator+ (SIMDFloat a, SIMDFloat b) noexcept
{
    SIMDFloat r;
    for (int i = 0; i < 4; ++i) r.lanes[i] = a.lanes[i] + b.lanes[i];
    return r;
}

inline SIMDFloat operator- (SIMDFloat a, SIMDFloat b) noexcept
{
    SIMDFloat r;
    for (int i = 0; i < 4; ++i) r.lanes[i] = a.lanes[i] - b.lanes[i];
    return r;
}

inline SIMDFloat operator* (SIMDFloat a, SIMDFloat b) noexcept
{
    SIMDFloat r;
    for (int i = 0; i < 4; ++i) r.lanes[i] = a.lanes[i] * b.lanes[i];
    return r;
}

inline SIMDFloat& operator+= (SIMDFloat& a, SIMDFloat b) noexcept { a = a + b; return a; }
inline SIMDFloat& operator-= (SIMDFloat& a, SIMDFloat b) noexcept { a = a - b; return a; }

/** Mascara por lanes (bool escalar por lane), como SIMDRegister. */
struct SIMDMask
{
    bool b[4] { false, false, false, false };
    bool operator[] (int i) const noexcept { return b[i]; }
};

inline SIMDMask simdGreaterThanOrEqual (SIMDFloat a, SIMDFloat b) noexcept
{
    SIMDMask m;
    for (int i = 0; i < 4; ++i) m.b[i] = a.lanes[i] >= b.lanes[i];
    return m;
}

inline SIMDFloat loadUnaligned (const float* ptr) noexcept { return SIMDFloat::fromRawArray (ptr); }
inline void    storeUnaligned (float* ptr, SIMDFloat v) noexcept { v.copyToRawArray (ptr); }

inline SIMDFloat setAll (float v) noexcept  { return SIMDFloat (v); }
inline SIMDFloat setZero() noexcept         { return SIMDFloat (0.0f); }

inline float sumRegister (SIMDFloat v) noexcept
{
    return v.lanes[0] + v.lanes[1] + v.lanes[2] + v.lanes[3];
}

inline SIMDFloat simdSelect (SIMDMask mask, SIMDFloat a, SIMDFloat b) noexcept
{
    SIMDFloat r;
    for (int i = 0; i < 4; ++i)
        r.lanes[i] = mask[i] ? a.lanes[i] : b.lanes[i];
    return r;
}

inline SIMDFloat simdMax (SIMDFloat a, SIMDFloat b) noexcept
{
    SIMDFloat r;
    for (int i = 0; i < 4; ++i) r.lanes[i] = a.lanes[i] > b.lanes[i] ? a.lanes[i] : b.lanes[i];
    return r;
}

inline SIMDFloat absRegister (SIMDFloat v) noexcept
{
    SIMDFloat r;
    for (int i = 0; i < 4; ++i) r.lanes[i] = std::abs (v.lanes[i]);
    return r;
}

} // namespace NEURONiK::DSP::Utils

#endif // __EMSCRIPTEN__
