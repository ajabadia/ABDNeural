#include "PluginEditor.h"
#include "../Core/BuildVersion.h"

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(p), processor(p), menuBar(this),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible(menuBar);
    addAndMakeVisible(keyboardComponent);
    addAndMakeVisible(mainTabs);
    
    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::FontOptions(24.0f).withStyle(juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(versionLabel);
    versionLabel.setJustificationType(juce::Justification::centred);

    // Initial Tabs
    mainTabs.addTab("DASHBOARD", juce::Colours::darkgrey, new juce::Component(), true);
    mainTabs.addTab("OSCILLATORS", juce::Colours::darkgrey, new juce::Component(), true);
    mainTabs.addTab("FILTER/ENV", juce::Colours::darkgrey, new juce::Component(), true);
    mainTabs.addTab("FX", juce::Colours::darkgrey, new juce::Component(), true);

    keyboardComponent.setAvailableRange(24, 96);
    
    setWantsKeyboardFocus(true);
    setSize(800, 600);
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    
    // Draw background or placeholders for Phase 2
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(20); // Room for menu
    
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRoundedRectangle(area.toFloat(), 5.0f, 2.0f);
    
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    g.drawFittedText("NEXUS", area.removeFromTop(40), juce::Justification::centred, 1);
    
    g.setFont(14.0f);
    g.drawFittedText("Neural Hybrid Synthesizer", area.removeFromTop(20), juce::Justification::centred, 1);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    
    // 1. Top Menu (Fixed 25)
    menuBar.setBounds(area.removeFromTop(25));
    
    // 2. Header (Title / Logo / Global controls)
    auto headerArea = area.removeFromTop(80);
    titleLabel.setBounds(headerArea.removeFromTop(40));
    versionLabel.setBounds(headerArea);
    
    // 3. Keyboard (Bottom 100)
    keyboardComponent.setBounds(area.removeFromBottom(100));
    
    // 4. Main Body (Tabs)
    mainTabs.setBounds(area.reduced(10));
}

juce::StringArray PluginEditor::getMenuBarNames()
{
    return { "File", "Edit", "Help" };
}

juce::PopupMenu PluginEditor::getMenuForIndex(int /*menuIndex*/, const juce::String& menuName)
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

void PluginEditor::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
    if (menuItemID == 14) // Options
    {
        if (juce::AudioProcessor::wrapperType_Standalone == processor.wrapperType)
        {
            // In a future step, we can pass the device manager here. 
            // For now, we show a professional dialog.
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                "NEXUS Options",
                "To configure Audio/MIDI devices in Standalone mode, we will implement a custom panel in Phase 5.\n\n"
                "Note: The native JUCE options button has been disabled to keep the UI clean.",
                "OK");
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                "NEXUS Options",
                "Audio/MIDI settings are managed by your DAW/Host when running as a plugin.",
                "OK");
        }
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
