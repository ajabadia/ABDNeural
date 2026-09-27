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
    static constexpr const char* engineType       = "engineType";
    static constexpr const char* oscLevel         = "oscLevel";
    // oscPitchCoarse was retired on 2026-09-19: declared here since day one but never added
    // to the layout, so no engine, panel or preset ever read it. It only existed as a
    // promise (a leftover of the old NexusParams draft, whose siblings oscPitchFine,
    // oscPitchOctave and oscHarmonicCount were never adopted either). Coarse tuning would
    // be a FEATURE, not pending wiring: the pitch path that does exist is the per-voice MPE
    // bend (IVoice::notePitchBend, in semitones), and a global transpose would belong in
    // the host next to velocityCurve/midiChannel — no engine or WASM ABI change needed.
    // See DSP_PARAMETERS.md, "Parámetros sin consumidor".
    static constexpr const char* oscInharmonicity = "oscInharmonicity";
    static constexpr const char* oscRoughness     = "oscRoughness";
    // harmMix was retired on 2026-09-16: it was never read by any engine, so it only
    // existed as a promise. Old presets that still contain it load normally, and the
    // child is dropped the next time they are saved (see PresetManager migration).
    static constexpr const char* morphX           = "morphX";
    static constexpr const char* morphZ           = "morphZ";
    // FASE 11.3: eje temporal de las capas 1 y 2 (sampler por capa). Son dos
    // parametros de verdad —no estado interno— porque la matriz tiene que
    // poder modularlos y un preset tiene que guardarlos; su default (0.0) es el
    // frame canonico, asi que con un modelo de una capa (todo el legado) son
    // inertes y el sonido no cambia.
    static constexpr const char* morphZ2          = "morphZ2";
    static constexpr const char* morphZ3          = "morphZ3";
    static constexpr const char* morphY           = "morphY";
    static constexpr const char* oscExciteNoise   = "oscExciteNoise";
    static constexpr const char* excitationColor  = "excitationColor";
    static constexpr const char* impulseMix       = "impulseMix";
    static constexpr const char* oscExciteBow     = "oscExciteBow";
    static constexpr const char* resonatorRes     = "resonatorRes";
    static constexpr const char* unisonDetune     = "unisonDetune";
    static constexpr const char* unisonSpread     = "unisonSpread";
    static constexpr const char* unisonEnabled    = "unisonEnabled";

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
    // filterEnvAmount was retired on 2026-09-26: with ENV 2 travelling through
    // the mod matrix (default route ENV 2 -> Filter Cutoff), the knob was the
    // SECOND depth on the same path (matrix amount × knob). The matrix amount
    // is now THE depth (bipolar, +/-1: negative inverts the envelope); the
    // destination "Filter Env Amt" (index 12) survives and adds to the 1.0
    // routing factor, so the label keeps its meaning. Old presets that still
    // contain it load normally and the child is dropped on next save
    // (see PresetManager migration, same precedent as harmMix).
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

/**
 * @brief One modulation destination: the label a mod slot stores, and the
 *        parameter that destination drives (nullptr for "Off").
 *
 * @details THIS ORDER IS PRESET STATE. A mod destination is stored as its INDEX
 *          (`mod1Destination` = 20 is what an old preset means by "Odd/Even
 *          Bal"), so entries may be APPENDED but never reordered or removed.
 *          That is why the labels and the ids live in the same table instead of
 *          being matched by hand in two files.
 *
 *          Which ENGINE can use a destination is deliberately NOT here: it is
 *          derived from `engineCoverageFor (parameterId)` in
 *          ParameterDescriptors.h, so a destination can never claim a different
 *          engine than the parameter it actually drives.
 */
struct ModDestination
{
    const char* label;
    const char* parameterId;   //!< nullptr for "Off" (drives nothing)
};

/** @brief Modulation destinations in preset-index order (see ModDestination). */
inline const std::vector<ModDestination>& getModDestinationTable()
{
    static const std::vector<ModDestination> table
    {
        { "Off",            nullptr },
        { "Osc Level",      IDs::oscLevel },
        { "Inharmonicity",  IDs::oscInharmonicity },
        { "Roughness",      IDs::oscRoughness },
        { "Morph X",        IDs::morphX },
        { "Morph Y",        IDs::morphY },
        { "Amp Attack",     IDs::envAttack },
        { "Amp Decay",      IDs::envDecay },
        { "Amp Sustain",    IDs::envSustain },
        { "Amp Release",    IDs::envRelease },
        { "Filter Cutoff",  IDs::filterCutoff },
        { "Filter Res",     IDs::filterRes },
        // Index 12: "Filter Env Amt" — the parameter was retired (2026-09-26;
        // the matrix amount IS the depth) but the destination LABEL stays: the
        // indices are the preset format. The engine adds it to the 1.0 routing
        // factor of ENV 2 (AdditiveVoice::modEnvFltDepth), so the label keeps
        // its meaning: more/less/inverted envelope through the route.
        { "Filter Env Amt", nullptr },
        { "Flt Attack",     IDs::filterAttack },
        { "Flt Decay",      IDs::filterDecay },
        { "Flt Sustain",    IDs::filterSustain },
        { "Flt Release",    IDs::filterRelease },
        { "Saturation",     IDs::fxSaturation },
        { "Delay Time",     IDs::fxDelayTime },
        { "Delay FB",       IDs::fxDelayFeedback },
        { "Odd/Even Bal",   IDs::resonatorParity },
        { "Spectral Shift", IDs::resonatorShift },
        { "Harm Roll-off",  IDs::resonatorRolloff },
        { "Excite Noise",   IDs::oscExciteNoise },
        { "Excite Color",   IDs::excitationColor },
        { "Impulse Mix",    IDs::impulseMix },
        { "Res Bank Res",   IDs::resonatorRes },
        { "Unison Detune",  IDs::unisonDetune },
        // APPEND siempre: los choice de la matriz guardan INDICE de preset
        // (insertar en medio re-mapearia presets guardados).
        { "Morph Z",        IDs::morphZ },
        { "Morph Z 2",      IDs::morphZ2 },
        { "Morph Z 3",      IDs::morphZ3 },
    };

    return table;
}

