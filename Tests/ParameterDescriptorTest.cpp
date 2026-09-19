/*
  ==============================================================================

    ParameterDescriptorTest.cpp
    Created: 16 Sep 2026
    Description: Guards the parameter descriptor contract:

                   1. descriptors are coherent with the real APVTS layout;
                   2. the contract keeps the pinned values the WebUI depends on;
                   3. the committed generated artifacts are up to date.

  ==============================================================================
*/

#include "../Source/State/ParameterDescriptorExport.h"
#include "../Source/State/ParameterDefinitions.h"

#include <juce_events/juce_events.h>

#include <iostream>

namespace
{
    // Pinned so adding or removing a parameter is a deliberate, visible change.
    constexpr int EXPECTED_PARAMETER_COUNT = 70;
    // Audited against the real references in Source/, not against an assumption:
    // see the DSP_PARAMETERS.md section "Estado de implementación DSP".
    constexpr int EXPECTED_IMPLEMENTED_COUNT = 65;
    constexpr int EXPECTED_UI_ONLY_COUNT = 4;
    constexpr int EXPECTED_NOT_ROUTED_COUNT = 1;

    int failures = 0;

    void check (bool condition, const juce::String& description)
    {
        if (condition)
        {
            std::cout << "  [ok]   " << description << '\n';
            return;
        }

        std::cout << "  [FAIL] " << description << '\n';
        ++failures;
    }

    bool nearlyEqual (float a, float b, float tolerance = 1.0e-4f)
    {
        return std::abs (a - b) <= tolerance;
    }

    const NEURONiK::State::ParameterDescriptor* requireDescriptor (const juce::String& id)
    {
        const auto* descriptor = NEURONiK::State::findParameterDescriptor (id);
        check (descriptor != nullptr, "descriptor exists for '" + id + "'");
        return descriptor;
    }
}

