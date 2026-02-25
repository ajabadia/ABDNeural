/*
  ==============================================================================

    SIMDWrapper.h
    Created: 31 Jan 2026
    Description: Robust portability layer for SIMD operations.
                 Uses static methods of juce::dsp::SIMDRegister to avoid 
                 operator ambiguity and cross-version issues.

  ==============================================================================
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>

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
 * Broadcasts a single float to all elements of a SIMD register.
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

} // namespace NEURONiK::DSP::Utils
