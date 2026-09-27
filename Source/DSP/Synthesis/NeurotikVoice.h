/*
  ==============================================================================

    NeurotikVoice.h
    Created: 30 Jan 2026
    Description: Resonator bank based voice (Neurotik Engine).

  ==============================================================================
*/

#pragma once

#include "DspCore.h"

#include "../IVoice.h"
#include "../CoreModules/ResonatorBank.h"
#include "../CoreModules/Envelope.h"
#include <array>

namespace NEURONiK::DSP::Synthesis {

class NeurotikVoice : public IVoice
{
public:
    // voiceIndex evita semillas identicas entre voces: con la misma semilla,
    // el ruido de excitacion seria identico en unison (artefacto audible).
    explicit NeurotikVoice (int voiceIndex = 0);
    ~NeurotikVoice() override = default;

    struct Params
    {
        float level = 1.0f;
        float attack = 10.0f, decay = 100.0f, sustain = 0.7f, release = 500.0f;
        float resonatorResonance = 0.99f;
        float morphX = 0.5f, morphY = 0.5f;
        float morphZ = 0.0f; // FASE 10: frame canonico por defecto
        // FASE 11.3: el eje temporal de las CAPAS 1 y 2 (solo suenan si el modelo
        // cargado las tiene). Default 0.0 = frame canonico: el legado no se mueve.
        float morphZ2 = 0.0f;   // FASE 11.3: z de la capa 1
        float morphZ3 = 0.0f;   // FASE 11.3: z de la capa 2
        // FASE 11.4: el VOLUMEN de las capas 1 y 2 (0 = callada). Default 1.0: el legado.
        float layerGain2 = 1.0f;
        float layerGain3 = 1.0f;
        float excitationNoise = 1.0f;
        float excitationColor = 0.5f; // 0.0 (Brown) to 1.0 (Violet?)
        float impulseMix = 0.0f;     // Mix between noise and impulse
        float bowExcite = 0.0f;      // Bow (arco continuo): excitation sostenida
                                     // con ganancia plena. 0 = legacy (impulso+ruido)
        float unisonDetune = 0.01f;
        float unisonSpread = 0.5f;
    };

    // --- IVoice Implementation ---
    VoiceType getType() const noexcept override { return VoiceType::Neurotik; }
    void prepare(double sampleRate, int samplesPerBlock) override;
    void noteOn(int midiNoteNumber, float velocity) override;
    void noteOff(float velocity, bool allowTail) override;
    bool renderNextBlock(dsp::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
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

    dsp::Random random;
    
    // Excitation state
    float lastNoiseSample = 0.0f;
    float impulseTrigger = 0.0f;
    // Bow (arco continuo): oscilador de banda de friccion
    float bowPhase = 0.0f;
    double voiceSampleRate = 44100.0;

    // MPE State
    float mpePitchBend = 0.0f;
    float mpePressure = 0.0f;
    float mpeTimbre = 0.0f;

    // Smoothers
    dsp::LinearSmoothedValue<float> morphXSmoother;
    dsp::LinearSmoothedValue<float> morphYSmoother;
    dsp::LinearSmoothedValue<float> morphZSmoother;
    // FASE 11.3: los z de las capas 1 y 2, con el mismo glide que morphZ.
    dsp::LinearSmoothedValue<float> morphZ2Smoother;
    dsp::LinearSmoothedValue<float> morphZ3Smoother;
    // FASE 11.4: el volumen de las capas 1 y 2, con el mismo glide (20 ms).
    dsp::LinearSmoothedValue<float> layerGain2Smoother;
    dsp::LinearSmoothedValue<float> layerGain3Smoother;
    dsp::LinearSmoothedValue<float> resonanceSmoother;
    dsp::LinearSmoothedValue<float> unisonDetuneSmoother;

    dspDeclareNonCopyableWithLeakDetector(NeurotikVoice)
};

} // namespace NEURONiK::DSP::Synthesis
