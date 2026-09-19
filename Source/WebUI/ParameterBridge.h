/*
  ==============================================================================

    ParameterBridge.h
    Created: 16 Sep 2026
    Description: Two way parameter bridge between a JUCE AudioProcessorValueTreeState
                 and the WebUI, carried by the WebView2 channel of
                 juce::WebBrowserComponent.

                 This class owns the protocol and none of the transport: it never
                 touches a WebBrowserComponent, so it can be unit tested with a
                 real APVTS and a lambda instead of a window. The host wires the
                 transport with setSender() plus a `nativeEvent` listener.

                 WIRE FORMAT (both directions are juce::var objects)

                   native -> JS, event id "event"
                     { action: "syncAllParams", version: <n>, parameterCount: <n>,
                       parameters: [ { id, value, real, text }, ... ] }
                     { action: "parameterChanged", id, value, real, text }

                   JS -> native, event id "nativeEvent"
                     { action: "parameterChanged", id, value,
                       gesture: "begin" | "change" | "end" }
                     { action: "requestState" }

                   PRESET MESSAGES (additive to v1; a page or host without them
                   keeps working — unknown actions are counted, never fatal)

                     JS -> native
                       { action: "listPresets" }
                       { action: "loadPreset", name }
                       { action: "savePreset", name }

                     native -> JS
                       { action: "presetList", presets: [name...], current }
                       { action: "presetError", operation, detail }

                     A successful load answers with a FULL parameter snapshot plus a
                     fresh presetList; a successful save answers with a presetList.
                     Preset names are sanitised by the native side: no path
                     separators, no ".." — a rejected name answers presetError.

                   MIDI MESSAGES (additive to v1; let the page play the synth and
                   mirror external MIDI state on the shared keyboard)

                     JS -> native (page keyboard / wheels / panic)
                       { action: "midiNoteOn", note: 0..127, velocity: 0..1 }
                       { action: "midiNoteOff", note: 0..127 }
                       { action: "midiPitchBend", value: -1..+1 }
                       { action: "midiModWheel", value: 0..1 }
                       { action: "midiPanic" }   // all notes off in the plugin

                     native -> JS (host poll of the plugin's external MIDI view)
                       { action: "midiNoteState", held: [note...], pitchBend: -1..+1,
                         modWheel: 0..1 }

                     Notes outside 0..127 and values outside their range are
                     rejected with stats.midiRejected (never thrown). The state
                     message drives keyboard.setPitchBend/setModWheel/
                     notesOffVisual, which apply host-driven moves WITHOUT
                     echoing them back as user input.

                   SPECTRAL MODEL SLOTS (additive to v1; the four slots A..D the
                   native RESONATOR panel loaded, now owned by the page)

                     native -> JS
                       { action: "modelsState", slots: [ { slot, name, isValid,
                         amplitudes: [64], frequencyOffsets: [64] }, ... ] }
                       { action: "modelError", slot, detail }

                     JS -> native
                       { action: "loadModel", slot: 0..3 }

                     A preset is APVTS state PLUS up to four SpectralModel slots
                     (64 partials each) the Resonator morphs between; the models
                     never live in the APVTS, so they travel separately. `name`
                     is the slot's display name ("EMPTY" for a slot nothing was
                     loaded into), so both surfaces show the SAME list instead of
                     each one guessing.

                     loadModel is a STATE ACTION, not a parameter edit: the page
                     can neither name a path nor read the disk, so the backend
                     asks the host (the native panel's loadA..loadD dialog) and
                     answers ASYNCHRONOUSLY with a fresh modelsState, or with
                     modelError when the user cancels or the file is unusable.
                     A slot that is not an integer inside 0..numSlots answers
                     modelError as well; with no backend installed, every request
                     does. `modelLoads` counts the requests handed to the backend.

                   `value` is ALWAYS the normalised 0..1 value the APVTS uses, the
                   same convention as JUCE's own WebSliderRelay. `real` (denormalised)
                   and `text` are informational and only travel native -> JS, so the
                   two sides can never disagree about which scale is on the wire.

                 CHANGE DETECTION AND ECHO

                   Outgoing changes are *polled* by publishPendingChanges() instead of
                   pushed from parameter listeners: that is what keeps the bridge free
                   of the "listener registered twice / event lost" failure mode listed
                   in the ROADMAP, and it also does something a listener cannot do for
                   free, namely diff by value so a parameter that is set to the value it
                   already had produces no traffic.

                   A change applied from JS updates the entry's lastReported value, so
                   it is never echoed back to the page that just sent it.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <functional>
#include <vector>

namespace NEURONiK::WebUI
{

/** @brief Event ids of the WebView2 channel.
 *
 *  VERSIONED CONTRACT: these literals are the wire format of the bridge and are
 *  pinned by `WebUI/contracts/bridge-protocol.json`, versioned in git.
 *  Tests/BridgeProtocolContractTest.cpp compares that file against these
 *  constants, so changing a literal here or in the JSON without the other is a
 *  test failure. The full specification lives in DOCS/BRIDGE_PROTOCOL.md.
 */
