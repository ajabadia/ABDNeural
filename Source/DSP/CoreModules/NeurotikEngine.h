/*
  ==============================================================================

    NeurotikEngine.h
    Created: 30 Jan 2026
    Description: Engine implementation for the Neurotik synthesizer.

  ==============================================================================
*/

#pragma once

#include "../BaseEngine.h"
#include "../Synthesis/NeurotikVoice.h"
#include "../../Common/SpectralModel.h"
#include <array>

namespace NEURONiK::DSP {

class NeurotikEngine : public BaseEngine
{
public:
    NeurotikEngine();
    ~NeurotikEngine() override = default;

    // --- ISynthesisEngine Implementation ---
    Type getType() const override { return Type::Neurotik; }
    void prepare(double sampleRate, int samplesPerBlock) override;
    void renderNextBlock(dsp::AudioBuffer<float>& buffer, dsp::MidiBuffer& midiMessages) override;
    void updateParameters() override;
    void getSpectralData(float* destination64) const override;
    void getEnvelopeLevels(float& amp, float& filter) const override;
    void getModulationValues(float* destination, int count) const override;

    // --- Specific API ---
    void setVoiceParams(const ::NEURONiK::DSP::Synthesis::NeurotikVoice::Params& p);

    /** MORPH del pad XY (2026-09-26): read-modify-write de
        pendingVoiceParams (el mismo canal que setVoiceParams). */
    void setMorph (float morphX, float morphY) override;
    void setMorphZ (float morphZ) override;
    void setVoiceLayerMorph (float layerGain2, float layerGain3) override;

    /** ADSR de la voz neurotik (ms; sustain en 0..1). Solo los cuatro tramos de
        la ENV de AMP: este motor no tiene envolvente de filtro
        (`getFilterEnvelopeLevel` es 0 fijo), asi que los otros cuatro del
        canal se ignoran — el puente WASM los conserva en su espejo y los
        reenvia cuando se vuelve a NEURONiK. */
    void setVoiceEnvelope (float attack, float decay, float sustain, float release,
                           float fAttack, float fDecay, float fSustain, float fRelease) override;
    void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) override;

    void setGlobalParams(const GlobalParams& p) override { pendingGlobalParams = p; }

private:
    void handleMidiEvent(const dsp::MidiMessage& m) override;
    void applyModulation() override;

    std::unique_ptr<IVoice> createVoice (int index) override;

    ::NEURONiK::DSP::Synthesis::NeurotikVoice::Params pendingVoiceParams;
    std::array<float, 64> lastModulations { 0.0f };

    dspDeclareNonCopyableWithLeakDetector(NeurotikEngine)
};

} // namespace NEURONiK::DSP
