#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Main/NEURONiKProcessor.h"
#include "MidiLearner.h"
#include "Panels/PresetPanel.h"

namespace NEURONiK::UI {

class ParameterPanel : public juce::Component
{
public:
    ParameterPanel(NEURONiKProcessor& p);
    ~ParameterPanel() override = default;

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
    void randomizeParameters();

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;

    PresetPanel presetPanel;

    // Envelope Controls
    RotaryControl attack, decay, sustain, release;

    // Filter & Master Controls
    RotaryControl cutoff, resonance;

    juce::TextButton randomizeButton{ "RANDOM" };

    juce::Label titleLabel{ "title", "NEURONiK" };
    juce::Label subtitleLabel{ "subtitle", "Advanced Hybrid Synthesizer" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterPanel)
};

} // namespace NEURONiK::UI
