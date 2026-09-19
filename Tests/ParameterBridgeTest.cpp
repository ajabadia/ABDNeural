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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

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

    /** @brief Preset backend stub for the preset-message section. */
    struct FakePresetController final : public PresetController
    {
        juce::StringArray names { "Init Preset", "Glass Bells" };
        juce::String current = "Init Preset";
        bool failLoads = false;
        bool failSaves = false;
        int loads = 0;
        int saves = 0;

        [[nodiscard]] juce::StringArray listPresets() const override { return names; }

        bool loadPreset (const juce::String& name) override
        {
            if (failLoads || ! names.contains (name))
                return false;

            ++loads;
            current = name;
            return true;
        }

        bool savePreset (const juce::String& name) override
        {
            if (failSaves || name.isEmpty() || name.containsAnyOf ("/\\:*?\"<>|")
                || name.contains (".."))
                return false;

            ++saves;

            if (! names.contains (name))
                names.add (name);

            current = name;
            return true;
        }

        [[nodiscard]] juce::String getCurrentPreset() const override { return current; }
    };

    /** @brief Spectral-model backend stub for the modelsState/loadModel sections. */
    struct FakeModelController final : public NativeModelController
    {
        int numSlots = 4;
        float amp0 = 0.5f;
        float freq0 = 0.25f;
        bool valid = true;
        /** Display names, one per slot: what the page shows next to A..D. */
        juce::StringArray slotNames { "EMPTY", "EMPTY", "EMPTY", "EMPTY" };
        /** Slots the bridge asked to load, in request order. */
        std::vector<int> loads;

        int getNumModelSlots() const override { return numSlots; }

        void getCurrentModel (int, std::array<float, 64>& amplitudes,
                              std::array<float, 64>& frequencyOffsets,
                              bool& isValid) const override
        {
            amplitudes.fill (0.0f);
            frequencyOffsets.fill (0.0f);
            amplitudes[0] = amp0;
            frequencyOffsets[0] = freq0;
            isValid = valid;
        }

        juce::String getModelName (int slot) const override
        {
            if (slot < 0 || slot >= slotNames.size())
                return "EMPTY";

            return slotNames[slot];
        }

        void loadModel (int slot) override { loads.push_back (slot); }
    };

    /** @brief `{ action: <name>, name: <preset> }`, the shape preset messages use. */
    juce::var jsPresetAction (const juce::String& action, const juce::String& presetName)
    {
        juce::DynamicObject::Ptr message = new juce::DynamicObject();
        message->setProperty ("action", action);
        message->setProperty ("name", presetName);
        return juce::var (message.get());
    }

    /** @brief MIDI backend stub for the MIDI-message section. */
    struct FakeMidiController final : public MidiController
    {
        std::vector<int> held;
        float lastPitch = -99.0f;
        float lastMod = -99.0f;
        int panics = 0;
        int noteOns = 0;
        int noteOffs = 0;

        void noteOn (int note, float /*velocity*/) override { ++noteOns; held.push_back (note); }
        void noteOff (int note) override
        {
            ++noteOffs;
            held.erase (std::remove (held.begin(), held.end(), note), held.end());
        }
        void pitchBend (float normalized) override { lastPitch = normalized; }
        void modWheel (float normalized) override { lastMod = normalized; }
        void allNotesOff() override { ++panics; held.clear(); }
    };

    /** @brief `{ action: <name>, ...fields }`, the shape MIDI messages use. */
    juce::var jsMidiAction (const juce::String& action, int note = -1,
                            double first = -999.0, double second = -999.0)
    {
        juce::DynamicObject::Ptr message = new juce::DynamicObject();
        message->setProperty ("action", action);

        if (note >= 0)
            message->setProperty ("note", note);

        if (first > -999.0)
            message->setProperty (note >= 0 ? "velocity" : "value", first);

        juce::ignoreUnused (second);
        return juce::var (message.get());
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

    moveFromNative (apvts, IDs::filterCutoff, 0.75f);

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

    moveFromNative (apvts, IDs::masterLevel, 0.6f);
    moveFromNative (apvts, IDs::engineType, 1.0f);
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
    const auto afterDetach = bridge.getStats();        check (afterDetach.snapshotsSent == beforeDetach.snapshotsSent
                   && afterDetach.changesSent == beforeDetach.changesSent,
               "with no transport nothing counts as sent (the page is not loaded yet)");

    // --- 8. Preset messages (additive to protocol v1) ----------------------------
    std::cout << "\nPreset messages\n";

    {
        FakePresetController fake;
        bridge.setPresetController (&fake);
        bridge.setSender (recorder.sender());   // section 7 detached it
        bridge.resetStats();
        recorder.messages.clear();

        bridge.handleJsEvent (jsAction (BridgeActions::listPresets));

        const auto listed = recorder.withAction (BridgeActions::presetList);
        check (listed.size() == 1, "listPresets answers exactly one presetList");

        const auto* listObject = listed.size() == 1 ? listed[0].getDynamicObject() : nullptr;
        const auto* names = listObject != nullptr
                                ? listObject->getProperty ("presets").getArray()
                                : nullptr;
        check (names != nullptr && names->size() == 2,
               "presetList carries the backend's preset names");
        check (listObject != nullptr
                   && listObject->getProperty ("current").toString() == "Init Preset",
               "presetList names the current preset");

        // A successful load answers with a full snapshot AND a fresh presetList.
        recorder.messages.clear();
        bridge.handleJsEvent (jsPresetAction (BridgeActions::loadPreset, "Glass Bells"));

        check (bridge.getStats().presetsLoaded == 1, "the successful load is counted");
        check (fake.loads == 1 && fake.current == "Glass Bells",
               "the backend received the load and switched");
        check (recorder.withAction (BridgeActions::syncAllParams).size() == 1,
               "a load answers syncAllParams (the state was rewritten)");
        check (recorder.withAction (BridgeActions::presetList).size() == 1,
               "a load answers presetList (the current preset moved)");

        // Traversal and separators never reach the backend: they answer presetError.
        bridge.resetStats();
        recorder.messages.clear();

        bridge.handleJsEvent (jsPresetAction (BridgeActions::loadPreset, "../escape"));

        const auto errors = recorder.withAction (BridgeActions::presetError);
        check (errors.size() == 1, "an unsafe preset name answers presetError");
        check (fake.loads == 1, "the unsafe name never reached the backend");
        check (bridge.getStats().presetErrors == 1, "the rejection is counted in stats");
        check (recorder.withAction (BridgeActions::syncAllParams).empty(),
               "a rejected load never resynchronises the page");

        // A name that is merely unknown is a backend-level failure, not a rejection.
        bridge.handleJsEvent (jsPresetAction (BridgeActions::loadPreset, "Does Not Exist"));
        check (bridge.getStats().presetErrors == 2,
               "a missing preset is also a presetError");

        // Save: happy path adds the name to the list; a backend failure is reported.
        bridge.resetStats();
        recorder.messages.clear();

        bridge.handleJsEvent (jsPresetAction (BridgeActions::savePreset, "Pad Nocturno"));

        check (bridge.getStats().presetsSaved == 1 && fake.saves == 1,
               "a successful save is counted on both sides");
        check (recorder.withAction (BridgeActions::presetList).size() == 1,
               "a save answers presetList with the new name");

        fake.failSaves = true;
        bridge.handleJsEvent (jsPresetAction (BridgeActions::savePreset, "Broken"));
        check (bridge.getStats().presetErrors == 1,
               "a failed save answers presetError");

        // Without a backend the preset messages degrade to presetError, never throw.
        bridge.setPresetController (nullptr);
        bridge.resetStats();
        recorder.messages.clear();

        bridge.handleJsEvent (jsPresetAction (BridgeActions::savePreset, "Whatever"));

        const auto noBackend = recorder.withAction (BridgeActions::presetError);
        check (noBackend.size() == 1
                   && noBackend[0].getDynamicObject()->getProperty ("detail").toString()
                          .contains ("no preset backend"),
               "without a backend every preset action answers presetError");
    }

    // --- 9. Spectral models (additive to protocol v1) -----------------------------
    std::cout << "\nSpectral models\n";

    {
        FakeModelController fakeModels;
        bridge.setModelController (&fakeModels);
        bridge.setSender (recorder.sender());
        bridge.resetStats();
        recorder.messages.clear();

        bridge.handleJsEvent (jsAction (BridgeActions::requestState));

        const auto models = recorder.withAction (BridgeActions::modelsState);
        check (models.size() == 1, "a snapshot carries one modelsState (models piggyback on it)");
        check (bridge.getStats().modelsSent == 1, "the modelsState emission is counted");

        const auto* modelsObject = models.size() == 1 ? models[0].getDynamicObject() : nullptr;
        const auto* slots = modelsObject != nullptr
                                ? modelsObject->getProperty ("slots").getArray()
                                : nullptr;
        check (slots != nullptr && slots->size() == 4,
               "modelsState carries the four model slots");

        if (slots != nullptr && slots->size() == 4)
            if (const auto* entry = (*slots)[0].getDynamicObject(); entry != nullptr)
            {
                const auto validVar = entry->getProperty ("isValid");
                const auto* amps = entry->getProperty ("amplitudes").getArray();
                const auto* freqs = entry->getProperty ("frequencyOffsets").getArray();

                check ((int) entry->getProperty ("slot") == 0
                           && validVar.isBool() && static_cast<bool> (validVar),
                       "slot 0 is published as valid");
                check (entry->getProperty ("name").toString() == fakeModels.slotNames[0],
                       "slot 0 travels with its display name (the page cannot read the disk)");
                check (amps != nullptr && amps->size() == 64
                           && static_cast<double> ((*amps)[0]) == static_cast<double> (fakeModels.amp0),
                       "amplitudes travel as 64 doubles (slot 0 amplitude matches)");
                check (freqs != nullptr && freqs->size() == 64
                           && static_cast<double> ((*freqs)[0]) == static_cast<double> (fakeModels.freq0),
                       "frequencyOffsets travel as 64 doubles (slot 0 offset matches)");
            }

        // A load rewrites the state AND the models: the snapshot hook fires and the
        // loadPreset handler sends its own (independent, idempotent) modelsState.
        FakePresetController fakePresets;
        bridge.setPresetController (&fakePresets);
        recorder.messages.clear();

        bridge.handleJsEvent (jsPresetAction (BridgeActions::loadPreset, "Glass Bells"));

        check (recorder.withAction (BridgeActions::modelsState).size() == 2,
               "a loadPreset answers modelsState twice (snapshot hook + load hook)");

        // Without a backend the message disappears entirely (additive degradation).
        bridge.setPresetController (nullptr);
        bridge.setModelController (nullptr);
        bridge.resetStats();
        recorder.messages.clear();

        bridge.handleJsEvent (jsAction (BridgeActions::requestState));

        check (recorder.withAction (BridgeActions::modelsState).empty(),
               "without a model backend no modelsState is sent");
    }

    // --- 10. Model loading (additive to protocol v1) -------------------------------
    std::cout << "\nModel loading\n";

    {
        FakeModelController fakeModels;
        bridge.setModelController (&fakeModels);
        bridge.setSender (recorder.sender());
        bridge.resetStats();
        recorder.messages.clear();

        const auto loadJs = [] (const juce::var& slot)
        {
            juce::DynamicObject::Ptr message = new juce::DynamicObject();
            message->setProperty ("action", BridgeActions::loadModel);
            message->setProperty ("slot", slot);
            return juce::var (message.get());
        };

        // The request reaches the backend; the ANSWER is asynchronous (the dialog
        // has not closed yet), so nothing else may be sent here.
        bridge.handleJsEvent (loadJs (2));

        check (fakeModels.loads == std::vector<int> { 2 },
               "loadModel hands the slot to the backend");
        check (bridge.getStats().modelLoads == 1,
               "the accepted request is counted in stats.modelLoads");
        check (recorder.withAction (BridgeActions::modelError).empty(),
               "an accepted request stays silent until the dialog answers");

        // The completion: the page gets the slots again, with the name the
        // processor recorded for the loaded slot.
        fakeModels.slotNames.set (2, "Cristal");
        bridge.sendModelsState();

        const auto afterLoad = recorder.withAction (BridgeActions::modelsState);
        check (afterLoad.size() == 1, "the completion publishes a fresh modelsState");

        if (afterLoad.size() == 1)
        {
            const auto* slots = afterLoad[0].getDynamicObject()->getProperty ("slots").getArray();

            check (slots != nullptr && slots->size() == 4
                       && (*slots)[2].getDynamicObject()->getProperty ("name").toString() == "Cristal",
                   "the fresh modelsState carries the loaded slot's name");
        }

        // Cancelled dialog or unusable file: the backend answers modelError.
        bridge.sendModelError (2, "no file chosen");

        const auto errors = recorder.withAction (BridgeActions::modelError);
        check (errors.size() == 1
                   && (int) errors[0].getDynamicObject()->getProperty ("slot") == 2
                   && errors[0].getDynamicObject()->getProperty ("detail").toString() == "no file chosen",
               "modelError names the slot and the reason");
        check (bridge.getStats().modelErrors == 1, "modelError is counted");

        // Out-of-range and fractional slots never reach the backend: the wire is
        // untrusted, and a silently truncated slot would look like it worked.
        recorder.messages.clear();
        bridge.handleJsEvent (loadJs (4));
        bridge.handleJsEvent (loadJs (-1));
        bridge.handleJsEvent (loadJs (1.5));

        check (fakeModels.loads.size() == 1,
               "slots outside 0..numSlots-1 and fractional slots never reach the backend");

        const auto rejects = recorder.withAction (BridgeActions::modelError);
        check (rejects.size() == 3, "every rejected slot answers modelError");

        if (rejects.size() == 3)
            check (rejects[0].getDynamicObject()->getProperty ("detail").toString().contains ("out of range")
                       && rejects[2].getDynamicObject()->getProperty ("detail").toString().contains ("integer"),
                   "out-of-range and fractional slots say exactly why");

        // Without a backend every request answers modelError: a slot that looks
        // loaded and sounds empty is the bug this message exists to prevent.
        bridge.setModelController (nullptr);
        recorder.messages.clear();
        bridge.resetStats();

        bridge.handleJsEvent (loadJs (0));

        const auto noBackend = recorder.withAction (BridgeActions::modelError);
        check (noBackend.size() == 1
                   && noBackend[0].getDynamicObject()->getProperty ("detail").toString()
                          .contains ("no model backend"),
               "without a backend every loadModel answers modelError");
        check (bridge.getStats().modelLoads == 0,
               "a request with no backend is not counted as a load");
    }

    // ============================================================================
    // Section 9 — MIDI messages (additive to v1): routing, ranges, panic.
    // ============================================================================
    {
        std::cout << "\n--- section 9: midi messages\n";
        bridge.resetStats();
        recorder.messages.clear();
        bridge.setSender (recorder.sender());   // section 8 detached it

        FakeMidiController midi;
        bridge.setMidiController (&midi);

        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiNoteOn, 60, 0.9));
        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiNoteOn, 64, 0.5));
        check (midi.held == std::vector<int> ({ 60, 64 }) && midi.noteOns == 2,
               "midiNoteOn forwards note+velocity to the controller");

        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiNoteOff, 60));
        check (midi.held == std::vector<int> ({ 64 }), "midiNoteOff removes the note");

        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiPitchBend, -1, 0.5));
        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiModWheel, -1, 0.25));
        check (std::abs (midi.lastPitch - 0.5f) < 1.0e-6f
                   && std::abs (midi.lastMod - 0.25f) < 1.0e-6f,
               "wheels forward their normalised values");

        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiPanic));
        check (midi.panics == 1 && midi.held.empty(), "midiPanic calls allNotesOff");

        // Out-of-range fields are rejected BEFORE reaching the controller.
        const auto statsBefore = bridge.getStats();
        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiNoteOn, 200, 0.5));   // note > 127
        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiNoteOn, 60, 1.5));    // velocity > 1
        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiPitchBend, -1, 2.0)); // pitch > 1
        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiModWheel, -1, -0.1)); // mod < 0
        check (bridge.getStats().midiRejected == 4 && midi.noteOns == 2,
               "out-of-range MIDI fields are counted and never forwarded");

        // Without a backend the actions are accepted (forwarded) and silently
        // dropped: 2 noteOn + noteOff + pitch + mod + panic + this one = 7 total.
        bridge.handleJsEvent (jsMidiAction (BridgeActions::midiNoteOn, 72, 1.0));
        check (bridge.getStats().midiForwarded == 7,
               "without a backend MIDI actions are accepted and dropped");

        bridge.setMidiController (nullptr);
    }

    std::cout << '\n' << (failures == 0 ? "All checks passed." : "Checks failed.") << '\n';
    return failures == 0 ? 0 : 1;
}
