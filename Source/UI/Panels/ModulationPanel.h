#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "../CustomUIComponents.h"

class NEURONiKProcessor;

namespace NEURONiK::UI {

class ModulationPanel : public juce::Component, private juce::Timer
{
public:
    ModulationPanel(NEURONiKProcessor& p);
    ~ModulationPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    struct LfoUiElements {
        juce::ComboBox waveform, syncMode, division;
        RotaryControl rate, depth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveformAttachment, syncModeAttachment, divisionAttachment;
    };

    struct ModSlotUiElements {
        juce::ComboBox source, destination;
        RotaryControl amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sourceAttachment, destinationAttachment;
    };

    void setupLfoControls(int lfoIndex);
    void setupModSlotControls(int slotIndex);
    void setupAmountControl(RotaryControl& ctrl, const juce::String& paramID);

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;

    std::array<LfoUiElements, 2> lfos;
    std::array<ModSlotUiElements, 4> modSlots;
    
    GlassBox lfo1Box { "LFO 1" }, lfo2Box { "LFO 2" }, matrixBox { "MODULATION MATRIX" };
    juce::Label sourceTitle { "src", "SOURCE" }, destTitle { "dst", "DESTINATION" }, amountTitle { "amt", "AMOUNT" };
    SharedKnobLookAndFeel sharedLNF;
 
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationPanel)
};

} // namespace NEURONiK::UI
