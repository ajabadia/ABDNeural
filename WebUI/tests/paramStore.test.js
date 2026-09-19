/**
 * Integration tests for the vanilla store (src/contracts/paramStore.js), the
 * port of the pilot's `useParameterControls` hook.
 *
 * Where the hook tests rendered with renderHook + Testing Library, here the
 * store is just an object: `getState()` in, `subscribe()` out. The behaviours
 * pinned are the ones the E2E selftest only sees from the outside:
 *
 *   - seeding happens in the store's NORMALISED state space (a real-unit seed
 *     was a genuine bug: morphX defaulted to 0 instead of 0.5);
 *   - gesture phases: begin marks the drag window, changes ride as 'change',
 *     everything without a drag lands as 'end';
 *   - native -> JS: snapshots merge into state (unknown ids ignored) and bump
 *     snapshotVersion; single parameterChanged updates one id;
 *   - mount with a bridge announces pageLoaded and requests state;
 *   - local mode keeps the page functional with no __JUCE__ at all.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { PILOT_PARAMETER_IDS, getDescriptor, toNormalized } from '../src/contracts/parameters.js';
import { createParameterStore } from '../src/contracts/paramStore.js';
import { NATIVE_TO_JS_EVENT_ID } from '../src/bridge/bridgeCore.js';

/** Same fake backend contract as tests/bridgeCore.test.js. */
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
    dispatchFromNative(eventId, message) {
      for (const { fn } of listeners.get(eventId) ?? []) fn(message);
    },
  };
}

