/*
  ==============================================================================

    BaseEngine.h
    Created: 30 Jan 2026
    Description: Base class for synthesis engines, providing shared FX and LFO logic.

  ==============================================================================
*/

#pragma once

#include "DspCore.h"

#include "ISynthesisEngine.h"
#include "IVoice.h"
#include "Effects/Saturation.h"
#include "Effects/Delay.h"
#include "Effects/Chorus.h"
#include "Effects/Reverb.h"
#include "CoreModules/LFO.h"
// juce_core solo por JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (detector
// de fugas de Debug). La frontera MIDI del motor ya es dsp::Midi*.
#include <juce_core/juce_core.h>
#include <vector>
#include <memory>
#include <atomic>

namespace NEURONiK::DSP {

class BaseEngine : public ISynthesisEngine
{
public:
    BaseEngine();
    virtual ~BaseEngine() override = default;

    // --- ISynthesisEngine Shared Implementation ---
    void prepare(double sampleRate, int samplesPerBlock) override;
    void updateParameters() override;
    void reset() override;
    void handleMidiMessage(const dsp::MidiMessage& msg) override;
    
    float getLfoValue(int index) const override;
    void getModulationValues(float* destination, int count) const override;
    
    // Voices management
    int getNumActiveVoices() const override;
    void setPolyphony(int numVoices) override;
    void allNotesOff() override;

protected:
    /** Subclasses must call this at the end of their renderNextBlock. */
    void applyGlobalFX(dsp::AudioBuffer<float>& buffer);
    
    /** Subclasses must implement this to route MIDI to their specific voice types. */
    virtual void handleMidiEvent(const dsp::MidiMessage& m) = 0;
    
    /** Common MIDI processing loop. */
    void processMidiBuffer(dsp::MidiBuffer& midiMessages);

    std::vector<std::unique_ptr<IVoice>> voices;
    std::atomic<int> activeVoiceLimit { 16 };

    // Shared FX
    Effects::Saturation saturation;
    Effects::Delay delay;
    Effects::Chorus chorus;
    Effects::Reverb reverb;
    dsp::LinearSmoothedValue<float> masterLevelSmoother;

    // Shared LFOs (semillas distintas: S&H decorrelacionado entre lfo1/lfo2)
    Core::LFO lfo1 { 0x1D0F1u }, lfo2 { 0x1D0F2u };
    std::atomic<float> lfo1Value { 0.0f };
    std::atomic<float> lfo2Value { 0.0f };

    GlobalParams pendingGlobalParams;
    GlobalParams currentGlobalParams;

    double currentSampleRate = 48000.0;
    int currentSamplesPerBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BaseEngine)
};

} // namespace NEURONiK::DSP
