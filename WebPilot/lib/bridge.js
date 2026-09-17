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
 * @returns {{ available: boolean, sendParameterChange: Function, sendRequestState: Function, announcePageLoaded: Function, dispose: Function }}
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

    dispose() {
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeSnapshot]);
      carrier.removeEventListener([NATIVE_TO_JS_EVENT_ID, removeChange]);
    },
  };
}

export { NATIVE_TO_JS_EVENT_ID, JS_TO_NATIVE_EVENT_ID, PAGE_LOADED_EVENT_ID };
