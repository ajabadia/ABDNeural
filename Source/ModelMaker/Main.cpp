/*
  ==============================================================================

    NEURONiK Model Maker
    Created: 27 Jan 2026
    Description: Standalone application for analyzing audio and creating spectral models.

  ==============================================================================
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

namespace NEURONiK::ModelMaker {

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
            setContentOwned(new MainComponent(), true);
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

} // namespace NEURONiK::ModelMaker

// This macro generates the main() function that launches the app.
START_JUCE_APPLICATION(NEURONiK::ModelMaker::ModelMakerApplication)
