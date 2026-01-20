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
        "osc1_pitch", "Osc1 Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f
    ));

    return { params.begin(), params.end() };
}

} // namespace Nexus::State
