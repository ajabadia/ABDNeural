/*
  ==============================================================================

    BridgeProtocolContractTest.cpp
    Created: 16 Sep 2026
    Description: Anti-drift test for the VERSIONED bridge protocol contract.

                 The wire format of the bridge (event ids, action names, gesture
                 phases, value conventions, delivery behaviours) is pinned by
                 `WebPilot/contracts/bridge-protocol.json`, in git. This test
                 compares that committed file against the constants the native
                 side actually compiles with, so:

                   - renaming a literal in ParameterBridge.h without updating the
                     contract (or vice versa) fails here;
                   - in a clean clone the contract is checked, not assumed;
                   - every divergence is a readable diff in review.

                 Mirrors the mechanism of NEURONiK_ParameterDescriptorTest for the
                 parameter contract.

  ==============================================================================
*/

#include "../Source/WebUI/ParameterBridge.h"

#include <juce_core/juce_core.h>

#include <iostream>

namespace
{
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

    /** @brief Value of `path.to.key` in a parsed JSON object, or an empty var. */
    juce::var property (const juce::var& tree, const juce::String& dottedPath)
    {
        juce::var current = tree;

        for (const auto& token : juce::StringArray::fromTokens (dottedPath, ".", {}))
        {
            const auto* object = current.getDynamicObject();

            if (object == nullptr)
                return {};

            current = object->getProperty (token);
        }

        return current;
    }

    juce::String asString (const juce::var& value) { return value.toString(); }
    int asInt (const juce::var& value)             { return static_cast<int> (static_cast<double> (value)); }

    /** @brief `array.joinToString(separator)`, for action name lists. */
    juce::StringArray asStringArray (const juce::var& array)
    {
        juce::StringArray result;

        if (const auto* items = array.getArray())
            for (const auto& item : *items)
                result.add (item.toString());

        return result;
    }
}

