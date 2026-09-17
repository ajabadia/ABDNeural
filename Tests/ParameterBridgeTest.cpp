/*
  ==============================================================================

    ParameterBridgeTest.cpp
    Created: 16 Sep 2026
    Description: Tests for the WebUI parameter bridge, with a real APVTS (the one
                 createLayoutApvts() builds from the plugin's own layout) and a
                 recording lambda standing in for the WebView2 transport.

                 What it pins down:

                 - the mirrored set is the layout, so the bridge can never drift
                   from the parameter contract the WebUI is generated from;
                 - the wire carries normalised values, plus real/text for display;
                 - incoming changes are clamped, unknown ids ignored and malformed
                   messages counted instead of throwing;
                 - a change from the page is never echoed back to it;
                 - an external (native) change produces exactly one message per
                   parameter, and no repeat when nothing moved;
                 - gestures always end, including when the page reloads mid drag.

  ==============================================================================
*/

#include "../Source/State/ParameterDefinitions.h"
#include "../Source/State/ParameterDescriptors.h"
#include "../Source/WebUI/ParameterBridge.h"

#include <cmath>
#include <iostream>

namespace
{
    using namespace NEURONiK;
    using namespace NEURONiK::WebUI;
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
               description + " (got " + juce::String (actual, 6) + ")");
    }

    //==============================================================================
    // Messages, written the way each side writes them

    /** @brief `{ action: <name> }`, optionally with more properties. */
    juce::var jsAction (const juce::String& action, const juce::String& id = {},
                        juce::var value = {}, const juce::String& gesture = {})
    {
        juce::DynamicObject::Ptr message = new juce::DynamicObject();
        message->setProperty ("action", action);

        if (id.isNotEmpty())
            message->setProperty ("id", id);

        if (! value.isVoid())
            message->setProperty ("value", value);

        if (gesture.isNotEmpty())
            message->setProperty ("gesture", gesture);

        return juce::var (message.get());
    }

    /** @brief A well formed change, the only shape the WebUI is supposed to send. */
    juce::var jsChange (const juce::String& id, double normalisedValue, const juce::String& gesture = {})
    {
        return jsAction (BridgeActions::parameterChanged, id, juce::var (normalisedValue), gesture);
    }

    /** @brief Collects the messages the bridge hands to the transport. */
    struct Recorder
    {
        std::vector<juce::var> messages;

        ParameterBridge::Sender sender()
        {
            return [this] (const juce::var& message) { messages.push_back (message); };
        }

        /** @brief Messages whose action matches, in arrival order. */
        std::vector<juce::var> withAction (const juce::String& action) const
        {
            std::vector<juce::var> found;

            for (const auto& message : messages)
                if (const auto* object = message.getDynamicObject();
                    object != nullptr && object->getProperty ("action").toString() == action)
                    found.push_back (message);

            return found;
        }
    };

    /** @brief The entry of a snapshot message for one id, or an empty var. */
    juce::var findSnapshotEntry (const juce::var& snapshot, const juce::String& id)
    {
        const auto* object = snapshot.getDynamicObject();

        if (object == nullptr)
            return {};

        const auto* parameters = object->getProperty ("parameters").getArray();

        if (parameters == nullptr)
            return {};

        for (const auto& entry : *parameters)
            if (const auto* entryObject = entry.getDynamicObject();
                entryObject != nullptr && entryObject->getProperty ("id").toString() == id)
                return entry;

        return {};
    }

    /** @brief The `value` (normalised) a wire message carries, or -1 when absent. */
    double wireValue (const juce::var& message)
    {
        const auto* object = message.getDynamicObject();

        if (object == nullptr || ! object->getProperty ("value").isDouble())
            return -1.0;

        return static_cast<double> (object->getProperty ("value"));
    }

    /** @brief Moves a parameter the way the native editor would. */
    void moveFromNative (juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float normalised)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (normalised);
    }
}

