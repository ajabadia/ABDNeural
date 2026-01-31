/*
  ==============================================================================

    NeurotikVoice.h
    Created: 30 Jan 2026
    Description: Resonator bank based voice (Neurotik Engine).

  ==============================================================================
*/

#pragma once

#include "../IVoice.h"
#include "../CoreModules/ResonatorBank.h"
#include "../CoreModules/Envelope.h"
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace NEURONiK::DSP::Synthesis {

class NeurotikVoice : public IVoice
{
public:
    NeurotikVoice();
    ~NeurotikVoice() override = default;

    struct Params
    {
        float level = 1.0f;
        float attack = 10.0f, decay = 100.0f, sustain = 0.7f, release = 500.0f;
        float resonatorResonance = 0.99f;
        float morphX = 0.5f, morphY = 0.5f;
        float excitationNoise = 1.0f;
        float excitationColor = 0.5f; // 0.0 (Brown) to 1.0 (Violet?)
        float impulseMix = 0.0f;     // Mix between noise and impulse
        float unisonDetune = 0.01f;
        float unisonSpread = 0.5f;
    };

    // --- IVoice Implementation ---
    void prepare(double sampleRate, int samplesPerBlock) override;
    void noteOn(int midiNoteNumber, float velocity) override;
    void noteOff(float velocity, bool allowTail) override;
    bool renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    bool isActive() const override;
    int getCurrentlyPlayingNote() const override { return currentNote; }
    void updateParameters() override;
    void reset() override;

    void setChannel(int channel) override { midiChannel = channel; }
    int getChannel() const override { return midiChannel; }

    // --- MPE ---
    void notePitchBend(float bendSemitones) override;
    void notePressure(float pressure) override;
    void noteTimbre(float timbre) override;

    void setParams(const Params& p) { pendingParams = p; }
    // For visualization
    float getAmpEnvelopeLevel() const { return ampEnvelope.getLastOutput(); }
    float getFilterEnvelopeLevel() const { return 0.0f; }
    void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) { resonatorBank.loadModel(model, slot); }
    const std::array<float, 64>& getPartialAmplitudes() const { return resonatorBank.getPartialAmplitudes(); }

private:
    Core::ResonatorBank resonatorBank;
    Core::Envelope ampEnvelope;
    
    Params currentParams;
    Params pendingParams;

    int currentNote = -1;
    int midiChannel = 1;
    float currentVelocity = 0.0f;
    float baseFreq = 440.0f;

    juce::Random random;
    
    // Excitation state
    float lastNoiseSample = 0.0f;
    float impulseTrigger = 0.0f;

    // MPE State
    float mpePitchBend = 0.0f;
    float mpePressure = 0.0f;
    float mpeTimbre = 0.0f;

    // Smoothers
    juce::LinearSmoothedValue<float> morphXSmoother;
    juce::LinearSmoothedValue<float> morphYSmoother;
    juce::LinearSmoothedValue<float> resonanceSmoother;
    juce::LinearSmoothedValue<float> unisonDetuneSmoother;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeurotikVoice)
};

} // namespace NEURONiK::DSP::Synthesis
