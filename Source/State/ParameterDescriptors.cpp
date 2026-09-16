/*
  ==============================================================================

    ParameterDescriptors.cpp

  ==============================================================================
*/

#include "ParameterDescriptors.h"

#include "ParameterDefinitions.h"

#include <juce_events/juce_events.h>

namespace NEURONiK::State
{
namespace
{
    /**
        @brief Minimal AudioProcessor used only to materialise the real APVTS.

        @details `ParameterLayout` keeps its parameters private and hands them to
                 an AudioProcessor on construction, so an owner is required to
                 read them back. This probe owns nothing else, is never rendered,
                 and exists so descriptors are always derived from the same
                 layout the plugin uses.
    */
    class LayoutProbe final : public juce::AudioProcessor
    {
    public:
        LayoutProbe() : AudioProcessor (BusesProperties()) {}

        const juce::String getName() const override { return "NEURONiK Parameter Probe"; }

        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }

        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override {}

        bool hasEditor() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }

        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        bool isMidiEffect() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }

        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}

        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    /** @brief Read the real parameters produced by createParameterLayout().
     *  @details The probe + APVTS live only for the duration of the call: APVTS
     *           starts a timer, so it must not outlive the message manager.
     */
    std::vector<const juce::RangedAudioParameter*> readLayoutParameters (LayoutProbe& probe)
    {
        const juce::AudioProcessorValueTreeState state {
            probe, nullptr, juce::Identifier ("NEURONiKContractProbe"), createParameterLayout()
        };

        std::vector<const juce::RangedAudioParameter*> parameters;

        for (const auto* parameter : probe.getParameters())
            if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (parameter))
                parameters.push_back (ranged);

        return parameters;
    }

    LayoutApvts buildLayoutApvts()
    {
        LayoutApvts result;
        result.processor = std::make_unique<LayoutProbe>();
        result.apvts = std::make_unique<juce::AudioProcessorValueTreeState> (
            *result.processor, nullptr, juce::Identifier ("NEURONiKContractProbe"), createParameterLayout());

        return result;
    }

    ParameterDescriptor describe (const juce::RangedAudioParameter& parameter)
    {
        ParameterDescriptor descriptor;

        descriptor.id    = parameter.getParameterID();
        descriptor.name  = parameter.getName (64);
        descriptor.unit  = parameter.getLabel();
        descriptor.group = parameterGroupFor (descriptor.id);

        const auto& range = parameter.getNormalisableRange();

        descriptor.minValue      = range.start;
        descriptor.maxValue      = range.end;
        descriptor.interval      = range.interval;
        descriptor.skew          = range.skew;
        descriptor.symmetricSkew = range.symmetricSkew;
        descriptor.discrete      = parameter.isDiscrete();

        // APVTS reports defaults normalised to 0..1, so denormalise for consumers.
        descriptor.defaultNormalized = parameter.getDefaultValue();
        descriptor.defaultValue      = range.convertFrom0to1 (descriptor.defaultNormalized);

        if (const auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (&parameter))
        {
            descriptor.kind    = ParameterKind::choice;
            descriptor.choices = choice->choices;
            descriptor.defaultChoiceIndex = juce::roundToInt (descriptor.defaultValue);
        }
        else if (dynamic_cast<const juce::AudioParameterBool*> (&parameter) != nullptr)
        {
            descriptor.kind = ParameterKind::boolean;
            descriptor.choices = { "Off", "On" };
            descriptor.defaultChoiceIndex = descriptor.defaultValue > 0.5f ? 1 : 0;
        }
        else
        {
            descriptor.kind = ParameterKind::floatingPoint;
        }

        descriptor.dspStatus = dspStatusFor (descriptor.id);
        descriptor.engines   = engineCoverageFor (descriptor.id);
        descriptor.dspNote   = dspNoteFor (descriptor.id);

        return descriptor;
    }
}

juce::String parameterKindName (ParameterKind kind)
{
    switch (kind)
    {
        case ParameterKind::choice:  return "choice";
        case ParameterKind::boolean: return "bool";
        case ParameterKind::floatingPoint:
        default:                     return "float";
    }
}

juce::String parameterDspStatusName (ParameterDspStatus status)
{
    switch (status)
    {
        case ParameterDspStatus::uiOnly:    return "uiOnly";
        case ParameterDspStatus::notRouted: return "notRouted";
        case ParameterDspStatus::implemented:
        default:                            return "implemented";
    }
}

juce::String parameterEngineName (ParameterEngine engine)
{
    switch (engine)
    {
        case ParameterEngine::neuronik: return "neuronik";
        case ParameterEngine::neurotik: return "neurotik";
        case ParameterEngine::host:     return "host";
        case ParameterEngine::none:     return "none";
        case ParameterEngine::both:
        default:                        return "both";
    }
}

juce::StringArray getUiOnlyParameterIds()
{
    // Present in the APVTS and wired to panel behaviour, but never forwarded to a
    // DSP engine: the panels apply the action themselves.
    // Only the randomise action of ParameterPanel reads these; the DSP never does.
    return {
        IDs::randomStrength,    // scaling of the randomise button
        IDs::freezeResonator,   // randomise guards, applied by the panel
        IDs::freezeFilter,
        IDs::freezeEnvelopes
    };
}

