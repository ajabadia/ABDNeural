#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <map>
#include "../Serialization/PresetManager.h"
#include "MidiMappingManager.h"
#include "../DSP/ISynthesisEngine.h"
#include "../Common/SpectralModel.h"
#include "ModulationTargets.h"

namespace NEURONiK::DSP { class NeuronikEngine; }

class NEURONiKProcessor : public juce::AudioProcessor,
                     public juce::AudioProcessorValueTreeState::Listener,
                     public juce::MidiKeyboardState::Listener,
                     public juce::ValueTree::Listener
{
public:
    NEURONiKProcessor();
    ~NEURONiKProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // --- MIDI Learn ---
    void enterMidiLearnMode(const juce::String& paramID);
    void clearMidiLearnForParameter(const juce::String& paramID);

    // --- Model Loading ---
    void loadModel(const juce::File& file, int slot);
    void reloadModels();

    // --- Patch Copy/Paste ---
    void copyPatchToClipboard();
    void pastePatchFromClipboard();

    // --- Editor Creation ---
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // --- JUCE Boilerplate ---
    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    // --- State Management ---
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // --- Getters ---
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    const std::array<juce::String, 4>& getModelNames() const { return modelNames; }
    NEURONiK::Serialization::PresetManager& getPresetManager() { return *presetManager; }
    NEURONiK::Serialization::PresetManager& getPresetManager() const { return *presetManager; }
    NEURONiK::Main::MidiMappingManager& getMidiMappingManager() { return *midiMappingManager; }

    // --- Real-time Visualization Data ---
    std::array<std::atomic<float>, 64> spectralDataForUI;
    
    // Envelope Visualization Atomics
    std::atomic<float> uiEnvelope { 0.0f };     // Amp Env Level
    std::atomic<float> uiFEnvelope { 0.0f };    // Filter Env Level
    
    // Parameters for ADSR Visualizers (snapshot from APVTS)
    std::atomic<float> uiAttack { 0.0f }, uiDecay { 0.0f }, uiSustain { 0.0f }, uiRelease { 0.0f };
    std::atomic<float> uiFAttack { 0.0f }, uiFDecay { 0.0f }, uiFSustain { 0.0f }, uiFRelease { 0.0f };
    
    // Macro visualization
    std::atomic<float> uiMorphX { 0.0f };
    std::atomic<float> uiMorphY { 0.0f };

    std::atomic<float> lfo1ValueForUI { 0.0f };
    std::atomic<float> lfo2ValueForUI { 0.0f };

    // --- Polyphony Management ---
    struct EditorSettings {
        std::unique_ptr<juce::FileChooser> chooser;
    };

    void setPolyphony(int numVoices);
    int getPolyphony() const;
    EditorSettings& getEditorSettings();

public:
    // --- Modulation Access (Safe Atomic Indexing) ---
    std::atomic<float>& getModulationValue(::NEURONiK::ModulationTarget target) noexcept
    {
        return modulationValues[static_cast<size_t>(target)];
    }

    const std::atomic<float>& getModulationValue(::NEURONiK::ModulationTarget target) const noexcept
    {
        return modulationValues[static_cast<size_t>(target)];
    }

protected:
    // --- MidiKeyboardState::Listener overrides ---
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

    // ValueTree::Listener
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeRedirected(juce::ValueTree& tree) override;


private:
    std::array<std::atomic<float>, ::NEURONiK::ModulationTargetCount> modulationValues;

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<NEURONiK::Serialization::PresetManager> presetManager;
    juce::MidiKeyboardState keyboardState;
    std::unique_ptr<NEURONiK::DSP::ISynthesisEngine> engine;

    // --- UI MIDI Message Injection (Safe FIFO) ---
    juce::AbstractFifo midiFifo;
    struct QueuedMidiMessage {
        juce::MidiMessage message;
        int sampleOffset;
    };
    std::array<QueuedMidiMessage, 1024> midiQueue;

    void synchronizeEngineParameters();

    // --- Lock-Free Command Queue (Model Loading) ---
    struct EngineCommand {
        enum Type { LoadModel, Reset, Unknown };
        Type type = Unknown;
        int slot = 0;
        NEURONiK::Common::SpectralModel modelData;
    };
    
    juce::AbstractFifo commandFifo;
    std::array<EngineCommand, 32> commandQueue;
    
    void processCommands();

    std::array<juce::String, 4> modelNames;

    // --- MIDI Real-time values for Modulation ---
    std::atomic<float> pitchBendValue { 0.5f };
    std::atomic<float> modWheelValue { 0.0f };
    std::atomic<float> aftertouchValue { 0.0f };

    // --- MIDI Learn ---
    std::atomic<bool> midiLearnActive { false };
    juce::String parameterToLearn;
    std::unique_ptr<NEURONiK::Main::MidiMappingManager> midiMappingManager;

    std::atomic<int> currentPolyphony { 8 };
    EditorSettings editorSettings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NEURONiKProcessor)
};
