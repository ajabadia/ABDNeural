#pragma once
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class PluginEditor : public juce::AudioProcessorEditor,
                     public juce::MenuBarModel
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    // --- MenuBarModel overrides ---
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

private:
    PluginProcessor& processor;
    
    juce::MenuBarComponent menuBar;
    juce::MidiKeyboardComponent keyboardComponent;

    // UI Structure
    juce::TabbedComponent mainTabs { juce::TabbedButtonBar::TabsAtTop };
    
    // Header Components (Placeholders for now)
    juce::Label titleLabel { "title", "NEXUS" };
    juce::Label versionLabel { "version", "Neural Hybrid Synth" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
