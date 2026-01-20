/*
  ==============================================================================

    Main.cpp
    Created: 20 Jan 2026
    Description: Custom Standalone Application entry point for NEXUS.
                 Allows hiding the native 'Options' button and full UI control.

  ==============================================================================
*/

#include <memory>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

#if JucePlugin_Build_Standalone

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
        standaloneWindow = std::make_unique<juce::StandaloneFilterWindow>(getApplicationName(), 
                                                                         juce::Colours::black, 
                                                                         nullptr, 
                                                                         false);

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

START_JUCE_APPLICATION(Nexus::NexusStandaloneApplication)

#endif
