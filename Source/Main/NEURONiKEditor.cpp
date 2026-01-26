#include "NEURONiKEditor.h"
#include "../Core/BuildVersion.h"
#include "../State/ParameterDefinitions.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace NEURONiK::State;

NEURONiKEditor::NEURONiKEditor(NEURONiKProcessor& p)
    : AudioProcessorEditor(p),
      processor(p),
      menuBar(this),
      keyboardComponent(p.getKeyboardState(), juce::MidiKeyboardComponent::horizontalKeyboard),
      visualizer(p),
      generalPanel(p),
      oscPanel(p),
      filterEnvPanel(p),
      fxPanel(p),
      modulationPanel(p),
      presetBrowser(p)
{
    addAndMakeVisible(menuBar);
    addAndMakeVisible(keyboardComponent);
    addAndMakeVisible(mainTabs);
    addAndMakeVisible(visualizer);

    titleLabel.setFont(juce::Font(juce::FontOptions(32.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    titleLabel.setJustificationType(juce::Justification::centred);

    versionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    versionLabel.setJustificationType(juce::Justification::centred);

    mainTabs.addTab("GENERAL",    juce::Colours::darkgrey, &generalPanel, false);
    mainTabs.addTab("RESONATOR",  juce::Colours::darkgrey, &oscPanel, false);
    mainTabs.addTab("FILTER/ENV", juce::Colours::darkgrey, &filterEnvPanel, false);
    mainTabs.addTab("FX",         juce::Colours::darkgrey, &fxPanel, false);
    mainTabs.addTab("LFO/MOD",    juce::Colours::darkgrey, &modulationPanel, false);
    mainTabs.addTab("BROWSER",    juce::Colours::black,    &presetBrowser,   false);

    keyboardComponent.setAvailableRange(24, 96);

    setWantsKeyboardFocus(true);
    setSize(800, 600);
}

void NEURONiKEditor::updateModelNames()
{
    const auto& names = processor.getModelNames();
    for (int i = 0; i < 4; ++i)
        oscPanel.setModelName(i, names[i]);
}

void NEURONiKEditor::paint(juce::Graphics& g)
{
    auto backgroundColor = juce::Colour(0xFF1A1A1A);
    g.fillAll(backgroundColor);

    auto area = getLocalBounds();

    auto headerArea = area.removeFromTop(105);
    juce::Colour headerColor = juce::Colour(0xFF252525);
    g.setColour(headerColor);
    g.fillRect(headerArea);

    g.setColour(juce::Colours::cyan.withAlpha(0.3f));
    g.fillRect(headerArea.withY(headerArea.getBottom() - 1).withHeight(1));

    juce::ColourGradient grad(juce::Colours::cyan.withAlpha(0.1f), 0, 25,
                             juce::Colours::transparentBlack, 0, 105, false);
    g.setGradientFill(grad);
    g.fillRect(headerArea);
}

void NEURONiKEditor::resized()
{
    auto area = getLocalBounds();

    menuBar.setBounds(area.removeFromTop(25));

    auto headerArea = area.removeFromTop(80);
    auto titleArea = headerArea.removeFromLeft(headerArea.getWidth() / 2);
    auto visualizerArea = headerArea;

    titleLabel.setBounds(titleArea.removeFromTop(40));
    versionLabel.setBounds(titleArea);
    visualizer.setBounds(visualizerArea.reduced(5));

    auto keyboardArea = area.removeFromBottom(100);
    keyboardComponent.setBounds(keyboardArea);

    float numWhiteKeys = 43.0f;
    keyboardComponent.setKeyWidth(static_cast<float>(keyboardArea.getWidth()) / numWhiteKeys);

    mainTabs.setBounds(area.reduced(10));
}

juce::StringArray NEURONiKEditor::getMenuBarNames()
{
    return { "File", "Edit", "Help" };
}

juce::PopupMenu NEURONiKEditor::getMenuForIndex(int, const juce::String& menuName)
{
    juce::PopupMenu menu;

    if (menuName == "File")
    {
        menu.addItem(10, "New Session"); // Placeholder for future
    }
    else if (menuName == "Edit")
    {
        menu.addItem(1, "Load Preset...");
        menu.addItem(2, "Save Preset...");
        menu.addSeparator();
        menu.addItem(60, "Copy Patch");
        menu.addItem(61, "Paste Patch");
        menu.addSeparator();

        juce::PopupMenu midiChannelMenu;
        auto* choiceParam = processor.getAPVTS().getParameter(IDs::midiChannel);
        int currentChoice = static_cast<int>(choiceParam->getValue() * (choiceParam->getNumSteps() - 1));

        for (int i = 0; i < 17; ++i)
        {
            midiChannelMenu.addItem(20 + i, (i == 0) ? "Omni" : "Channel " + juce::String(i), true, i == currentChoice);
        }
        menu.addSubMenu("MIDI Channel", midiChannelMenu);
        menu.addSeparator();

        juce::PopupMenu voicesMenu;
        int currentVoices = processor.getPolyphony();
        voicesMenu.addItem(50, "Mono (1 Voice)", true, currentVoices == 1);
        voicesMenu.addItem(51, "2 Voices", true, currentVoices == 2);
        voicesMenu.addItem(52, "3 Voices", true, currentVoices == 3);
        voicesMenu.addItem(53, "4 Voices", true, currentVoices == 4);
        voicesMenu.addItem(54, "6 Voices", true, currentVoices == 6);
        voicesMenu.addItem(55, "8 Voices", true, currentVoices == 8);

        menu.addSubMenu("Voices", voicesMenu);
        menu.addSeparator();
        menu.addItem(14, "Options...");
    }
    else if (menuName == "Help")
    {
        menu.addItem(101, "About...");
    }

    return menu;
}

void NEURONiKEditor::menuItemSelected(int menuItemID, int)
{
    if (menuItemID == 1) // Load Preset
    {
        auto fileChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        
        processor.getEditorSettings().chooser = std::make_unique<juce::FileChooser>("Select a preset to load...",
            processor.getPresetManager().getPresetsDirectory(),
            "*.neuronikpreset");

        processor.getEditorSettings().chooser->launchAsync(fileChooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
                processor.getPresetManager().loadPresetFromFile(file);
        });
    }
    else if (menuItemID == 2) // Save Preset
    {
        auto fileChooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

        processor.getEditorSettings().chooser = std::make_unique<juce::FileChooser>("Save current preset...",
            processor.getPresetManager().getPresetsDirectory(),
            "*.neuronikpreset");

        processor.getEditorSettings().chooser->launchAsync(fileChooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File())
                processor.getPresetManager().savePresetToFile(file);
        });
    }
    else if (menuItemID >= 20 && menuItemID < 37)
    {
        auto* param = processor.getAPVTS().getParameter(IDs::midiChannel);
        param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1(static_cast<float>(menuItemID - 20)));
    }
    else if (menuItemID >= 50 && menuItemID <= 55)
    {
        int voices = 1;
        switch(menuItemID)
        {
            case 50: voices = 1; break;
            case 51: voices = 2; break;
            case 52: voices = 3; break;
            case 53: voices = 4; break;
            case 54: voices = 6; break;
            case 55: voices = 8; break;
        }
        processor.setPolyphony(voices);
    }
    else if (menuItemID == 60)
    {
        processor.copyPatchToClipboard();
    }
    else if (menuItemID == 61)
    {
        processor.pastePatchFromClipboard();
    }
    else if (menuItemID == 14)
    {
#if JucePlugin_Build_Standalone
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            holder->showAudioSettingsDialog();
            return;
        }
#endif

        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "NEURONiK Options",
            "Audio/MIDI settings are managed by your DAW/Host when running as a plugin.",
            "OK");
    }
    else if (menuItemID == 101)
    {
        juce::String aboutText;
        aboutText << "NEURONiK Synthesizer\n"
                  << "Advanced Neural Hybrid Processor\n\n"
                  << "Version: " << NEURONIK_BUILD_VERSION << "\n"
                  << "Build: " << NEURONIK_BUILD_TIMESTAMP << "\n\n"
                  << "© 2026 ABD Neural";

        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                               "About NEURONiK",
                                               aboutText,
                                               "OK");
    }
}
