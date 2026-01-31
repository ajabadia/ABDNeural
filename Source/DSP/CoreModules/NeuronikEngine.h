/*
  ==============================================================================

    NeuronikEngine.h
    Created: 29 Jan 2026
    Description: Central synthesis engine.

  ==============================================================================
*/

#pragma once

#include "../BaseEngine.h"
#include "../Synthesis/AdditiveVoice.h"
#include "../../Common/SpectralModel.h"
#include <array>

namespace NEURONiK::DSP {

class NeuronikEngine : public BaseEngine
{
public:
    NeuronikEngine();
    ~NeuronikEngine() override = default;

    // --- ISynthesisEngine Implementation ---
    void prepare(double sampleRate, int samplesPerBlock) override;
    void renderNextBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    void updateParameters() override;
    void getSpectralData(float* destination64) const override;
    void getEnvelopeLevels(float& amp, float& filter) const override;
    void getModulationValues(float* destination, int count) const override;

    // --- Specific API ---
    void setVoiceParams(const ::NEURONiK::DSP::Synthesis::AdditiveVoice::Params& p);
    
    void setGlobalParams(const GlobalParams& p) override { pendingGlobalParams = p; }

    void loadModel(const NEURONiK::Common::SpectralModel& model, int slot) override;

private:
    void handleMidiEvent(const juce::MidiMessage& m) override;
    void applyModulation();

    ::NEURONiK::DSP::Synthesis::AdditiveVoice::Params pendingVoiceParams;
    std::array<float, 64> lastModulations { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NeuronikEngine)
};

} // namespace NEURONiK::DSP
