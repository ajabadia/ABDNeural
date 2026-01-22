/*
  ==============================================================================

    SpectralVisualizer.h
    Created: 22 Jan 2026
    Description: Real-time visualization of the 64 partials' amplitudes.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace Nexus::UI {

/**
 * @class SpectralVisualizer
 * @brief Displays the 64 harmonic partials of the Resonator.
 */
class SpectralVisualizer : public juce::Component, public juce::Timer
{
public:
    SpectralVisualizer(juce::AudioProcessorValueTreeState& vts);
    ~SpectralVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    juce::AudioProcessorValueTreeState& vts_;
    std::array<float, 64> harmonicProfile_;
    
    // Aesthetic elements
    juce::Colour accentColour_ = juce::Colours::cyan;
    
    void updateProfile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralVisualizer)
};

} // namespace Nexus::UI
