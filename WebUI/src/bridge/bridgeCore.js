/**
 * Transport between the NEURONiK page and the JUCE host over the WebView2 channel.
 *
 * PORTADO de `WebPilot/lib/bridge.js` SIN cambios de comportamiento: es JS puro
 * (la contrapartida JS del protocolo versionado `WebUI/contracts/bridge-protocol.json`,
 * que ya tiene test propio en C++ y en JS). El armazón React del piloto murió en el
 * ticket 8.4; esto no.
 *
 * Directions (JUCE 8), the same ones the shared guard in
 * `ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js` enforces on C++:
 *
 *   - JS -> native: `window.__JUCE__.backend.emitEvent("nativeEvent", message)`
 *     received by the host's `withEventListener("nativeEvent", ...)`.
 *   - native -> JS: `window.__JUCE__.backend.addEventListener("event", listener)`
 *     fired by the host's `emitEventIfBrowserIsVisible("event", ...)`.
 *
 * Wire format (must stay in sync with Source/WebUI/ParameterBridge.h):
 *
 *   native -> JS  { action: "syncAllParams", version, parameterCount,
 *                   parameters: [{ id, value, real, text }, ...] }
 *                 { action: "parameterChanged", id, value, real, text }
 *
 *   JS -> native  { action: "parameterChanged", id, value,
 *                   gesture: "begin" | "change" | "end" }
 *                 { action: "requestState" }
 *
 *   PRESETS (additive to protocol v1; see bridge-protocol.json)
 *
 *   JS -> native  { action: "listPresets" }
 *                 { action: "loadPreset", name }
 *                 { action: "savePreset", name }
 *
 *   native -> JS  { action: "presetList", presets: [name...], current }
 *                 { action: "presetError", operation, detail }
 *
 *   A successful load also triggers a full parameter snapshot + a fresh presetList
 *   from the host; a successful save answers with a presetList. Names are
 *   sanitised natively — a rejected name answers presetError, never a file write.
 *
 *   MIDI (additive to protocol v1; see bridge-protocol.json)
 *
 *   JS -> native  { action: "midiNoteOn", note: 0..127, velocity: 0..1 }
 *                 { action: "midiNoteOff", note: 0..127 }
 *                 { action: "midiPitchBend", value: -1..+1 }
 *                 { action: "midiModWheel", value: 0..1 }
 *                 { action: "midiPanic" }
 *
 *   native -> JS  { action: "midiNoteState", held: [note...], pitchBend, modWheel }
 *
 *   The state message feeds the shared keyboard's host-driven feedback API, so
 *   hardware/DAW MIDI reaching the plugin is mirrored on the page wheels/keys.
 *
 *   SPECTRAL MODELS (additive to protocol v1; see bridge-protocol.json)
 *
 *   native -> JS  { action: "modelsState", slots: [{ slot, name, isValid,
 *                   amplitudes: [64], frequencyOffsets: [64] }, ...] }
 *                 { action: "modelError", slot, detail }
 *
 *   JS -> native  { action: "loadModel", slot: 0..3 }
 *
 *   A preset is APVTS state PLUS up to four SpectralModel slots (64 partials
 *   each) the Resonator morphs between; they never live in the APVTS, so they
 *   travel separately. Sent after every syncAllParams and loadPreset; the page
 *   forwards them to the AudioWorklet (re-applying after every engine switch).
 *   `name` is the slot's display name (the page cannot read the disk), and
 *   sendLoadModel is the page ASKING THE HOST for a file dialog: it is the only
 *   request whose answer arrives later, as a fresh modelsState or as modelError.
 *
 * `value` is ALWAYS the normalised 0..1 value; `real` and `text` are display only.
 *
 * When `window.__JUCE__` is absent (a plain browser, `vite dev` on its own) the
 * module reports `available: false` and every send is a no-op, so the page keeps
 * working in local mode exactly as before the bridge existed.
 */

const NATIVE_TO_JS_EVENT_ID = 'event';
const JS_TO_NATIVE_EVENT_ID = 'nativeEvent';
const PAGE_LOADED_EVENT_ID = 'pageLoaded';

