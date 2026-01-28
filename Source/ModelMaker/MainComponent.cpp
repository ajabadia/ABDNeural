/*
  ==============================================================================

    MainComponent.cpp
    Created: 27 Jan 2026
    Description: Main UI implementation for NEURONiK Model Maker.

  ==============================================================================
*/

#include <juce_data_structures/juce_data_structures.h>
#include "MainComponent.h"
#include "Version.h"

namespace NEURONiK::ModelMaker {

MainComponent::MainComponent()
    : loadButton("LOAD AUDIO"),
      waveBox("WAVEFORM INPUT"),
      spectralBox("SPECTRAL ANALYSIS (64 PARTIALS)"),
      pitchLabel("Pitch", "Detected Pitch (Hz):"),
      analyzeButton("ANALYZE"),
      playOriginalButton("PLAY ORIGINAL"),
      playModelButton("PLAY MODEL"),
      recordButton("REC"),
      stopButton("STOP"),
      exportButton("EXPORT MODEL"),
      menuBar(this) // Init with model
{
    formatManager.registerBasicFormats();

    // Menu
    addAndMakeVisible(menuBar);

    // Audio Init - Enable Inputs!
    // Initialize with 2 inputs, 2 outputs.
    // Note: If no input device is available, it might fallback or fail silently on inputs.
    deviceManager.initialiseWithDefaultDevices(2, 2); 
    deviceManager.addAudioCallback(this);

    // Header
    titleLabel.setText("NEURONiK MODEL MAKER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(24.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    addAndMakeVisible(loadButton);
    loadButton.onClick = [this]() { loadFile(); };

    fileNameLabel.setText("No file loaded", juce::dontSendNotification);
    fileNameLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(fileNameLabel);

    // Pitch UI
    addAndMakeVisible(pitchLabel);
    pitchLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(pitchEditor);
    pitchEditor.setText("0.0", juce::dontSendNotification);
    pitchEditor.setInputRestrictions(8, "0123456789.");
    pitchEditor.setJustification(juce::Justification::centred);
    pitchEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.2f));
    pitchEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::cyan.withAlpha(0.3f));

    // Note Combos
    addAndMakeVisible(noteCombo);
    noteCombo.addItem("Auto", 1);
    noteCombo.addItem("C", 2); noteCombo.addItem("C#", 3); noteCombo.addItem("D", 4);
    noteCombo.addItem("D#", 5); noteCombo.addItem("E", 6); noteCombo.addItem("F", 7);
    noteCombo.addItem("F#", 8); noteCombo.addItem("G", 9); noteCombo.addItem("G#", 10);
    noteCombo.addItem("A", 11); noteCombo.addItem("A#", 12); noteCombo.addItem("B", 13);
    noteCombo.setSelectedId(1); // Auto
    noteCombo.onChange = [this] { updateFreqFromRootNote(); };

    addAndMakeVisible(octaveCombo);
    octaveCombo.addItem("Auto", 1);
    for (int i = -2; i <= 8; ++i)
        octaveCombo.addItem(juce::String(i), i + 4); // ID offset to avoid clash with Auto=1
    octaveCombo.setSelectedId(1); // Auto
    octaveCombo.onChange = [this] { updateFreqFromRootNote(); };
    
    // Visualizers
    addAndMakeVisible(waveBox);
    addAndMakeVisible(spectralBox);

    // Footer
    addAndMakeVisible(playOriginalButton);
    playOriginalButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2ECC71).withAlpha(0.2f));
    playOriginalButton.onClick = [this]() { playOriginal(); };

    addAndMakeVisible(playModelButton);
    playModelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF9B59B6).withAlpha(0.2f));
    playModelButton.onClick = [this]() { playModel(); };

    addAndMakeVisible(recordButton);
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red.withAlpha(0.6f));
    recordButton.onClick = [this]() 
    {
        if (isRecording) stopRecording();
        else startRecording();
    };

    addAndMakeVisible(stopButton);
    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFE74C3C).withAlpha(0.2f));
    stopButton.onClick = [this]() { stopPlayback(); };

    addAndMakeVisible(analyzeButton);
    analyzeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFD35400)); // Orange accent
    analyzeButton.onClick = [this]() { analyzeAudio(); };

    addAndMakeVisible(exportButton);
    exportButton.onClick = [this]() { exportModel(); };

    // Initial state
    playOriginalButton.setEnabled(false);
    playModelButton.setEnabled(false);
    stopButton.setEnabled(false);
    exportButton.setEnabled(false);
    analyzeButton.setEnabled(false);

    setSize(800, 600);
}

