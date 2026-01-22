#include "NEXUSProcessor.h"
#include "NEXUSEditor.h"
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include "../State/ParameterDefinitions.h"
#include "../DSP/Synthesis/ResonatorVoice.h"
#include "../DSP/Synthesis/ResonatorSound.h"

NEXUSProcessor::NEXUSProcessor()
    : apvts(*this, nullptr, "Parameters", Nexus::State::createParameterLayout())
{
}

void NEXUSProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    // Initialize Synthesiser
    synth.setCurrentPlaybackSampleRate(sampleRate);
    
    // Add Voices
    setupSynth();
}

void NEXUSProcessor::setupSynth()
{
    synth.clearVoices();
    for (int i = 0; i < 16; ++i)
    {
        synth.addVoice(new Nexus::DSP::Synthesis::ResonatorVoice());
    }
    
    synth.clearSounds();
    synth.addSound(new Nexus::DSP::Synthesis::ResonatorSound());
}

void NEXUSProcessor::releaseResources()
{
}

void NEXUSProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    // auto totalNumInputChannels  = getTotalNumInputChannels();
    // auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear buffer to avoid loud noise or unexpected sound
    buffer.clear();

    // Handle on-screen keyboard events and external MIDI
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    // Update Voice Parameters
    Nexus::DSP::Synthesis::ResonatorVoice::VoiceParams params;
    params.oscLevel = apvts.getRawParameterValue("osc1_level")->load();
    params.attack = apvts.getRawParameterValue("env_attack")->load();
    params.decay = apvts.getRawParameterValue("env_decay")->load();
    params.sustain = apvts.getRawParameterValue("env_sustain")->load();
    params.release = apvts.getRawParameterValue("env_release")->load();
    params.filterCutoff = apvts.getRawParameterValue("filter_cutoff")->load();
    params.filterRes = apvts.getRawParameterValue("filter_res")->load();
    params.resonatorRollOff = apvts.getRawParameterValue("resonator_rolloff")->load();

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<Nexus::DSP::Synthesis::ResonatorVoice*>(synth.getVoice(i)))
        {
            voice->updateParameters(params);
        }
    }

    // Render Synthesis
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

void NEXUSProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> floatBuffer(buffer.getNumChannels(), buffer.getNumSamples());
    processBlock(floatBuffer, midi);
}

juce::AudioProcessorEditor* NEXUSProcessor::createEditor()
{
    return new NEXUSEditor(*this);
}

bool NEXUSProcessor::hasEditor() const
{
    return true;
}

const juce::String NEXUSProcessor::getName() const
{
    return JucePlugin_Name;
}

void NEXUSProcessor::getStateInformation(juce::MemoryBlock& /*destData*/)
{
}

void NEXUSProcessor::setStateInformation(const void* /*data*/, int /*sizeInBytes*/)
{
}
//==============================================================================
// This creates new instances of the plugin..
// Forced __cdecl by removing JUCE_CALLTYPE to match VS 2026 linker expectation
juce::AudioProcessor* createPluginFilter()
{
    return new NEXUSProcessor();
}
