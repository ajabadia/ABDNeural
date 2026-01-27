/*
  ==============================================================================

    MainComponent.cpp
    Created: 27 Jan 2026
    Description: Main UI implementation for NEURONiK Model Maker.

  ==============================================================================
*/

#include "MainComponent.h"

namespace NEURONiK::ModelMaker {

MainComponent::MainComponent()
{
    formatManager.registerBasicFormats();

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

    // Visualizers
    addAndMakeVisible(waveBox);
    addAndMakeVisible(spectralBox);

    // Footer
    addAndMakeVisible(analyzeButton);
    analyzeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFFD35400)); // Orange accent
    analyzeButton.onClick = [this]() { analyzeAudio(); };

    addAndMakeVisible(exportButton);
    exportButton.onClick = [this]() { exportModel(); };

    exportButton.setEnabled(false);
    analyzeButton.setEnabled(false);

    setSize(800, 600);
}

MainComponent::~MainComponent()
{
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

void MainComponent::loadFile()
{
    fileChooser = std::make_unique<juce::FileChooser>("Select Audio File",
                                                     juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                     "*.wav;*.aif;*.flac");

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            auto* reader = formatManager.createReaderFor(file);
            if (reader != nullptr)
            {
                std::unique_ptr<juce::AudioFormatReader> readerPtr(reader);
                loadedAudio.setSize((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&loadedAudio, 0, (int)reader->lengthInSamples, 0, true, true);
                
                loadedSampleRate = reader->sampleRate;
                thumbnail.setSource(new juce::FileInputSource(file));
                
                fileNameLabel.setText(file.getFileName(), juce::dontSendNotification);
                analyzeButton.setEnabled(true);
                repaint();
            }
        }
    });
}

void MainComponent::analyzeAudio()
{
    if (loadedAudio.getNumSamples() == 0) return;

    // TODO: Detect F0 efficiently. For now, assume user provides a single cycle or short loop
    // and we scan for F0 or assume C3?
    // Proper analysis requires pitch detection.
    // For MVP: Let's assume the user loaded a C3 note (approx 130.81 Hz) or we try to detect.
    
    // Simple Auto-correlation based pitch detection?
    // Or just ask user? 
    // Let's force C3 for MVP or do a basic Zero Crossing if simple.
    // Better: Basic AutoCorrelation.
    
    // For this MVP step 1: Fixed pitch check or assume typically tuned samples.
    // Let's try to detect peak in spectrum in valid range (80Hz - 1000Hz).
    
    // ACTUALLY: The Analyzer takes F0. Let's start with a default C3 (130.81 Hz).
    float f0 = 130.81f; 

    currentModel = analyzer.analyze(loadedAudio, loadedSampleRate, f0);
    
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

    fileChooser = std::make_unique<juce::FileChooser>("Save Model",
                                                     juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                     .getChildFile("NEURONiK").getChildFile("Models").getChildFile("User"),
                                                     "*.neuronikmodel");

    auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [jsonString](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
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
    auto area = getLocalBounds().reduced(20);

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
    analyzeButton.setBounds(footerArea.removeFromRight(150).reduced(0, 2));

    area.removeFromBottom(20); // Spacer

    // Boxes (Split remaining space)
    auto boxHeight = area.getHeight() / 2 - 10;
    waveBox.setBounds(area.removeFromTop(boxHeight));
    area.removeFromTop(20);
    spectralBox.setBounds(area);
}

} // namespace NEURONiK::ModelMaker
