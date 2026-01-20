#include "PluginEditor.h"
#include "../Core/BuildVersion.h"

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(p), processor(p), menuBar(this)
{
    addAndMakeVisible(menuBar);
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
    menuBar.setBounds(getLocalBounds().removeFromTop(25));
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
