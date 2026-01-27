#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../CustomUIComponents.h"

class NEURONiKProcessor;

namespace NEURONiK::UI
{

class PresetPanel : public juce::Component,
                  public juce::Timer
{
public:
    PresetPanel(NEURONiKProcessor& p);
    ~PresetPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void timerCallback() override;

private:
    void updatePresetList();
    void promptForPresetName();

    NEURONiKProcessor& processor;
    juce::AudioProcessorValueTreeState& vts;

    juce::ComboBox presetCombo;
    juce::TextButton saveButton{ "SAVE" };
    juce::TextButton deleteButton{ "DEL" };

    // Glass Boxes
    GlassBox presetBox { "PRESET BROWSER" };
    // We can add more boxes later, like "MACROS" or "SETLIST"

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetPanel)
};

} // namespace NEURONiK::UI
