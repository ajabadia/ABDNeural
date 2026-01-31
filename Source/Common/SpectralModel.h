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
 */
struct SpectralModel
{
    std::array<float, 64> amplitudes;
    std::array<float, 64> frequencyOffsets;
    bool isValid = false;

    // Optional: Add JSON serialization helper later
};

} // namespace NEURONiK::Common
