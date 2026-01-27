#include "PresetPanel.h"
#include "../../Main/NEURONiKProcessor.h"
#include "../../State/ParameterDefinitions.h"

namespace NEURONiK::UI
{

using namespace NEURONiK::State;

PresetPanel::PresetPanel(NEURONiKProcessor& p)
    : processor(p), vts(p.getAPVTS())
{
    addAndMakeVisible(presetBox);

    presetCombo.setTextWhenNothingSelected("Select Preset...");
    presetCombo.setJustificationType(juce::Justification::centredLeft);
    presetBox.addAndMakeVisible(presetCombo);
    
    presetBox.addAndMakeVisible(saveButton);
    presetBox.addAndMakeVisible(deleteButton);
    
    // Refresh presets from Manager
    updatePresetList();

    presetCombo.onChange = [this] {
        if (presetCombo.getSelectedId() > 0)
        {
            processor.getPresetManager().loadPreset(presetCombo.getText());
        }
    };

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
    juce::ignoreUnused(g);
}

void PresetPanel::resized()
{
    auto area = getLocalBounds().reduced(5);
    
    // Use full width for now, maybe add more sections later
    presetBox.setBounds(area.removeFromTop(100).reduced(3));
    
    auto c = presetBox.getContentArea();
    auto row = c.removeFromTop(30); // Single row for controls
    
    int btnWidth = 70;
    deleteButton.setBounds(row.removeFromRight(btnWidth).reduced(5, 2));
    saveButton.setBounds(row.removeFromRight(btnWidth).reduced(5, 2));
    presetCombo.setBounds(row.reduced(5, 2));
}

} // namespace NEURONiK::UI