namespace BridgeProtocolVersion
{
    /** @brief Bump when a wire change is NOT backwards compatible (new required
     *         field, renamed literal, changed scale). Additive optional fields do
     *         not require a bump, only a contract file update.
     */
    inline constexpr int current = 1;
}

namespace BridgeEventIds
{
    inline constexpr const char* nativeToJs = "event";        //!< emitEventIfBrowserIsVisible
    inline constexpr const char* jsToNative = "nativeEvent";  //!< withEventListener
    inline constexpr const char* pageLoaded = "pageLoaded";   //!< withEventListener
}

/** @brief Action names of both directions, kept in one place so no side invents one.
 *  @see BridgeEventIds — same versioned-contract rules apply.
 */
namespace BridgeActions
{
    inline constexpr const char* syncAllParams = "syncAllParams";
    inline constexpr const char* parameterChanged = "parameterChanged";
    inline constexpr const char* requestState = "requestState";

    // Preset management (additive to protocol v1; see the header doc above).
    inline constexpr const char* listPresets = "listPresets";
    inline constexpr const char* loadPreset = "loadPreset";
    inline constexpr const char* savePreset = "savePreset";
    inline constexpr const char* presetList = "presetList";
    inline constexpr const char* presetError = "presetError";

    // Spectral models (additive to protocol v1; see bridge-protocol.json)
    inline constexpr const char* modelsState = "modelsState";

    // Model loading (additive to protocol v1): the page asks the HOST for a file
    // dialog (it can neither name a path nor read the disk) and the host answers
    // with modelsState or modelError.
    inline constexpr const char* loadModel = "loadModel";
    inline constexpr const char* modelError = "modelError";

    // State actions (additive to protocol v1; see bridge-protocol.json). These are
    // not parameter edits: the backend performs them on the APVTS itself.
    inline constexpr const char* randomize = "randomize";

    // MIDI (additive to protocol v1; see the header doc above).
    inline constexpr const char* midiNoteOn = "midiNoteOn";
    inline constexpr const char* midiNoteOff = "midiNoteOff";
    inline constexpr const char* midiPitchBend = "midiPitchBend";
    inline constexpr const char* midiModWheel = "midiModWheel";
    inline constexpr const char* midiPanic = "midiPanic";
    inline constexpr const char* midiNoteState = "midiNoteState";
}

/**
 * @class PresetController
 * @brief What the bridge needs from a preset backend, and nothing more.
 *
 * The bridge owns the WIRE side of the preset messages; the host injects an
 * adapter around the plugin's PresetManager. Keeping this an interface lets
 * ParameterBridgeTest exercise the protocol without a real preset directory.
 */
class PresetController
{
public:
    virtual ~PresetController() = default;

