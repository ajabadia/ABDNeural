#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../DSP/Synthesis/ResonatorVoice.h"
#include "../DSP/Effects/Saturation.h"
#include "../DSP/Effects/Delay.h"

class NEXUSProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    NEXUSProcessor();
    ~NEXUSProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

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

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    juce::Synthesiser synth;

    // Cache for voice parameters
    Nexus::DSP::Synthesis::ResonatorVoice::VoiceParams voiceParams;
    std::atomic<bool> parametersNeedUpdating { true };

    void setupSynth();
    void updateVoiceParameters();

    // --- Global FX ---
    Nexus::DSP::Effects::Saturation saturationProcessor;
    Nexus::DSP::Effects::Delay delayProcessor;
    
    // Smoothing for global parameters
    juce::LinearSmoothedValue<float> masterLevelSmoother;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NEXUSProcessor)
};