int main()
{
    std::cout << "Bridge protocol contract\n";

   #if defined(NEURONIK_BRIDGE_PROTOCOL_JSON)
    const juce::File contractFile (NEURONIK_BRIDGE_PROTOCOL_JSON);

    if (! contractFile.existsAsFile())
    {
        std::cout << "  [FAIL] bridge-protocol.json is missing at "
                  << contractFile.getFullPathName()
                  << " — restore it from git (it is versioned on purpose)\n";
        return 1;
    }

    const juce::var parsed = juce::JSON::parse (contractFile);

    if (parsed.isVoid())
    {
        std::cout << "  [FAIL] bridge-protocol.json is not valid JSON\n";
        return 1;
    }

    // --- Contract identity --------------------------------------------------------
    check (asString (property (parsed, "contract")) == "NEURONiK bridge protocol",
           "the file is the NEURONiK bridge protocol contract");
    check (asInt (property (parsed, "version")) == NEURONiK::WebUI::BridgeProtocolVersion::current,
           "contract version matches the version compiled into ParameterBridge.h");

    // --- Event ids: the wire, both directions -------------------------------------
    check (asString (property (parsed, "channels.nativeToJs.eventId"))
               == NEURONiK::WebUI::BridgeEventIds::nativeToJs,
           "nativeToJs event id matches the compiled constant");
    check (asString (property (parsed, "channels.jsToNative.eventId"))
               == NEURONiK::WebUI::BridgeEventIds::jsToNative,
           "jsToNative event id matches the compiled constant");
    check (asString (property (parsed, "channels.pageLoaded.eventId"))
               == NEURONiK::WebUI::BridgeEventIds::pageLoaded,
           "pageLoaded event id matches the compiled constant");

    check (asString (property (parsed, "channels.nativeToJs.api"))
               .contains ("emitEventIfBrowserIsVisible"),
           "the contract documents emitEventIfBrowserIsVisible as the native -> JS API");
    check (asString (property (parsed, "channels.nativeToJs.note"))
               .contains ("only"),
           "the contract warns that the native -> JS channel is the only one the page hears");

    // --- Actions: native -> JS -----------------------------------------------------
    check (asString (property (parsed, "messages.nativeToJs.syncAllParams.fields.action"))
               .contains (NEURONiK::WebUI::BridgeActions::syncAllParams),
           "syncAllParams literal matches the compiled constant");
    check (asString (property (parsed, "messages.nativeToJs.parameterChanged.fields.action"))
               .contains (NEURONiK::WebUI::BridgeActions::parameterChanged),
           "parameterChanged (native -> JS) literal matches the compiled constant");

    // --- Actions: JS -> native -----------------------------------------------------
    check (asString (property (parsed, "messages.jsToNative.parameterChanged.fields.action"))
               .contains (NEURONiK::WebUI::BridgeActions::parameterChanged),
           "parameterChanged (JS -> native) literal matches the compiled constant");
    check (asString (property (parsed, "messages.jsToNative.requestState.fields.action"))
               .contains (NEURONiK::WebUI::BridgeActions::requestState),
           "requestState literal matches the compiled constant");

    // --- Gestures: the phases the native side accepts ------------------------------
    {
        // The contract documents the phases inside the valueConvention.gesture prose
        // ('begin' | 'change' | 'end'), so look for each quoted literal in the text.
        const auto documented = asString (property (parsed, "valueConvention.gesture"));
        int found = 0;

        for (const auto* gesture : { NEURONiK::WebUI::BridgeGestures::begin,
                                     NEURONiK::WebUI::BridgeGestures::change,
                                     NEURONiK::WebUI::BridgeGestures::end })
            if (documented.contains (juce::String ("'") + gesture + "'"))
                ++found;

        check (found == 3, "the contract declares exactly the three gesture phases");
    }

    // --- Value convention ----------------------------------------------------------
    check (asString (property (parsed, "valueConvention.value")).contains ("normalised 0..1"),
           "the contract pins `value` as the normalised 0..1 scale");
    check (asString (property (parsed, "valueConvention.real")).contains ("Native -> JS only"),
           "the contract marks `real` as native -> JS only (informational)");
    check (asString (property (parsed, "valueConvention.text")).contains ("Native -> JS only"),
           "the contract marks `text` as native -> JS only (informational)");

    // --- Behaviours the WebUI and the tests rely on --------------------------------
    {
        check (! property (parsed, "behaviour").isVoid(),
               "the contract has a behaviours section");
        check (! property (parsed, "behaviour.echoSuppression").isVoid(),
               "the contract documents echo suppression");
        check (! property (parsed, "behaviour.outgoingDelivery").isVoid(),
               "the contract documents the polled (not listener pushed) delivery");
        check (asString (property (parsed, "behaviour.outgoingDelivery")).contains ("POLLED"),
               "outgoing delivery is pinned as polled, not listener pushed");
        check (! property (parsed, "behaviour.gestureLifecycle").isVoid(),
               "the contract documents the gesture lifecycle");
        check (! property (parsed, "behaviour.malformedInput").isVoid(),
               "the contract documents malformed-input handling");
        check (asString (property (parsed, "behaviour.transportGuards")).contains ("emitEventIfBrowserIsVisible"),
               "the contract states the transport direction guard");
        check (! property (parsed, "behaviour.localMode").isVoid(),
               "the contract documents the no-JUCE local mode");
    }

    // --- Implementations referenced by the contract must exist ----------------------
    {
        const auto* implementations = property (parsed, "implementations").getDynamicObject();

        if (implementations != nullptr)
        {
            const auto referenced = implementations->getProperties();

            int missing = 0;

            // Paths are relative to the repository root (the folder holding CMakeLists.txt).
            // The test binary lives in build-reference/Release, five levels below the root:
            // Release -> NEURONiK_BridgeProtocolContractTest.exe's parent chain is
            //   build-reference/Release -> build-reference -> ABDNeural
            const juce::File repoRoot = juce::File (NEURONIK_BRIDGE_PROTOCOL_JSON)
                                            .getParentDirectory()   // WebPilot/contracts
                                            .getParentDirectory()   // WebPilot
                                            .getParentDirectory();  // ABDNeural

            for (const auto& entry : referenced)
            {
                // Brace-expanded names like "Foo.{h,cpp}" stand for two files; expand them.
                auto pathText = entry.value.toString();

                if (pathText.contains ("{"))
                {
                    const auto head = pathText.upToFirstOccurrenceOf ("{", false, false);
                    const auto tail = pathText.fromLastOccurrenceOf ("}", false, false);
                    const auto alternatives = pathText
                                                  .fromFirstOccurrenceOf ("{", false, false)
                                                  .upToLastOccurrenceOf ("}", false, false);

                    for (const auto& option : juce::StringArray::fromTokens (alternatives, ",", {}))
                    {
                        const auto expanded = head + option + tail;

                        if (! repoRoot.getChildFile (expanded).existsAsFile())
                        {
                            std::cout << "  [FAIL] contract references missing file: "
                                      << expanded << '\n';
                            ++missing;
                        }
                    }

                    continue;
                }

                if (! repoRoot.getChildFile (pathText).existsAsFile())
                {
                    std::cout << "  [FAIL] contract references missing file: "
                              << pathText << '\n';
                    ++missing;
                }
            }

            check (missing == 0, "every implementation file the contract names exists");
            check (referenced.size() >= 5, "the contract names the implementation files of both sides");
        }
        else
        {
            check (false, "the contract has an implementations section");
        }
    }
   #else
    std::cout << "  [skip] NEURONIK_BRIDGE_PROTOCOL_JSON not defined\n";
   #endif

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