MainComponent::~MainComponent()
{
    deviceManager.removeAudioCallback(this);
    transportSource.setSource(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF101010)); // Deep dark background

    // Subtle background mesh or gradient
    juce::ColourGradient bgGrad(juce::Colour(0xFF1A1A1A), 0, 0,
                               juce::Colour(0xFF050505), 0, (float)getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillAll();

    // Draw Spectral View
    auto specArea = spectralBox.getBounds().reduced(10).withTrimmedTop(25);
    paintSpectralView(g, specArea);

    // Draw Waveform View
    auto waveArea = waveBox.getBounds().reduced(10).withTrimmedTop(25);
    if (thumbnail.getNumChannels() > 0)
    {
        g.setColour(juce::Colours::cyan.withAlpha(0.6f));
        thumbnail.drawChannels(g, waveArea, 0.0, thumbnail.getTotalLength(), 1.0f);
    }
}

// --- Audio Callbacks ---

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    currentSampleRate = device->getCurrentSampleRate();
    transportSource.prepareToPlay(512, currentSampleRate); 
    
    previewResonator.setSampleRate(currentSampleRate);
    previewResonator.reset();
}

void MainComponent::audioDeviceStopped()
{
    transportSource.releaseResources();
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);

    // Recording Logic
    if (isRecording && numInputChannels > 0)
    {
        int currentSize = recordingBuffer.getNumSamples();
        if (currentSize < 10 * 60 * 48000) // Limit to 10 mins
        {
            recordingBuffer.setSize(1, currentSize + numSamples, true, true, false);
            recordingBuffer.copyFrom(0, currentSize, inputChannelData[0], numSamples);
        }
    }

    // Clear outputs
    for (int i = 0; i < numOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);

    // Create a proxy buffer wrapper for the external output data
    juce::AudioBuffer<float> proxyOutputBuffer(outputChannelData, numOutputChannels, numSamples);
    juce::AudioSourceChannelInfo bufferToFill(&proxyOutputBuffer, 0, numSamples);
    
    transportSource.getNextAudioBlock(bufferToFill);

    if (isPlayingModel)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float samp = previewResonator.processSample() * 0.5f; 
            if (i < bufferToFill.numSamples)
            {
                for (int ch = 0; ch < numOutputChannels; ++ch)
                    outputChannelData[ch][i] += samp;
            }
        }
    }
}

// --- Recording & Sync ---

void MainComponent::startRecording()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
    stopPlayback();

    recordingBuffer.setSize(1, 0); 
    isRecording = true;
    
    recordButton.setButtonText("STOP REC");
    playOriginalButton.setEnabled(false);
    playModelButton.setEnabled(false);
    analyzeButton.setEnabled(false);
    fileNameLabel.setText("Recording...", juce::dontSendNotification);
}

void MainComponent::stopRecording()
{
    isRecording = false;
    recordButton.setButtonText("REC");

    if (recordingBuffer.getNumSamples() > 0)
    {
        loadedAudio.makeCopyOf(recordingBuffer);
        loadedSampleRate = currentSampleRate;

        thumbnail.reset(1, loadedSampleRate, loadedAudio.getNumSamples());
        thumbnail.addBlock(0, loadedAudio, 0, loadedAudio.getNumSamples());

        // Write to a temporary file so we can reuse the loading/transport logic
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("NEURONiK_Recording.wav");
        
        {
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(new juce::FileOutputStream(tempFile),
                loadedSampleRate, (unsigned int)loadedAudio.getNumChannels(), 16, juce::StringPairArray(), 0));
            
            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer(loadedAudio, 0, loadedAudio.getNumSamples());
                writer.reset(); // Close file
            }
        }

        if (tempFile.existsAsFile())
        {
            auto* reader = formatManager.createReaderFor(tempFile);
            if (reader != nullptr)
            {
                readerSource.reset(new juce::AudioFormatReaderSource(reader, true));
                transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);
            }
        }

        playOriginalButton.setEnabled(true);
        analyzeButton.setEnabled(true);
        fileNameLabel.setText("Recorded Audio", juce::dontSendNotification);
        
        // Auto-detect pitch immediately? Or wait. Let's wait for user or automate it?
        // Plan says: "auto-detect pitch".
        detectedFrequency = analyzer.detectPitch(loadedAudio, loadedSampleRate);
        pitchEditor.setText(juce::String(detectedFrequency, 2), juce::dontSendNotification);
        updateRootNoteFromFreq(detectedFrequency);
    }
}

