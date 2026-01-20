// Source/State/ParameterDefinitions.h
// ============================================================================
// NEXUS Parameter Definitions
// Centralized parameter definitions to avoid magic strings/numbers
// ============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace NexusParams {
    
    // ========================================================================
    // OSCILLATOR / PITCH
    // ========================================================================
    namespace Oscillator {
        const juce::Identifier PITCH_COARSE = "oscPitchCoarse";           // -24 to +24 semitones
        const juce::Identifier PITCH_FINE = "oscPitchFine";              // -1.0 to +1.0 semitones
        const juce::Identifier PITCH_OCTAVE = "oscPitchOctave";          // -3 to +3 octaves
        const juce::Identifier HARMONIC_COUNT = "oscHarmonicCount";      // 8 to 64 partials
        const juce::Identifier HARMONIC_INHARMONICITY = "oscInharmonicity"; // 0.0 to 1.0
    }
    
    // ========================================================================
    // HARMONY / NOISE BALANCE
    // ========================================================================
    namespace Harmony {
        const juce::Identifier HARMONIC_MIX = "harmMix";                 // 0.0 (all noise) to 1.0 (all harmonic)
        const juce::Identifier NOISE_ROUGHNESS = "noiseRoughness";       // 0.0 (smooth) to 1.0 (filtered)
        const juce::Identifier NOISE_FILTER_TYPE = "noiseFilterType";    // LP, HP, BP, Notch
    }
    
    // ========================================================================
    // ENVELOPE / AMPLITUDE
    // ========================================================================
    namespace Envelope {
        const juce::Identifier AMP_ATTACK = "envAmpAttack";              // 0.1 to 5000 ms
        const juce::Identifier AMP_DECAY = "envAmpDecay";                // 0.1 to 5000 ms
        const juce::Identifier AMP_SUSTAIN = "envAmpSustain";            // 0.0 to 1.0
        const juce::Identifier AMP_RELEASE = "envAmpRelease";            // 0.1 to 5000 ms
        
        const juce::Identifier FILTER_ATTACK = "envFilterAttack";        // 0.1 to 5000 ms
        const juce::Identifier FILTER_DECAY = "envFilterDecay";          // 0.1 to 5000 ms
        const juce::Identifier FILTER_SUSTAIN = "envFilterSustain";      // 0.0 to 1.0
        const juce::Identifier FILTER_RELEASE = "envFilterRelease";      // 0.1 to 5000 ms
        const juce::Identifier FILTER_DEPTH = "envFilterDepth";          // -5000 to +5000 Hz
        
        const juce::Identifier MOD_ATTACK = "envModAttack";              // 0.1 to 5000 ms
        const juce::Identifier MOD_DECAY = "envModDecay";                // 0.1 to 5000 ms
        const juce::Identifier MOD_SUSTAIN = "envModSustain";            // 0.0 to 1.0
        const juce::Identifier MOD_RELEASE = "envModRelease";            // 0.1 to 5000 ms
    }
    
    // ========================================================================
    // FILTER / RESONANCE
    // ========================================================================
    namespace Filter {
        enum class Type { LP24, LP12, HP6, BP, Notch };
        
        const juce::Identifier CUTOFF_FREQ = "filterCutoff";             // 20 to 20000 Hz (logarithmic)
        const juce::Identifier RESONANCE_Q = "filterResonance";          // 0.5 to 10.0
        const juce::Identifier FILTER_TYPE = "filterType";               // Choice: LP24/LP12/HP6/BP/Notch
        const juce::Identifier KEYBOARD_TRACKING = "filterKeyboardTracking"; // 0.0 to 1.0
    }
    
    // ========================================================================
    // LFO (Low Frequency Oscillator)
    // ========================================================================
    namespace LFO {
        enum class Waveform { Sine, Triangle, Sawtooth, Square, Random, SampleHold };
        
        const juce::Identifier RATE_1 = "lfo1Rate";                      // 0.1 to 20 Hz
        const juce::Identifier DEPTH_1 = "lfo1Depth";                    // 0.0 to 1.0
        const juce::Identifier WAVEFORM_1 = "lfo1Waveform";              // Choice: Sine/Triangle/Saw/Square/Random
        const juce::Identifier PHASE_1 = "lfo1Phase";                    // 0.0 to 360 degrees
        const juce::Identifier RETRIGGER_1 = "lfo1Retrigger";            // Bool: reset on note on?
        
        const juce::Identifier RATE_2 = "lfo2Rate";                      // 0.1 to 20 Hz
        const juce::Identifier DEPTH_2 = "lfo2Depth";                    // 0.0 to 1.0
        const juce::Identifier WAVEFORM_2 = "lfo2Waveform";              // Choice
        const juce::Identifier PHASE_2 = "lfo2Phase";                    // 0.0 to 360 degrees
        const juce::Identifier RETRIGGER_2 = "lfo2Retrigger";            // Bool
    }
    
    // ========================================================================
    // MODULATION / PITCH / VIBRATO
    // ========================================================================
    namespace Modulation {
        const juce::Identifier VIBRATO_DEPTH = "modVibratoDepth";        // 0.0 to 100 cents
        const juce::Identifier VIBRATO_RATE = "modVibratoRate";          // 0.1 to 20 Hz
        const juce::Identifier VIBRATO_DELAY = "modVibratoDelay";        // 0 to 5000 ms
        
        const juce::Identifier PITCH_BEND_RANGE = "modPitchBendRange";   // 1 to 24 semitones
        const juce::Identifier GLIDE_TIME = "modGlideTime";              // 0 (off) to 5000 ms
        const juce::Identifier GLIDE_MODE = "modGlideMode";              // Linear, Exponential
    }
    
    // ========================================================================
    // TIMBRE / TRANSFER / MORPHING
    // ========================================================================
    namespace Timbre {
        const juce::Identifier TRANSFER_ENABLED = "timbreTransferEnabled";       // Bool
        const juce::Identifier TRANSFER_INTENSITY = "timbreTransferIntensity";   // 0.0 to 1.0
        const juce::Identifier MORPHING_AMOUNT = "timbremorphingAmount";         // 0.0 (Timbre A) to 1.0 (Timbre B)
        const juce::Identifier TIMBRE_PRESET = "timbrePreset";                   // Choice: Warm/Bright/Dark/etc
    }
    
    // ========================================================================
    // VOICE / POLYPHONY
    // ========================================================================
    namespace Voice {
        enum class Mode { Polyphonic, Monophonic, Unison };
        
        const juce::Identifier COUNT = "voiceCount";                     // 1 to 16 (reduced on RPi)
        const juce::Identifier MODE = "voiceMode";                       // Choice: Poly/Mono/Unison
        const juce::Identifier UNISON_DETUNE = "voiceUnisonDetune";      // 0.0 to 100 cents
        const juce::Identifier UNISON_SPREAD = "voiceUnisonSpread";      // 0.0 to 1.0 (stereo width)
    }
    
    // ========================================================================
    // EFFECTS / EQ
    // ========================================================================
    namespace Effects {
        // Resonant Filter (main effects chain filter)
        const juce::Identifier FILTER_CUTOFF = "effectsFilterCutoff";    // 20 to 20000 Hz
        const juce::Identifier FILTER_RESONANCE = "effectsFilterResonance"; // 0.5 to 10.0
        const juce::Identifier FILTER_TYPE = "effectsFilterType";        // Choice: LP24/LP12/HP6/BP/Notch
        
        // Compressor
        const juce::Identifier COMP_THRESHOLD = "effectsCompThreshold";  // -80 to 0 dB
        const juce::Identifier COMP_RATIO = "effectsCompRatio";          // 1:1 to 16:1
        const juce::Identifier COMP_ATTACK = "effectsCompAttack";        // 1 to 100 ms
        const juce::Identifier COMP_RELEASE = "effectsCompRelease";      // 10 to 1000 ms
        const juce::Identifier COMP_MAKEUP = "effectsCompMakeup";        // 0.0 to 1.0
        
        // Distortion
        const juce::Identifier DISTORTION_AMOUNT = "effectsDistortionAmount"; // 0.0 to 1.0
        const juce::Identifier DISTORTION_TYPE = "effectsDistortionType"; // Soft-clip, Saturation, Hard-clip
        
        // Delay
        const juce::Identifier DELAY_TIME = "effectsDelayTime";          // 10 to 2000 ms
        const juce::Identifier DELAY_FEEDBACK = "effectsDelayFeedback";  // 0.0 to 0.95
        const juce::Identifier DELAY_MIX = "effectsDelayMix";            // 0.0 to 1.0
        
        // Reverb
        const juce::Identifier REVERB_ROOM_SIZE = "effectsReverbRoomSize"; // 0.0 to 1.0
        const juce::Identifier REVERB_DAMPING = "effectsReverbDamping";  // 0.0 to 1.0
        const juce::Identifier REVERB_WIDTH = "effectsReverbWidth";      // 0.0 to 1.0
        const juce::Identifier REVERB_MIX = "effectsReverbMix";          // 0.0 to 1.0
        
        // Chorus
        const juce::Identifier CHORUS_RATE = "effectsChorusRate";        // 0.1 to 5.0 Hz
        const juce::Identifier CHORUS_DEPTH = "effectsChorusDepth";      // 0.0 to 1.0
        const juce::Identifier CHORUS_MIX = "effectsChorusMix";          // 0.0 to 1.0
    }
    
    // ========================================================================
    // MASTER / OUTPUT
    // ========================================================================
    namespace Master {
        const juce::Identifier OUTPUT_VOLUME = "masterVolume";           // -80 to 0 dB
        const juce::Identifier OUTPUT_PAN = "masterPan";                 // -1.0 (left) to 1.0 (right)
        const juce::Identifier LIMITER_ENABLED = "masterLimiterEnabled"; // Bool
        const juce::Identifier LIMITER_THRESHOLD = "masterLimiterThreshold"; // -20 to 0 dB
    }
    
    // ========================================================================
    // MISCELLANEOUS / SYSTEM
    // ========================================================================
    namespace System {
        const juce::Identifier MIDI_LEARN_MODE = "sysMidiLearnMode";     // Bool
        const juce::Identifier MIDI_LEARN_CC = "sysMidiLearnCC";         // 0-127
        const juce::Identifier TUNING_REFERENCE = "sysTuningReference";   // 420 to 460 Hz (A4)
    }
    
    // ========================================================================
    // CONSTANTS & RANGES (for reference)
    // ========================================================================
    
    // Pitch ranges
    static constexpr float PITCH_COARSE_MIN = -24.0f;
    static constexpr float PITCH_COARSE_MAX = 24.0f;
    static constexpr float PITCH_FINE_MIN = -1.0f;
    static constexpr float PITCH_FINE_MAX = 1.0f;
    
    // Frequency ranges
    static constexpr float FREQ_MIN = 20.0f;
    static constexpr float FREQ_MAX = 20000.0f;
    
    // Time ranges
    static constexpr float TIME_MIN_MS = 0.1f;
    static constexpr float TIME_MAX_MS = 5000.0f;
    
    // LFO ranges
    static constexpr float LFO_RATE_MIN = 0.1f;
    static constexpr float LFO_RATE_MAX = 20.0f;
    
    // Voice ranges
    static constexpr int VOICE_COUNT_MIN = 1;
    static constexpr int VOICE_COUNT_MAX = 16;
    static constexpr int VOICE_COUNT_RPI = 6;  // Reduced on Raspberry Pi
    
    // Harmonic ranges
    static constexpr int HARMONIC_COUNT_MIN = 8;
    static constexpr int HARMONIC_COUNT_MAX = 64;
    static constexpr int HARMONIC_COUNT_RPI = 32;  // Reduced on Raspberry Pi
    
} // namespace NexusParams

// ============================================================================
// Factory function to create AudioProcessorValueTreeState
// ============================================================================
juce::AudioProcessorValueTreeState createAudioProcessorValueTreeState(
    juce::AudioProcessor& processor);

// Helper to get parameter float value safely
inline float getParameterValue(
    juce::AudioProcessorValueTreeState& apvts,
    const juce::Identifier& paramID) noexcept {
    auto* param = apvts.getParameter(paramID.toString());
    return param ? param->getValue() : 0.0f;
}

// Helper to set parameter value
inline void setParameterValue(
    juce::AudioProcessorValueTreeState& apvts,
    const juce::Identifier& paramID,
    float normalizedValue) noexcept {
    if (auto* param = apvts.getParameter(paramID.toString())) {
        param->setValueNotifyingHost(normalizedValue);
    }
}
