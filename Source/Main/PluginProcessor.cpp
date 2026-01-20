#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../State/ParameterDefinitions.h"

PluginProcessor::PluginProcessor()
    : apvts(*this, nullptr, "Parameters", Nexus::State::createParameterLayout())
{
}

void PluginProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void PluginProcessor::releaseResources()
{
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    buffer.clear();
}

void PluginProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> floatBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    processBlock(floatBuffer, midi);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

bool PluginProcessor::hasEditor() const
{
    return true;
}

const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& /*destData*/)
{
}

void PluginProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
