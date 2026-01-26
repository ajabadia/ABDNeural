#include "PresetPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI
{

using namespace NEURONiK::State;

PresetPanel::PresetPanel(NEURONiKProcessor& p)
    : processor(p)
{
    addAndMakeVisible(presetCombo);
    addAndMakeVisible(saveButton);
    addAndMakeVisible(deleteButton);

    presetCombo.setTextWhenNothingSelected("Select Preset...");
    presetCombo.setJustificationType(juce::Justification::centredLeft);
    
    // Refresh presets from Manager
    updatePresetList();

    presetCombo.onChange = [this] {
        if (presetCombo.getSelectedId() > 0)
        {
            processor.getPresetManager().loadPreset(presetCombo.getText());
        }
    };

    saveButton.onClick = [this] {
        juce::AlertWindow::showAsync(juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::NoIcon)
            .withTitle("Save Preset")
            .withMessage("Enter a name for your preset:")
            .withButton("Save")
            .withButton("Cancel")
            .withAssociatedComponent(this),
            [this](int result) {
                if (result == 0) // Save
                {
                    // How to get text? AlertWindow handles are tricky with lambdas.
                    // Better to use showOkCancelBox or custom dialog.
                    // For now, let's use a simpler text entry mock or prompt.
                    // JUCE AlertWindow with text input requires older sync method or custom component.
                    // Let's assume a simplified flow for now:
                    // Create a robust input window in next step if needed. 
                    // Actually, showNativeDialog or similar.
                    // Standard JUCE way for input:
                    promptForPresetName();
                }
            });
    };
    
    // Overriding the lambda above because we need a specific Helper
    saveButton.onClick = [this] { promptForPresetName(); };

    deleteButton.onClick = [this] {
        auto currentPreset = presetCombo.getText();
        if (currentPreset.isNotEmpty())
        {
            processor.getPresetManager().deletePreset(currentPreset);
            updatePresetList();
            presetCombo.setText("Init Preset");
        }
    };

    // BPM Setup
    bpmSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 60, 20);
    bpmSlider.setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
    addAndMakeVisible(bpmSlider);

    bpmLabel.setText("MASTER BPM", juce::dontSendNotification);
    bpmLabel.setJustificationType(juce::Justification::centred);
    bpmLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::cyan.withAlpha(0.7f));
    addAndMakeVisible(bpmLabel);

    bpmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(), IDs::masterBPM, bpmSlider);

    startTimer(500); // Check for preset changes every 500ms
}

void PresetPanel::timerCallback()
{
    auto currentFromManager = processor.getPresetManager().getCurrentPreset();
    if (presetCombo.getText() != currentFromManager)
    {
        presetCombo.setText(currentFromManager, juce::dontSendNotification);
    }
}

void PresetPanel::updatePresetList()
{
    presetCombo.clear();
    auto presets = processor.getPresetManager().getAllPresets();
    int id = 1;
    for (const auto& preset : presets)
    {
        presetCombo.addItem(preset, id++);
    }
    
    // Select current if matches
    presetCombo.setText(processor.getPresetManager().getCurrentPreset(), juce::dontSendNotification);
}

void PresetPanel::promptForPresetName()
{
    auto* alert = new juce::AlertWindow("Save Preset", "Enter a name for the preset:", juce::AlertWindow::NoIcon, this);
    alert->addTextEditor("presetName", processor.getPresetManager().getCurrentPreset());
    alert->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    
    alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int result) {
        if (result == 1)
        {
            auto name = alert->getTextEditorContents("presetName");
            if (name.isNotEmpty())
            {
                processor.getPresetManager().savePreset(name);
                updatePresetList();
            }
        }
    }), true); // delete when done
}

void PresetPanel::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().reduced(20);
    
    // Background for divisions
    g.setColour(juce::Colour(0xFF252525));
    
    auto presetArea = area.removeFromTop(100);
    g.fillRoundedRectangle(presetArea.toFloat(), 10.0f);
    
    area.removeFromTop(20);
    auto extraArea = area.removeFromTop(150);
    g.fillRoundedRectangle(extraArea.toFloat(), 10.0f);

    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.drawRoundedRectangle(presetArea.toFloat(), 10.0f, 1.0f);
    g.drawRoundedRectangle(extraArea.toFloat(), 10.0f, 1.0f);
    
    g.setColour(juce::Colours::cyan.withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(14.0f).withStyle("Bold")));
    g.drawText("PRESET MANAGEMENT", presetArea.removeFromTop(30).reduced(10, 0), juce::Justification::centredLeft);
    g.drawText("GLOBAL SETTINGS", extraArea.removeFromTop(30).reduced(10, 0), juce::Justification::centredLeft);
}

void PresetPanel::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    auto presetArea = area.removeFromTop(100);
    presetArea.removeFromTop(40); // Title space
    auto comboRow = presetArea.reduced(20, 10);
    deleteButton.setBounds(comboRow.removeFromRight(60));
    saveButton.setBounds(comboRow.removeFromRight(60).translated(-5, 0));
    presetCombo.setBounds(comboRow.reduced(0, 5));
    
    area.removeFromTop(20);
    auto extraArea = area.removeFromTop(150);
    extraArea.removeFromTop(40); // Title space
    
    auto bpmArea = extraArea.removeFromLeft(120).reduced(10);
    bpmLabel.setBounds(bpmArea.removeFromTop(20));
    bpmSlider.setBounds(bpmArea);
}

} // namespace NEURONiK::UI