void MainComponent::updateFreqFromRootNote()
{
    int noteId = noteCombo.getSelectedId();
    int octId = octaveCombo.getSelectedId();

    if (noteId > 1 && octId > 1) 
    {
        int semi = noteId - 2; 
        int octave = octId - 4;
        
        int midiNote = (octave + 2) * 12 + semi;
        midiNote = juce::jlimit(0, 127, midiNote);
        
        float freq = (float)juce::MidiMessage::getMidiNoteInHertz(midiNote);
        pitchEditor.setText(juce::String(freq, 2));
    }
}

void MainComponent::updateRootNoteFromFreq(float freqHz)
{
    if (freqHz <= 0) return;
    
    int midiNote = juce::roundToInt(12.0 * std::log2(freqHz / 440.0) + 69.0);
    
    int octave = (midiNote / 12) - 2; 
    int semi = midiNote % 12;
    
    int noteId = semi + 2;
    int octId = octave + 4;
    
    if (noteCombo.getSelectedId() == 1) noteCombo.setSelectedId(noteId, juce::dontSendNotification);
    if (octaveCombo.getSelectedId() == 1) octaveCombo.setSelectedId(octId, juce::dontSendNotification);
}

// --- Playback Controls ---

void MainComponent::playOriginal()
{
    stopPlayback(); // Reset state
    transportSource.setPosition(0.0);
    transportSource.start();
    stopButton.setEnabled(true);
}

void MainComponent::playModel()
{
    stopPlayback();
    previewResonator.reset();
    isPlayingModel = true;
    stopButton.setEnabled(true);
}

void MainComponent::stopPlayback()
{
    transportSource.stop();
    isPlayingModel = false;
    stopButton.setEnabled(false);
}

void MainComponent::loadFile()
{
    juce::File startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    
    juce::String lastPath = loadSetting("lastAudioPath");
    if (lastPath.isNotEmpty())
        startDir = juce::File(lastPath);

    fileChooser = std::make_unique<juce::FileChooser>("Select Audio File",
                                                     startDir,
                                                     "*.wav;*.aif;*.flac");

    auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(browserFlags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            saveSetting("lastAudioPath", file.getParentDirectory().getFullPathName());

            auto* reader = formatManager.createReaderFor(file);
            if (reader != nullptr)
            {
                std::unique_ptr<juce::AudioFormatReader> readerPtr(reader);
                
                // Load into memory for analysis
                loadedAudio.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&loadedAudio, 0, (int)reader->lengthInSamples, 0, true, true);
                
                loadedSampleRate = reader->sampleRate;
                
                // Load into Transport for playback
                readerSource.reset(new juce::AudioFormatReaderSource(readerPtr.release(), true));
                transportSource.setSource(readerSource.get(), 0, nullptr, reader->sampleRate);
                
                thumbnail.setSource(new juce::FileInputSource(file));
                fileNameLabel.setText(file.getFileName(), juce::dontSendNotification);
                
                // Auto Detect Pitch
                detectedFrequency = analyzer.detectPitch(loadedAudio, loadedSampleRate);
                pitchEditor.setText(juce::String(detectedFrequency, 2), juce::dontSendNotification);

                analyzeButton.setEnabled(true);
                playOriginalButton.setEnabled(true);
                playModelButton.setEnabled(false); // Enable only after analysis
                repaint();
            }
        }
    });
}

void MainComponent::analyzeAudio()
{
    if (loadedAudio.getNumSamples() == 0) return;

    float f0 = pitchEditor.getText().getFloatValue();
    if (f0 < 20.0f) f0 = 130.81f; 

    // Discrepancy check
    int noteId = noteCombo.getSelectedId();
    int octId = octaveCombo.getSelectedId();
    
    if (noteId > 1 && octId > 1) 
    {
        int selectedMidi = (octId - 4 + 2) * 12 + (noteId - 2);
        float rawDetectF = analyzer.detectPitch(loadedAudio, loadedSampleRate);
        int rawMidi = juce::roundToInt(12.0 * std::log2(rawDetectF / 440.0) + 69.0);
        
        if (std::abs(rawMidi - selectedMidi) > 1)
        {
             juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                    "Pitch Discrepancy",
                                                    "Detected: " + juce::String(rawDetectF, 1) + " Hz\n" +
                                                    "Selected Note: " + juce::MidiMessage::getMidiNoteName(selectedMidi, true, true, 3) + "\n\n" +
                                                    "Syncing to detected pitch for accuracy.",
                                                    "OK");
             
             f0 = rawDetectF;
             pitchEditor.setText(juce::String(f0, 2), juce::dontSendNotification);
             updateRootNoteFromFreq(f0);
        }
    }

    performAnalysis(f0);
}