describe('createParameterStore', () => {
  beforeEach(() => {
    window.__pilotReady = undefined;
  });

  afterEach(() => {
    delete window.__JUCE__;
  });

  it('seeds defaults in NORMALISED space, not real units', () => {
    const store = createParameterStore();
    const { parameters } = store.getState();

    for (const id of PILOT_PARAMETER_IDS) {
      const descriptor = getDescriptor(id);

      if (descriptor.kind === 'float')
        expect(parameters[id]).toBe(toNormalized(descriptor, descriptor.defaultValue));
      else
        expect(parameters[id]).toBeGreaterThanOrEqual(0);
    }

    // Contract fact worth pinning: morphX is 0..1 with default 0 -> normalised 0.
    // (A wrong -1..1 assumption once expected 0.5 here; the generated contract
    // is the only source of truth for ranges and defaults.)
    expect(getDescriptor('morphX')).toMatchObject({ minValue: 0, maxValue: 1, defaultValue: 0 });
    expect(parameters.morphX).toBe(0);
  });

  it('local mode: state edits work with no window.__JUCE__', () => {
    const store = createParameterStore();
    store.start();

    expect(store.getState().bridgeAvailable).toBe(false);

    store.pushParameter('masterLevel', 0.75, 'end');

    expect(store.getState().parameters.masterLevel).toBe(0.75);
    expect(store.getState().changeCount).toBe(1);
  });

  it('start with a bridge announces pageLoaded, requests state and marks ready', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };

    const store = createParameterStore();
    store.start();

    expect(store.getState().bridgeAvailable).toBe(true);
    expect(window.__pilotReady).toBe(true);
    expect(backend.emitted.map((entry) => entry.message.action)).toEqual([
      'pageLoaded',
      'requestState',
      'listPresets',
    ]);

    store.dispose();
    expect(window.__pilotReady).toBe(false);
  });

  it('randomize: es una accion de ESTADO y sin host no finge nada', () => {
    const store = createParameterStore();

    // Sin host no hay APVTS que sortear: no se emite nada y devuelve false, para
    // que la pagina no pueda decir "he sorteado" cuando no hay plugin.
    expect(store.randomize()).toBe(false);

    const backend = makeBackend();
    window.__JUCE__ = { backend };

    const online = createParameterStore();
    online.start();

    expect(online.randomize()).toBe(true);
    expect(backend.emitted.at(-1).message).toEqual({ action: 'randomize' });

    online.dispose();
    store.dispose();
  });

  it('loadModel: pide la ranura al host y el error de la carga llega al estado', () => {
    const store = createParameterStore();

    // Sin host no hay dialogo que abrir: no se emite nada y devuelve false.
    expect(store.loadModel(0)).toBe(false);

    const backend = makeBackend();
    window.__JUCE__ = { backend };

    const online = createParameterStore();
    online.start();

    expect(online.loadModel(2)).toBe(true);
    expect(backend.emitted.at(-1).message).toEqual({ action: 'loadModel', slot: 2 });

    // El host cancela el dialogo: el motivo queda en el estado (la vista lo enseña).
    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'modelError', slot: 2, detail: 'no file chosen',
    });

    expect(online.getState().modelError).toEqual({ slot: 2, detail: 'no file chosen' });

    // Y una respuesta nueva del motor limpia el fallo: los slots frescos son la
    // verdad de lo que hay cargado.
    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'modelsState',
      slots: [{ slot: 2, name: 'Campana', isValid: true, amplitudes: [], frequencyOffsets: [] }],
    });

    expect(online.getState().modelError).toBeNull();
    expect(online.getState().models[0].name).toBe('Campana');

    // Y un intento nuevo tambien lo limpia: el mensaje viejo es de otra carga.
    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'modelError', slot: 2, detail: 'not a usable .neuronikmodel: x',
    });
    expect(online.getState().modelError).not.toBeNull();

    online.loadModel(2);
    expect(online.getState().modelError).toBeNull();

    online.dispose();
    store.dispose();
  });

  it('preset flow: loadPreset sends the wire message, presetList/presetError update state', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const store = createParameterStore();
    store.start();

    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'presetList',
      presets: ['Init Preset', 'Glass Bells'],
      current: 'Init Preset',
    });

    expect(store.getState().presetState).toEqual({
      presets: ['Init Preset', 'Glass Bells'],
      current: 'Init Preset',
    });
    expect(store.getState().presetError).toBeNull();

    store.loadPreset('Glass Bells');

    expect(backend.emitted.at(-1).message).toEqual({ action: 'loadPreset', name: 'Glass Bells' });

    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'presetError',
      operation: 'loadPreset',
      detail: 'preset not found: No Existe',
    });

    expect(store.getState().presetError).toEqual({
      operation: 'loadPreset',
      detail: 'preset not found: No Existe',
    });

    store.savePreset('Pad Nocturno');

    expect(backend.emitted.at(-1).message).toEqual({ action: 'savePreset', name: 'Pad Nocturno' });
  });

  it('handleChange without a drag lands as "end", and as "change" inside one', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const store = createParameterStore();
    store.start();

    const morphXSeed = store.getState().parameters.morphX;

    store.handleChange('masterLevel', 0.25);

    store.handleGesture('morphX', 'begin');
    store.handleChange('morphX', 0.5);
    store.handleGesture('morphX', 'end');
    store.handleChange('morphX', 0.6);

    const changes = backend.emitted
      .filter((entry) => entry.message.action === 'parameterChanged');

    expect(changes).toEqual([
      expect.objectContaining({ message: { action: 'parameterChanged', id: 'masterLevel', value: 0.25, gesture: 'end' } }),
      // gesture begin carries the value AT DRAG START (the seeded default)
      expect.objectContaining({ message: { action: 'parameterChanged', id: 'morphX', value: morphXSeed, gesture: 'begin' } }),
      expect.objectContaining({ message: { action: 'parameterChanged', id: 'morphX', value: 0.5, gesture: 'change' } }),
      expect.objectContaining({ message: { action: 'parameterChanged', id: 'morphX', value: 0.6, gesture: 'end' } }),
    ]);
  });

  it('merges a native snapshot into state and bumps snapshotVersion', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const store = createParameterStore();
    store.start();

    const versionBefore = store.getState().snapshotVersion;

    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'syncAllParams',
      version: 3,
      parameterCount: 2,
      parameters: [
        { id: 'masterLevel', value: 0.9, real: 0.9, text: '90%' },
        { id: 'ghostParameter', value: 0.1, real: 0.1, text: '?' }, // unknown -> ignored
      ],
    });

    expect(store.getState().parameters.masterLevel).toBe(0.9);
    expect(store.getState().parameters.morphX).toBeDefined(); // untouched
    expect(store.getState().snapshotVersion).toBe(versionBefore + 1);
  });

  it('snapshot and single change keep contractErrors in sync (normalised space)', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const store = createParameterStore();
    store.start();

    expect(store.getState().contractErrors).toEqual([]);

    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'parameterChanged', id: 'masterLevel', value: 1.5, real: 1.5, text: '150%',
    });

    expect(store.getState().contractErrors).toEqual([
      '"masterLevel" out of normalised range: 1.5',
    ]);
  });

  it('applies a native parameterChanged for known ids only', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const store = createParameterStore();
    store.start();

    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'parameterChanged', id: 'morphY', value: 0.2, real: -0.6, text: '-0.60',
    });
    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'parameterChanged', id: 'ghostParameter', value: 0.8, real: 0.8, text: '?',
    });

    expect(store.getState().parameters.morphY).toBe(0.2);
  });

  it('midi: __pilotSendMidi drives the wire path and midiNoteState updates midiState', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const store = createParameterStore();
    store.start();

    expect(typeof window.__pilotSendMidi).toBe('function');

    window.__pilotSendMidi({ action: 'midiNoteOn', note: 60, velocity: 0.9 });
    window.__pilotSendMidi({ action: 'midiPitchBend', value: -0.5 });
    window.__pilotSendMidi({ action: 'midiModWheel', value: 0.5 });
    window.__pilotSendMidi({ action: 'midiPanic' });
    window.__pilotSendMidi({ action: 'garbage' }); // unknown: ignored

    const actions = backend.emitted.map((entry) => entry.message.action);
    expect(actions).toContain('midiNoteOn');
    expect(actions).toContain('midiPitchBend');
    expect(actions).toContain('midiModWheel');
    expect(actions).toContain('midiPanic');
    expect(actions).not.toContain('garbage');
    expect(backend.emitted.find((e) => e.message.action === 'midiNoteOn').message)
      .toEqual({ action: 'midiNoteOn', note: 60, velocity: 0.9 });

    window.__pilotSendMidi({ action: 'midiNoteOff', note: 60 });
    expect(backend.emitted.at(-1).message).toEqual({ action: 'midiNoteOff', note: 60 });

    backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
      action: 'midiNoteState', held: [60], pitchBend: -0.5, modWheel: 0.5,
    });
    expect(store.getState().midiState).toEqual({ held: [60], pitchBend: -0.5, modWheel: 0.5 });
  });

  it('midi handle is removed on dispose', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };

    const store = createParameterStore();
    store.start();
    expect(window.__pilotSendMidi).toBeDefined();

    store.dispose();
    expect(window.__pilotSendMidi).toBeUndefined();
  });

  it('dispose drops the native listeners and re-subscribing state stays consistent', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const store = createParameterStore();
    store.start();

    // Una por mensaje nativo del contrato: snapshot, parameterChanged, presetList,
    // presetError, midiNoteState, modelsState y modelError.
    expect(backend.listenerCount(NATIVE_TO_JS_EVENT_ID)).toBe(7);

    store.dispose();

    expect(backend.listenerCount(NATIVE_TO_JS_EVENT_ID)).toBe(0);
    expect(store.getState().bridgeAvailable).toBe(false);
  });

  it('subscribe paints immediately, on every change and unsubscribes cleanly', () => {
    const store = createParameterStore();
    const listener = vi.fn();

    const unsubscribe = store.subscribe(listener);

    expect(listener).toHaveBeenCalledTimes(1);
    expect(listener.mock.calls[0][0].parameters.masterLevel).toBeDefined();

    store.pushParameter('masterLevel', 0.4, 'end');
    expect(listener).toHaveBeenCalledTimes(2);
    expect(listener.mock.calls[1][0].parameters.masterLevel).toBe(0.4);

    unsubscribe();
    store.pushParameter('masterLevel', 0.9, 'end');
    expect(listener).toHaveBeenCalledTimes(2);
  });

  it('start is idempotent (no duplicated host announcements)', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const store = createParameterStore();

    store.start();
    store.start();

    expect(backend.emitted.map((entry) => entry.message.action)).toEqual([
      'pageLoaded',
      'requestState',
      'listPresets',
    ]);
  });
});
