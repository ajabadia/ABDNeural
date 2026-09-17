/**
 * Transport between the pilot page and the JUCE host over the WebView2 channel.
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
 * `value` is ALWAYS the normalised 0..1 value; `real` and `text` are display only.
 *
 * When `window.__JUCE__` is absent (a plain browser, `next dev` on its own) the
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
 * @returns {{ available: boolean, sendParameterChange: Function, sendRequestState: Function, announcePageLoaded: Function, sendListPresets: Function, sendLoadPreset: Function, sendSavePreset: Function, dispose: Function }}
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

    dispose() {
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeSnapshot]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeChange]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removePresetList]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removePresetError]);
    },
  };
}

export { NATIVE_TO_JS_EVENT_ID, JS_TO_NATIVE_EVENT_ID, PAGE_LOADED_EVENT_ID };
