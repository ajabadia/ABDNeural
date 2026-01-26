#pragma once
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "NEURONiKProcessor.h"
#include "../UI/ParameterPanel.h"
#include "../UI/Panels/OscillatorPanel.h"
#include "../UI/Panels/FilterEnvPanel.h"
#include "../UI/Panels/FXPanel.h"
#include "../UI/Panels/ModulationPanel.h"
#include "../UI/Panels/ModulationPanel.h"
#include "../UI/SpectralVisualizer.h"
#include "../UI/Browser/PresetBrowser.h"

class NEURONiKEditor : public juce::AudioProcessorEditor,
                     public juce::MenuBarModel
{
public:
    explicit NEURONiKEditor(NEURONiKProcessor&);
    ~NEURONiKEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    void updateModelNames();

    // --- MenuBarModel overrides ---
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

private:
    NEURONiKProcessor& processor;

    juce::MenuBarComponent menuBar;
    juce::MidiKeyboardComponent keyboardComponent;

    // UI Structure
    juce::TabbedComponent mainTabs { juce::TabbedButtonBar::TabsAtTop };

    // Header Components
    juce::Label titleLabel { "title", "NEURONiK" };
    juce::Label versionLabel { "version", "Advanced Hybrid Synthesizer" };
    NEURONiK::UI::SpectralVisualizer visualizer;

    // Panels
    NEURONiK::UI::ParameterPanel generalPanel;
    NEURONiK::UI::OscillatorPanel oscPanel;
    NEURONiK::UI::FilterEnvPanel filterEnvPanel;
    NEURONiK::UI::FXPanel fxPanel;
    NEURONiK::UI::ModulationPanel modulationPanel;
    NEURONiK::UI::Browser::PresetBrowser presetBrowser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NEURONiKEditor)
};
