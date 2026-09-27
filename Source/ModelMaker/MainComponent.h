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
#include "Analysis/LayerView.h"   // FASE 11.5: los datos de vista de las capas
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
    // 2026-09-25: el indicador de rejilla es clicable (ver `useDetectedFrequency`).
    void mouseUp (const juce::MouseEvent&) override;
    // 2026-09-27: el indicador de rejilla es alcanzable y accionable CON
    // TECLADO (Tab lo enfoca; Space/Enter actuan el mismo camino del clic).
    bool keyPressed (const juce::KeyPress&) override;
    void paintOverChildren (juce::Graphics&) override;   // anillo de foco
    void focusStateChangedCallback();                    // repinta el anillo

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
    juce::Label pitchGuardLabel; // aviso de desviacion de pitch (guardia) + residuo de rejilla
    juce::Label gridLabel;       // indicador de rejilla detectada (f0 + residuo);
                                 // CLICABLE: carga esa f0 en el editor de pitch

    // 2026-09-27: foco/teclado del indicador de rejilla (Tab + Space/Enter).
    bool gridLabelHasFocus = false;                          // anillo pintado?
    juce::FocusChangeListener* gridFocusCallback = nullptr;  // repinta al entrar/salir

    // Visualizers
    GlassBox waveBox;
    GlassBox spectralBox;
    GlassBox layersBox;          // FASE 11.5: la vista de capas (11.2)

    // Footer Controls
    juce::Label pitchLabel;
    juce::ComboBox framesCombo;   // FASE 10.4: frames temporales del analisis
    juce::TextEditor pitchEditor;
    // 2026-09-25: modo REJILLA FIJA — el usuario declara la rejilla y el
    // analisis NO sigue el pitch por ventana (lo dice el indicador y lo escribe
    // el modelo en "gridFixed").
    juce::ToggleButton fixedGridButton { "Rejilla fija" };
    // 2026-09-25: OFFSETS TRANSPONIBLES — el usuario declara que los
    // offsets son RATIOS contra la rejilla (delta-n/n): el motor los escala
    // por base/f0 al renderizar, asi que la inharmonicidad se transpone con
    // el teclado. El modelo lo escribe ("offsetsTranspose").
    juce::ToggleButton offsetsTransposeButton { "Offsets transp." };
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
    Analysis::LayerView currentLayerView;   // FASE 11.5: que indice fue a que capa

    // Helpers
    void loadFile();
    void analyzeAudio();
    void performAnalysis(float f0, bool fixedGrid);
    void updateGridIndicator();
    void useDetectedFrequency();   // clic en el indicador -> editor de pitch
    int  selectedFrameCount() const;
    void exportModel();
    void startRecording();
    void stopRecording();
    void updateFreqFromRootNote();
    void updateRootNoteFromFreq(float freqHz);
    void paintSpectralView(juce::Graphics& g, juce::Rectangle<int> area);
    void paintLayerView(juce::Graphics& g, juce::Rectangle<int> area);

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
