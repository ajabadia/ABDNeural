#include "NEXUSProcessor.h"
#include "NEXUSEditor.h"
#include "../State/ParameterDefinitions.h"
#include "../DSP/Synthesis/ResonatorVoice.h"
#include "../DSP/Synthesis/ResonatorSound.h"

using namespace Nexus::State;

NEXUSProcessor::NEXUSProcessor()
    : apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    for (auto& param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            apvts.addParameterListener(p->getParameterID(), this);
        }
    }

    masterLevelSmoother.reset(getSampleRate(), 0.05);

    // Initial parameter load
    parameterChanged(IDs::morphX, apvts.getRawParameterValue(IDs::morphX)->load());
    parameterChanged(IDs::morphY, apvts.getRawParameterValue(IDs::morphY)->load());
    parameterChanged(IDs::oscLevel, apvts.getRawParameterValue(IDs::oscLevel)->load());
    parameterChanged(IDs::envAttack, apvts.getRawParameterValue(IDs::envAttack)->load());
    parameterChanged(IDs::envDecay, apvts.getRawParameterValue(IDs::envDecay)->load());
    parameterChanged(IDs::envSustain, apvts.getRawParameterValue(IDs::envSustain)->load());
    parameterChanged(IDs::envRelease, apvts.getRawParameterValue(IDs::envRelease)->load());
    parameterChanged(IDs::filterCutoff, apvts.getRawParameterValue(IDs::filterCutoff)->load());
    parameterChanged(IDs::filterRes, apvts.getRawParameterValue(IDs::filterRes)->load());
    parameterChanged(IDs::resonatorRolloff, apvts.getRawParameterValue(IDs::resonatorRolloff)->load());
    parameterChanged(IDs::resonatorParity, apvts.getRawParameterValue(IDs::resonatorParity)->load());
    parameterChanged(IDs::resonatorShift, apvts.getRawParameterValue(IDs::resonatorShift)->load());
    parameterChanged(IDs::oscInharmonicity, apvts.getRawParameterValue(IDs::oscInharmonicity)->load());
    parameterChanged(IDs::oscRoughness, apvts.getRawParameterValue(IDs::oscRoughness)->load());
    parameterChanged(IDs::fxSaturation, apvts.getRawParameterValue(IDs::fxSaturation)->load());
    parameterChanged(IDs::fxDelayTime, apvts.getRawParameterValue(IDs::fxDelayTime)->load());
    parameterChanged(IDs::fxDelayFeedback, apvts.getRawParameterValue(IDs::fxDelayFeedback)->load());

    masterLevelSmoother.setCurrentAndTargetValue(apvts.getRawParameterValue(IDs::masterLevel)->load());
}

NEXUSProcessor::~NEXUSProcessor()
{
    for (auto& param : getParameters())
    {
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            apvts.removeParameterListener(p->getParameterID(), this);
        }
    }
}

void NEXUSProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    synth.setCurrentPlaybackSampleRate(sampleRate);
    setupSynth();

    masterLevelSmoother.reset(sampleRate, 0.05);
    delayProcessor.prepare(sampleRate, static_cast<int>(sampleRate * 2.0));
}

void NEXUSProcessor::setupSynth()
{
    synth.clearVoices();
    for (int i = 0; i < 16; ++i)
        synth.addVoice(new Nexus::DSP::Synthesis::ResonatorVoice());

    synth.clearSounds();
    synth.addSound(new Nexus::DSP::Synthesis::ResonatorSound());
}

void NEXUSProcessor::releaseResources() {}

void NEXUSProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == IDs::morphX) voiceParams.morphX = newValue;
    else if (parameterID == IDs::morphY) voiceParams.morphY = newValue;
    else if (parameterID == IDs::oscLevel) voiceParams.oscLevel = newValue;
    else if (parameterID == IDs::envAttack) voiceParams.attack = newValue;
    else if (parameterID == IDs::envDecay) voiceParams.decay = newValue;
    else if (parameterID == IDs::envSustain) voiceParams.sustain = newValue;
    else if (parameterID == IDs::envRelease) voiceParams.release = newValue;
    else if (parameterID == IDs::filterCutoff) voiceParams.filterCutoff = newValue;
    else if (parameterID == IDs::filterRes) voiceParams.filterRes = newValue;
    else if (parameterID == IDs::resonatorRolloff) voiceParams.resonatorRollOff = newValue;
    else if (parameterID == IDs::resonatorParity) voiceParams.resonatorParity = newValue;
    else if (parameterID == IDs::resonatorShift) voiceParams.resonatorShift = newValue;
    else if (parameterID == IDs::oscInharmonicity) voiceParams.inharmonicity = newValue;
    else if (parameterID == IDs::oscRoughness) voiceParams.roughness = newValue;
    else if (parameterID == IDs::fxSaturation) saturationProcessor.setAmount(newValue);
    else if (parameterID == IDs::fxDelayTime) delayProcessor.setParameters(newValue, apvts.getRawParameterValue(IDs::fxDelayFeedback)->load());
    else if (parameterID == IDs::fxDelayFeedback) delayProcessor.setParameters(apvts.getRawParameterValue(IDs::fxDelayTime)->load(), newValue);
    else if (parameterID == IDs::masterLevel) masterLevelSmoother.setTargetValue(newValue);

    parametersNeedUpdating = true;
}

void NEXUSProcessor::updateVoiceParameters()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<Nexus::DSP::Synthesis::ResonatorVoice*>(synth.getVoice(i)))
            voice->updateParameters(voiceParams);

    parametersNeedUpdating = false;
}

void NEXUSProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (parametersNeedUpdating.load())
        updateVoiceParameters();

    buffer.clear();
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());

    // FX Chain
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            data[s] = saturationProcessor.processSample(data[s]);
    }
    delayProcessor.processBlock(buffer);

    if (!masterLevelSmoother.isSmoothing())
        buffer.applyGain(masterLevelSmoother.getCurrentValue());
    else
        masterLevelSmoother.applyGain(buffer, buffer.getNumSamples());
}

void NEXUSProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> floatBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    processBlock(floatBuffer, midi);

    // Manual conversion from float to double buffer
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* src = floatBuffer.getReadPointer(channel);
        double* dst = buffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            dst[sample] = static_cast<double>(src[sample]);
        }
    }
}

juce::AudioProcessorEditor* NEXUSProcessor::createEditor() { return new NEXUSEditor(*this); }
bool NEXUSProcessor::hasEditor() const { return true; }
const juce::String NEXUSProcessor::getName() const { return JucePlugin_Name; }

void NEXUSProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void NEXUSProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr && xmlState->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        parametersNeedUpdating = true;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NEXUSProcessor(); }
