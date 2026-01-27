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
#include "../UI/SpectralVisualizer.h"
#include "../UI/Browser/PresetBrowser.h"
#include "../UI/LcdDisplay.h"
#include "../UI/LcdMenuManager.h"

class NEURONiKEditor : public juce::AudioProcessorEditor,
                     public juce::MenuBarModel,
                     public juce::AudioProcessorValueTreeState::Listener,
                     private juce::Timer
{
public:
    explicit NEURONiKEditor(NEURONiKProcessor&);
    ~NEURONiKEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    
    // --- APVTS Listener ---
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void updateModelNames();
    void updateLcdDefault();

    // --- Button Handling ---
    void buttonStateChanged(juce::Button* b);
    void handleButtonAction(juce::Button* b, bool isRepeat);
    void timerCallback() override;

    juce::Button* activeHoldButton = nullptr;
    int holdCounter = 0;

    // --- MenuBarModel overrides ---
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    void showMidiSpecifications();

private:
    NEURONiKProcessor& processor;

    juce::MenuBarComponent menuBar;
    juce::MidiKeyboardComponent keyboardComponent;

    // UI Structure
    juce::TabbedComponent mainTabs { juce::TabbedButtonBar::TabsAtTop };

    // Header Components (Hardware Interface)
    NEURONiK::UI::LcdDisplay lcdDisplay;
    
    // Command Buttons
    NEURONiK::UI::CustomButton menuBtn { "MENU" }, okBtn { "OK" };
    
    // Navigation D-Pad (Characters instead of ArrowButtons)
    NEURONiK::UI::CustomButton leftBtn { "<" }, rightBtn { ">" };
    NEURONiK::UI::CustomButton upBtn { "^" }, downBtn { "v" };
    
    NEURONiK::UI::LcdMenuManager menuManager;
    
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