    [[nodiscard]] virtual juce::StringArray listPresets() const = 0;
    /** @returns false when the preset does not exist (nothing was loaded). */
    virtual bool loadPreset (const juce::String& name) = 0;
    /** @returns false when the preset file could not be written. */
    virtual bool savePreset (const juce::String& name) = 0;
    [[nodiscard]] virtual juce::String getCurrentPreset() const = 0;
};

/**
 * @class RandomizeController
 * @brief What the bridge needs from a state-action backend, and nothing more.
 *
 * The RANDOMIZE was a panel action implemented in `ParameterPanel` (the panel
 * that is being retired), which is why the page could not have it. It lives in
 * `State/ParameterRandomizer` now and this controller is the wire side: the page
 * asks, the processor randomises. Strength comes from the `randomStrength`
 * parameter, so the page sends no fields and cannot invent a strength the
 * parameter does not have.
 */
class RandomizeController
{
public:
    virtual ~RandomizeController() = default;

    /** @returns how many parameters changed value (0 with nothing to do). */
    virtual int randomize() = 0;
};

/**
 * @class MidiController
 * @brief What the bridge needs from a MIDI backend, and nothing more.
 *
 * The bridge owns the WIRE side of the MIDI messages; the host injects an
 * adapter around the plugin's injection FIFO (the same path the native editor
 * uses), so bridge tests exercise the protocol without an audio device.
 */
class MidiController
{
public:
    virtual ~MidiController() = default;

    virtual void noteOn (int note, float velocity) = 0;
    virtual void noteOff (int note) = 0;
    /** @param normalized -1..+1 (0 = centre), same scale as the wire. */
    virtual void pitchBend (float normalized) = 0;
    /** @param normalized 0..1 (CC1). */
    virtual void modWheel (float normalized) = 0;
    /** @brief Stop every sounding note (page panic / safety). */
    virtual void allNotesOff() = 0;
};

/**
 * @class NativeModelController
 * @brief What the bridge needs to publish spectral models, and nothing more.
 *
 * A preset is APVTS state PLUS up to four SpectralModel slots (64 partials each)
 * the Resonator morphs between. The models never live in the APVTS, so the
 * bridge asks this backend for the engine's current view and serialises them.
 * The call happens on the message thread while the audio thread only ever swaps
 * whole 64+1-float POD models (Resonator::loadModel), so no extra locking is
 * required for a telemetry-grade copy.
 */
class NativeModelController
{
public:
    virtual ~NativeModelController() = default;

    /** @brief Number of model slots the backend exposes (0 disables publishing). */
    [[nodiscard]] virtual int getNumModelSlots() const = 0;

    /** @brief Copies slot's model into out (amplitudes + frequencyOffsets). */
    virtual void getCurrentModel (int slot, std::array<float, 64>& amplitudes,
                                  std::array<float, 64>& frequencyOffsets,
                                  bool& isValid) const = 0;

    /**
     * @brief Display name of a slot's model, or an empty/"EMPTY" string when the
     *        slot holds nothing. Travels on the wire so the page shows the same
     *        list the plugin has (the native panel drew these names at the XY
     *        pad corners and the page has no other way to know them).
     */
    [[nodiscard]] virtual juce::String getModelName (int slot) const = 0;

    /**
     * @brief Starts loading a model into `slot`.
     * @details The backend owns the OUT-OF-BAND half of the action: the page
     *          cannot name a path (untrusted wire) nor read the disk, so the host
     *          asks the operating system for a file and loads it. That is
     *          asynchronous, so this returns nothing: the answer travels later
     *          over the bridge as a fresh modelsState (success) or a modelError
     *          (the user cancelled, or the file is not a usable model).
     *
     *          Called on the message thread, like every other bridge entry point.
     */
    virtual void loadModel (int slot) = 0;
};

/** @brief Gesture phases a JS change can declare. */
namespace BridgeGestures
{
    inline constexpr const char* begin = "begin";
    inline constexpr const char* change = "change";
    inline constexpr const char* end = "end";
}

