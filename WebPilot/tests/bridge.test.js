/**
 * Contract tests for the JS transport (lib/bridge.js) — the web half of the
 * versioned bridge protocol (`contracts/bridge-protocol.json`).
 *
 * The C++ side has its own suite (ParameterBridgeTest, BridgeProtocolContractTest);
 * this pins what the PAGE emits and what it accepts, so the wire format cannot
 * drift in JS without a test failing. The fake backend mimics the JUCE 8
 * injection contract: addEventListener returns a numeric id, removeEventListener
 * takes [eventId, id].
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import {
  createBridgeTransport,
  JS_TO_NATIVE_EVENT_ID,
  NATIVE_TO_JS_EVENT_ID,
  PAGE_LOADED_EVENT_ID,
} from '../lib/bridge.js';

/** Fake of window.__JUCE__.backend with the observable behaviours the page relies on. */
function makeBackend() {
  const listeners = new Map();
  let nextId = 1;

  return {
    emitted: [],
    emitEvent(eventId, message) {
      this.emitted.push({ eventId, message });
    },
    addEventListener(eventId, fn) {
      const id = nextId++;
      if (!listeners.has(eventId)) listeners.set(eventId, []);
      listeners.get(eventId).push({ id, fn });
      return id;
    },
    removeEventListener([eventId, id]) {
      const list = listeners.get(eventId) ?? [];
      const index = list.findIndex((entry) => entry.id === id);
      if (index >= 0) list.splice(index, 1);
    },
    listenerCount(eventId) {
      return (listeners.get(eventId) ?? []).length;
    },
    /** Simulate the host's emitEventIfBrowserIsVisible("event", ...). */
    dispatchFromNative(eventId, message) {
      for (const { fn } of listeners.get(eventId) ?? []) fn(message);
    },
  };
}

