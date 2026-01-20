/*
  ==============================================================================

    Main.cpp
    Created: 20 Jan 2026
    Description: Custom Standalone Application entry point for NEXUS.
                 Allows hiding the native 'Options' button and full UI control.

  ==============================================================================
*/

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include "PluginProcessor.h"

namespace Nexus
{

class NexusStandaloneApplication : public juce::JUCEApplication
{
public:
    NexusStandaloneApplication() = default;

    const juce::String getApplicationName() override       { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override    { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& /*commandLine*/) override
    {
        // Use JUCE's default StandaloneFilterWindow but we could customize it further here
        // The flag STANDALONE_SHOW_SETTINGS_BUTTON=FALSE in CMake usually works with the default app,
        // but by using a custom app we ensure total control.
        standaloneWindow.reset(new juce::StandaloneFilterWindow(getApplicationName(), 
                                                               juce::Colours::black, 
                                                               nullptr, 
                                                               false)); // Show settings button = false

        standaloneWindow->setTitleBarButtonsRequired(juce::StandaloneFilterWindow::allButtons, false);
        standaloneWindow->setVisible(true);
        standaloneWindow->setResizable(true, true);
    }

    void shutdown() override
    {
        standaloneWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override {}

private:
    std::unique_ptr<juce::StandaloneFilterWindow> standaloneWindow;
};

} // namespace Nexus

// This macro generates the main() function
START_JUCE_APPLICATION(Nexus::NexusStandaloneApplication)
