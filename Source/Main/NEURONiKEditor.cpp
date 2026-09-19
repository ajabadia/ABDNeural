#include "NEURONiKEditor.h"

#include "Core/BuildVersion.h"
#include "State/ParameterDefinitions.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

using namespace NEURONiK::State;

NEURONiKEditor::NEURONiKEditor (NEURONiKProcessor& p)
    : AudioProcessorEditor (p),
      processor (p)
{
   #if defined(NEURONIK_HAS_WEBUI_VIEW)
    // La pagina ES la interfaz: el componente trae su proveedor de recursos
    // (embebido), el puente de parametros y los tres adaptadores del procesador.
    webView = std::make_unique<NEURONiK::WebUI::NeuronikWebView> (p);
    addAndMakeVisible (*webView);
   #else
    noWebUiNotice.setText ("NEURONiK: compilado sin interfaz web (NEURONIK_HAS_WEBUI_VIEW off).",
                           juce::dontSendNotification);
    noWebUiNotice.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (noWebUiNotice);
   #endif

    addAndMakeVisible (menuBar);

    setResizable (true, true);
    setResizeLimits (baseWidth / 2, baseHeight / 2, baseWidth * 3, baseHeight * 3);
    setSize (baseWidth, baseHeight);

    // El poll del puente. Hacia la pagina van dos cosas de cadencia distinta: los
    // cambios de parametro en cada tick, y el estado MIDI externo (notas
    // retenidas + ruedas) cada ~180 ms, que es lo que hace que el teclado de la
    // pagina refleje el MIDI del DAW. Misma cadencia que la bancada del piloto.
    startTimerHz (33);
}

NEURONiKEditor::~NEURONiKEditor()
{
    stopTimer();
}

void NEURONiKEditor::paint (juce::Graphics& g)
{
    // El hueco hasta que WebView2 pinta su primera pagina. Se rellena con el
    // color del chasis en vez de dejar el fondo del host: 8.5 mide ese arranque
    // (~7 s en frio en la bancada, que es lo que domina la apertura del editor).
    g.fillAll (juce::Colour (0xff12161c));
}

void NEURONiKEditor::resized()
{
    auto area = getLocalBounds();

    menuBar.setBounds (area.removeFromTop (menuBarHeight));

   #if defined(NEURONIK_HAS_WEBUI_VIEW)
    // La base compartida ajusta el navegador a SUS bounds, asi que el webview solo
    // necesita saber que area es suya: sigue cada resize sin logica propia. Es el
    // "sin regresion en resized()" del ticket.
    webView->setBounds (area);
   #else
    noWebUiNotice.setBounds (area);
   #endif
}

void NEURONiKEditor::timerCallback()
{
   #if defined(NEURONIK_HAS_WEBUI_VIEW)
    webView->poll();
   #endif
}

void NEURONiKEditor::setZoom (float scale)
{
    zoomScale = juce::jlimit (0.5f, 3.0f, scale);

   #if defined(NEURONIK_HAS_WEBUI_VIEW)
    webView->setPageZoom (zoomScale);
   #else
    juce::ignoreUnused (zoomScale);
   #endif
}

juce::StringArray NEURONiKEditor::getMenuBarNames()
{
    return { "File", "Edit", "Help" };
}

juce::PopupMenu NEURONiKEditor::getMenuForIndex (int, const juce::String& menuName)
{
    juce::PopupMenu menu;

    if (menuName == "File")
    {
        menu.addItem (10, "New Session"); // Placeholder for future
       #if JucePlugin_Build_Standalone
        menu.addSeparator();
        menu.addItem (999, "Exit");
       #endif
    }
    else if (menuName == "Edit")
    {
        menu.addItem (1, "Load Preset...");
        menu.addItem (2, "Save Preset...");
        menu.addSeparator();
        menu.addItem (60, "Copy Patch");
        menu.addItem (61, "Paste Patch");
        menu.addSeparator();

        juce::PopupMenu midiChannelMenu;
        auto* choiceParam = processor.getAPVTS().getParameter (IDs::midiChannel);
        int currentChoice = static_cast<int> (choiceParam->getValue() * (choiceParam->getNumSteps() - 1));

        for (int i = 0; i < 17; ++i)
            midiChannelMenu.addItem (20 + i, (i == 0) ? "Omni" : "Channel " + juce::String (i), true, i == currentChoice);

        menu.addSubMenu ("MIDI Channel", midiChannelMenu);
        menu.addSeparator();

        juce::PopupMenu voicesMenu;
        int currentVoices = processor.getPolyphony();
        voicesMenu.addItem (50, "Mono (1 Voice)", true, currentVoices == 1);
        voicesMenu.addItem (51, "2 Voices", true, currentVoices == 2);
        voicesMenu.addItem (52, "3 Voices", true, currentVoices == 3);
        voicesMenu.addItem (53, "4 Voices", true, currentVoices == 4);
        voicesMenu.addItem (54, "6 Voices", true, currentVoices == 6);
        voicesMenu.addItem (55, "8 Voices", true, currentVoices == 8);

        menu.addSubMenu ("Voices", voicesMenu);
        menu.addSeparator();

        juce::PopupMenu zoomMenu;
        zoomMenu.addItem (300, "0.5x", true, zoomScale == 0.5f);
        zoomMenu.addItem (301, "1x (Normal)", true, zoomScale == 1.0f);
        zoomMenu.addItem (302, "1.25x", true, zoomScale == 1.25f);
        zoomMenu.addItem (303, "1.5x", true, zoomScale == 1.5f);
        zoomMenu.addItem (304, "2x", true, zoomScale == 2.0f);
        zoomMenu.addItem (305, "3x", true, zoomScale == 3.0f);
        menu.addSubMenu ("Zoom", zoomMenu);

        menu.addSeparator();
        menu.addItem (14, "Options...");
    }
    else if (menuName == "Help")
    {
        menu.addItem (103, "Random Parameters...");
        menu.addItem (102, "MIDI Specifications...");
        menu.addSeparator();
        menu.addItem (101, "About...");
    }

    return menu;
}