int main()
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    auto layout = State::createLayoutApvts();
    auto& apvts = *layout.apvts;

    ParameterBridge bridge (apvts);
    Recorder recorder;
    bridge.setSender (recorder.sender());

    // --- 1. The mirrored set is the layout ---------------------------------------
    std::cout << "Mirrored set\n";

    const auto descriptors = State::getParameterDescriptors();

    check (bridge.getParameterCount() == static_cast<int> (descriptors.size()),
           "the bridge mirrors every parameter of the layout ("
               + juce::String (bridge.getParameterCount()) + " vs "
               + juce::String (static_cast<int> (descriptors.size())) + " descriptors)");

    {
        int matched = 0;

        for (const auto& descriptor : descriptors)
        {
            const auto index = bridge.indexOfParameter (descriptor.id);

            if (index >= 0 && bridge.getParameterId (index) == descriptor.id)
                ++matched;
        }

        check (matched == static_cast<int> (descriptors.size()),
               "every contract descriptor has a mirrored parameter under the same id ("
                   + juce::String (matched) + " matched)");

        check (bridge.indexOfParameter ("harmMix") < 0,
               "a retired id is not mirrored any more");
        check (bridge.getParameterId (-1).isEmpty() && bridge.getParameterId (9999).isEmpty(),
               "out of range index lookups are empty instead of undefined");
    }

    // --- 2. The snapshot ---------------------------------------------------------
    std::cout << "\nSnapshot\n";

    apvts.getParameter (IDs::masterLevel)->setValueNotifyingHost (0.8f);
    apvts.getParameter (IDs::filterCutoff)->setValueNotifyingHost (0.5f);
    recorder.messages.clear();

    bridge.sendFullSnapshot();

    const auto snapshots = recorder.withAction (BridgeActions::syncAllParams);
    check (snapshots.size() == 1, "a full snapshot is one message");

    const auto snapshot = snapshots.empty() ? juce::var{} : snapshots.front();

    if (const auto* object = snapshot.getDynamicObject())
    {
        check (static_cast<int> (object->getProperty ("parameterCount")) == bridge.getParameterCount(),
               "the snapshot declares how many parameters it carries");
    }
    else
    {
        check (false, "the snapshot is an object");
    }

    const auto master = findSnapshotEntry (snapshot, IDs::masterLevel);
    const auto* masterObject = master.getDynamicObject();

    check (masterObject != nullptr, "the snapshot carries the requested parameter");
    check (masterObject != nullptr && masterObject->getProperty ("value").isDouble(),
           "values travel as numbers");
    check (masterObject != nullptr && masterObject->getProperty ("text").toString().isNotEmpty(),
           "every entry carries the display text the APVTS formats");
    checkClose (wireValue (master), 0.8, "masterLevel travels as the normalised value");

    // The reason both scales travel: a range that is not 0..1 must not be mistaken
    // for a normalised one.
    const auto cutoff = findSnapshotEntry (snapshot, IDs::filterCutoff);
    const auto* cutoffObject = cutoff.getDynamicObject();

    check (cutoffObject != nullptr, "the skewed parameter is in the snapshot");

    if (cutoffObject != nullptr)
    {
        checkClose (wireValue (cutoff), 0.5, "a skewed parameter travels normalised");
        check (static_cast<double> (cutoffObject->getProperty ("real")) > 1.0,
               "the same entry also carries the real value, so the page never guesses the scale");
    }

    check (bridge.publishPendingChanges() == 0,
           "a snapshot marks everything as reported, so the poller stays silent");

    recorder.messages.clear();
    bridge.sendFullSnapshot();

    check (recorder.withAction (BridgeActions::syncAllParams).size() == 1,
           "a second snapshot is sent on demand");
    check (recorder.messages.size() == 1,
           "snapshots do not also emit per parameter changes");

    // --- 3. JS -> native ---------------------------------------------------------
    std::cout << "\nIncoming changes\n";

    bridge.resetStats();
    recorder.messages.clear();

    bridge.handleJsEvent (jsChange (IDs::masterLevel, 0.25));

    checkClose (apvts.getParameter (IDs::masterLevel)->getValue(), 0.25,
                "a JS change reaches the APVTS");
    check (bridge.getStats().appliedFromJs == 1, "the applied change is counted");
    check (bridge.publishPendingChanges() == 0,
           "the value the page just sent is not echoed back to it");
    check (recorder.messages.empty(), "and nothing was sent at all");

    bridge.handleJsEvent (jsChange (IDs::masterLevel, 0.25));
    check (bridge.getStats().idleChanges == 1,
           "setting a parameter to the value it already had is counted as idle");

    bridge.handleJsEvent (jsChange (IDs::masterLevel, 1.5));
    checkClose (apvts.getParameter (IDs::masterLevel)->getValue(), 1.0,
                "an out of range value is clamped to the top of the range");

    bridge.handleJsEvent (jsChange (IDs::masterLevel, -0.5));
    checkClose (apvts.getParameter (IDs::masterLevel)->getValue(), 0.0,
                "an out of range value is clamped to the bottom of the range");

    // --- 4. Anything the page can get wrong --------------------------------------
    std::cout << "\nMalformed input\n";

    bridge.resetStats();
    const auto untouched = apvts.getParameter (IDs::engineType)->getValue();

    bridge.handleJsEvent (juce::var (42));                                   // not an object
    bridge.handleJsEvent (juce::var{});                                       // void
    bridge.handleJsEvent (jsAction ("nonsense"));                             // unknown action
    bridge.handleJsEvent (jsAction (BridgeActions::parameterChanged));        // no id
    bridge.handleJsEvent (jsAction (BridgeActions::parameterChanged, IDs::engineType,
                                    juce::var ("half way")));                 // value is a string
    bridge.handleJsEvent (jsChange (IDs::engineType, 1.0, "wiggle"));         // unknown gesture
    bridge.handleJsEvent (jsChange ("noSuchParameter", 0.5));                 // id not in the layout

    check (bridge.getStats().rejectedMessages == 6,
           "six malformed messages rejected, counted rather than thrown ("
               + juce::String (bridge.getStats().rejectedMessages) + ")");
    check (bridge.getStats().unknownIds == 1, "the unknown id is counted separately");
    check (bridge.getStats().appliedFromJs == 0 && bridge.getStats().idleChanges == 0,
           "and none of them reached a parameter");
    checkClose (apvts.getParameter (IDs::engineType)->getValue(), untouched,
                "the parameter they mentioned is still where it was");

    // --- 5. Native -> JS ---------------------------------------------------------
    std::cout << "\nOutgoing changes\n";

    bridge.resetStats();
    recorder.messages.clear();

    moveFromNative (apvts, IDs::filterCutoff, 0.75);

    check (bridge.publishPendingChanges() == 1,
           "a change made by the native side is published on the next poll");

    {
        const auto changes = recorder.withAction (BridgeActions::parameterChanged);

        check (changes.size() == 1, "one message per parameter, not one per poll");
        check (! changes.empty() && changes.front().getDynamicObject() != nullptr
                   && changes.front().getDynamicObject()->getProperty ("id").toString() == IDs::filterCutoff,
               "the message names the parameter that moved");
        checkClose (changes.empty() ? -1.0 : wireValue (changes.front()), 0.75,
                    "and carries its new value");
    }

    check (bridge.publishPendingChanges() == 0,
           "polling again with no movement sends nothing (no duplicate events)");

    moveFromNative (apvts, IDs::masterLevel, 0.6);
    moveFromNative (apvts, IDs::engineType, 1.0);
    recorder.messages.clear();

    check (bridge.publishPendingChanges() == 2,
           "two parameters moving in the same tick produce two messages, one each");

    // --- 6. Gestures -------------------------------------------------------------
    std::cout << "\nGestures\n";

    bridge.resetStats();
    recorder.messages.clear();

    bridge.handleJsEvent (jsChange (IDs::masterLevel, 0.3, BridgeGestures::begin));
    bridge.handleJsEvent (jsChange (IDs::masterLevel, 0.4));
    bridge.handleJsEvent (jsChange (IDs::masterLevel, 0.5, BridgeGestures::end));

    check (bridge.getStats().gesturesOpened == 1, "the drag opened exactly one gesture");
    check (bridge.getStats().gesturesClosed == 1, "and closed exactly one");
    checkClose (apvts.getParameter (IDs::masterLevel)->getValue(), 0.5,
                "every phase of the drag reached the parameter");
    check (bridge.closeOpenGestures() == 0, "nothing is left open after a complete drag");

    // A page that reloads mid drag can never send the matching end.
    bridge.handleJsEvent (jsChange (IDs::masterLevel, 0.7, BridgeGestures::begin));
    check (bridge.closeOpenGestures() == 1,
           "a reload closes the gesture the page left open");
    check (bridge.getStats().gesturesClosed == 2, "the forced close is counted");
    check (bridge.closeOpenGestures() == 0, "closing twice is harmless");

    // An end without a begin is tolerated: the value still lands, nothing phantom.
    bridge.resetStats();
    bridge.handleJsEvent (jsChange (IDs::filterCutoff, 0.2, BridgeGestures::end));

    check (bridge.getStats().gesturesOpened == 0 && bridge.getStats().gesturesClosed == 0,
           "an end without a begin neither opens nor closes a gesture");
    checkClose (apvts.getParameter (IDs::filterCutoff)->getValue(), 0.2,
                "and still writes the value");

    // --- 7. Requests and transport states ---------------------------------------
    std::cout << "\nRequests and transport\n";

    bridge.resetStats();
    recorder.messages.clear();

    bridge.handleJsEvent (jsAction (BridgeActions::requestState));

    check (bridge.getStats().snapshotsSent == 1,
           "requestState answers with a snapshot (the page asks for it on mount)");
    check (bridge.getStats().rejectedMessages == 0, "and is not counted as an error");

    bridge.setSender ({});
    check (! bridge.hasSender(), "the transport can be removed");

    const auto beforeDetach = bridge.getStats();
    bridge.sendFullSnapshot();
    bridge.publishPendingChanges();
    const auto afterDetach = bridge.getStats();

    check (afterDetach.snapshotsSent == beforeDetach.snapshotsSent
               && afterDetach.changesSent == beforeDetach.changesSent,
           "with no transport nothing counts as sent (the page is not loaded yet)");

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