function backend() {
  return typeof window !== 'undefined' && window.__JUCE__?.backend ? window.__JUCE__.backend : null;
}

/**
 * The JUCE-injected backend, or null in a plain browser.
 *
 * Exported because "is there a host on the other side?" is a question more than
 * one module has to answer (the audio policy of src/audio/policy.js does), and
 * there must be ONE detection: two copies of this check would be two ways of
 * being wrong.
 */
export function nativeBackend() {
  return backend();
}

/**
 * Create a bridge transport bound to one set of callbacks.
 *
 * @param {object} handlers
 * @param {(parameters: Array<{id: string, value: number, real: number, text: string}>) => void}
 *   handlers.onSnapshot  full state from the host
 * @param {(id: string, value: number) => void}
 *   handlers.onParameterChanged  one parameter moved on the native side
 * @param {({ presets: string[], current: string }) => void} [handlers.onPresetList]
 *   the host's preset list and which preset is current
 * @param {({ operation: string, detail: string }) => void} [handlers.onPresetError]
 *   a preset operation the host rejected or failed
 * @param {({ held: number[], pitchBend: number, modWheel: number }) => void} [handlers.onMidiState]
 *   the plugin's external MIDI view (held notes + wheel positions), ~6x per second
 * @param {(slots: Array<{slot:number, name:string, isValid:boolean, amplitudes:number[], frequencyOffsets:number[]}>) => void} [handlers.onModels]
 *   the engine's current spectral model slots (sent with every snapshot)
 * @param {({ slot: number, detail: string }) => void} [handlers.onModelError]
 *   a model load that did NOT happen (cancelled dialog, unusable file, bad slot)
 * @returns {{ available: boolean, sendParameterChange: Function, sendRequestState: Function, announcePageLoaded: Function, sendListPresets: Function, sendLoadPreset: Function, sendSavePreset: Function, sendRandomize: Function, sendLoadModel: Function, sendMidiNoteOn: Function, sendMidiNoteOff: Function, sendMidiPitchBend: Function, sendMidiModWheel: Function, sendMidiPanic: Function, dispose: Function }}
 */
