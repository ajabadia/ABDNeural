/*
  ==============================================================================

    NEURONiK Model Maker
    Created: 27 Jan 2026
    Description: Standalone application for analyzing audio and creating spectral models.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

// Moved to global namespace to eliminate any macro-related linker issues
class ModelMakerApplication : public juce::JUCEApplication
{
public:
    ModelMakerApplication() {}

    const juce::String getApplicationName() override       { return "NEURONiK Model Maker"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);
    }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setUsingNativeTitleBar(true);
            
            auto* content = new NEURONiK::ModelMaker::MainComponent();
            setContentOwned(content, true);
            
            setResizable(true, true);
            
            // Set default size similar to a utility app
            centreWithSize(800, 600);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

// This macro generates the application factory and standard entry point.
START_JUCE_APPLICATION(ModelMakerApplication)
