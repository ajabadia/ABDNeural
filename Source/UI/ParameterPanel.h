#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "CustomUIComponents.h"
#include "../Main/ModulationTargets.h"
#include "../State/ParameterRandomizer.h"

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

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;

    // Envelope Controls
    RotaryControl attack, decay, sustain, release;

    // Filter & Master Controls
    RotaryControl randomStrength;
    VerticalSliderControl masterLevel;

    // Unison Controls. There is no enable toggle: the parameter exists in the
    // APVTS for preset compatibility but the engine ignores it, so exposing it
    // would be a control that does nothing.
    RotaryControl unisonDetune;
    RotaryControl unisonSpread;

    // RANDOM: la logica del sorteo es COMPARTIDA (State/ParameterRandomizer), para
    // que la bancada y la pagina hagan exactamente lo mismo. El generador vive aqui
    // (sin getSystemRandom(): regla WASM 7A).
    juce::Random random { static_cast<juce::int64> (juce::Time::getMillisecondCounter()) };
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