void NEURONiKEditor::menuItemSelected (int menuItemID, int)
{
    if (menuItemID == 1) // Load Preset
    {
        auto fileChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        chooser = std::make_unique<juce::FileChooser> ("Select a preset to load...",
            processor.getPresetManager().getPresetsDirectory(),
            "*.neuronikpreset");

        chooser->launchAsync (fileChooserFlags, [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
                processor.getPresetManager().loadPresetFromFile (file);
        });
    }
    else if (menuItemID == 2) // Save Preset
    {
        auto fileChooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

        chooser = std::make_unique<juce::FileChooser> ("Save current preset...",
            processor.getPresetManager().getPresetsDirectory(),
            "*.neuronikpreset");

        chooser->launchAsync (fileChooserFlags, [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File())
                processor.getPresetManager().savePresetToFile (file);
        });
    }
    else if (menuItemID >= 20 && menuItemID < 37)
    {
        auto* param = processor.getAPVTS().getParameter (IDs::midiChannel);
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (static_cast<float> (menuItemID - 20)));
    }
    else if (menuItemID >= 50 && menuItemID <= 55)
    {
        int voices = 1;
        switch (menuItemID)
        {
            case 50: voices = 1; break;
            case 51: voices = 2; break;
            case 52: voices = 3; break;
            case 53: voices = 4; break;
            case 54: voices = 6; break;
            case 55: voices = 8; break;
            default: break;
        }
        processor.setPolyphony (voices);
    }
    else if (menuItemID == 60)
    {
        auto xml = processor.getFullState().createXml();
        if (xml != nullptr) juce::SystemClipboard::copyTextToClipboard (xml->toString());
    }
    else if (menuItemID == 61)
    {
        auto xmlString = juce::SystemClipboard::getTextFromClipboard();
        auto xml = juce::parseXML (xmlString);
        if (xml != nullptr)
            processor.setFullState (juce::ValueTree::fromXml (*xml));
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

        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
            "NEURONiK Options",
            "Audio/MIDI settings are managed by your DAW/Host when running as a plugin.",
            "OK");
    }
    else if (menuItemID == 101)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
            "About NEURONiK",
            "NEURONiK Synthesizer\n"
            "Hybrid Spectral Morphing Synthesizer\n\n"
            "Dual Engine Architecture:\n"
            "\xe2\x80\xa2 NEURONiK: Advanced Additive Engine\n"
            "\xe2\x80\xa2 Neurotik: Physical Modeling Resonator\n\n"
            "Developed by ABD.",
            "OK");
    }
    else if (menuItemID == 102)
    {
        showMidiSpecifications();
    }
    else if (menuItemID == 103)
    {
        juce::String info = "RANDOM BUTTON - FREEZE CONTROLS\n"
                            "================================\n\n"
                            "The RANDOM button randomizes parameters within musical ranges.\n"
                            "Use FREEZE buttons to protect specific sections.\n\n"
                            "Las ayudas por seccion (RESONATOR / FILTER-FX / ENVELOPES) viven\n"
                            "en la interfaz web: esto es el resto provisional que el ticket\n"
                            "8.3 se lleva a la pagina.";

        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
            "Random Parameters Help",
            info,
            "OK");
    }
    else if (menuItemID >= 300 && menuItemID <= 305)
    {
        float scale = 1.0f;
        switch (menuItemID)
        {
            case 300: scale = 0.5f; break;
            case 301: scale = 1.0f; break;
            case 302: scale = 1.25f; break;
            case 303: scale = 1.5f; break;
            case 304: scale = 2.0f; break;
            case 305: scale = 3.0f; break;
            default: break;
        }
        setZoom (scale);
    }
    else if (menuItemID == 999)
    {
       #if JucePlugin_Build_Standalone
        juce::JUCEApplication::quit();
       #endif
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

    specs << "NEUROTIK SPECIFIC:\n";
    specs << "20: Excite Noise\n";
    specs << "21: Noise Color\n";
    specs << "22: Impulse Mix\n";
    specs << "23: Resonator Resonance\n\n";

    specs << "OTHER CONTROLS:\n";
    specs << "Pitch Bend: Global Pitch\n";
    specs << "Mod Wheel: Routable (Mod Matrix)\n";
    specs << "Aftertouch: Routable (Mod Matrix)\n";

    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
        "MIDI Specifications",
        specs,
        "OK");
}
