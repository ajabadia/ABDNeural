#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

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

    juce::ComboBox presetCombo;
    juce::TextButton saveButton{ "SAVE" };
    juce::TextButton deleteButton{ "DEL" };

    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetPanel)
};

} // namespace NEURONiK::UI
