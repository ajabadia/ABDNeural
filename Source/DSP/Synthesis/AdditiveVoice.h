/*
  ==============================================================================

    AdditiveVoice.h
    Created: 29 Jan 2026
    Description: Stand-alone polyphonic additive voice.

  ==============================================================================
*/

#pragma once

#include "DspCore.h"

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
        // filterEnvAmount se retiro (2026-09-26): la matriz es LA profundidad.
        // La ruta ENV 2 -> Filter Cutoff parte de factor 1.0 (modEnvCutoff) y el
        // destino "Filter Env Amt" (12) suma a ella (IVoice::modEnvFltDepth).
        float fAttack = 10.0f, fDecay = 100.0f, fSustain = 0.7f, fRelease = 500.0f;
        float resonatorRollOff = 1.0f;
        float resonatorParity = 0.5f;
        float resonatorShift = 1.0f;
        float morphX = 0.5f;
        float morphY = 0.5f;
        float morphZ = 0.0f; // FASE 10: frame canonico por defecto
        // FASE 11.3: el eje temporal de las CAPAS 1 y 2 (solo suenan si el modelo
        // cargado las tiene). Default 0.0 = frame canonico: el legado no se mueve.
        float morphZ2 = 0.0f;
        float morphZ3 = 0.0f;
        // FASE 11.4: el VOLUMEN de las capas 1 y 2 (0 = callada). Default 1.0:
        // el legado — las capas suenan enteras como en 11.3.
        float layerGain2 = 1.0f;
        float layerGain3 = 1.0f;
        float inharmonicity = 0.0f;
        float roughness = 0.0f;
        float unisonDetune = 0.01f;
        float unisonSpread = 0.5f;
        int velocityCurve = 0;
    };

    // --- IVoice Implementation ---
    VoiceType getType() const noexcept override { return VoiceType::Additive; }
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
    dsp::LinearSmoothedValue<float> cutoffSmoother;
    dsp::LinearSmoothedValue<float> resSmoother;
    dsp::LinearSmoothedValue<float> morphXSmoother;
    dsp::LinearSmoothedValue<float> morphYSmoother;
    dsp::LinearSmoothedValue<float> morphZSmoother;
    // FASE 11.3: los z de las capas 1 y 2, con el mismo glide que morphZ.
    dsp::LinearSmoothedValue<float> morphZ2Smoother;
    dsp::LinearSmoothedValue<float> morphZ3Smoother;
    // FASE 11.4: el volumen de las capas 1 y 2, con el mismo glide (20 ms).
    dsp::LinearSmoothedValue<float> layerGain2Smoother;
    dsp::LinearSmoothedValue<float> layerGain3Smoother;
    dsp::LinearSmoothedValue<float> inharmonicitySmoother;
    dsp::LinearSmoothedValue<float> roughnessSmoother;
    dsp::LinearSmoothedValue<float> paritySmoother;
    dsp::LinearSmoothedValue<float> shiftSmoother;
    dsp::LinearSmoothedValue<float> rollOffSmoother;
    dsp::LinearSmoothedValue<float> unisonDetuneSmoother;
    dsp::LinearSmoothedValue<float> unisonSpreadSmoother;

    // MPE State
    float mpePitchBend = 0.0f;  // semitones
    float mpePressure = 0.0f;   // 0..1
    float mpeTimbre = 0.0f;     // 0..1
};

} // namespace NEURONiK::DSP::Synthesis
