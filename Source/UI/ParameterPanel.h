#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Main/NEURONiKProcessor.h"
#include "MidiLearner.h"
#include "Panels/PresetPanel.h"

namespace NEURONiK::UI {

class ParameterPanel : public juce::Component, private juce::Timer
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
        std::atomic<float>* boundModularValue = nullptr;
    };

    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue = nullptr);
    void randomizeParameters();
    void timerCallback() override;

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;

    PresetPanel presetPanel;

    // Envelope Controls
    RotaryControl attack, decay, sustain, release;

    // Filter & Master Controls
    RotaryControl cutoff, resonance, randomStrength;

    juce::TextButton randomizeButton{ "RANDOM" };
    
    juce::ToggleButton freezeResBtn { "FREEZE RES" };
    juce::ToggleButton freezeFltBtn { "FREEZE FLT" };
    juce::ToggleButton freezeEnvBtn { "FREEZE ENV" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeResAttach, freezeFltAttach, freezeEnvAttach;
    std::unique_ptr<MidiLearner> freezeResMidi, freezeFltMidi, freezeEnvMidi;
    std::vector<std::unique_ptr<juce::LookAndFeel_V4>> lnfs;

    juce::Label titleLabel{ "title", "NEURONiK" };
    juce::Label subtitleLabel{ "subtitle", "Advanced Hybrid Synthesizer" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterPanel)
};

} // namespace NEURONiK::UI
