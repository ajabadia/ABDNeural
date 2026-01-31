#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "CustomUIComponents.h"
#include "EnvelopeVisualizer.h"
#include "../Main/ModulationTargets.h"

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
    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText, ::NEURONiK::ModulationTarget modTarget = ::NEURONiK::ModulationTarget::Count);
    void setupControl(VerticalSliderControl& control, const juce::String& paramID, const juce::String& labelText, ::NEURONiK::ModulationTarget modTarget = ::NEURONiK::ModulationTarget::Count);
    void randomizeParameters();

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;

    // Envelope Controls
    RotaryControl attack, decay, sustain, release;

    // Filter & Master Controls
    RotaryControl randomStrength;
    VerticalSliderControl masterLevel;

    std::unique_ptr<EnvelopeVisualizer> adsrVisualizer;

    // Unison Controls
    RotaryControl unisonDetune;
    RotaryControl unisonSpread;
    juce::ToggleButton unisonEnabled { "ENABLE UNISON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> unisonEnabledAttach;

    juce::TextButton randomizeButton{ "RANDOM" };
    
    juce::ToggleButton freezeResBtn { "FREEZE RESONATOR" };
    juce::ToggleButton freezeFltBtn { "FREEZE FILTER/FX" };
    juce::ToggleButton freezeEnvBtn { "FREEZE ENVELOPES" };

    juce::ComboBox engineSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> engineAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeResAttach, freezeFltAttach, freezeEnvAttach;
    std::unique_ptr<MidiLearner> freezeResMidi, freezeFltMidi, freezeEnvMidi, engineMidi;
    SharedKnobLookAndFeel sharedLNF;
    VerticalSliderLookAndFeel verticalLNF;

    GlassBox ampEnvBox { "AMPLITUDE ENVELOPE" };
    GlassBox unisonBox { "SPECTRAL UNISON" };
    GlassBox globalSettingsBox { "OTHERS" };
    GlassBox globalBox { "DASHBOARD / GLOBAL" };
    
    juce::Label titleLabel { "title", "AXIONiK" };
    juce::Label versionLabel { "version", "Hybrid Spectral Synthesizer" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterPanel)
};

} // namespace NEURONiK::UI
