#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../CustomUIComponents.h"
#include "../../Main/ModulationTargets.h"

// Forward declaration
class NEURONiKProcessor;

namespace NEURONiK::UI {

class FXPanel : public juce::Component,
                  private juce::Timer
{
public:
    FXPanel(NEURONiKProcessor& p);
    ~FXPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    struct ChoiceControl {
        juce::ComboBox comboBox;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText, juce::Component& parent, ::NEURONiK::ModulationTarget modTarget = ::NEURONiK::ModulationTarget::Count);
    void setupChoice(ChoiceControl& control, const juce::String& paramID, const juce::String& labelText, juce::Component& parent);

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;
    
    // Saturation & Delay
    RotaryControl saturation, delayTime, delayFeedback;
    ChoiceControl delaySync, delayDivision;

    // Chorus
    RotaryControl chorusRate, chorusDepth, chorusMix;

    // Reverb
    RotaryControl reverbSize, reverbDamping, reverbWidth, reverbMix;

    // BPM
    RotaryControl masterBPM;

    // LEDs
    LedIndicator saturationLed, delayLed, chorusLed, reverbLed;

    GlassBox saturationBox { "SATURATION" }, delayBox { "DELAY" }, chorusBox { "CHORUS" }, reverbBox { "REVERB" }, masterBox { "BPM" };
    SharedKnobLookAndFeel sharedLNF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FXPanel)
};

} // namespace NEURONiK::UI
