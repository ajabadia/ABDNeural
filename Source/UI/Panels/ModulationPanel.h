/*
  ==============================================================================

    ModulationPanel.h
    Created: 28 Jan 2026
    Description: UI component for LFOs and the modulation matrix.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "../MidiLearner.h"

class NEURONiKProcessor;

namespace NEURONiK::UI {

// Helper struct for UI controls
struct ControlElement {
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    std::unique_ptr<MidiLearner> midiLearner;
};

class ModulationPanel : public juce::Component
{
public:
    ModulationPanel(NEURONiKProcessor& p);
    ~ModulationPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct LfoUiElements {
        juce::ComboBox waveform, syncMode, division;
        ControlElement rate, depth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment, syncModeAttachment, divisionAttachment;
    };

    struct ModSlotUiElements {
        juce::ComboBox source, destination;
        ControlElement amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment, destinationAttachment;
    };

    void setupLfoControls(int lfoIndex);
    void setupModSlotControls(int slotIndex);
    void setupRotaryControl(ControlElement& ctrl, const juce::String& paramID, const juce::String& labelText);
    void setupAmountControl(ControlElement& ctrl, const juce::String& paramID);

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;

    std::array<LfoUiElements, 2> lfos;
    std::array<ModSlotUiElements, 4> modSlots;
    
    juce::Label sourceTitle { "src", "SOURCE" }, destTitle { "dst", "DESTINATION" }, amountTitle { "amt", "AMOUNT" };
 
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationPanel)
};

} // namespace NEURONiK::UI
