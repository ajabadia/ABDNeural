/*
  ==============================================================================

    SpectralVisualizer.h
    Created: 22 Jan 2026
    Description: Real-time visualization of the 64 partials' amplitudes.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

// Forward declaration to avoid including NEURONiKProcessor.h in a header
class NEURONiKProcessor;

namespace NEURONiK::UI {

/**
 * @class SpectralVisualizer
 * @brief Displays the 64 harmonic partials of the Resonator.
 */
class SpectralVisualizer : public juce::Component, public juce::Timer
{
public:
    // The constructor now takes the main processor reference
    explicit SpectralVisualizer(NEURONiKProcessor& p);
    ~SpectralVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    // A reference to the processor to access real-time spectral data
    NEURONiKProcessor& processor;

    std::array<float, 64> harmonicProfile_;
    
    // Aesthetic elements
    juce::Colour accentColour_;
    
    void updateProfile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralVisualizer)
};

} // namespace NEURONiK::UI
