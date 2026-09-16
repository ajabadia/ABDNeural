/*
  ==============================================================================

    PresetRoundTripTest.cpp
    Created: 16 Sep 2026
    Description: Preset save/load round trip, including the two cases that make
                 parameter retirement safe:

                 - a preset saved by an older build still carries the removed
                   `harmMix` (and could carry any future id): loading must apply
                   everything it still knows and ignore the rest;
                 - re-saving must drop those dead ids, so they do not travel
                   forward forever.

                 The test works on temporary files only. Constructing a
                 PresetManager has one side effect: it creates the real presets
                 directory in Documents/NEURONiK/Presets if it is missing.

  ==============================================================================
*/

#include "../Source/Serialization/PresetManager.h"
#include "../Source/Serialization/PresetMigration.h"
#include "../Source/State/ParameterDefinitions.h"
#include "../Source/State/ParameterDescriptors.h"

#include <iostream>

namespace
{
    using namespace NEURONiK;
    using namespace NEURONiK::State;

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

    void checkClose (double actual, double expected, const juce::String& description)
    {
        check (std::abs (actual - expected) < 1.0e-4,
               description + " (got " + juce::String (actual, 5) + ")");
    }

    /** Writes a real (not normalised) value through the parameter itself. */
    void setReal (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->getNormalisableRange().convertTo0to1 (value));
    }

    float readReal (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        const auto* value = apvts.getRawParameterValue (id);
        return value != nullptr ? value->load() : -1.0f;
    }

    /** Number of children carrying the given id attribute, in a preset file. */
    int countChildrenWithId (const juce::File& file, const juce::String& id)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr)
            return -1;

        int count = 0;
        for (int i = 0; i < xml->getNumChildElements(); ++i)
            if (xml->getChildElement (i)->getStringAttribute ("id") == id)
                ++count;

        return count;
    }

    int countParameterChildren (const juce::File& file)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr)
            return -1;

        int count = 0;
        for (int i = 0; i < xml->getNumChildElements(); ++i)
            if (xml->getChildElement (i)->hasTagName ("PARAM"))
                ++count;

        return count;
    }

    /**
     * Canonical text of every parameter value, so two states can be compared
     * exactly. Only PARAM children are included: file metadata is not plugin
     * state, and child order is irrelevant.
     */
    juce::String canonicalParameterState (juce::AudioProcessorValueTreeState& apvts)
    {
        const auto tree = apvts.copyState();
        juce::StringArray entries;

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild (i);
            if (! child.hasType ("PARAM"))
                continue;

            entries.add (child.getProperty ("id").toString()
                             + "=" + child.getProperty ("value").toString());
        }

        entries.sort (true);
        return entries.joinIntoString ("|");
    }

    int countMetadataChildren (const juce::File& file)
    {
        auto xml = juce::parseXML (file);
        if (xml == nullptr)
            return -1;

        int count = 0;
        for (int i = 0; i < xml->getNumChildElements(); ++i)
            if (xml->getChildElement (i)->hasTagName ("METADATA"))
                ++count;

        return count;
    }
}

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    auto layout = createLayoutApvts();
    auto& apvts = *layout.apvts;

    Serialization::PresetManager presetManager (apvts);

    auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("NEURONiK_PresetRoundTrip");
    directory.createDirectory();
    auto file = directory.getChildFile ("RoundTrip" + Serialization::PresetManager::presetExtension);

    std::cout << "NEURONiK preset round trip\n";

    // --- 1. Save, change, load ---------------------------------------------------
    std::cout << "\nRound trip\n";

    setReal (apvts, IDs::masterLevel, 0.42f);
    setReal (apvts, IDs::morphX, 0.33f);
    setReal (apvts, IDs::engineType, 1.0f);
    setReal (apvts, IDs::filterCutoff, 640.0f);
    setReal (apvts, IDs::lfo1SyncMode, 1.0f);
    setReal (apvts, IDs::midiChannel, 5.0f);

    // Retired from the panel, but still a live parameter: it has to keep round-tripping.
    setReal (apvts, IDs::unisonEnabled, 1.0f);

    presetManager.savePresetToFile (file);

    check (file.existsAsFile(), "the preset file was written");
    check (countChildrenWithId (file, "harmMix") == 0,
           "a fresh preset does not contain the retired harmMix");

    setReal (apvts, IDs::masterLevel, 0.1f);
    setReal (apvts, IDs::morphX, 0.9f);
    setReal (apvts, IDs::engineType, 0.0f);
    setReal (apvts, IDs::filterCutoff, 100.0f);
    setReal (apvts, IDs::lfo1SyncMode, 0.0f);
    setReal (apvts, IDs::midiChannel, 0.0f);
    setReal (apvts, IDs::unisonEnabled, 0.0f);

    presetManager.loadPresetFromFile (file);

    checkClose (readReal (apvts, IDs::masterLevel), 0.42, "masterLevel came back");
    checkClose (readReal (apvts, IDs::morphX), 0.33, "morphX came back");
    checkClose (readReal (apvts, IDs::engineType), 1.0, "engineType came back");
    checkClose (readReal (apvts, IDs::filterCutoff), 640.0, "filterCutoff came back (skewed range)");
    checkClose (readReal (apvts, IDs::lfo1SyncMode), 1.0, "lfo1SyncMode came back (choice)");
    checkClose (readReal (apvts, IDs::midiChannel), 5.0, "midiChannel came back (choice)");
    checkClose (readReal (apvts, IDs::unisonEnabled), 1.0,
                "unisonEnabled still round-trips although its control was removed");

    // --- 2. A preset from an older build ----------------------------------------
    std::cout << "\nLegacy preset with removed and unknown ids\n";

    auto legacyFile = directory.getChildFile ("Legacy" + Serialization::PresetManager::presetExtension);

    {
        // Build the file the way an old build would have: real parameters plus the
        // retired harmMix and a parameter that does not exist yet.
        setReal (apvts, IDs::masterLevel, 0.77f);
        setReal (apvts, IDs::filterRes, 0.6f);

        auto xml = apvts.copyState().createXml();
        check (xml != nullptr, "state can be serialised to XML");

        // "harmMix" as a literal on purpose: the id no longer exists in IDs::, which
        // is exactly the situation an old preset file is in.
        auto* retired = xml->createNewChildElement ("PARAM");
        retired->setAttribute ("id", "harmMix");
        retired->setAttribute ("value", "0.5");

        auto* future = xml->createNewChildElement ("PARAM");
        future->setAttribute ("id", "someFutureParameter");
        future->setAttribute ("value", "0.25");

        check (xml->writeTo (legacyFile), "the legacy preset file was written");
    }

    setReal (apvts, IDs::masterLevel, 0.2f);
    setReal (apvts, IDs::filterRes, 0.1f);

    presetManager.loadPresetFromFile (legacyFile);

    checkClose (readReal (apvts, IDs::masterLevel), 0.77, "known values of a legacy preset apply");
    checkClose (readReal (apvts, IDs::filterRes), 0.6, "every known value of a legacy preset applies");
    check (apvts.getParameter ("harmMix") == nullptr,
           "the removed harmMix is not resurrected by an old preset");
    check (apvts.getParameter ("someFutureParameter") == nullptr,
           "an unknown future id does not create a parameter");

    // Re-save the preset that was just loaded: the dead ids must not come back.
    presetManager.savePresetToFile (legacyFile);
    check (countChildrenWithId (legacyFile, "harmMix") == 0,
           "re-saving a legacy preset drops harmMix");
    check (countChildrenWithId (legacyFile, "someFutureParameter") == 0,
           "re-saving a legacy preset drops unknown ids");

    // --- 3. A legacy preset is sound-neutral ------------------------------------
    //
    // The audible part of "an old preset still sounds the same" is one claim per
    // parameter, so it is asserted rather than listened to: the retired harmMix
    // child must not change a single parameter value.
    std::cout << "\nLegacy harmMix child is sound-neutral\n";

    auto cleanFile = directory.getChildFile ("Clean" + Serialization::PresetManager::presetExtension);
    auto legacyHarmFile = directory.getChildFile ("LegacyHarm" + Serialization::PresetManager::presetExtension);

    setReal (apvts, IDs::masterLevel, 0.55f);
    setReal (apvts, IDs::morphY, 0.4f);
    setReal (apvts, IDs::envAttack, 0.2f);
    setReal (apvts, IDs::fxReverbMix, 0.3f);
    presetManager.savePresetToFile (cleanFile);

    {
        // Same values, plus the child an older build would have written.
        auto xml = apvts.copyState().createXml();
        auto* retired = xml->createNewChildElement ("PARAM");
        retired->setAttribute ("id", "harmMix");
        retired->setAttribute ("value", "0.5");
        check (xml->writeTo (legacyHarmFile), "the legacy-harm preset was written");
    }

    check (countParameterChildren (legacyHarmFile) == countParameterChildren (cleanFile) + 1,
           "the two files differ only by the extra child");

    presetManager.loadPresetFromFile (cleanFile);
    const auto stateWithoutHarmMix = canonicalParameterState (apvts);

    presetManager.loadPresetFromFile (legacyHarmFile);
    const auto stateWithHarmMix = canonicalParameterState (apvts);

    check (stateWithoutHarmMix.isNotEmpty(), "the canonical state is not empty");
    check (stateWithHarmMix == stateWithoutHarmMix,
           "loading a preset with harmMix yields exactly the same parameter state");
    check (! stateWithHarmMix.contains ("harmMix"),
           "no harmMix entry survives in the loaded state");

    // --- 4. Metadata lives in the file, not in the plugin state -----------------
    std::cout << "\nFile metadata\n";

    presetManager.setTagsForPreset (legacyFile, { "bass", "legacy" });
    presetManager.loadPresetFromFile (legacyFile);
    presetManager.savePresetToFile (legacyFile);

    check (countMetadataChildren (legacyFile) == 1,
           "re-saving keeps a single METADATA element (no duplication)");
    check (presetManager.getTagsForPreset (legacyFile).size() == 2,
           "the tags survived the load/save cycle");

    // --- 5. Degenerate inputs ---------------------------------------------------
    std::cout << "\nDegenerate inputs\n";

    {
        auto missing = directory.getChildFile ("DoesNotExist" + Serialization::PresetManager::presetExtension);
        const auto before = readReal (apvts, IDs::masterLevel);
        presetManager.loadPresetFromFile (missing);
        checkClose (readReal (apvts, IDs::masterLevel), before,
                    "loading a missing file changes nothing and does not crash");
    }

    {
        auto malformed = directory.getChildFile ("Malformed" + Serialization::PresetManager::presetExtension);
        malformed.replaceWithText ("this is not xml at all");
        const auto before = readReal (apvts, IDs::masterLevel);
        presetManager.loadPresetFromFile (malformed);
        checkClose (readReal (apvts, IDs::masterLevel), before,
                    "a malformed preset is ignored instead of wiping the state");
    }

    {
        // A preset that is valid XML but has no parameter children at all. APVTS
        // resets parameters whose child is missing to their default, which is the
        // behaviour a partial preset from an older build relies on.
        setReal (apvts, IDs::masterLevel, 0.15f);
        setReal (apvts, IDs::filterRes, 0.85f);

        auto empty = directory.getChildFile ("Empty" + Serialization::PresetManager::presetExtension);
        juce::XmlElement root ("NEURONiK");
        root.writeTo (empty);
        presetManager.loadPresetFromFile (empty);

        const auto* levelDefault = findParameterDescriptor (IDs::masterLevel);
        const auto* resonanceDefault = findParameterDescriptor (IDs::filterRes);

        checkClose (readReal (apvts, IDs::masterLevel),
                    levelDefault != nullptr ? levelDefault->defaultValue : 0.8,
                    "a missing parameter falls back to its default instead of keeping stale values");
        checkClose (readReal (apvts, IDs::filterRes),
                    resonanceDefault != nullptr ? resonanceDefault->defaultValue : 0.0,
                    "the whole missing set falls back to defaults, not just one parameter");
    }

    // --- 6. Migration is a no-op for a current preset ----------------------------
    std::cout << "\nMigration helper\n";

    {
        auto current = apvts.copyState();
        const int removed = Serialization::migratePresetState (current, *layout.processor);
        check (removed == 0, "migrating an up to date state removes nothing");

        auto legacy = apvts.copyState();
        juce::ValueTree retired ("PARAM");
        retired.setProperty ("id", "harmMix", nullptr);
        retired.setProperty ("value", 0.5, nullptr);
        legacy.appendChild (retired, nullptr);

        const int removedAgain = Serialization::migratePresetState (legacy, *layout.processor);
        check (removedAgain == 1, "migrating a legacy state removes exactly the dead id");
        check (Serialization::currentParameterIds (*layout.processor).size() > 0,
               "the processor reports its current parameter ids");
    }

    directory.deleteRecursively();

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
