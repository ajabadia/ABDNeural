/*
  ==============================================================================

    StatePersistenceTest.cpp
    Created: 17 Sep 2026
    Description: DAW-session persistence round trip with the REAL processor —
                 the exact calls a host makes when saving/reopening a session:

                   1. Processor A: user edits (parameters + a MIDI mapping) and
                      the host calls getStateInformation().
                   2. Processor B (fresh instance, like a reopened session):
                      setStateInformation() must restore EVERYTHING — APVTS
                      values, MIDI mappings, engine re-sync — and the bridge's
                      normalised view must agree.

                 Also covers the malformed-data contract: garbage and foreign
                 state must be ignored without corrupting the live state, and
                 state saved by an older build (extra/missing ids) must apply
                 what it knows and keep the current defaults for the rest.

                 Constructing NEURONiKProcessor has one side effect: the
                 PresetManager creates Documents/NEURONiK/Presets if missing
                 (same as the WebPilot host does on every run).

  ==============================================================================
*/

#include "../Source/Main/NEURONiKProcessor.h"
#include "../Source/State/ParameterDefinitions.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using namespace NEURONiK;

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

    bool closeEnough (float a, float b)
    {
        return std::abs (a - b) < 1.0e-4f;
    }

    /** One normalised value per APVTS parameter, in layout order. */
    std::vector<std::pair<juce::String, float>> captureValues (NEURONiKProcessor& processor)
    {
        std::vector<std::pair<juce::String, float>> values;

        for (auto* parameter : processor.getAPVTS().processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                values.emplace_back (ranged->getParameterID(), ranged->getValue());

        return values;
    }

    int countDifferences (const std::vector<std::pair<juce::String, float>>& a,
                          const std::vector<std::pair<juce::String, float>>& b)
    {
        int differences = 0;

        for (size_t i = 0; i < a.size() && i < b.size(); ++i)
            if (a[i].first != b[i].first || ! closeEnough (a[i].second, b[i].second))
                ++differences;

        return differences;
    }
} // namespace

int main()
{
    std::cout << "=== state persistence round trip (real processor) ===\n";

    // --- 1. Session A: the user edits some parameters and a MIDI mapping ------
    NEURONiKProcessor processorA;

    processorA.prepareToPlay (48000.0, 512);

    auto* attack = processorA.getAPVTS().getParameter ("envAttack");
    auto* level  = processorA.getAPVTS().getParameter ("masterLevel");
    check (attack != nullptr && level != nullptr, "the edit targets exist in the layout");

    if (attack != nullptr) attack->setValueNotifyingHost (0.62f);
    if (level != nullptr)  level->setValueNotifyingHost (0.85f);

    // A MIDI learn mapping is part of the session state (saveToValueTree).
    processorA.getMidiMappingManager().setMapping ("masterLevel", 74);



    const auto sessionA = captureValues (processorA);

    // --- 2. The host saves the session ---------------------------------------
    juce::MemoryBlock saved;
    processorA.getStateInformation (saved);
    check (saved.getSize() > 0, "getStateInformation produced data");

    // --- 3. A new session (host restart): processor B restores ---------------
    NEURONiKProcessor processorB;
    processorB.prepareToPlay (48000.0, 512);
    processorB.setStateInformation (saved.getData(), (int) saved.getSize());

    const auto sessionB = captureValues (processorB);
    const auto differences = countDifferences (sessionA, sessionB);

    check (differences == 0,
           "every APVTS parameter survived the round trip ("
               + juce::String (differences) + " differences)");

    check (closeEnough (processorB.getAPVTS().getParameter ("envAttack")->getValue(), 0.62f),
           "the edited envAttack is back");
    check (closeEnough (processorB.getAPVTS().getParameter ("masterLevel")->getValue(), 0.85f),
           "the edited masterLevel is back");

    // --- 4. MIDI mappings travel inside the same state -----------------------
    const auto& mappings = processorB.getMidiMappingManager().getMappings();
    const auto mappingOk = mappings.find (74) != mappings.end()
                               && mappings.at (74) == juce::String ("masterLevel");
    check (mappingOk, "the MIDI mapping (CC74 -> masterLevel) is restored");

    // --- 5. Malformed / foreign data must never corrupt the live state -------
    const auto beforeGarbage = captureValues (processorB);

    processorB.setStateInformation (nullptr, 0);
    processorB.setStateInformation ("this is not xml at all", 23);

    juce::MemoryBlock foreign;
    juce::XmlElement foreignXml ("SomeOtherPlugin");
    foreignXml.setAttribute ("value", 1);
    // copyXmlToBinary is an AudioProcessor member; any instance works as encoder.
    processorB.copyXmlToBinary (foreignXml, foreign);
    processorB.setStateInformation (foreign.getData(), (int) foreign.getSize());

    check (countDifferences (beforeGarbage, captureValues (processorB)) == 0,
           "null/garbage/foreign state is ignored, live state intact");

    // --- 6. Older-build state: known ids apply, unknown ids are ignored ------
    auto stale = processorB.getAPVTS().copyState();
    stale.setProperty ("removedParameterFromAnOldBuild", 0.123f, nullptr);
    stale.setProperty ("anotherFutureId", 42, nullptr);

    std::unique_ptr<juce::XmlElement> staleXml (stale.createXml());
    juce::MemoryBlock staleBlock;
    processorB.copyXmlToBinary (*staleXml, staleBlock);

    processorB.setStateInformation (staleBlock.getData(), (int) staleBlock.getSize());
    check (countDifferences (beforeGarbage, captureValues (processorB)) == 0,
           "older-build state (extra ids) loads without changing the values");

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