void MainComponent::performAnalysis(float f0)
{
    currentModel = analyzer.analyze(loadedAudio, loadedSampleRate, f0);
    
    // Prepare Preview
    previewResonator.loadModel(currentModel, 0); // Load into slot A
    previewResonator.setBaseFrequency(f0);
    
    playModelButton.setEnabled(true);
    exportButton.setEnabled(true);
    repaint();
}

void MainComponent::exportModel()
{
    // Serialize to JSON
    juce::DynamicObject* modelObj = new juce::DynamicObject();
    
    juce::Array<juce::var> amps;
    juce::Array<juce::var> offsets;
    
    for (int i = 0; i < 64; ++i)
    {
        amps.add(currentModel.amplitudes[i]);
        offsets.add(currentModel.frequencyOffsets[i]);
    }
    
    modelObj->setProperty("amplitudes", amps);
    modelObj->setProperty("frequencyOffsets", offsets);
    modelObj->setProperty("name", fileNameLabel.getText());
    modelObj->setProperty("description", "Created with NEURONiK Model Maker");

    juce::var json(modelObj);
    juce::String jsonString = juce::JSON::toString(json);

    juce::File startDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                          .getChildFile("NEURONiK").getChildFile("Models").getChildFile("User");

    juce::String lastPath = loadSetting("lastExportPath");
    if (lastPath.isNotEmpty())
        startDir = juce::File(lastPath);

    // Default filename derived from loaded audio
    juce::String defaultFileName = "model.neuronikmodel";
    juce::String currentText = fileNameLabel.getText();
    if (currentText.isNotEmpty() && currentText != "No file loaded")
    {
        defaultFileName = juce::File(currentText).getFileNameWithoutExtension() + ".neuronikmodel";
    }

    fileChooser = std::make_unique<juce::FileChooser>("Save Model",
                                                     startDir.getChildFile(defaultFileName),
                                                     "*.neuronikmodel");

    auto browserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(browserFlags, [jsonString, this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            saveSetting("lastExportPath", file.getParentDirectory().getFullPathName());
            file.replaceWithText(jsonString);
        }
    });
}

void MainComponent::paintSpectralView(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Draw 64 bars
    float barWidth = (float)area.getWidth() / 64.0f;
    float maxBarHeight = (float)area.getHeight();
    
    for (int i = 0; i < 64; ++i)
    {
        float amp = currentModel.amplitudes[i]; // 0 to 1
        float barHeight = amp * maxBarHeight;
        
        juce::Rectangle<float> bar(
            area.getX() + i * barWidth,
            area.getBottom() - barHeight,
            barWidth - 1.0f,
            barHeight
        );
        
        g.setColour(juce::Colours::orange.withAlpha(0.8f));
        g.fillRect(bar);
        
        // Reflection/Glow
        if (amp > 0.1f)
        {
            g.setColour(juce::Colours::orange.withAlpha(0.3f));
            g.fillRect(bar.withHeight(2.0f).translated(0, -2.0f));
        }
    }
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    
    // Menu Bar
    menuBar.setBounds(area.removeFromTop(juce::LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));

    area.reduce(20, 20); // Add padding after menu

    // Header
    auto headerArea = area.removeFromTop(40);
    titleLabel.setBounds(headerArea.removeFromLeft(300));
    exportButton.setBounds(headerArea.removeFromRight(120).reduced(0, 4));
    headerArea.removeFromRight(20); 
    loadButton.setBounds(headerArea.removeFromRight(100).reduced(0, 4));
    fileNameLabel.setBounds(headerArea); // Remaining space

    area.removeFromTop(20); // Spacer

    // Footer
    auto footerArea = area.removeFromBottom(40);
    
    // Left side: Playback & Record
    recordButton.setBounds(footerArea.removeFromLeft(60).reduced(0, 4));
    footerArea.removeFromLeft(10);
    playOriginalButton.setBounds(footerArea.removeFromLeft(120).reduced(0, 4));
    footerArea.removeFromLeft(10);
    playModelButton.setBounds(footerArea.removeFromLeft(120).reduced(0, 4));
    footerArea.removeFromLeft(10);
    stopButton.setBounds(footerArea.removeFromLeft(80).reduced(0, 4));

    // Right side: Analysis & Pitch
    analyzeButton.setBounds(footerArea.removeFromRight(120).reduced(0, 2));
    footerArea.removeFromRight(20);
    
    // Pitch & Note
    pitchEditor.setBounds(footerArea.removeFromRight(60).reduced(0, 8));
    pitchLabel.setBounds(footerArea.removeFromRight(100));
    
    footerArea.removeFromRight(10);
    octaveCombo.setBounds(footerArea.removeFromRight(50).reduced(0, 8));
    noteCombo.setBounds(footerArea.removeFromRight(60).reduced(0, 8));
    
    area.removeFromBottom(20); // Spacer

    // Boxes (Split remaining space)
    auto boxHeight = area.getHeight() / 2 - 10;
    waveBox.setBounds(area.removeFromTop(boxHeight));
    area.removeFromTop(20);
    spectralBox.setBounds(area);
}

