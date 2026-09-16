/*
  ==============================================================================

    BaseEngine.cpp
    Created: 30 Jan 2026

  ==============================================================================
*/

#include "BaseEngine.h"

#include "CoreModules/RhythmicDivision.h"

namespace NEURONiK::DSP {

BaseEngine::BaseEngine()
{
}

void BaseEngine::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentSamplesPerBlock = samplesPerBlock;

    saturation.prepare(sampleRate);
    delay.prepare(sampleRate, static_cast<int>(sampleRate * 2.0)); // 2s max
    chorus.prepare(sampleRate);
    reverb.prepare(sampleRate);
    
    lfo1.setSampleRate(sampleRate);
    lfo2.setSampleRate(sampleRate);
    
    masterLevelSmoother.reset(sampleRate, 0.05);
    
    for (auto& voice : voices)
    {
        if (voice) voice->prepare(sampleRate, samplesPerBlock);
    }
}

void BaseEngine::updateParameters()
{
    currentGlobalParams = pendingGlobalParams;
    
    saturation.setDrive(currentGlobalParams.saturationAmt);
    delay.setParameters(currentGlobalParams.delayTime, currentGlobalParams.delayFB);
    chorus.setParameters(currentGlobalParams.chorusRate,
                         currentGlobalParams.chorusDepth,
                         currentGlobalParams.chorusMix);
    reverb.setParameters(currentGlobalParams.reverbSize,
                         currentGlobalParams.reverbDamping,
                         currentGlobalParams.reverbWidth,
                         currentGlobalParams.reverbMix);
    
    masterLevelSmoother.setTargetValue(currentGlobalParams.masterLevel);

    // Tempo sync: the fields existed in GlobalParams but were never applied, so the
    // LFOs always ran free and the SYNC controls did nothing.
    const double bpm = currentGlobalParams.bpm;

    auto applyLfo = [bpm] (Core::LFO& lfo, const GlobalParams::LFOParams& p)
    {
        lfo.setWaveform (static_cast<Core::LFO::Waveform> (juce::jlimit (0, 5, p.waveform)));
        lfo.setRate (p.rateHz);
        lfo.setDepth (p.depth);
        lfo.setSyncMode (p.syncMode == 0 ? Core::LFO::SyncMode::Free
                                         : Core::LFO::SyncMode::TempoSync);
        lfo.setTempoBPM (bpm);
        lfo.setRhythmicDivision (Core::quarterNotesForDivision (p.rhythmicDivision));
    };

    applyLfo (lfo1, currentGlobalParams.lfo1);
    applyLfo (lfo2, currentGlobalParams.lfo2);
    
    for (auto& voice : voices)
    {
        if (voice) voice->updateParameters();
    }
}

void BaseEngine::reset()
{
    saturation.resetState();
    delay.reset();
    chorus.reset();
    reverb.reset();
    
    lfo1.reset();
    lfo2.reset();
    
    for (auto& voice : voices)
    {
        if (voice) voice->reset();
    }
}

void BaseEngine::handleMidiMessage(const juce::MidiMessage& msg)
{
    handleMidiEvent(msg);
}

float BaseEngine::getLfoValue(int index) const
{
    return (index == 0) ? lfo1Value.load() : lfo2Value.load();
}

void BaseEngine::getModulationValues(float* destination, int count) const
{
    if (destination == nullptr || count <= 0) return;
    
    // Default modulation values if not overridden
    for (int i = 0; i < count; ++i) destination[i] = 0.0f;
}

int BaseEngine::getNumActiveVoices() const
{
    int active = 0;
    for (auto& voice : voices)
        if (voice && voice->isActive()) active++;
    return active;
}

void BaseEngine::setPolyphony(int numVoices)
{
    activeVoiceLimit.store(juce::jlimit(1, 32, numVoices));
}

void BaseEngine::allNotesOff()
{
    // Release rather than reset: keeps the envelope tail, so no click.
    for (auto& voice : voices)
        if (voice != nullptr && voice->isActive())
            voice->noteOff (0.0f, true);
}

void BaseEngine::processMidiBuffer(juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        handleMidiEvent(metadata.getMessage());
    }
}

void BaseEngine::applyGlobalFX(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();

    // 1. Process LFOs
    lfo1Value.store(lfo1.processBlock(numSamples));
    lfo2Value.store(lfo2.processBlock(numSamples));

    // 2. Global Effects
    saturation.processBlock(buffer);
    chorus.processBlock(buffer);
    delay.processBlock(buffer);
    reverb.processBlock(buffer);

    // 3. Output Level
    masterLevelSmoother.applyGain(buffer, numSamples);
}

} // namespace NEURONiK::DSP