/**
 * @class ParameterBridge
 * @brief Mirrors an APVTS both ways over the WebView2 channel.
 *
 * Everything here must run on the message thread: incoming JS events arrive there
 * (JUCE dispatches WebView2 callbacks on the message thread) and
 * setValueNotifyingHost() must not be called from anywhere else.
 */
class ParameterBridge
{
public:
    /** @brief Sends one message to the WebUI. Wired by the host to
     *         emitEventIfBrowserIsVisible(). A null sender is valid: the bridge
     *         then only records what it would have sent (page not loaded yet).
     */
    using Sender = std::function<void (const juce::var&)>;

    /** @brief Counters that make the bridge observable from tests and diagnostics. */
    struct Stats
    {
        int snapshotsSent = 0;      //!< syncAllParams messages emitted
        int changesSent = 0;        //!< parameterChanged messages emitted
        int appliedFromJs = 0;      //!< JS changes actually written to the APVTS
        int idleChanges = 0;        //!< JS changes that matched the current value
        int unknownIds = 0;         //!< JS changes for ids the layout does not define
        int rejectedMessages = 0;   //!< malformed messages or unknown actions
        int gesturesOpened = 0;     //!< begin gestures accepted
        int gesturesClosed = 0;     //!< gestures closed, including the forced ones
        int presetsLoaded = 0;      //!< successful loadPreset operations
        int presetsSaved = 0;       //!< successful savePreset operations
        int presetErrors = 0;       //!< failed/rejected preset operations
        int midiForwarded = 0;      //!< MIDI actions accepted and forwarded
        int midiRejected = 0;       //!< MIDI actions with out-of-range fields
        int modelsSent = 0;         //!< modelsState messages emitted
        int modelLoads = 0;         //!< loadModel requests handed to the backend
        int modelErrors = 0;        //!< modelError messages emitted
        int randomized = 0;         //!< state actions accepted (one per randomize)
        int randomizedParameters = 0; //!< parameters those actions actually moved
    };

    /** @brief Bridge `stateToBridge`, mirroring every parameter it contains. */
    explicit ParameterBridge (juce::AudioProcessorValueTreeState& stateToBridge);

    /** @brief Install the transport. Passing {} disconnects the bridge. */
    void setSender (Sender newSender);

    /**
     * @brief Install the preset backend. Passing nullptr disables the preset
     *        messages: they answer presetError instead of crashing or lying.
     *        Must be called on the message thread, before the page loads.
     */
    void setPresetController (PresetController* newController) noexcept;

    /**
     * @brief Install the MIDI backend. Passing nullptr makes the page's MIDI
     *        actions no-ops that still count as forwarded (silent mode).
     *        Must be called on the message thread, before the page loads.
     */
    void setMidiController (MidiController* newController) noexcept;

    /** @brief Installs the spectral-model backend; nullptr disables modelsState. */
    void setModelController (NativeModelController* newController) noexcept;

    /**
     * @brief Install the state-action backend. Passing nullptr makes randomize a
     *        no-op that still counts as accepted (silent mode), same rule as MIDI.
     *        Must be called on the message thread, before the page loads.
     */
    void setRandomizeController (RandomizeController* newController) noexcept;

    /**
     * @brief Send one midiNoteState message (held notes + wheel positions)
     *        from the given external MIDI view. Called by the host poll so the
     *        page keyboard mirrors hardware/DAW MIDI. No-op without a sender.
     */
    void sendMidiNoteState (const std::vector<int>& heldNotes,
                            float pitchBendNormalized, float modWheelNormalized);

    /**
     * @brief Re-publishes the spectral model slots (modelsState).
     * @details Public because a model load finishes ASYNCHRONOUSLY: the file
     *          dialog answers long after handleJsEvent() returned, so it is the
     *          backend (not this class) that decides when the page is told. A
     *          preset load rewrites every modelPath<slot> and answers with this
     *          too. No-op without a model controller, and silent without a
     *          transport (bridges are alive before the page is).
     */
    void sendModelsState();