// --- Persistence Helpers ---

juce::File MainComponent::getSettingsFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("NEURONiK").getChildFile("ModelMaker.xml");
}

void MainComponent::saveSetting(const juce::String& key, const juce::String& value)
{
    auto file = getSettingsFile();
    if (!file.getParentDirectory().exists())
        file.getParentDirectory().createDirectory();

    std::unique_ptr<juce::XmlElement> root;
    if (file.exists())
        root = juce::XmlDocument::parse(file);
    
    if (root == nullptr)
        root = std::make_unique<juce::XmlElement>("SETTINGS");

    root->setAttribute(key, value);
    root->writeTo(file);
}

juce::String MainComponent::loadSetting(const juce::String& key)
{
    auto file = getSettingsFile();
    if (file.exists())
    {
        auto root = juce::XmlDocument::parse(file);
        if (root != nullptr)
            return root->getStringAttribute(key);
    }
    return {};
}


// --- MenuBarModel ---

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int menuIndex, const juce::String& /*menuName*/)
{
    juce::PopupMenu menu;

    if (menuIndex == 0) // File
    {
        menu.addItem(1, "Load Audio...", true, false);
        menu.addItem(2, "Export Model...", exportButton.isEnabled(), false);
        menu.addSeparator();
        menu.addItem(3, "Exit", true, false);
    }
    else if (menuIndex == 1) // Edit
    {
        menu.addItem(4, "Analyze", analyzeButton.isEnabled(), false);
        menu.addSeparator();
        menu.addItem(5, "Play Original", playOriginalButton.isEnabled(), false);
        menu.addItem(6, "Play Model", playModelButton.isEnabled(), false);
        menu.addItem(7, "Stop", stopButton.isEnabled(), false);
        menu.addSeparator();
        menu.addItem(9, "Options...", true, false);
    }
    else if (menuIndex == 2) // Help
    {
        menu.addItem(8, "About NEURONiK Model Maker...", true, false);
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
    switch (menuItemID)
    {
        case 1: loadFile(); break;
        case 2: exportModel(); break;
        case 3: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
        case 4: analyzeAudio(); break;
        case 5: playOriginal(); break;
        case 6: playModel(); break;
        case 7: stopPlayback(); break;
        case 8:
        {
            juce::String version = juce::String(NEURONIK_MODELMAKER_VERSION_MAJOR) + "." +
                                   juce::String(NEURONIK_MODELMAKER_VERSION_MINOR) + "." +
                                   juce::String(NEURONIK_MODELMAKER_VERSION_SUB);

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "About NEURONiK Model Maker",
                "NEURONiK Model Maker\nVersion " + version + "\n\n"
                "Part of the NEURONiK Synthesizer Suite.\n"
                "(c) 2026 ABD Neural Audio",
                "OK"
            );
            break;
        }
        case 9:
        {
            juce::DialogWindow::LaunchOptions opt;
            opt.dialogTitle = "Audio Settings";
            opt.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
            opt.content.setOwned(new juce::AudioDeviceSelectorComponent(deviceManager, 0, 2, 0, 2, false, false, true, false));
            opt.content->setSize(500, 300);
            opt.launchAsync();
            break;
        }
    }
}

} // namespace NEURONiK::ModelMaker