juce::StringArray getNotRoutedParameterIds()
{
    // Read nowhere outside createParameterLayout(): moving them has no audible
    // effect today. Verified against NEURONiKProcessor and the UI sources.
    //
    // The single survivor is deliberate, not an oversight:
    //   unisonEnabled   its control was removed from the panel because the engine
    //                   never read it. Gating unison on it was rejected: the default
    //                   is off, so every existing preset would have gone silent.
    //                   The parameter stays so those presets keep loading unchanged.
    // harmMix was removed from the layout instead: keeping a dead parameter "for
    // compatibility" only pays off when it is read somewhere, and it never was.
    return {
        IDs::unisonEnabled
    };
}

ParameterDspStatus dspStatusFor (const juce::String& id)
{
    if (getUiOnlyParameterIds().contains (id))    return ParameterDspStatus::uiOnly;
    if (getNotRoutedParameterIds().contains (id)) return ParameterDspStatus::notRouted;

    return ParameterDspStatus::implemented;
}

ParameterEngine engineCoverageFor (const juce::String& id)
{
    // Resolved by the processor rather than by an engine: the channel filter and the
    // velocity curve run on the incoming MIDI buffer, the delay sync is turned into
    // seconds there, and MIDI thru is applied to the outgoing buffer.
    if (id == IDs::midiChannel
        || id == IDs::velocityCurve
        || id == IDs::midiThru
        || id == IDs::fxDelaySync
        || id == IDs::fxDelayDivision)
        return ParameterEngine::host;

    if (dspStatusFor (id) != ParameterDspStatus::implemented)
        return ParameterEngine::none;

    // Only assigned inside the Neuronik branch of synchronizeEngineParameters().
    if (id == IDs::oscInharmonicity
        || id == IDs::oscRoughness
        || id == IDs::resonatorParity
        || id == IDs::resonatorShift
        || id == IDs::resonatorRolloff
        || id.startsWith ("filter"))
        return ParameterEngine::neuronik;

    // Only assigned inside the Neurotik branch.
    if (id == IDs::oscExciteNoise
        || id == IDs::excitationColor
        || id == IDs::impulseMix
        || id == IDs::resonatorRes)
        return ParameterEngine::neurotik;

    // Global block, shared by both branches (including engineType, which picks one).
    return ParameterEngine::both;
}

juce::String dspNoteFor (const juce::String& id)
{
    if (id == IDs::velocityCurve)
        return "Applied by the processor to incoming note-ons, including the on-screen keyboard; Linear is the identity";
    if (id == IDs::midiThru)
        return "Echoes the engine MIDI output back to the host when enabled; off by default, so the plugin sends no MIDI";
    if (id == IDs::midiChannel)
        return "Filters incoming MIDI in the processor; the on-screen keyboard always sounds";
    if (id == IDs::masterBPM)
        return "Drives LFO tempo sync and the synced delay time";
    if (id == IDs::lfo1SyncMode || id == IDs::lfo1RhythmicDivision
        || id == IDs::lfo2SyncMode || id == IDs::lfo2RhythmicDivision)
        return "Tempo sync applied to the LFO through GlobalParams";
    if (id == IDs::fxDelaySync || id == IDs::fxDelayDivision)
        return "Resolved into delay seconds by the processor when sync is on";
    if (id == IDs::unisonEnabled)
        return "Retired from the panel: nothing reads it, unison amount comes from detune and spread";
    if (id == IDs::randomStrength)
        return "Panel action only: strength of the randomise button";
    if (id.startsWith ("freeze"))
        return "Panel action only: the UI performs the freeze";

    return {};
}

juce::String dspNoteForUnroutedId (const juce::String& id)
{
    if (id == IDs::oscPitchCoarse)
        return "Declared in the IDs namespace but absent from the layout";

    return {};
}

juce::String parameterGroupFor (const juce::String& id)
{
    if (id == IDs::masterLevel
        || id == IDs::masterBPM
        || id == IDs::randomStrength
        || id == IDs::velocityCurve
        || id.startsWith ("midi")
        || id.startsWith ("freeze"))
        return "global";

    if (id.startsWith ("lfo1")) return "lfo1";
    if (id.startsWith ("lfo2")) return "lfo2";
    if (id.startsWith ("mod"))  return "modMatrix";

    if (id.startsWith ("fxSaturation") || id.startsWith ("fxDelay")) return "fx";
    if (id.startsWith ("fxChorus"))                                  return "chorus";
    if (id.startsWith ("fxReverb"))                                  return "reverb";

    if (id.startsWith ("env"))    return "envelope";
    if (id.startsWith ("filter")) return "filter";

    if (id.startsWith ("resonator") || id == IDs::resonatorRes) return "resonator";

    return "oscillator";
}

std::vector<ParameterDescriptor> getParameterDescriptors()
{
    LayoutProbe probe;
    const auto parameters = readLayoutParameters (probe);

    std::vector<ParameterDescriptor> descriptors;
    descriptors.reserve (parameters.size());

    for (const auto* parameter : parameters)
        descriptors.push_back (describe (*parameter));

    return descriptors;
}

const ParameterDescriptor* findParameterDescriptor (const juce::String& id)
{
    // Plain data only: no APVTS or probe is kept alive between calls.
    static const std::vector<ParameterDescriptor> descriptors = getParameterDescriptors();

    for (const auto& descriptor : descriptors)
        if (descriptor.id == id)
            return &descriptor;

    return nullptr;
}

juce::StringArray getUnroutedParameterIds()
{
    // Declared in IDs:: but never added to createParameterLayout().
    // See DSP_PARAMETERS.md for the behaviour notes behind each entry.
    return { IDs::oscPitchCoarse };
}

LayoutApvts createLayoutApvts()
{
    return buildLayoutApvts();
}

} // namespace NEURONiK::State
