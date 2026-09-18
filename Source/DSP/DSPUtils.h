/*
  ==============================================================================

    DSPUtils.h
    Created: 30 Jan 2026
    Description: Centralized utilities for audio parameter validation and buffer 
                 sanitization to prevent NaN/Infinity propagation.

  ==============================================================================
*/

#pragma once

#include "DspCore.h"
#include "DspDebug.h"

#include <cmath>
#include <limits>

namespace NEURONiK::DSP {

/**
 * Validates and clamps an audio parameter to a safe range.
 * In Debug: Asserts if value is invalid.
 * In Release: Clamps silently and optionally logs warning.
 */
template<typename T>
inline T validateAudioParam(T value, T minVal, T maxVal, T fallback, const char* paramName) noexcept
{
    // Use std::isfinite to detect NaN and Infinity
    if (!std::isfinite(value) || value < minVal || value > maxVal)
    {
        // Aviso solo en Debug: dspDbg/dspAssert compilan a nada en Release
        // (misma puerta que JUCE_DEBUG), asi que la puerta explicita sobra.
        // ignoreUnused evita el C4100 de paramName cuando el aviso no existe.
        dsp::ignoreUnused (paramName);
        dspDbg ("WARNING: Invalid " << paramName << " (" << value
                                    << ") clamped to " << fallback);
        dspAssert (false);
        return fallback;
    }
    
    return dsp::jlimit(minVal, maxVal, value);
}

/**
 * Sanitizes an audio buffer, replacing NaN/Inf with silence.
 * Returns true if any invalid values were found.
 */
inline bool sanitizeAudioBuffer(dsp::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
{
    bool foundInvalid = false;
    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            if (!std::isfinite(data[i]))
            {
                dspAssert (false); // aviso en Debug; en Release solo silencia
                data[i] = 0.0f; // Silence in release
                foundInvalid = true;
            }
        }
    }
    
    return foundInvalid;
}

} // namespace NEURONiK::DSP
