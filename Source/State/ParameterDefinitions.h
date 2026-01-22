#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

namespace Nexus::State {

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "master_level", "Master Level", 
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f
    ));
    
    // Osc 1
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "osc1_level", "Osc1 Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f
    ));
    
    // Envelope
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "env_attack", "Attack",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.01f
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "env_decay", "Decay",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.1f
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "env_sustain", "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "env_release", "Release",
        juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.5f), 0.5f
    ));

    // Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filter_cutoff", "Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.3f), 20000.0f
    ));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "filter_res", "Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f
    ));

    // Resonator
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "resonator_rolloff", "Harmonic Roll-off",
        juce::NormalisableRange<float>(0.1f, 4.0f, 0.0f, 0.5f), 1.0f
    ));

    return { params.begin(), params.end() };
}

} // namespace Nexus::State
