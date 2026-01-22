#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace Nexus::UI {

class ParameterPanel : public juce::Component
{
public:
    ParameterPanel(juce::AudioProcessorValueTreeState& vts);
    ~ParameterPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct RotaryControl {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText);

    juce::AudioProcessorValueTreeState& vts;

    // Envelope Controls
    RotaryControl attack, decay, sustain, release;

    // Filter & Resonator Controls
    RotaryControl cutoff, resonance;
    RotaryControl rollOff;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterPanel)
};

} // namespace Nexus::UI
