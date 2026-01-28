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
    // --- Header ---
    addAndMakeVisible(lcdDisplay);
    addAndMakeVisible(menuBtn);
    addAndMakeVisible(okBtn);
    
    // Add D-Pad
    addAndMakeVisible(leftBtn);
    addAndMakeVisible(rightBtn);
    addAndMakeVisible(upBtn);
    addAndMakeVisible(downBtn);
    
    // Setup Listeners
    for (auto* b : { &menuBtn, &okBtn, &leftBtn, &rightBtn, &upBtn, &downBtn }) {
        b->onStateChange = [this, b] { buttonStateChanged(b); };
    }

    menuBtn.onClick = [this] { 
        menuManager.onMenuPress(); 
        updateLcdDefault();
    };
    
    okBtn.onClick = [this] { 
        menuManager.onOkPress(); 
        updateLcdDefault();
    };

    updateLcdDefault();
    
    // Listen to ALL parameters
    for (auto* param : p.getAPVTS().processor.getParameters())
    {
        if (auto* pSafe = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            p.getAPVTS().addParameterListener(pSafe->getParameterID(), this);
    }
    addAndMakeVisible(menuBar);
    addAndMakeVisible(keyboardComponent);
    addAndMakeVisible(mainTabs);
    addAndMakeVisible(visualizer);

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

NEURONiKEditor::~NEURONiKEditor()
{
    // Unregister listeners
    for (auto* param : processor.getAPVTS().processor.getParameters())
    {
        if (auto* pSafe = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            processor.getAPVTS().removeParameterListener(pSafe->getParameterID(), this);
    }
}

void NEURONiKEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    auto* param = processor.getAPVTS().getParameter(parameterID);
    if (!param) return;

    juce::String name = param->getName(16);
    juce::String valStr = param->getCurrentValueAsText();
    
    // Move to UI thread
    juce::MessageManager::callAsync([this, name, valStr] {
        lcdDisplay.showParameterPreview(name, valStr);
    });

    juce::ignoreUnused(newValue);
}

void NEURONiKEditor::updateLcdDefault()
{
    juce::String l1 = menuManager.getLine1();
    juce::String l2 = menuManager.getLine2();

    if (menuManager.getState() == NEURONiK::UI::LcdMenuManager::State::Idle)
    {
        l1 = "PATCH: " + processor.getPresetManager().getCurrentPreset().toUpperCase();
        l2 = "NEURONiK READY";
    }
    else if (menuManager.isEditing())
    {
        auto item = menuManager.getCurrentItem();
        auto paramID = item.paramID;

        if (item.type == NEURONiK::UI::LcdMenuManager::ItemType::MidiCC)
        {
            int cc = processor.getMidiMappingManager().getCCForParam(paramID);
            juce::String ccStr = (cc < 0) ? "---" : juce::String(cc);
            
            // Check for potential conflict if we were to move?
            // (The setMapping logic resolves them, but we display the current)
            l2 = "> " + item.label + ": CC " + ccStr;
            
            // Add 'L' if it has a mapping? Wait, user says 'L' for MIDI Learn active?
            // Usually 'L' means it's learned. 
            if (cc >= 0) l2 += " [L]";
        }
        else if (auto* param = processor.getAPVTS().getParameter(paramID))
        {
            float val = param->getValue();
            juce::String valStr;
            
            if (param->isDiscrete())
                valStr = param->getCurrentValueAsText();
            else
                valStr = juce::String(param->convertFrom0to1(val), 3);
                
            l2 = "> " + menuManager.getLine2() + ": " + valStr;
        }
    }

    lcdDisplay.setDefaultText(l1, l2);
}

// --- Button Logic ---
void NEURONiKEditor::timerCallback()
{
    if (activeHoldButton && activeHoldButton->isDown())
    {
        holdCounter++;
        handleButtonAction(activeHoldButton, true);
    }
    else
    {
        stopTimer();
        activeHoldButton = nullptr;
        holdCounter = 0;
    }
}

void NEURONiKEditor::buttonStateChanged(juce::Button* b)
{
    if (b->isDown())
    {
        // Only D-Pad buttons use the repeat logic.
        // Command buttons (MENU/OK) use standard onClick.
        if (b == &leftBtn || b == &rightBtn || b == &upBtn || b == &downBtn)
        {
            holdCounter = 0;
            activeHoldButton = b;
            handleButtonAction(b, false); // First trigger
            startTimer(200);
        }
    }
    else if (activeHoldButton == b)
    {
        stopTimer();
        activeHoldButton = nullptr;
        holdCounter = 0;
    }
}

void NEURONiKEditor::updateModelNames()
{
    const auto& names = processor.getModelNames();
    for (int i = 0; i < 4; ++i)
        oscPanel.setModelName(i, names[i]);
}

// We need to implement the timer callback for the hold timer. 
// Since NEURONiKEditor doesn't inherit Timer (parameterPanel does), we need to add inheritance or use a helper.
// Recommendation: Add "private juce::Timer" inheritance to NEURONiKEditor in the previous step (header).
// We'll assume I missed adding the inheritance in the header edit, so I will add the logic to a NEW method 
// and handle the inheritance fix in the header/source if needed.
// WAIT: I declared `juce::Timer holdTimer` member in older edit? 
// No, I declared `juce::Timer holdTimer;` which is WRONG because Timer is an interface (mostly) or needs virtual callback.
// JUCE Timer is a mixin class usually. One cannot instantiate `juce::Timer` directly unless it has a callback assigned (not standard JUCE).
// Standard JUCE: Inherit from Timer.
// FIX: I will use a simple logical fix: I will remove the member `juce::Timer holdTimer` and instead make NEURONiKEditor inherit from Timer 
// or use `juce::Time::waitForMillisecondCounter` (blocking, bad).
// Okay, let's use the `callById` parameter of `startTimer` if available, or just implement `timerCallback` in Editor.
// Editor ALREADY inherits `AudioProcessorValueTreeState::Listener`. 
// I will add `private juce::Timer` to inheritance in the next fix if needed.
// FOR NOW: I'll assume I can add the logic to `handleButtonAction`.

void NEURONiKEditor::handleButtonAction(juce::Button* b, bool isRepeat)
{
    if (!b) return;
    
    // We only process D-Pad buttons here. MENU/OK are handled by onClick.
    if (b != &leftBtn && b != &rightBtn && b != &upBtn && b != &downBtn)
        return;

    int direction = (b == &rightBtn || b == &upBtn) ? 1 : -1;
    
    // Acceleration Logic
    float paramDelta = 0.01f;
    if (isRepeat && holdCounter > 8) { // > 1.6s @ 200ms
        paramDelta = 0.05f;
    }

    // LEFT / RIGHT: Menu Navigation
    if (b == &leftBtn || b == &rightBtn)
    {
        if (!menuManager.isEditing())
        {
            menuManager.onEncoderRotate(direction);
        }
    }
    // UP / DOWN: Value adjustment (or Preset navigation in Idle)
    else if (b == &upBtn || b == &downBtn)
    {
        auto state = menuManager.getState();
        
        if (state == NEURONiK::UI::LcdMenuManager::State::Idle)
        {
            if (direction > 0) processor.getPresetManager().loadNextPreset();
            else processor.getPresetManager().loadPreviousPreset();
        }
        else
        {
            auto item = menuManager.getCurrentItem();
            auto paramID = item.paramID;

            if (item.type == NEURONiK::UI::LcdMenuManager::ItemType::MidiCC)
            {
                if (paramID.isNotEmpty())
                {
                    int currentCC = processor.getMidiMappingManager().getCCForParam(paramID);
                    int newCC = juce::jlimit(-1, 127, currentCC + direction);
                    processor.getMidiMappingManager().setMapping(paramID, newCC);
                    
                    if (!menuManager.isEditing())
                        menuManager.onOkPress();
                }
            }
            else if (item.type == NEURONiK::UI::LcdMenuManager::ItemType::Action)
            {
                if (paramID == "RESET_MIDI")
                {
                    processor.getMidiMappingManager().resetToDefaults();
                }
            }
            else if (paramID.isNotEmpty())
            {
                // ... existing parameter edit logic ...
                if (auto* param = processor.getAPVTS().getParameter(paramID))
                {
                    // Discrete logic
                    if (param->isDiscrete())
                    {
                        float steps = (float)param->getNumSteps();
                        if (steps > 1) paramDelta = 1.0f / (steps - 1.0f);
                        else paramDelta = 1.0f;
                    }
                    
                    float currentVal = param->getValue();
                    float newVal = juce::jlimit(0.0f, 1.0f, currentVal + (paramDelta * direction));
                    
                    if (newVal != currentVal)
                        param->setValueNotifyingHost(newVal);
                    
                    if (!menuManager.isEditing())
                        menuManager.onOkPress();
                }
            }
        }
    }
    
    updateLcdDefault();
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
    auto lcdArea = headerArea.removeFromLeft(static_cast<int>(headerArea.getWidth() * 0.6f)).reduced(10, 5);
    auto visualizerArea = headerArea;

    // Split LCD area to include hardware controls
    auto hardwareArea = lcdArea.removeFromRight(110);
    lcdDisplay.setBounds(lcdArea);
    
    // Layout:
    // Left Col: MENU / OK
    // Right Grid: < > (Row 1), ^ v (Row 2)
    
    auto cmdCol = hardwareArea.removeFromLeft(50);
    menuBtn.setBounds(cmdCol.removeFromTop(cmdCol.getHeight() / 2).reduced(2));
    okBtn.setBounds(cmdCol.reduced(2));

    auto padArea = hardwareArea.reduced(2);
    auto row1 = padArea.removeFromTop(padArea.getHeight() / 2);
    auto row2 = padArea;
    
    leftBtn.setBounds(row1.removeFromLeft(row1.getWidth()/2).reduced(2));
    rightBtn.setBounds(row1.reduced(2));
    
    upBtn.setBounds(row2.removeFromLeft(row2.getWidth()/2).reduced(2));
    downBtn.setBounds(row2.reduced(2));

    visualizer.setBounds(visualizerArea.reduced(5));

    auto keyboardArea = area.removeFromBottom(100);
    keyboardComponent.setBounds(keyboardArea);

    float numWhiteKeys = 43.0f;
    keyboardComponent.setKeyWidth(static_cast<float>(keyboardArea.getWidth()) / numWhiteKeys);

    mainTabs.setBounds(area.reduced(10));
}

void NEURONiKEditor::setZoom(float scale)
{
    zoomScale = scale;
    
    // Base size is 800x600
    int newWidth = juce::roundToInt(800.0f * zoomScale);
    int newHeight = juce::roundToInt(600.0f * zoomScale);
    
    // If we are in a wrapper (plugin), we might need to notify the host.
    // AudioProcessorEditor::setSize will do this.
    setSize(newWidth, newHeight);
    
    // Apply affine transform if we want a "real" visual zoom 
    // but JUCE's setSize + resized logic is usually cleaner for plugins.
    // However, for advanced scaling, using AffineTransform on the top level is smoother.
    setTransform(juce::AffineTransform::scale(zoomScale));
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

        juce::PopupMenu zoomMenu;
        zoomMenu.addItem(300, "1x (Normal)", true, zoomScale == 1.0f);
        zoomMenu.addItem(301, "2x", true, zoomScale == 2.0f);
        zoomMenu.addItem(302, "3x", true, zoomScale == 3.0f);
        zoomMenu.addItem(303, "4x", true, zoomScale == 4.0f);
        menu.addSubMenu("Zoom", zoomMenu);

        menu.addSeparator();
        menu.addItem(14, "Options...");
    }
    else if (menuName == "Help")
    {
        menu.addItem(101, "About...");
        menu.addItem(102, "MIDI Specifications...");
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
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
            "About NEURONiK",
            "NEURONiK Synthesizer\n"
            "Enhanced Hybrid Resynthesis Engine\n\n"
            "Inspired by Neural models.\n"
            "Developed by ABD.",
            "OK");
    }
    else if (menuItemID == 102)
    {
        showMidiSpecifications();
    }
    else if (menuItemID >= 300 && menuItemID <= 303)
    {
        setZoom(static_cast<float>(menuItemID - 299));
    }
}
void NEURONiKEditor::showMidiSpecifications()
{
    juce::String specs = "FACTORY MIDI CC MAPPINGS\n"
                         "=========================\n\n";

    specs << "74: Filter Cutoff\n";
    specs << "71: Filter Resonance\n";
    specs << "07: Master Volume\n";
    specs << "73: Envelope Attack\n";
    specs << "72: Envelope Release\n";
    specs << "12: Morph X\n";
    specs << "13: Morph Y\n";
    specs << "14: Inharmonicity\n";
    specs << "15: Roughness\n";
    specs << "16: Odd/Even Balance\n";
    specs << "17: Spectral Shift\n";
    specs << "18: Harmonic Roll-off\n";
    specs << "79: Filter Env Amount\n";
    specs << "91: Saturation\n";
    specs << "93: Chorus Mix\n";
    specs << "94: Delay Time\n";
    specs << "95: Reverb Mix\n\n";
    
    specs << "OTHER CONTROLS:\n";
    specs << "Pitch Bend: Global Pitch\n";
    specs << "Mod Wheel: Routable (Mod Matrix)\n";
    specs << "Aftertouch: Routable (Mod Matrix)\n";

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
        "MIDI Specifications",
        specs,
        "OK");
}