int main()
{
    using namespace NEURONiK::State;

    // Reading descriptors materialises an APVTS, which needs a message manager.
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const auto descriptors = getParameterDescriptors();

    std::cout << "NEURONiK parameter descriptor contract\n";

    // --- 1. Coherence with the APVTS layout -------------------------------------
    std::cout << "\nLayout coherence\n";

    check (! descriptors.empty(), "descriptor table is not empty");
    check (descriptors.size() == EXPECTED_PARAMETER_COUNT,
           "descriptor count matches the known contract size ("
               + juce::String (descriptors.size()) + " vs " + juce::String (EXPECTED_PARAMETER_COUNT) + ")");

    juce::StringArray seenIds;
    bool idsUnique = true;
    bool rangesValid = true;
    bool defaultsInRange = true;
    bool groupsPresent = true;
    bool choicesValid = true;

    for (const auto& descriptor : descriptors)
    {
        if (seenIds.contains (descriptor.id))
            idsUnique = false;
        seenIds.add (descriptor.id);

        if (descriptor.maxValue < descriptor.minValue || descriptor.interval < 0.0f)
            rangesValid = false;

        if (descriptor.defaultValue < descriptor.minValue - 1.0e-4f
            || descriptor.defaultValue > descriptor.maxValue + 1.0e-4f)
            defaultsInRange = false;

        if (descriptor.group.isEmpty())
            groupsPresent = false;

        if (descriptor.kind == ParameterKind::choice)
        {
            if (descriptor.choices.size() < 2
                || descriptor.defaultChoiceIndex < 0
                || descriptor.defaultChoiceIndex >= descriptor.choices.size())
                choicesValid = false;
        }
        else if (! descriptor.choices.isEmpty() && descriptor.kind == ParameterKind::floatingPoint)
        {
            choicesValid = false;
        }
    }

    check (idsUnique, "parameter IDs are unique");
    check (rangesValid, "every range is ordered and has a non-negative interval");
    check (defaultsInRange, "every default sits inside its own range");
    check (groupsPresent, "every descriptor carries a presentation group");
    check (choicesValid, "choice descriptors expose valid choice lists and default index");

    bool lookupComplete = true;
    for (const auto& descriptor : descriptors)
        if (findParameterDescriptor (descriptor.id) == nullptr)
            lookupComplete = false;

    check (lookupComplete, "every descriptor is reachable through findParameterDescriptor");
    check (findParameterDescriptor ("__does_not_exist__") == nullptr,
           "unknown IDs resolve to nullptr");

    // --- 2. Unrouted IDs stay explicitly tracked --------------------------------
    std::cout << "\nUnrouted IDs\n";

    juce::StringArray layoutIds;
    for (const auto& descriptor : descriptors)
        layoutIds.add (descriptor.id);

    // Empty since 2026-09-19: the last entry, oscPitchCoarse, was retired from IDs:: as
    // well (nothing read it), so the contract has no phantom IDs left in either direction.
    // The loop below stays: the moment a new ID is declared outside the layout, this is
    // what forces it to be acknowledged here and in the generated contract.
    check (getUnroutedParameterIds().isEmpty(),
           "no ID is declared outside the layout (unexpected: "
               + getUnroutedParameterIds().joinIntoString (", ") + ")");

    for (const auto& unrouted : getUnroutedParameterIds())
        check (! layoutIds.contains (unrouted),
               "unrouted ID '" + unrouted + "' is still absent from the layout");

    // --- 3. Pinned contract values the WebUI relies on --------------------------
    std::cout << "\nPinned values\n";

    if (const auto* masterLevel = requireDescriptor (IDs::masterLevel))
    {
        check (nearlyEqual (masterLevel->minValue, 0.0f) && nearlyEqual (masterLevel->maxValue, 1.0f),
               "masterLevel range is [0, 1]");
        check (nearlyEqual (masterLevel->defaultValue, 0.8f), "masterLevel default is 0.8");
        check (masterLevel->kind == ParameterKind::floatingPoint, "masterLevel is a float parameter");
        check (masterLevel->group == "global", "masterLevel is grouped under global");
    }

    if (const auto* morphX = requireDescriptor (IDs::morphX))
        check (nearlyEqual (morphX->defaultValue, 0.0f), "morphX default is 0");

    if (const auto* engineType = requireDescriptor (IDs::engineType))
    {
        check (engineType->kind == ParameterKind::choice, "engineType is a choice parameter");
        check (engineType->choices.joinIntoString ("|") == "NEURONiK|Neurotik",
               "engineType choices are NEURONiK|Neurotik");
        check (engineType->defaultChoiceIndex == 0, "engineType defaults to index 0");
    }

    if (const auto* cutoff = requireDescriptor (IDs::filterCutoff))
    {
        check (nearlyEqual (cutoff->minValue, 20.0f) && nearlyEqual (cutoff->maxValue, 20000.0f),
               "filterCutoff range is [20, 20000]");
        check (nearlyEqual (cutoff->defaultValue, 20000.0f), "filterCutoff default is 20000");
        check (nearlyEqual (cutoff->skew, 0.3f), "filterCutoff keeps its 0.3 skew");
    }

    if (const auto* division = requireDescriptor (IDs::fxDelayDivision))
    {
        check (division->choices.size() == 9, "fxDelayDivision exposes 9 rhythmic divisions");
        check (division->defaultChoiceIndex == 2, "fxDelayDivision defaults to 1/4");
    }

    if (const auto* unison = requireDescriptor (IDs::unisonEnabled))
    {
        check (unison->kind == ParameterKind::boolean, "unisonEnabled is a bool parameter");
        check (unison->choices.size() == 2, "bool descriptors expose Off|On choices");
    }

    if (const auto* resonance = requireDescriptor (IDs::resonatorRes))
        check (nearlyEqual (resonance->interval, 0.0f) && nearlyEqual (resonance->skew, 0.4f),
               "resonatorRes keeps its 0.4 skew");

    // --- 4. DSP implementation status -------------------------------------------
    std::cout << "\nDSP implementation status\n";

    int implementedCount = 0;
    int uiOnlyCount = 0;
    int notRoutedCount = 0;
    bool enginesConsistent = true;

    for (const auto& descriptor : descriptors)
    {
        switch (descriptor.dspStatus)
        {
            case ParameterDspStatus::uiOnly:    ++uiOnlyCount;    break;
            case ParameterDspStatus::notRouted: ++notRoutedCount; break;
            case ParameterDspStatus::implemented:
            default:                            ++implementedCount; break;
        }

        const bool statusIsImplemented = descriptor.dspStatus == ParameterDspStatus::implemented;
        const bool enginesAreNone = descriptor.engines == ParameterEngine::none;

        if (statusIsImplemented == enginesAreNone)
            enginesConsistent = false;
    }

    check (implementedCount == EXPECTED_IMPLEMENTED_COUNT,
           "implemented parameters: " + juce::String (implementedCount)
               + " (expected " + juce::String (EXPECTED_IMPLEMENTED_COUNT) + ")");
    check (uiOnlyCount == EXPECTED_UI_ONLY_COUNT,
           "uiOnly parameters: " + juce::String (uiOnlyCount)
               + " (expected " + juce::String (EXPECTED_UI_ONLY_COUNT) + ")");
    check (notRoutedCount == EXPECTED_NOT_ROUTED_COUNT,
           "notRouted parameters: " + juce::String (notRoutedCount)
               + " (expected " + juce::String (EXPECTED_NOT_ROUTED_COUNT) + ")");
    check (enginesConsistent,
           "implemented parameters name an engine and the rest report 'none'");

    int notesMissing = 0;
    for (const auto& descriptor : descriptors)
        if (descriptor.dspStatus != ParameterDspStatus::implemented && descriptor.dspNote.isEmpty())
            ++notesMissing;

    check (notesMissing == 0, "every divergent parameter explains itself through dspNote");

    if (const auto* cutoff = requireDescriptor (IDs::filterCutoff))
        check (cutoff->engines == ParameterEngine::neuronik,
               "filterCutoff is consumed only by the Neuronik path");

    if (const auto* resonator = requireDescriptor (IDs::resonatorRes))
        check (resonator->engines == ParameterEngine::neurotik,
               "resonatorRes is consumed only by the Neurotik path");

    if (const auto* level = requireDescriptor (IDs::masterLevel))
        check (level->engines == ParameterEngine::both,
               "masterLevel is consumed by both paths");

    // harmMix was retired rather than wired: it is gone from the layout, so the
    // contract must not offer it to any consumer.
    check (findParameterDescriptor ("harmMix") == nullptr,
           "the retired harmMix is no longer part of the contract");

    // oscPitchCoarse was retired on 2026-09-19 by the same rule as harmMix: it was a
    // promise nobody kept. Its pitch siblings in the old NexusParams draft were never
    // adopted either, and the pitch path the engine does have is the per-voice MPE bend.
    check (findParameterDescriptor ("oscPitchCoarse") == nullptr,
           "the retired oscPitchCoarse is no longer part of the contract");

    check (! getNotRoutedParameterIds().contains (IDs::velocityCurve)
               && ! getNotRoutedParameterIds().contains (IDs::midiThru),
           "velocityCurve and midiThru left the unrouted list");
    check (getNotRoutedParameterIds().size() == EXPECTED_NOT_ROUTED_COUNT,
           "the unrouted list matches the pinned count");

    if (const auto* freeze = requireDescriptor (IDs::freezeFilter))
        check (freeze->dspStatus == ParameterDspStatus::uiOnly,
               "freezeFilter is flagged as a panel action");

    if (const auto* mix = requireDescriptor (IDs::fxChorusMix))
        check (mix->engines == ParameterEngine::both, "fxChorusMix is shared by both engines");

    // Connected by the 2026-09-16 wiring pass: channel filtering, velocity
    // shaping, MIDI thru, tempo sync and the effect parameters that previously
    // only reached a mix value.
    if (const auto* channel = requireDescriptor (IDs::midiChannel))
    {
        check (channel->dspStatus == ParameterDspStatus::implemented,
               "midiChannel is now routed to the input filter");
        check (channel->engines == ParameterEngine::host,
               "midiChannel reports host coverage (no engine involved)");
    }

    if (const auto* velocity = requireDescriptor (IDs::velocityCurve))
    {
        check (velocity->dspStatus == ParameterDspStatus::implemented,
               "velocityCurve is now applied to incoming note-ons");
        check (velocity->engines == ParameterEngine::host,
               "velocityCurve reports host coverage (shaped on the MIDI buffer)");
    }

    if (const auto* thru = requireDescriptor (IDs::midiThru))
    {
        check (thru->dspStatus == ParameterDspStatus::implemented,
               "midiThru now gates the engine MIDI output");
        check (thru->engines == ParameterEngine::host,
               "midiThru reports host coverage (no engine involved)");
    }

    if (const auto* bpm = requireDescriptor (IDs::masterBPM))
        check (bpm->dspStatus == ParameterDspStatus::implemented,
               "masterBPM is now routed to the LFO and the delay");

    if (const auto* sync = requireDescriptor (IDs::lfo1SyncMode))
        check (sync->dspStatus == ParameterDspStatus::implemented,
               "lfo1SyncMode is now routed to the LFO");

    if (const auto* sync2 = requireDescriptor (IDs::lfo2RhythmicDivision))
        check (sync2->dspStatus == ParameterDspStatus::implemented,
               "lfo2RhythmicDivision is now routed to the LFO");

    if (const auto* delaySync = requireDescriptor (IDs::fxDelaySync))
        check (delaySync->engines == ParameterEngine::host,
               "fxDelaySync is resolved host side into delay seconds");

    if (const auto* chorusRate = requireDescriptor (IDs::fxChorusRate))
        check (chorusRate->dspStatus == ParameterDspStatus::implemented,
               "fxChorusRate now reaches the chorus");

    if (const auto* reverbSize = requireDescriptor (IDs::fxReverbSize))
        check (reverbSize->dspStatus == ParameterDspStatus::implemented,
               "fxReverbSize now reaches the reverb");

    if (const auto* unison = requireDescriptor (IDs::unisonEnabled))
        check (unison->dspStatus == ParameterDspStatus::notRouted,
               "unisonEnabled is still flagged as not routed (no consumer)");

    if (const auto* strength = requireDescriptor (IDs::randomStrength))
        check (strength->dspStatus == ParameterDspStatus::uiOnly,
               "randomStrength stays uiOnly: the panel randomise action reads it");

    // --- 5. Generated artifacts are in sync -------------------------------------
    std::cout << "\nGenerated artifacts\n";

   #if defined(NEURONIK_PARAMETER_ARTIFACTS_DIR)
    const juce::File artifactsDirectory (NEURONIK_PARAMETER_ARTIFACTS_DIR);

    const struct
    {
        const char* fileName;
        juce::String contents;
    } expected[] =
    {
        { ParameterArtifacts::jsonFileName, buildParameterArtifactsJson() },
        { ParameterArtifacts::javaScriptFileName, buildParameterArtifactsJavaScript() },
        { ParameterArtifacts::typeScriptFileName, buildParameterArtifactsTypeScript() }
    };

    // Line endings are ignored so the check survives CRLF checkouts.
    const auto stripLineEndings = [] (juce::String text)
    {
        return text.removeCharacters ("\r\n");
    };

    for (const auto& file : expected)
    {
        const auto target = artifactsDirectory.getChildFile (file.fileName);

        if (! target.existsAsFile())
        {
            check (false, juce::String (file.fileName)
                              + " is missing — run NEURONiK_ParameterExport "
                                "(see HANDOFF.md) to regenerate the contract artifacts");
            continue;
        }

        const auto onDisk = stripLineEndings (target.loadFileAsString());
        const auto fresh = stripLineEndings (file.contents);

        check (onDisk == fresh,
               juce::String (file.fileName) + " matches a fresh export");
    }
   #else
    std::cout << "  [skip] NEURONIK_PARAMETER_ARTIFACTS_DIR not defined\n";
   #endif

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