    /**
     * @brief modelError message: the answer when a load does NOT happen.
     * @param slot   the slot the request named, or -1 when it named none.
     * @param detail human readable reason (cancelled, not a model, out of range).
     * @details Counts in stats.modelErrors. Never thrown, and never a silent
     *          failure: the page shows the reason next to the slot.
     */
    void sendModelError (int slot, const juce::String& detail);

    /**
     * @brief Handle one message from the WebUI.
     * @details Unknown actions and malformed messages are counted, never thrown:
     *          a page reload can race with an in flight message.
     */
    void handleJsEvent (const juce::var& message);

    /** @brief Send the complete state (page load, or an explicit requestState). */
    void sendFullSnapshot();

    /**
     * @brief Send every parameter whose value changed since it was last reported.
     * @returns how many parameterChanged messages were sent (one per parameter,
     *          never one per change: a fast drag produces one message per poll).
     */
    int publishPendingChanges();

    /**
     * @brief Close every gesture the page left open.
     * @details Called when the page (re)loads. Without this a reload in the middle
     *          of a drag would leave the parameter in gesture forever, which a host
     *          would record as a stuck automation touch.
     * @returns how many gestures had to be closed.
     */
    int closeOpenGestures();

    /** @brief The full state as it goes on the wire. */
    [[nodiscard]] juce::var buildSnapshotVar() const;

    /** @brief Number of parameters the bridge mirrors. */
    [[nodiscard]] int getParameterCount() const noexcept;

    /** @brief Parameter id at a mirrored index (layout order), or an empty string. */
    [[nodiscard]] juce::String getParameterId (int index) const;

    /** @brief Index of a mirrored parameter id, or -1 when it is not in the layout. */
    [[nodiscard]] int indexOfParameter (const juce::String& id) const;

    /** @brief Current normalised value of a mirrored parameter, or 0 when unknown. */
    [[nodiscard]] float getNormalisedValue (const juce::String& id) const;

    /** @brief Current transport, for tests and diagnostics. */
    [[nodiscard]] bool hasSender() const noexcept { return sender != nullptr; }

    [[nodiscard]] Stats getStats() const noexcept { return stats; }
    void resetStats() noexcept { stats = {}; }

private:
    struct Entry
    {
        juce::RangedAudioParameter* parameter = nullptr;
        juce::String id;
        float lastReported = 0.0f;   //!< normalised value the page already knows
        bool reported = false;       //!< false until the page has ever been told
        bool gestureOpen = false;    //!< a JS gesture is open on this parameter
    };

    /** @brief One parameter as it goes on the wire. */
    [[nodiscard]] juce::var describe (const Entry& entry) const;

    /** @brief Write one JS change into the APVTS. */
    void applyParameterChange (const juce::DynamicObject& message);

    /** @brief presetList message from the controller's view of the world. */
    void sendPresetList();

    /** @brief presetError message; counts in stats.presetErrors. */
    void sendPresetError (const char* operation, const juce::String& detail);

    void handleListPresets();
    void handleLoadPreset (const juce::DynamicObject& message);
    void handleSavePreset (const juce::DynamicObject& message);

    /** @brief MIDI actions: validate ranges, forward to the controller. */
    void handleMidiAction (const juce::String& action, const juce::DynamicObject& message);

    /** @brief loadModel: validate the slot, hand the request to the backend. */
    void handleLoadModel (const juce::DynamicObject& message);

    /** @brief State actions (randomize): run on the backend, count what moved. */
    void handleRandomize();

    /** @brief Deliver a message if a transport is installed, counting the kind. */
    void send (const juce::var& message, bool isSnapshot);

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Entry> entries;
    Sender sender;
    PresetController* presets = nullptr;   //!< not owned; the host outlives it
    MidiController* midi = nullptr;        //!< not owned; the host outlives it
    NativeModelController* models = nullptr; //!< not owned; the host outlives it
    RandomizeController* randomizer = nullptr; //!< not owned; the host outlives it
    int snapshotVersion = 0;
    Stats stats;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterBridge)
};

} // namespace NEURONiK::WebUI
