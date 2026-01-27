#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../XYPad.h"
#include "../CustomUIComponents.h"

// Forward declaration to avoid including NEURONiKProcessor.h in a header
class NEURONiKProcessor;

namespace NEURONiK::UI {

class OscillatorPanel : public juce::Component, public juce::Button::Listener, private juce::Timer
{
public:
    // The constructor now takes the main processor reference
    OscillatorPanel(NEURONiKProcessor& p);
    ~OscillatorPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void setModelName(int slot, const juce::String& name);
    void timerCallback() override;

private:
    void setupControl(RotaryControl& ctrl, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue = nullptr);

    NEURONiKProcessor& processor;

    XYPad xyPad;
    RotaryControl inharmonicity;
    RotaryControl roughness;
    RotaryControl parity;
    RotaryControl shift;
    RotaryControl rollOff;

    juce::TextButton loadA, loadB, loadC, loadD;
    std::array<juce::String, 4> modelNames;

    std::unique_ptr<juce::FileChooser> fileChooser;
    SharedKnobLookAndFeel sharedLNF;
    GlassBox modelBox { "NEURAL MODELS" }, engineBox { "SPECTRAL ENGINE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OscillatorPanel)
};

} // namespace NEURONiK::UI
