/*
  ==============================================================================

    ParameterDefinitions.h
    Created: 22 Jan 2026
    Description: Centralized parameter definitions for NEURONiK.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

namespace NEURONiK::State {

namespace IDs {
    // Oscillator / Neural Core
    const juce::String oscLevel         = "oscLevel";
    const juce::String oscPitchCoarse   = "oscPitchCoarse";
    const juce::String oscInharmonicity = "oscInharmonicity";
    const juce::String oscRoughness     = "oscRoughness";
    const juce::String harmMix          = "harmMix";
    const juce::String morphX           = "morphX";
    const juce::String morphY           = "morphY";

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
    const juce::String masterBPM   = "masterBPM";

    // Global / MIDI
    const juce::String midiThru = "midiThru";
    const juce::String midiChannel = "midiChannel";

    // LFO 1
    const juce::String lfo1Waveform = "lfo1Waveform";
    const juce::String lfo1RateHz = "lfo1RateHz";
    const juce::String lfo1SyncMode = "lfo1SyncMode";
    const juce::String lfo1RhythmicDivision = "lfo1RhythmicDivision";
    const juce::String lfo1Depth = "lfo1Depth";

    // LFO 2
    const juce::String lfo2Waveform = "lfo2Waveform";
    const juce::String lfo2RateHz = "lfo2RateHz";
    const juce::String lfo2SyncMode = "lfo2SyncMode";
    const juce::String lfo2RhythmicDivision = "lfo2RhythmicDivision";
    const juce::String lfo2Depth = "lfo2Depth";

    // Modulation Matrix
    const juce::String mod1Source = "mod1Source";
    const juce::String mod1Destination = "mod1Destination";
    const juce::String mod1Amount = "mod1Amount";
    const juce::String mod2Source = "mod2Source";
    const juce::String mod2Destination = "mod2Destination";
    const juce::String mod2Amount = "mod2Amount";
    const juce::String mod3Source = "mod3Source";
    const juce::String mod3Destination = "mod3Destination";
    const juce::String mod3Amount = "mod3Amount";
    const juce::String mod4Source = "mod4Source";
    const juce::String mod4Destination = "mod4Destination";
    const juce::String mod4Amount = "mod4Amount";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::masterLevel, "Master Level", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::masterBPM, "Master BPM", juce::NormalisableRange<float>(20.0f, 400.0f), 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscLevel, "Osc Level", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscInharmonicity, "Inharmonicity", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscRoughness, "Roughness", juce::NormalisableRange<float>(0.0f, 0.5f), 0.0f)); // Range reduced
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::harmMix, "Harmonic Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphX, "Morph X", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphY, "Morph Y", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envAttack, "Attack", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envDecay, "Decay", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envSustain, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envRelease, "Release", juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.5f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterCutoff, "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.3f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterRes, "Resonance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorRolloff, "Harmonic Roll-off", juce::NormalisableRange<float>(0.1f, 4.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorParity, "Odd/Even Balance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorShift, "Spectral Shift", juce::NormalisableRange<float>(0.5f, 2.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxSaturation, "Saturation", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxDelayTime, "Delay Time", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.5f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxDelayFeedback, "Delay FB", juce::NormalisableRange<float>(0.0f, 0.95f), 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::midiThru, "MIDI Thru", false));

    juce::StringArray midiChannels = { "Omni", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::midiChannel, "MIDI Channel", midiChannels, 0));

    juce::StringArray lfoWaveforms = { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random S&H" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1Waveform, "LFO 1 Wave", lfoWaveforms, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo1RateHz, "LFO 1 Rate", juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.5f), 1.0f));
    juce::StringArray lfoSyncModes = { "Free", "Tempo Sync" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1SyncMode, "LFO 1 Sync", lfoSyncModes, 0));
    juce::StringArray rhythmicDivisions = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4t", "1/8t", "1/16t" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1RhythmicDivision, "LFO 1 Div", rhythmicDivisions, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo1Depth, "LFO 1 Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2Waveform, "LFO 2 Wave", lfoWaveforms, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo2RateHz, "LFO 2 Rate", juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2SyncMode, "LFO 2 Sync", lfoSyncModes, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2RhythmicDivision, "LFO 2 Div", rhythmicDivisions, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo2Depth, "LFO 2 Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    juce::StringArray modDestinations = { "Off", "Osc Level", "Inharmonicity", "Roughness", "Morph X", "Morph Y", "Attack", "Decay", "Sustain", "Release", "Filter Cutoff", "Filter Res", "Saturation", "Delay Time", "Delay FB" };
    juce::StringArray modSources = { "Off", "LFO 1", "LFO 2", "Pitch Bend", "Mod Wheel" };

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod1Source, "Mod 1 Source", modSources, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod1Destination, "Mod 1 Dest", modDestinations, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod1Amount, "Mod 1 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod2Source, "Mod 2 Source", modSources, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod2Destination, "Mod 2 Dest", modDestinations, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod2Amount, "Mod 2 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod3Source, "Mod 3 Source", modSources, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod3Destination, "Mod 3 Dest", modDestinations, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod3Amount, "Mod 3 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod4Source, "Mod 4 Source", modSources, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod4Destination, "Mod 4 Dest", modDestinations, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod4Amount, "Mod 4 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    return { params.begin(), params.end() };
}

} // namespace NEURONiK::State
