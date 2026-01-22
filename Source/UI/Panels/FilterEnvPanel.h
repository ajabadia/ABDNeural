#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

namespace Nexus::UI {

class FilterEnvPanel : public juce::Component
{
public:
    FilterEnvPanel(juce::AudioProcessorValueTreeState& vts);
    ~FilterEnvPanel() override = default;

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
    
    RotaryControl attack, decay, sustain, release;
    RotaryControl cutoff, resonance;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterEnvPanel)
};

} // namespace Nexus::UI
