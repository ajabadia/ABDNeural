#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "CustomUIComponents.h"
#include "EnvelopeVisualizer.h"

class NEURONiKProcessor;

namespace NEURONiK::UI {

class ParameterPanel : public juce::Component, public juce::Timer
{
public:
    ParameterPanel(NEURONiKProcessor& p);
    ~ParameterPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue = nullptr);
    void randomizeParameters();

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;

    // Envelope Controls
    RotaryControl attack, decay, sustain, release;

    // Filter & Master Controls
    RotaryControl randomStrength;
    RotaryControl masterBPM;

    std::unique_ptr<EnvelopeVisualizer> adsrVisualizer;

    juce::TextButton randomizeButton{ "RANDOM" };
    
    juce::ToggleButton freezeResBtn { "FREEZE RES" };
    juce::ToggleButton freezeFltBtn { "FREEZE FLT" };
    juce::ToggleButton freezeEnvBtn { "FREEZE ENV" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeResAttach, freezeFltAttach, freezeEnvAttach;
    std::unique_ptr<MidiLearner> freezeResMidi, freezeFltMidi, freezeEnvMidi;
    SharedKnobLookAndFeel sharedLNF;

    GlassBox ampEnvBox { "AMPLITUDE ENVELOPE" }, globalBox { "DASHBOARD / GLOBAL" };
    
    juce::Label titleLabel { "title", "NEURONiK" };
    juce::Label versionLabel { "version", "Advanced Hybrid Synthesizer" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterPanel)
};

} // namespace NEURONiK::UI
