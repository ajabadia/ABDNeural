/*
  ==============================================================================

    MainComponent.h
    Created: 27 Jan 2026
    Description: Main UI for NEURONiK Model Maker.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "Analysis/SpectralAnalyzer.h"
#include "../DSP/CoreModules/Resonator.h" // Reuse existing Resonator from plugin

#include "ModelMakerWidgets.h"

namespace NEURONiK {
namespace ModelMaker {

// --- Main Component ---

class MainComponent : public juce::Component,
                      public juce::AudioIODeviceCallback,
                      public juce::MenuBarModel
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // MenuBarModel Overrides
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;
    
    void setZoom(float scale);

private:
    // Header
    // juce::Label titleLabel; // Replaced by MenuBar mostly, or kept below
    juce::Label titleLabel;
    juce::MenuBarComponent menuBar;
    CustomButton loadButton;
    juce::Label fileNameLabel;

    // Visualizers
    GlassBox waveBox;
    GlassBox spectralBox;

    // Footer Controls
    juce::Label pitchLabel;
    juce::TextEditor pitchEditor;
    juce::ComboBox noteCombo;
    juce::ComboBox octaveCombo;
    CustomButton analyzeButton;
    CustomButton playOriginalButton;
    CustomButton playModelButton;
    CustomButton recordButton;
    CustomButton stopButton;
    CustomButton exportButton;

    // State
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::AudioBuffer<float> loadedAudio;
    juce::AudioBuffer<float> recordingBuffer;
    double loadedSampleRate = 44100.0;
    float detectedFrequency = 0.0f;
    bool isRecording = false;
    
    // Tools
    juce::AudioThumbnailCache thumbnailCache { 1 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };
    Analysis::SpectralAnalyzer analyzer;
    NEURONiK::Common::SpectralModel currentModel;

    // Helpers
    void loadFile();
    void analyzeAudio();
    void performAnalysis(float f0);
    void exportModel();
    void startRecording();
    void stopRecording();
    void updateFreqFromRootNote();
    void updateRootNoteFromFreq(float freqHz);
    void paintSpectralView(juce::Graphics& g, juce::Rectangle<int> area);

    // Audio Callbacks
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // Playback Helpers
    void playOriginal();
    void playModel();
    void stopPlayback();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)

private:
    // Audio Engine
    juce::AudioDeviceManager deviceManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    NEURONiK::DSP::Core::Resonator previewResonator;
    
    // Playback State
    bool isPlayingModel = false;
    double currentSampleRate = 48000.0;
    
    // Persistence Helpers
    juce::File getSettingsFile();
    void saveSetting(const juce::String& key, const juce::String& value);
    juce::String loadSetting(const juce::String& key);
    
    float zoomScale = 1.0f;
};

} // namespace ModelMaker
} // namespace NEURONiK
