/*
  ==============================================================================

    ParameterDescriptors.h
    Created: 16 Sep 2026
    Description: Framework-agnostic descriptor contract for NEURONiK parameters.

                 Descriptors are DERIVED from createParameterLayout() so the
                 APVTS remains the single source of truth: IDs, ranges,
                 intervals, skew, defaults and choice lists are read back from
                 the real parameters instead of being duplicated by hand.
                 The only hand-written metadata here is presentation grouping
                 (group) and the list of IDs that are declared but intentionally
                 not routed to the layout yet.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

namespace NEURONiK::State
{

/** @brief Value domain of a parameter, as exposed to non-JUCE consumers. */
enum class ParameterKind
{
    floatingPoint,
    choice,
    boolean
};

/**
 * @brief How far a parameter actually travels inside the plugin.
 * @details Established by inspecting NEURONiKProcessor::synchronizeEngineParameters()
 *          and the remaining references in the code base, not by assumption.
 */
enum class ParameterDspStatus
{
    implemented,   //!< Read by the processor and forwarded to the DSP (or selects the engine)
    uiOnly,        //!< Drives a panel action; the DSP never sees it
    notRouted      //!< Lives in the APVTS but is never read outside its own definition
};

/** @brief Where a parameter is consumed. */
enum class ParameterEngine
{
    both,          //!< Shared DSP: FX, LFOs and global values
    neuronik,      //!< Only the Neuronik branch of synchronizeEngineParameters()
    neurotik,      //!< Only the Neurotik branch
    host,          //!< Consumed by the processor / MIDI layer, not by an engine
    none           //!< Nothing consumes it
};

/** @brief Stable, serialisable description of a single parameter. */
struct ParameterDescriptor
{
    juce::String id;                  //!< APVTS parameter ID, e.g. "filterCutoff"
    juce::String name;                //!< Human readable name
    juce::String group;               //!< UI grouping (presentation only)
    juce::String unit;                //!< APVTS label, e.g. "Hz" (may be empty)

    ParameterKind kind = ParameterKind::floatingPoint;

    float minValue = 0.0f;            //!< Real (denormalised) range start
    float maxValue = 1.0f;            //!< Real (denormalised) range end
    float defaultValue = 0.0f;        //!< Real (denormalised) default
    float defaultNormalized = 0.0f;   //!< Default as APVTS reports it (0..1)

    float interval = 0.0f;            //!< 0 means continuous
    float skew = 1.0f;                //!< NormalisableRange skew factor
    bool symmetricSkew = false;
    bool discrete = false;

    juce::StringArray choices;        //!< choice only; empty for float
    int defaultChoiceIndex = 0;       //!< choice only

    ParameterDspStatus dspStatus = ParameterDspStatus::implemented;
    ParameterEngine engines = ParameterEngine::both;
    juce::String dspNote;             //!< Only set when the status needs explaining
};

/** @brief Machine readable name of a kind ("float", "choice", "bool"). */
juce::String parameterKindName (ParameterKind kind);

/** @brief Machine readable name of a DSP status ("implemented", "uiOnly", "notRouted"). */
juce::String parameterDspStatusName (ParameterDspStatus status);

/** @brief Machine readable engine coverage ("both", "neuronik", "neurotik", "host", "none"). */
juce::String parameterEngineName (ParameterEngine engine);

/** @brief DSP status for a parameter ID. Unknown IDs report notRouted. */
ParameterDspStatus dspStatusFor (const juce::String& id);

/** @brief Synthesis path coverage for a parameter ID. */
ParameterEngine engineCoverageFor (const juce::String& id);

/** @brief Short explanation for parameters whose status is not self evident. */
juce::String dspNoteFor (const juce::String& id);

/** @brief IDs that exist in the APVTS but are only wired to panel actions. */
juce::StringArray getUiOnlyParameterIds();

/** @brief IDs that exist in the APVTS but are never read by the processor. */
juce::StringArray getNotRoutedParameterIds();

/** @brief Presentation group for an ID. Defaults to "oscillator". */
juce::String parameterGroupFor (const juce::String& id);

/**
 * @brief Build the descriptor table from the real APVTS layout.
 * @details Parameters are read back from the objects createParameterLayout()
 *          produces, so IDs, ranges, intervals, skews, defaults and choice
 *          lists can never drift from the plugin.
 *
 *          Requires an initialised message manager, because an
 *          AudioProcessorValueTreeState starts a timer while it is alive. GUI
 *          apps and plugin editors already satisfy this; command line tools
 *          must create a juce::ScopedJuceInitialiser_GUI first.
 */
std::vector<ParameterDescriptor> getParameterDescriptors();

/** @brief Look up a descriptor by parameter ID, or nullptr when unknown. */
const ParameterDescriptor* findParameterDescriptor (const juce::String& id);

/**
 * @brief IDs declared in the IDs namespace that are deliberately not part of
 *        the layout yet. Empty since 2026-09-19 (oscPitchCoarse was retired).
 * @details Keeps the known divergences explicit and testable: the regression
 *          suite asserts every entry here is still absent from the layout, so
 *          wiring one up forces this list to be updated, and a new entry is
 *          exported to the contract as notInLayout instead of disappearing.
 */
juce::StringArray getUnroutedParameterIds();

/** @brief Short explanation for IDs that are declared but not in the layout. */
juce::String dspNoteForUnroutedId (const juce::String& id);

/**
 * @brief The real APVTS layout, hosted by a throwaway processor.
 * @details Lets tools and tests work with exactly the parameters the plugin
 *          ships (saving and loading preset state, for instance) without
 *          constructing the full NEURONiKProcessor, which needs an editor, a
 *          preset manager and a render engine.
 *
 *          Like getParameterDescriptors(), this needs an initialised message
 *          manager. Keep the returned struct alive for as long as the APVTS is
 *          used; the processor must outlive the state that refers to it.
 */
struct LayoutApvts
{
    std::unique_ptr<juce::AudioProcessor> processor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;
};

LayoutApvts createLayoutApvts();

} // namespace NEURONiK::State
