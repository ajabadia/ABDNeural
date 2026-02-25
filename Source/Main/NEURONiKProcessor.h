#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <map>
#include "../Serialization/PresetManager.h"
#include "MidiMappingManager.h"
#include "../DSP/ISynthesisEngine.h"
#include "../DSP/IVisualizationSource.h"
#include "../Common/SpectralModel.h"
#include "ModulationTargets.h"

namespace NEURONiK::Serialization { class PresetManager; }
namespace NEURONiK::Main { class MidiMappingManager; }

class NEURONiKProcessor : public juce::AudioProcessor,
                         public juce::AudioProcessorValueTreeState::Listener,
                         public juce::ValueTree::Listener,
                         public NEURONiK::DSP::IVisualizationSource
{
public:
    NEURONiKProcessor();
    ~NEURONiKProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    // --- Parameter Access ---
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    NEURONiK::Serialization::PresetManager& getPresetManager() { return *presetManager; }
    NEURONiK::Main::MidiMappingManager& getMidiMappingManager() { return *midiMappingManager; }

    void loadModel(const juce::File& file, int slot);
    void reloadModels();

    // --- Patch Copy/Paste (State Access Only) ---
    juce::ValueTree getFullState() { return apvts.copyState(); }
    void setFullState(const juce::ValueTree& newState) { apvts.replaceState(newState); }

    // --- Editor Creation ---
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // --- Persistence ---
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    const juce::String getName() const override;
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // --- APVTS Listener ---
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // --- MIDI Learn ---
    void enterMidiLearnMode(const juce::String& paramID);
    void clearMidiLearnForParameter(const juce::String& paramID);
    bool isMidiLearnActive() const { return midiLearnActive.load(); }
    juce::String getParameterToLearn() const { return parameterToLearn; }

    const std::array<juce::String, 4>& getModelNames() const { return modelNames; }

    // --- Thread-Safe Bridge for UI ---
    std::array<std::atomic<float>, 64> spectralDataForUI;
    std::atomic<float> uiEnvelope { 0.0f };
    std::atomic<float> uiFEnvelope { 0.0f };
    
    // Modulation visualization data
    std::array<std::atomic<float>, static_cast<size_t>(NEURONiK::ModulationTarget::Count)> modulationValues;

    // Visualization data Cache
    std::atomic<float> uiAttack { 0.0f };
    std::atomic<float> uiDecay { 0.0f };
    std::atomic<float> uiSustain { 0.0f };
    std::atomic<float> uiRelease { 0.0f };
    std::atomic<float> uiFAttack { 0.0f };
    std::atomic<float> uiFDecay { 0.0f };
    std::atomic<float> uiFSustain { 0.0f };
    std::atomic<float> uiFRelease { 0.0f };
    std::atomic<float> uiMorphX { 0.0f };
    std::atomic<float> uiMorphY { 0.0f };
    std::atomic<float> lfo1ValueForUI { 0.0f };
    std::atomic<float> lfo2ValueForUI { 0.0f };

    void setPolyphony(int numVoices);
    int getPolyphony() const;
    
    // --- IVisualizationSource Implementation ---
    void getSpectralDataForUI(float* destination64) const noexcept override;
    void getEnvelopeLevelsForUI(float& amp, float& filter) const noexcept override;
    float getLfoValueForUI(int lfoIndex) const noexcept override;
    float getModulationValueForUI(int targetIndex) const noexcept override;
    void getMorphCoordinatesForUI(float& x, float& y) const noexcept override;

public:
    // --- Modulation Access (Safe Atomic Indexing) ---
    std::atomic<float>& getModulationValue(NEURONiK::ModulationTarget target)
    {
        return modulationValues[static_cast<size_t>(target)];
    }

    // --- MIDI Injection (Used by Editor/Keyboard) ---
    void injectNoteOn(int midiChannel, int midiNoteNumber, float velocity);
    void injectNoteOff(int midiChannel, int midiNoteNumber, float velocity);

protected:
    // ValueTree::Listener
    void valueTreePropertyChanged(juce::ValueTree& tree, const juce::Identifier& property) override;
    void valueTreeRedirected(juce::ValueTree& tree) override;
    void valueTreeParentChanged(juce::ValueTree&) override {}
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override {}

private:
    void synchronizeEngineParameters();
    void processCommands();

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<NEURONiK::DSP::ISynthesisEngine> engine;
    std::unique_ptr<NEURONiK::Serialization::PresetManager> presetManager;
    std::unique_ptr<NEURONiK::Main::MidiMappingManager> midiMappingManager;

    std::atomic<int> currentPolyphony { 8 };
    std::atomic<bool> midiLearnActive { false };
    juce::String parameterToLearn;

    // MIDI Queue for Lock-Free injection
    struct QueuedMidi { juce::MidiMessage message; int sampleOffset; };
    std::array<QueuedMidi, 1024> midiQueue;
    juce::AbstractFifo midiFifo;

    // Command Queue for Engine (e.g. Model swaps)
    enum class EngineCommand { LoadModel };
    struct Command {
        EngineCommand type;
        int slot;
        NEURONiK::Common::SpectralModel modelData;
    };
    std::array<Command, 32> commandQueue;
    juce::AbstractFifo commandFifo;

    std::array<juce::String, 4> modelNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NEURONiKProcessor)
};
