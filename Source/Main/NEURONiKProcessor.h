#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include <map>
#include "../DSP/Synthesis/ResonatorVoice.h"
#include "../DSP/Effects/Saturation.h"
#include "../DSP/Effects/Delay.h"
#include "../DSP/CoreModules/LFO.h"
#include <atomic>
#include <map>
#include "../DSP/Synthesis/ResonatorVoice.h"
#include "../DSP/Effects/Saturation.h"
#include "../DSP/Effects/Delay.h"
#include "../DSP/CoreModules/LFO.h"
#include "../DSP/CoreModules/Resonator.h" // Include for SpectralModel
#include "../Serialization/PresetManager.h"

namespace NEURONiK::DSP::Effects {
    class Saturation;
    class Delay;
}

class NEURONiKProcessor : public juce::AudioProcessor,
                     public juce::AudioProcessorValueTreeState::Listener,
                     public juce::MidiKeyboardState::Listener
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

    // --- Real-time spectral data for UI ---
    std::array<std::atomic<float>, 64> spectralDataForUI;

protected:
    // --- MidiKeyboardState::Listener overrides ---
    void handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

private:
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<NEURONiK::Serialization::PresetManager> presetManager;
    juce::MidiKeyboardState keyboardState;
    juce::Synthesiser synth;

    // --- UI MIDI Message Injection ---
    juce::MidiBuffer uiMidiBuffer;
    juce::CriticalSection midiBufferLock;

    // --- Voice/Modulation Parameters ---
    NEURONiK::DSP::Synthesis::ResonatorVoice::VoiceParams voiceParams;
    std::atomic<bool> parametersNeedUpdating { true };
    std::atomic<bool> modulationNeedsUpdating { true };

    std::array<juce::String, 4> modelNames;

    // --- LFOs ---
    NEURONiK::DSP::Core::LFO lfo1, lfo2;

    // --- MIDI Real-time values for Modulation ---
    std::atomic<float> pitchBendValue { 0.5f };
    std::atomic<float> modWheelValue { 0.0f };

    // --- MIDI Learn ---
    std::atomic<bool> midiLearnActive { false };
    juce::String parameterToLearn;
    std::map<int, juce::String> midiCCMap;

    // --- Modulation Matrix ---
    struct ModulationRoute { int source=0, destination=0; float amount=0.0f; };
    std::array<ModulationRoute, 4> modulationMatrix;
    std::vector<juce::String> modulatableParameters;

    int getParameterIndex(const juce::String& paramID) const;

    // --- Setup Methods ---
    void setupSynth();
    void setupModulatableParameters();
    void updateVoiceParameters(int numSamples);
    void updateModulation();

public:
    void setPolyphony(int numVoices);
    int getPolyphony() const { return currentPolyphony; }

private:
    juce::CriticalSection processLock;
    int currentPolyphony = 8;

    // --- Global FX ---
    NEURONiK::DSP::Effects::Saturation saturationProcessor;
    NEURONiK::DSP::Effects::Delay delayProcessor;
    juce::LinearSmoothedValue<float> masterLevelSmoother;

    struct EditorSettings {
        std::unique_ptr<juce::FileChooser> chooser;
    } editorSettings;

public:
    EditorSettings& getEditorSettings() { return editorSettings; }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NEURONiKProcessor)
};