export function createBridgeTransport(handlers) {
  const carrier = backend();

  if (!carrier) {
    // Local mode: no JUCE injection, the page owns its state alone.
    return {
      available: false,
      sendParameterChange: () => {},
      sendRequestState: () => {},
      announcePageLoaded: () => {},
      sendListPresets: () => {},
      sendLoadPreset: () => {},
      sendSavePreset: () => {},
      sendRandomize: () => {},
      sendLoadModel: () => {},
      sendMidiNoteOn: () => {},
      sendMidiNoteOff: () => {},
      sendMidiPitchBend: () => {},
      sendMidiModWheel: () => {},
      sendMidiPanic: () => {},
      dispose: () => {},
    };
  }

  const emit = (message) => {
    try {
      carrier.emitEvent(JS_TO_NATIVE_EVENT_ID, message);
    } catch {
      // A WebView2 teardown can race a click; never take the page down with it.
    }
  };

  // JUCE's Backend.addEventListener returns a numeric id, and removeEventListener
  // takes it as [eventId, id] — verified against juce_gui_extra/native/javascript.
  const removeSnapshot = carrier.addEventListener(NATIVE_TO_JS_EVENT_ID, (message) => {
    if (message?.action === 'syncAllParams' && Array.isArray(message.parameters))
      handlers.onSnapshot(message.parameters);
  });

  const removeChange = carrier.addEventListener(NATIVE_TO_JS_EVENT_ID, (message) => {
    if (
      message?.action === 'parameterChanged'
      && typeof message.id === 'string'
      && typeof message.value === 'number'
    )
      handlers.onParameterChanged(message.id, message.value);
  });

  const removePresetList = carrier.addEventListener(NATIVE_TO_JS_EVENT_ID, (message) => {
    if (
      message?.action === 'presetList'
      && Array.isArray(message.presets)
      && typeof message.current === 'string'
    )
      handlers.onPresetList?.({ presets: message.presets, current: message.current });
  });

  const removePresetError = carrier.addEventListener(NATIVE_TO_JS_EVENT_ID, (message) => {
    if (
      message?.action === 'presetError'
      && typeof message.operation === 'string'
      && typeof message.detail === 'string'
    )
      handlers.onPresetError?.({ operation: message.operation, detail: message.detail });
  });

  const removeMidiState = carrier.addEventListener(NATIVE_TO_JS_EVENT_ID, (message) => {
    if (
      message?.action === 'midiNoteState'
      && Array.isArray(message.held)
      && typeof message.pitchBend === 'number'
      && typeof message.modWheel === 'number'
    )
      handlers.onMidiState?.({
        held: message.held,
        pitchBend: message.pitchBend,
        modWheel: message.modWheel,
      });
  });

  const removeModelsState = carrier.addEventListener(NATIVE_TO_JS_EVENT_ID, (message) => {
    if (
      message?.action === 'modelsState'
      && Array.isArray(message.slots)
    )
      handlers.onModels?.(message.slots);
  });

  const removeModelError = carrier.addEventListener(NATIVE_TO_JS_EVENT_ID, (message) => {
    if (
      message?.action === 'modelError'
      && typeof message.detail === 'string'
    )
      handlers.onModelError?.({ slot: Number(message.slot ?? -1), detail: message.detail });
  });

  return {
    available: true,

    /** Push one user change to the host. Gesture phases let the host automate. */
    sendParameterChange(id, value, gesture) {
      emit({ action: 'parameterChanged', id, value, gesture: gesture ?? 'change' });
    },

    /** Ask the host for the complete state (used on mount). */
    sendRequestState() {
      emit({ action: 'requestState' });
    },

    /**
     * Tell the host the page (re)loaded: it closes gestures the page left open and
     * answers with a full snapshot. This goes on its OWN event id — the host binds a
     * dedicated withEventListener("pageLoaded") for it.
     */
    announcePageLoaded() {
      try {
        carrier.emitEvent(PAGE_LOADED_EVENT_ID, { action: 'pageLoaded' });
      } catch {
        // Same teardown race as emit(): never fatal.
      }
    },

    sendListPresets() {
      emit({ action: 'listPresets' });
    },

    /** Ask the host to load a preset by name; answers presetList or presetError. */
    sendLoadPreset(name) {
      emit({ action: 'loadPreset', name });
    },

    /** Ask the host to save the current state as `name`; answers presetList or presetError. */
    sendSavePreset(name) {
      emit({ action: 'savePreset', name });
    },

    /** Page keyboard: one key down (velocity 0..1). */
    sendMidiNoteOn(note, velocity) {
      emit({ action: 'midiNoteOn', note, velocity });
    },

    /** Page keyboard: one key up. */
    sendMidiNoteOff(note) {
      emit({ action: 'midiNoteOff', note });
    },

    /** Page pitch wheel: -1..+1, 0 = centre. */
    sendMidiPitchBend(value) {
      emit({ action: 'midiPitchBend', value });
    },

    /** Page mod wheel: 0..1 (CC1). */
    sendMidiModWheel(value) {
      emit({ action: 'midiModWheel', value });
    },

    /** Page PANIC: every sounding note in the plugin stops. */
    sendMidiPanic() {
      emit({ action: 'midiPanic' });
    },

    /**
     * Page RANDOM: a STATE action, not a parameter edit. The processor randomises
     * its own APVTS (strength = the randomStrength parameter, freeze flags
     * honoured) and the page learns every moved value through the normal
     * parameterChanged path, so nothing extra has to be kept in sync.
     */
    sendRandomize() {
      emit({ action: 'randomize' });
    },

    /**
     * Page -> host: load a model file into slot 0..3 (A..D). The host opens the
     * file dialog, so the page sends no bytes and no path; the answer arrives
     * later as modelsState (slot filled) or as modelError (nothing loaded).
     */
    sendLoadModel(slot) {
      emit({ action: 'loadModel', slot });
    },

    dispose() {
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeSnapshot]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeChange]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removePresetList]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removePresetError]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeMidiState]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeModelsState]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeModelError]);
    },
  };
}

export { NATIVE_TO_JS_EVENT_ID, JS_TO_NATIVE_EVENT_ID, PAGE_LOADED_EVENT_ID };
