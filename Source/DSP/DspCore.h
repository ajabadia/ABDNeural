/*
  ==============================================================================

    DspCore.h
    Utilidades del nucleo DSP libres de JUCE.

    Puertos LITERALES de JUCE 8.0.12 (solo cambia el namespace):
      - juce_core/maths/juce_MathsFunctions.h  (jmin/jmax/jmap/jlimit,
        MathConstants, ignoreUnused, approximatelyEqual)
      - juce_audio_basics/buffers/juce_FloatVectorOperations.h/.cpp
        (ScopedNoDenormals, incluida la mascara MXCSR 0x8040: FTZ|DAZ)

    Regla de la casa (roadmap, "quitar juce_* del motor interno"): el port es
    una copia literal del fuente JUCE con el mismo orden de operaciones; el
    audio no puede cambiar ni un bit. Verificado por DSPReferenceTest
    (bit-exacto) y la paridad WASM<->nativo.

    WASM (emscripten): DSP_HAS_SSE_INTRINSICS no se define -> ScopedNoDenormals
    es un no-op, igual que en JUCE cuando no hay SSE/NEON (WebAssembly no
    flusha denormales y no necesita esta proteccion).

  ==============================================================================
*/

#pragma once

#include <cassert>
#include <cmath>
#include <cstdint>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
  #define DSP_HAS_SSE_INTRINSICS 1
#elif defined(__SSE2__)
  #define DSP_HAS_SSE_INTRINSICS 1
#endif

#if defined(DSP_HAS_SSE_INTRINSICS)
  #include <xmmintrin.h>
#endif

namespace dsp
{

//==============================================================================
/** Handy function for avoiding unused variables warning.
    Port literal de dsp::ignoreUnused. */
template <typename... Types>
void ignoreUnused (Types&&...) noexcept {}

//==============================================================================
/** Common constants.
    Port literal de dsp::MathConstants (mismos literales long double). */
template <typename FloatType>
struct MathConstants
{
    /** A predefined value for Pi */
    static constexpr FloatType pi = static_cast<FloatType> (3.141592653589793238L);

    /** A predefined value for 2 * Pi */
    static constexpr FloatType twoPi = static_cast<FloatType> (2 * 3.141592653589793238L);

    /** A predefined value for Pi / 2 */
    static constexpr FloatType halfPi = static_cast<FloatType> (3.141592653589793238L / 2);

    /** A predefined value for Euler's number */
    static constexpr FloatType euler = static_cast<FloatType> (2.71828182845904523536L);

    /** A predefined value for sqrt (2) */
    static constexpr FloatType sqrt2 = static_cast<FloatType> (1.4142135623730950488L);
};

//==============================================================================
/** Equivalent to operator==, but suppresses float-equality warnings.
    Sustituto del juce::approximatelyEqual de dos argumentos tal y como lo
    consumen las aserciones de JUCE (solo se usa dentro de dspAssert); la
    version con tolerancias de JUCE no aporta nada aqui y no cruza al audio. */
template <typename Type>
constexpr bool approximatelyEqual (Type a, Type b) noexcept
{
    return a == b;
}

//==============================================================================
// Asercion del port (sustituye a jassert): aborta en Debug, se compila fuera
// en Release. Nunca afecta al resultado numerico.

#if ! defined (NDEBUG)
  #define dspAssert(expression)      assert (expression)
#else
  #define dspAssert(expression)      static_cast<void> (true && (expression))
#endif

//==============================================================================
// Some indispensable min/max functions
// Ports literales de dsp::jmax / dsp::jmin.

/** Returns the larger of two values. */
template <typename Type>
constexpr Type jmax (Type a, Type b)                                   { return a < b ? b : a; }

/** Returns the larger of three values. */
template <typename Type>
constexpr Type jmax (Type a, Type b, Type c)                           { return a < b ? (b < c ? c : b) : (a < c ? c : a); }

/** Returns the larger of four values. */
template <typename Type>
constexpr Type jmax (Type a, Type b, Type c, Type d)                   { return jmax (a, jmax (b, c, d)); }

/** Returns the smaller of two values. */
template <typename Type>
constexpr Type jmin (Type a, Type b)                                   { return b < a ? b : a; }

/** Returns the smaller of three values. */
template <typename Type>
constexpr Type jmin (Type a, Type b, Type c)                           { return b < a ? (c < b ? c : b) : (c < a ? c : a); }

/** Returns the smaller of four values. */
template <typename Type>
constexpr Type jmin (Type a, Type b, Type c, Type d)                   { return jmin (a, jmin (b, c, d)); }

/** Remaps a normalised value (between 0 and 1) to a target range.
    This effectively returns (targetRangeMin + value0To1 * (targetRangeMax - targetRangeMin)).
    Port literal de dsp::jmap (3 argumentos). */
template <typename Type>
constexpr Type jmap (Type value0To1, Type targetRangeMin, Type targetRangeMax)
{
    return targetRangeMin + value0To1 * (targetRangeMax - targetRangeMin);
}

/** Remaps a value from a source range to a target range.
    Port literal de dsp::jmap (5 argumentos). */
template <typename Type>
Type jmap (Type sourceValue, Type sourceRangeMin, Type sourceRangeMax, Type targetRangeMin, Type targetRangeMax)
{
    dspAssert (! approximatelyEqual (sourceRangeMax, sourceRangeMin)); // mapping from a range of zero will produce NaN!
    return targetRangeMin + ((targetRangeMax - targetRangeMin) * (sourceValue - sourceRangeMin)) / (sourceRangeMax - sourceRangeMin);
}

/** Constrains a value to keep it within a given range.
    Port literal de dsp::jlimit (mismo orden de comparaciones). */
template <typename Type>
Type jlimit (Type lowerLimit,
             Type upperLimit,
             Type valueToConstrain) noexcept
{
    dspAssert (lowerLimit <= upperLimit); // if these are in the wrong order, results are unpredictable

    return valueToConstrain < lowerLimit ? lowerLimit
                                         : (upperLimit < valueToConstrain ? upperLimit
                                                                          : valueToConstrain);
}

//==============================================================================
/** Helper class providing an RAII-based mechanism for temporarily disabling
    denormals on your CPU.
    Port literal de dsp::ScopedNoDenormals: misma mascara 0x8040 (FTZ | DAZ)
    sobre el MXCSR via _mm_getcsr/_mm_setcsr. */
class ScopedNoDenormals
{
public:
    ScopedNoDenormals() noexcept
    {
      #if defined (DSP_HAS_SSE_INTRINSICS)
        intptr_t mask = 0x8040;

        fpsr = (intptr_t) _mm_getcsr();
        _mm_setcsr ((unsigned int) (fpsr | mask));
      #endif
    }

    ~ScopedNoDenormals() noexcept
    {
      #if defined (DSP_HAS_SSE_INTRINSICS)
        _mm_setcsr ((unsigned int) fpsr);
      #endif
    }

private:
  #if defined (DSP_HAS_SSE_INTRINSICS)
    intptr_t fpsr;
  #endif
};

//==============================================================================
// Aliases enteros usados por el DSP (equivalente a los de juce_core).
using uint32 = uint32_t;
using uint64 = uint64_t;

} // namespace dsp
