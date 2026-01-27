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
    static constexpr const char* oscLevel         = "oscLevel";
    static constexpr const char* oscPitchCoarse   = "oscPitchCoarse";
    static constexpr const char* oscInharmonicity = "oscInharmonicity";
    static constexpr const char* oscRoughness     = "oscRoughness";
    static constexpr const char* harmMix          = "harmMix";
    static constexpr const char* morphX           = "morphX";
    static constexpr const char* morphY           = "morphY";

    // Resonator Advanced (Legacy)
    static constexpr const char* resonatorRolloff = "resonatorRolloff";
    static constexpr const char* resonatorParity  = "resonatorParity";
    static constexpr const char* resonatorShift   = "resonatorShift";

    // Envelope
    static constexpr const char* envAttack  = "envAttack";
    static constexpr const char* envDecay   = "envDecay";
    static constexpr const char* envSustain = "envSustain";
    static constexpr const char* envRelease = "envRelease";

    // Filter
    static constexpr const char* filterCutoff    = "filterCutoff";
    static constexpr const char* filterRes       = "filterRes";
    static constexpr const char* filterEnvAmount = "filterEnvAmount";
    static constexpr const char* filterAttack    = "filterAttack";
    static constexpr const char* filterDecay     = "filterDecay";
    static constexpr const char* filterSustain   = "filterSustain";
    static constexpr const char* filterRelease   = "filterRelease";

    // FX
    static constexpr const char* fxSaturation    = "fxSaturation";
    static constexpr const char* fxDelayTime     = "fxDelayTime";
    static constexpr const char* fxDelayFeedback = "fxDelayFeedback";
    static constexpr const char* fxDelaySync     = "fxDelaySync";
    static constexpr const char* fxDelayDivision = "fxDelayDivision";
    
    // Chorus
    static constexpr const char* fxChorusRate  = "fxChorusRate";
    static constexpr const char* fxChorusDepth = "fxChorusDepth";
    static constexpr const char* fxChorusMix   = "fxChorusMix";

    // Reverb
    static constexpr const char* fxReverbSize    = "fxReverbSize";
    static constexpr const char* fxReverbDamping = "fxReverbDamping";
    static constexpr const char* fxReverbWidth   = "fxReverbWidth";
    static constexpr const char* fxReverbMix     = "fxReverbMix";

    // Master / Global
    static constexpr const char* masterLevel    = "masterLevel";
    static constexpr const char* masterBPM      = "masterBPM";
    static constexpr const char* midiThru       = "midiThru";
    static constexpr const char* midiChannel    = "midiChannel";
    static constexpr const char* randomStrength = "randomStrength";
    static constexpr const char* freezeResonator = "freezeResonator";
    static constexpr const char* freezeFilter    = "freezeFilter";
    static constexpr const char* freezeEnvelopes = "freezeEnvelopes";
    static constexpr const char* velocityCurve    = "velocityCurve";

    // LFO 1
    static constexpr const char* lfo1Waveform = "lfo1Waveform";
    static constexpr const char* lfo1RateHz = "lfo1RateHz";
    static constexpr const char* lfo1SyncMode = "lfo1SyncMode";
    static constexpr const char* lfo1RhythmicDivision = "lfo1RhythmicDivision";
    static constexpr const char* lfo1Depth = "lfo1Depth";

    // LFO 2
    static constexpr const char* lfo2Waveform = "lfo2Waveform";
    static constexpr const char* lfo2RateHz = "lfo2RateHz";
    static constexpr const char* lfo2SyncMode = "lfo2SyncMode";
    static constexpr const char* lfo2RhythmicDivision = "lfo2RhythmicDivision";
    static constexpr const char* lfo2Depth = "lfo2Depth";

    // Modulation Matrix
    static constexpr const char* mod1Source = "mod1Source";
    static constexpr const char* mod1Destination = "mod1Destination";
    static constexpr const char* mod1Amount = "mod1Amount";
    static constexpr const char* mod2Source = "mod2Source";
    static constexpr const char* mod2Destination = "mod2Destination";
    static constexpr const char* mod2Amount = "mod2Amount";
    static constexpr const char* mod3Source = "mod3Source";
    static constexpr const char* mod3Destination = "mod3Destination";
    static constexpr const char* mod3Amount = "mod3Amount";
    static constexpr const char* mod4Source = "mod4Source";
    static constexpr const char* mod4Destination = "mod4Destination";
    static constexpr const char* mod4Amount = "mod4Amount";
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
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterEnvAmount, "Filter Env Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterAttack, "Filter Attack", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterDecay, "Filter Decay", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterSustain, "Filter Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterRelease, "Filter Release", juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.5f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorRolloff, "Harmonic Roll-off", juce::NormalisableRange<float>(0.1f, 4.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorParity, "Odd/Even Balance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorShift, "Spectral Shift", juce::NormalisableRange<float>(0.5f, 2.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxSaturation, "Saturation", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxDelayTime, "Delay Time", juce::NormalisableRange<float>(0.01f, 2.0f, 0.0f, 0.5f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxDelayFeedback, "Delay FB", juce::NormalisableRange<float>(0.0f, 0.95f), 0.4f));
    
    juce::StringArray lfoSyncModes = { "Free", "Tempo Sync" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::fxDelaySync, "Delay Sync", lfoSyncModes, 0));
    juce::StringArray rhythmicDivisions = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4t", "1/8t", "1/16t" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::fxDelayDivision, "Delay Division", rhythmicDivisions, 2));

    // Chorus
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxChorusRate, "Chorus Rate", juce::NormalisableRange<float>(0.1f, 10.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxChorusDepth, "Chorus Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.2f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxChorusMix, "Chorus Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Reverb
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxReverbSize, "Reverb Size", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxReverbDamping, "Reverb Damping", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxReverbWidth, "Reverb Width", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::fxReverbMix, "Reverb Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::midiThru, "MIDI Thru", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::randomStrength, "Random Strength", 0.0f, 1.0f, 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::freezeResonator, "Freeze Resonator", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::freezeFilter, "Freeze Filter", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::freezeEnvelopes, "Freeze Envelopes", false));
    
    juce::StringArray curves = { "Linear", "Soft", "Hard" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::velocityCurve, "Velocity Curve", curves, 0));

    juce::StringArray midiChannels = { "Omni", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::midiChannel, "MIDI Channel", midiChannels, 0));

    juce::StringArray lfoWaveforms = { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random S&H" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1Waveform, "LFO 1 Wave", lfoWaveforms, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo1RateHz, "LFO 1 Rate", juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.5f), 1.0f));
    lfoSyncModes = { "Free", "Tempo Sync" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1SyncMode, "LFO 1 Sync", lfoSyncModes, 0));
    rhythmicDivisions = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4t", "1/8t", "1/16t" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo1RhythmicDivision, "LFO 1 Div", rhythmicDivisions, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo1Depth, "LFO 1 Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2Waveform, "LFO 2 Wave", lfoWaveforms, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo2RateHz, "LFO 2 Rate", juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2SyncMode, "LFO 2 Sync", lfoSyncModes, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::lfo2RhythmicDivision, "LFO 2 Div", rhythmicDivisions, 2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::lfo2Depth, "LFO 2 Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    juce::StringArray modDestinations = { 
        "Off", "Osc Level", "Inharmonicity", "Roughness", "Morph X", "Morph Y", 
        "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", 
        "Filter Cutoff", "Filter Res", "Filter Env Amt",
        "Flt Attack", "Flt Decay", "Flt Sustain", "Flt Release",
        "Saturation", "Delay Time", "Delay FB",
        "Odd/Even Bal", "Spectral Shift", "Harm Roll-off" };
    juce::StringArray modSources = { "Off", "LFO 1", "LFO 2", "Pitch Bend", "Mod Wheel", "Aftertouch" };

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
