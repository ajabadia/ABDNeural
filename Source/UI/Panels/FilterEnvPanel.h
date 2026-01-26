#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../MidiLearner.h"
#include "../EnvelopeVisualizer.h"

// Forward declaration
class NEURONiKProcessor;

namespace NEURONiK::UI {

class FilterEnvPanel : public juce::Component, private juce::Timer
{
public:
    FilterEnvPanel(NEURONiKProcessor& p);
    ~FilterEnvPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct RotaryControl {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        std::unique_ptr<MidiLearner> midiLearner;
        std::atomic<float>* boundModularValue = nullptr;
    };

    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue = nullptr);
    void timerCallback() override;

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;
    
    RotaryControl attack, decay, sustain, release;
    RotaryControl cutoff, resonance;
    
    std::unique_ptr<EnvelopeVisualizer> envVisualizer;
    std::vector<std::unique_ptr<juce::LookAndFeel_V4>> lnfs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterEnvPanel)
};

} // namespace NEURONiK::UI
