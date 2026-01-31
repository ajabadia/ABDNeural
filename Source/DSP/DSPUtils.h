/*
  ==============================================================================

    DSPUtils.h
    Created: 30 Jan 2026
    Description: Centralized utilities for audio parameter validation and buffer 
                 sanitization to prevent NaN/Infinity propagation.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
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
        #if JUCE_DEBUG
        // Only log in debug to avoid performance hit in release, 
        // though it shouldn't happen often if we are careful.
        DBG("WARNING: Invalid " << paramName << " (" << value << ") clamped to " << fallback);
        jassertfalse; 
        #endif
        return fallback;
    }
    
    return juce::jlimit(minVal, maxVal, value);
}

/**
 * Sanitizes an audio buffer, replacing NaN/Inf with silence.
 * Returns true if any invalid values were found.
 */
inline bool sanitizeAudioBuffer(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
{
    bool foundInvalid = false;
    
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            if (!std::isfinite(data[i]))
            {
                #if JUCE_DEBUG
                jassertfalse; // Alert in debug
                #endif
                data[i] = 0.0f; // Silence in release
                foundInvalid = true;
            }
        }
    }
    
    return foundInvalid;
}

} // namespace NEURONiK::DSP