describe('bridge transport', () => {
  beforeEach(() => {
    vi.stubGlobal('window', { ...window });
  });

  afterEach(() => {
    vi.unstubAllGlobals();
  });

  describe('local mode (no window.__JUCE__)', () => {
    it('reports unavailable and every send is a silent no-op', () => {
      const onSnapshot = vi.fn();
      const transport = createBridgeTransport({ onSnapshot });

      expect(transport.available).toBe(false);
      expect(() => transport.sendParameterChange('masterLevel', 0.5, 'change')).not.toThrow();
      expect(() => transport.sendRequestState()).not.toThrow();
      expect(() => transport.announcePageLoaded()).not.toThrow();
      expect(() => transport.dispose()).not.toThrow();
      expect(onSnapshot).not.toHaveBeenCalled();
    });
  });

  describe('JS -> native wire format', () => {
    it('sends parameterChanged with the gesture phase on the nativeEvent channel', () => {
      const backend = makeBackend();
      window.__JUCE__ = { backend };
      const transport = createBridgeTransport({});

      transport.sendParameterChange('morphX', 0.25, 'begin');

      expect(backend.emitted).toEqual([
        { eventId: JS_TO_NATIVE_EVENT_ID, message: { action: 'parameterChanged', id: 'morphX', value: 0.25, gesture: 'begin' } },
      ]);
    });

    it('defaults the gesture to "change" when omitted', () => {
      const backend = makeBackend();
      window.__JUCE__ = { backend };
      const transport = createBridgeTransport({});

      transport.sendParameterChange('morphY', 1);

      expect(backend.emitted[0].message.gesture).toBe('change');
    });

    it('sends requestState and pageLoaded (pageLoaded on its own event id)', () => {
      const backend = makeBackend();
      window.__JUCE__ = { backend };
      const transport = createBridgeTransport({});

      transport.sendRequestState();
      transport.announcePageLoaded();

      expect(backend.emitted).toEqual([
        { eventId: JS_TO_NATIVE_EVENT_ID, message: { action: 'requestState' } },
        { eventId: PAGE_LOADED_EVENT_ID, message: { action: 'pageLoaded' } },
      ]);
    });
  });

  describe('native -> JS dispatch', () => {
    it('routes a valid syncAllParams snapshot to onSnapshot', () => {
      const onSnapshot = vi.fn();
      const backend = makeBackend();
      window.__JUCE__ = { backend };
      createBridgeTransport({ onSnapshot });

      const snapshot = {
        action: 'syncAllParams',
        version: 1,
        parameterCount: 2,
        parameters: [
          { id: 'masterLevel', value: 0.5, real: 0.5, text: '50%' },
          { id: 'morphX', value: 0.25, real: -0.5, text: '-0.50' },
        ],
      };
      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, snapshot);

      expect(onSnapshot).toHaveBeenCalledWith(snapshot.parameters);
    });

    it('routes parameterChanged to onParameterChanged', () => {
      const onParameterChanged = vi.fn();
      const backend = makeBackend();
      window.__JUCE__ = { backend };
      createBridgeTransport({ onParameterChanged });

      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'parameterChanged', id: 'masterLevel', value: 0.75, real: 0.75, text: '75%',
      });

      expect(onParameterChanged).toHaveBeenCalledWith('masterLevel', 0.75);
    });

    it('ignores malformed messages instead of throwing', () => {
      const onSnapshot = vi.fn();
      const onParameterChanged = vi.fn();
      const backend = makeBackend();
      window.__JUCE__ = { backend };
      createBridgeTransport({ onSnapshot, onParameterChanged });

      expect(() => {
        backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, { action: 'syncAllParams' }); // no array
        backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, { action: 'syncAllParams', parameters: 'x' });
        backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, { action: 'parameterChanged', id: 42, value: 0.5 }); // id not string
        backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, { action: 'parameterChanged', id: 'morphX', value: 'x' }); // value not number
        backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, { action: 'unknownAction' });
        backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, null);
      }).not.toThrow();

      expect(onSnapshot).not.toHaveBeenCalled();
      expect(onParameterChanged).not.toHaveBeenCalled();
    });
  });

  describe('lifecycle', () => {
    it('dispose removes its native->JS listeners (no leaks, no further dispatch)', () => {
      const onSnapshot = vi.fn();
      const onParameterChanged = vi.fn();
      const backend = makeBackend();
      window.__JUCE__ = { backend };

      const transport = createBridgeTransport({ onSnapshot, onParameterChanged });
      expect(backend.listenerCount(NATIVE_TO_JS_EVENT_ID)).toBe(6);

      transport.dispose();
      expect(backend.listenerCount(NATIVE_TO_JS_EVENT_ID)).toBe(0);

      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'syncAllParams', parameters: [],
      });
      expect(onSnapshot).not.toHaveBeenCalled();
    });

    it('a throwing backend never takes the page down', () => {
      const backend = makeBackend();
      backend.emitEvent = () => { throw new Error('WebView2 teardown race'); };
      window.__JUCE__ = { backend };

      const transport = createBridgeTransport({});

      expect(() => transport.sendParameterChange('masterLevel', 0.5, 'change')).not.toThrow();
      expect(() => transport.announcePageLoaded()).not.toThrow();
    });
  });

  describe('midi messages (additive to protocol v1)', () => {
    it('senders emit the exact wire actions', () => {
      const backend = makeBackend();
      window.__JUCE__ = { backend };
      const transport = createBridgeTransport({});

      transport.sendMidiNoteOn(60, 0.9);
      transport.sendMidiNoteOff(60);
      transport.sendMidiPitchBend(-0.5);
      transport.sendMidiModWheel(0.75);
      transport.sendMidiPanic();

      expect(backend.emitted.map((entry) => entry.message)).toEqual([
        { action: 'midiNoteOn', note: 60, velocity: 0.9 },
        { action: 'midiNoteOff', note: 60 },
        { action: 'midiPitchBend', value: -0.5 },
        { action: 'midiModWheel', value: 0.75 },
        { action: 'midiPanic' },
      ]);
    });

    it('onMidiState receives held notes + wheels from midiNoteState', () => {
      const onMidiState = vi.fn();
      const backend = makeBackend();
      window.__JUCE__ = { backend };

      createBridgeTransport({ onMidiState });

      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'midiNoteState', held: [60, 64], pitchBend: -0.25, modWheel: 0.5,
      });
      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'midiNoteState', held: 'not-an-array', pitchBend: 0, modWheel: 0,
      }); // malformed: ignored

      expect(onMidiState).toHaveBeenCalledTimes(1);
      expect(onMidiState).toHaveBeenCalledWith({ held: [60, 64], pitchBend: -0.25, modWheel: 0.5 });
    });

    it('onModels receives the spectral model slots from modelsState', () => {
      const onModels = vi.fn();
      const backend = makeBackend();
      window.__JUCE__ = { backend };

      createBridgeTransport({ onModels });

      const slots = [
        { slot: 0, isValid: true,
          amplitudes: new Array(64).fill(0), frequencyOffsets: new Array(64).fill(0) },
      ];
      slots[0].amplitudes[0] = 0.5;

      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, { action: 'modelsState', slots });
      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'modelsState', slots: 'not-an-array',
      }); // malformed: ignored

      expect(onModels).toHaveBeenCalledTimes(1);
      expect(onModels).toHaveBeenCalledWith(slots);
    });
  });
});
