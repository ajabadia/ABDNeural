/*
  ==============================================================================

    AdditiveVoice.h
    Created: 29 Jan 2026
    Description: Stand-alone polyphonic additive voice.

  ==============================================================================
*/

#pragma once

#include "../IVoice.h"
#include "../CoreModules/Resonator.h"
#include "../CoreModules/Envelope.h"
#include "../CoreModules/FilterBank.h"

namespace NEURONiK::DSP::Synthesis {

class AdditiveVoice : public IVoice
{
public:
    AdditiveVoice();
    ~AdditiveVoice() override = default;

    struct Params
    {
        float oscLevel = 1.0f;
        float attack = 10.0f, decay = 100.0f, sustain = 0.7f, release = 500.0f;
        float filterCutoff = 20000.0f, filterRes = 0.1f;
        float fEnvAmount = 0.0f;
        float fAttack = 10.0f, fDecay = 100.0f, fSustain = 0.7f, fRelease = 500.0f;
        float resonatorRollOff = 1.0f;
        float resonatorParity = 0.5f;
        float resonatorShift = 1.0f;
        float morphX = 0.5f;
        float morphY = 0.5f;
        float inharmonicity = 0.0f;
        float roughness = 0.0f;
        float unisonDetune = 0.01f;
        float unisonSpread = 0.5f;
        int velocityCurve = 0;
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

    // --- Specific API ---
    void setParams(const Params& p) { pendingParams = p; }
    const NEURONiK::DSP::Core::Resonator& getResonator() const { return resonator; }
    void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) { resonator.loadModel(model, slot); }
    
    // For visualization
    float getAmpEnvelopeLevel() const { return ampEnvelope.getLastOutput(); }
    float getFilterEnvelopeLevel() const { return filterEnvelope.getLastOutput(); }

private:
    NEURONiK::DSP::Core::Resonator resonator;
    NEURONiK::DSP::Core::Envelope ampEnvelope;
    NEURONiK::DSP::Core::Envelope filterEnvelope;
    NEURONiK::DSP::Core::FilterBank filter;

    Params currentParams;
    Params pendingParams;

    int currentNote = -1;
    int midiChannel = 1;
    float currentVelocity = 0.0f;
    float originalFrequency = 440.0f;

    // Smoothers
    juce::LinearSmoothedValue<float> cutoffSmoother;
    juce::LinearSmoothedValue<float> resSmoother;
    juce::LinearSmoothedValue<float> morphXSmoother;
    juce::LinearSmoothedValue<float> morphYSmoother;
    juce::LinearSmoothedValue<float> inharmonicitySmoother;
    juce::LinearSmoothedValue<float> roughnessSmoother;
    juce::LinearSmoothedValue<float> paritySmoother;
    juce::LinearSmoothedValue<float> shiftSmoother;
    juce::LinearSmoothedValue<float> rollOffSmoother;
    juce::LinearSmoothedValue<float> unisonDetuneSmoother;
    juce::LinearSmoothedValue<float> unisonSpreadSmoother;

    // MPE State
    float mpePitchBend = 0.0f;  // semitones
    float mpePressure = 0.0f;   // 0..1
    float mpeTimbre = 0.0f;     // 0..1
};

} // namespace NEURONiK::DSP::Synthesis
