/*
  ==============================================================================

    BaseEngine.cpp
    Created: 30 Jan 2026

  ==============================================================================
*/

#include "BaseEngine.h"

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
    chorus.setMix(currentGlobalParams.chorusMix);
    reverb.setMix(currentGlobalParams.reverbMix);
    
    masterLevelSmoother.setTargetValue(currentGlobalParams.masterLevel);
    
    lfo1.setWaveform(static_cast<Core::LFO::Waveform>(currentGlobalParams.lfo1.waveform));
    lfo1.setRate(currentGlobalParams.lfo1.rateHz);
    lfo1.setDepth(currentGlobalParams.lfo1.depth);

    lfo2.setWaveform(static_cast<Core::LFO::Waveform>(currentGlobalParams.lfo2.waveform));
    lfo2.setRate(currentGlobalParams.lfo2.rateHz);
    lfo2.setDepth(currentGlobalParams.lfo2.depth);
    
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
