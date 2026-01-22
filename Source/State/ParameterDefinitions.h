/*
  ==============================================================================

    ParameterDefinitions.h
    Created: 22 Jan 2026
    Description: Centralized parameter definitions for NEXUS.
                 Aligned with Neural Synthesis Vision (Inspired by Hartmann Neuron).

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

namespace Nexus::State {

namespace IDs {
    // Oscillator / Neural Core
    const juce::String oscLevel         = "oscLevel";
    const juce::String oscPitchCoarse   = "oscPitchCoarse";
    const juce::String oscInharmonicity = "oscInharmonicity";
    const juce::String oscRoughness     = "oscRoughness";
    const juce::String harmMix          = "harmMix";
    const juce::String morphX           = "morphX"; // New X-axis for 2D morphing
    const juce::String morphY           = "morphY"; // New Y-axis for 2D morphing

    // Resonator Advanced (Legacy)
    const juce::String resonatorRolloff = "resonatorRolloff";
    const juce::String resonatorParity  = "resonatorParity";
    const juce::String resonatorShift   = "resonatorShift";

    // Envelope
    const juce::String envAttack  = "envAttack";
    const juce::String envDecay   = "envDecay";
    const juce::String envSustain = "envSustain";
    const juce::String envRelease = "envRelease";

    // Filter
    const juce::String filterCutoff = "filterCutoff";
    const juce::String filterRes    = "filterRes";

    // FX
    const juce::String fxSaturation    = "fxSaturation";
    const juce::String fxDelayTime     = "fxDelayTime";
    const juce::String fxDelayFeedback = "fxDelayFeedback";

    // Master
    const juce::String masterLevel = "masterLevel";
}

/**
 * Creates the parameter layout for the APVTS.
 */
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Master
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::masterLevel, "Master Level", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));

    // Oscillator / Neural
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::oscLevel, "Osc Level", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::oscInharmonicity, "Inharmonicity", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::oscRoughness, "Roughness", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::harmMix, "Harmonic Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::morphX, "Morph X", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::morphY, "Morph Y", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Envelope
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::envAttack, "Attack", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::envDecay, "Decay", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::envSustain, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::envRelease, "Release", juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.5f), 0.5f));

    // Filter
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::filterCutoff, "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.3f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::filterRes, "Resonance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));

    // Resonator (Legacy)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::resonatorRolloff, "Harmonic Roll-off", juce::NormalisableRange<float>(0.1f, 4.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::resonatorParity, "Odd/Even Balance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::resonatorShift, "Spectral Shift", juce::NormalisableRange<float>(0.5f, 2.0f, 0.0f, 0.5f), 1.0f));

    // FX
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::fxSaturation, "Saturation", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::fxDelayTime, "Delay Time", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.5f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::fxDelayFeedback, "Delay FB", juce::NormalisableRange<float>(0.0f, 0.95f), 0.4f));

    return { params.begin(), params.end() };
}

} // namespace Nexus::State
