#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../CustomUIComponents.h"
#include "../EnvelopeVisualizer.h"

// Forward declaration
class NEURONiKProcessor;

namespace NEURONiK::UI {

class FilterEnvPanel : public juce::Component, private juce::Timer
{
public:
    FilterEnvPanel(NEURONiKProcessor& p);
    ~FilterEnvPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setupControl(RotaryControl& control, const juce::String& paramID, const juce::String& labelText, std::atomic<float>* modValue = nullptr);
    void timerCallback() override;

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;
    
    // Filter controls
    RotaryControl cutoff, resonance, envAmount;
    
    // Filter Envelope controls
    RotaryControl fAttack, fDecay, fSustain, fRelease;
    
    std::unique_ptr<EnvelopeVisualizer> fEnvVisualizer;
    GlassBox filterBox { "FILTER" }, fEnvBox { "FILTER ENVELOPE" };
    SharedKnobLookAndFeel sharedLNF;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterEnvPanel)
};

} // namespace NEURONiK::UI
