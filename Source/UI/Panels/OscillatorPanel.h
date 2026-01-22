#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../SpectralVisualizer.h"
#include "../XYPad.h"

namespace Nexus::UI {

class OscillatorPanel : public juce::Component
{
public:
    OscillatorPanel(juce::AudioProcessorValueTreeState& vts);
    ~OscillatorPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& vts;

    SpectralVisualizer visualizer;
    XYPad xyPad;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscillatorPanel)
};

} // namespace Nexus::UI