/** @brief Engine selector labels, in APVTS index order (also preset state). */
inline juce::StringArray getEngineChoiceLabels()
{
    return { "NEURONiK", "Neurotik" };
}

/** @brief Destination labels in index order, derived from the table above. */
inline juce::StringArray getModDestinations()
{
    juce::StringArray labels;

    for (const auto& destination : getModDestinationTable())
        labels.add (destination.label);

    return labels;
}

inline juce::StringArray getModSources()
{
    // ENV 1 (amplitud) y ENV 2 (filtro) al FINAL: los choice se guardan por
    // indice y anadir en medio re-mapearia los presets guardados (6/7 son
    // indices nuevos, nunca usados antes).
    return { "Off", "LFO 1", "LFO 2", "Pitch Bend", "Mod Wheel", "Aftertouch",
             "ENV 1", "ENV 2" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::masterLevel, "Master Level", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::masterBPM, "Master BPM", juce::NormalisableRange<float>(20.0f, 400.0f), 120.0f));
    
    const auto engines = getEngineChoiceLabels();
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::engineType, "Engine Type", engines, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscLevel, "Osc Level", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscInharmonicity, "Inharmonicity", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscRoughness, "Roughness", juce::NormalisableRange<float>(0.0f, 0.5f), 0.0f)); // Range reduced
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphX, "Morph X", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphY, "Morph Y", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // Morph Z (FASE 10): eje temporal sobre los frames del modelo. Default 0.0
    // = frame canonico => bit-compatible con todo el legado (paridad A-E).
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphZ, "Morph Z", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    // FASE 11.3: los z de las capas 1 y 2 (mismo rango y mismo default que
    // morphZ). Con un modelo de una capa no llegan al sonido.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphZ2, "Morph Z 2", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::morphZ3, "Morph Z 3", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscExciteNoise, "Excite Noise", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::excitationColor, "Excite Color", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::impulseMix, "Impulse Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.8f));
    // Bow (arco continuo): default 0.0 = comportamiento bit-compatible con
    // lo existente (impulso+ruido); con >0 el sostenido canta con ganancia
    // plena (modo arco), arreglando el gain staging de las notas largas.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::oscExciteBow, "Bow Excite", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::resonatorRes, "Res Bank Resonance", juce::NormalisableRange<float>(0.5f, 1.0f, 0.0f, 0.4f), 0.99f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::unisonDetune, "Spectral Detune", juce::NormalisableRange<float>(0.0f, 0.1f, 0.0f, 0.5f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::unisonSpread, "Spectral Spread", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(IDs::unisonEnabled, "Spectral Unison", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envAttack, "Attack", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.01f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envDecay, "Decay", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.1f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envSustain, "Sustain", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::envRelease, "Release", juce::NormalisableRange<float>(0.01f, 5.0f, 0.0f, 0.5f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterCutoff, "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.3f), 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::filterRes, "Resonance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
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

    // Derived from the one table (getModDestinationTable), never re-typed here: a
    // destination added there shows up in every mod slot, in the same order.
    const auto modDestinations = getModDestinations();
    // SSOT: la lista de fuentes sale de getModSources() (8 items, con las ENV
    // al final): los defaults 6/7 de abajo son indices VALIDOS de esta lista.
    const juce::StringArray modSources = getModSources();

    // Defaults del PRESET NUEVO (las envolventes viajan por la matriz):
    // mod1 = ENV 1 -> Osc Level (profundidad nominal 1.0) y mod2 = ENV 2 ->
    // Filter Cutoff (1.0). Indices de choice: env1=6, env2=7 en getModSources();
    // Osc Level=1, Filter Cutoff=10 en getModDestinationTable(). Los presets
    // guardados llegan por PresetMigration, que inserta las mismas rutas en
    // una ranura libre (o no toca nada si no hay sitio: mismo sonido).
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod1Source, "Mod 1 Source", modSources, 6));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod1Destination, "Mod 1 Dest", modDestinations, 1));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod1Amount, "Mod 1 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod2Source, "Mod 2 Source", modSources, 7));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod2Destination, "Mod 2 Dest", modDestinations, 10));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod2Amount, "Mod 2 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod3Source, "Mod 3 Source", modSources, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod3Destination, "Mod 3 Dest", modDestinations, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod3Amount, "Mod 3 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod4Source, "Mod 4 Source", modSources, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(IDs::mod4Destination, "Mod 4 Dest", modDestinations, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(IDs::mod4Amount, "Mod 4 Amount", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));

    return { params.begin(), params.end() };
}

} // namespace NEURONiK::State
