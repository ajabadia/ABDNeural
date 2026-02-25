/*
  ==============================================================================

    IVisualizationSource.h
    Created: 31 Jan 2026
    Description: Interface for data sources providing real-time visualization data.

  ==============================================================================
*/

#pragma once

#include <atomic>
#include <array>

namespace NEURONiK::DSP {

/**
 * Interface that the UI components (Visualizers, LCDs) use to pull data from the engine.
 * This decouples the Processor from specific GUI implementation details.
 */
class IVisualizationSource {
public:
    virtual ~IVisualizationSource() = default;

    /** Returns the spectral data (e.g. partial amplitudes) for the UI. */
    virtual void getSpectralDataForUI(float* destination64) const noexcept = 0;

    /** Returns the current envelope levels (Amp, Filter). */
    virtual void getEnvelopeLevelsForUI(float& amp, float& filter) const noexcept = 0;

    /** Returns the current LFO values (0 or 1). */
    virtual float getLfoValueForUI(int lfoIndex) const noexcept = 0;

    /** Returns a modulation value for a specific target. */
    virtual float getModulationValueForUI(int targetIndex) const noexcept = 0;

    /** Returns the XY Pad coordinates. */
    virtual void getMorphCoordinatesForUI(float& x, float& y) const noexcept = 0;
};

} // namespace NEURONiK::DSP
