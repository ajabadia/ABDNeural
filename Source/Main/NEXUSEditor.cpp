#include "NEXUSEditor.h"
#include "../Core/BuildVersion.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

NEXUSEditor::NEXUSEditor(NEXUSProcessor& p)
    : AudioProcessorEditor(p), processor(p), menuBar(this),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
      parameterPanel(p.getAPVTS())
{
    addAndMakeVisible(menuBar);
    addAndMakeVisible(keyboardComponent);
    addAndMakeVisible(mainTabs);
    
    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font(juce::FontOptions(32.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    titleLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(versionLabel);
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    versionLabel.setJustificationType(juce::Justification::centred);

    // Initial Tabs
    mainTabs.addTab("DASHBOARD", juce::Colours::darkgrey, &parameterPanel, false);
    mainTabs.addTab("OSCILLATORS", juce::Colours::darkgrey, new juce::Component(), true);
    mainTabs.addTab("FILTER/ENV", juce::Colours::darkgrey, new juce::Component(), true);
    mainTabs.addTab("FX", juce::Colours::darkgrey, new juce::Component(), true);

    keyboardComponent.setAvailableRange(24, 96);
    
    setWantsKeyboardFocus(true);
    setSize(800, 600);
}

void NEXUSEditor::paint(juce::Graphics& g)
{
    // Professional Dark Theme Background
    auto backgroundColor = juce::Colour(0xFF1A1A1A); // Deep Charcoal
    g.fillAll(backgroundColor);
    
    auto area = getLocalBounds();
    
    // Header Style
    auto headerArea = area.removeFromTop(105); // 25 (menu) + 80 (header)
    juce::Colour headerColor = juce::Colour(0xFF252525);
    g.setColour(headerColor);
    g.fillRect(headerArea);
    
    // Subtle separator line
    g.setColour(juce::Colours::cyan.withAlpha(0.3f));
    g.fillRect(headerArea.withY(headerArea.getBottom() - 1).withHeight(1));

    // Decorative gradient for the title area
    juce::ColourGradient grad(juce::Colours::cyan.withAlpha(0.1f), 0, 25,
                             juce::Colours::transparentBlack, 0, 105, false);
    g.setGradientFill(grad);
    g.fillRect(headerArea);
}

void NEXUSEditor::resized()
{
    auto area = getLocalBounds();
    
    // 1. Top Menu (Fixed 25)
    menuBar.setBounds(area.removeFromTop(25));
    
    // 2. Header (Title / Logo / Global controls)
    auto headerArea = area.removeFromTop(80);
    titleLabel.setBounds(headerArea.removeFromTop(40));
    versionLabel.setBounds(headerArea);
    
    // 3. Keyboard (Bottom 100)
    auto keyboardArea = area.removeFromBottom(100);
    keyboardComponent.setBounds(keyboardArea);
    
    // Ensure keyboard fills the full width - 43 white keys in range 24-96
    float numWhiteKeys = 43.0f;
    keyboardComponent.setKeyWidth(static_cast<float>(keyboardArea.getWidth()) / numWhiteKeys);
    
    // 4. Main Body (Tabs)
    mainTabs.setBounds(area.reduced(10));
}

juce::StringArray NEXUSEditor::getMenuBarNames()
{
    return { "File", "Edit", "Help" };
}

juce::PopupMenu NEXUSEditor::getMenuForIndex(int /*menuIndex*/, const juce::String& menuName)
{
    juce::PopupMenu menu;

    if (menuName == "File")
    {
        menu.addItem(1, "Load Preset...");
        menu.addItem(2, "Save Preset...");
    }
    else if (menuName == "Edit")
    {
        menu.addItem(10, "Undo");
        menu.addItem(11, "Redo");
        menu.addSeparator();
        menu.addItem(12, "Copy Settings");
        menu.addItem(13, "Paste Settings");
        menu.addSeparator();
        menu.addItem(14, "Options...");
    }
    else if (menuName == "Help")
    {
        menu.addItem(100, "Documentation...");
        menu.addItem(101, "About...");
    }

    return menu;
}

void NEXUSEditor::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
    if (menuItemID == 14) // Options
    {
#if JucePlugin_Build_Standalone
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            holder->showAudioSettingsDialog();
            return;
        }
#endif
        
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "NEXUS Options",
            "Audio/MIDI settings are managed by your DAW/Host when running as a plugin.",
            "OK");
    }
    else if (menuItemID == 101) // About
    {
        juce::String aboutText;
        aboutText << "NEXUS Synthesizer\n"
                  << "Advanced Neural Hybrid Processor\n\n"
                  << "Version: " << NEXUS_BUILD_VERSION << "\n"
                  << "Build: " << NEXUS_BUILD_TIMESTAMP << "\n\n"
                  << "© 2026 ABD Neural";

        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "About NEXUS",
                                               aboutText,
                                               "OK");
    }
}
