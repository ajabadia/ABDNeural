#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../MidiLearner.h"

// Forward declaration
class NEURONiKProcessor;

namespace NEURONiK::UI {

class FXPanel : public juce::Component
{
public:
    FXPanel(NEURONiKProcessor& p);
    ~FXPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct RotaryControl {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        std::unique_ptr<MidiLearner> midiLearner;
    };

    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText);

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;
    
    RotaryControl saturation, delayTime, delayFeedback, masterLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FXPanel)
};

} // namespace NEURONiK::UI
