/*
  ==============================================================================

    ResonatorVoice.h
    Created: 21 Jan 2026
    Description: Polyphonic voice implementation for NEURONiK.
                 Integrates Resonator, FilterBank, and Envelope modules.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../CoreModules/Resonator.h"
#include "../CoreModules/Envelope.h"
#include "../CoreModules/FilterBank.h"
#include "ResonatorSound.h"

// Forward declaration to break circular dependency
class NEURONiKProcessor;

namespace NEURONiK::DSP::Synthesis {

class ResonatorVoice : public juce::SynthesiserVoice
{
public:
    explicit ResonatorVoice(NEURONiKProcessor* p);
    ~ResonatorVoice() override = default;

    struct VoiceParams
    {
        float oscLevel = 1.0f;
        float attack = 0.01f, decay = 0.1f, sustain = 0.7f, release = 0.5f;
        float filterCutoff = 20000.0f, filterRes = 0.1f;
        float fEnvAmount = 0.0f;
        float fAttack = 0.01f, fDecay = 0.1f, fSustain = 0.7f, fRelease = 0.5f;

        // Legacy parameters (to be phased out)
        float resonatorRollOff = 1.0f;
        float resonatorParity = 0.5f;
        float resonatorShift = 1.0f;

        // Neural Engine parameters
        float morphX = 0.5f;
        float morphY = 0.5f;
        float inharmonicity = 0.0f;
        float roughness = 0.0f;
        int velocityCurve = 0; // 0: Linear, 1: Soft, 2: Hard
        float masterAftertouch = 0.0f;
    };

    void loadModel(const NEURONiK::DSP::Core::SpectralModel& model, int slot);

    // --- juce::SynthesiserVoice Overrides ---

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void setCurrentPlaybackSampleRate(double newRate) override;
    
    // --- Parameter Updates ---
    void updateParameters(const VoiceParams& params);
    float getCurrentEnvelopeLevel() const { return ampEnvelope.getLastOutput(); }
    float getCurrentFilterEnvelopeLevel() const { return filterEnvelope.getLastOutput(); }

    // --- Real-time Processing ---
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        int startSample, int numSamples) override;

private:
    // --- DSP Chain ---
    NEURONiK::DSP::Core::Resonator resonator;
    NEURONiK::DSP::Core::Envelope ampEnvelope;
    NEURONiK::DSP::Core::Envelope filterEnvelope;
    NEURONiK::DSP::Core::FilterBank filter;

    float currentVelocity = 0.0f;
    VoiceParams currentParams; // Cached parameters

    float originalFrequency = 440.0f;
    float pitchBendRatio = 1.0f;

    NEURONiKProcessor* ownerProcessor; // Non-owning pointer to the main processor

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResonatorVoice)
};

} // namespace NEURONiK::DSP::Synthesis
