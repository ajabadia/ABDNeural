/**
 * Integration tests for useParameterControls (React Testing Library).
 *
 * The hook is the glue between the generated contract, the shared control
 * wrappers and the bridge transport. These tests render it with renderHook and
 * a fake window.__JUCE__ backend, pinning down the behaviours the E2E selftest
 * exercises only from the outside:
 *
 *   - seeding happens in the hook's NORMALISED state space (a real-unit seed
 *     was a genuine bug: morphX defaulted to 0 instead of 0.5);
 *   - gesture phases: begin marks the drag window, changes ride as 'change',
 *     everything without a drag lands as 'end';
 *   - native -> JS: snapshots merge into state (unknown ids ignored) and bump
 *     snapshotVersion; single parameterChanged updates one id;
 *   - mount with a bridge announces pageLoaded and requests state;
 *   - local mode keeps the page functional with no __JUCE__ at all.
 */

import { act, renderHook } from '@testing-library/react';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { PILOT_PARAMETER_IDS, getDescriptor, toNormalized } from '../lib/parameters.js';
import { useParameterControls } from '../lib/useParameterControls.js';
import { NATIVE_TO_JS_EVENT_ID } from '../lib/bridge.js';

/** Same fake backend contract as tests/bridge.test.js. */
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
    dispatchFromNative(eventId, message) {
      for (const { fn } of listeners.get(eventId) ?? []) fn(message);
    },
  };
}

describe('useParameterControls', () => {
  beforeEach(() => {
    window.__pilotReady = undefined;
  });

  afterEach(() => {
    delete window.__JUCE__;
  });

  it('seeds defaults in NORMALISED space, not real units', () => {
    const { result } = renderHook(() => useParameterControls());

    for (const id of PILOT_PARAMETER_IDS) {
      const descriptor = getDescriptor(id);

      if (descriptor.kind === 'float')
        expect(result.current.parameters[id]).toBe(toNormalized(descriptor, descriptor.defaultValue));
      else
        expect(result.current.parameters[id]).toBeGreaterThanOrEqual(0);
    }

    // Contract fact worth pinning: morphX is 0..1 with default 0 -> normalised 0.
    // (A wrong -1..1 assumption once expected 0.5 here; the generated contract
    // is the only source of truth for ranges and defaults.)
    expect(getDescriptor('morphX')).toMatchObject({ minValue: 0, maxValue: 1, defaultValue: 0 });
    expect(result.current.parameters.morphX).toBe(0);
  });

  it('local mode: state edits work with no window.__JUCE__', () => {
    const { result } = renderHook(() => useParameterControls());

    expect(result.current.bridgeAvailable).toBe(false);

    act(() => {
      result.current.pushParameter('masterLevel', 0.75, 'end');
    });

    expect(result.current.parameters.masterLevel).toBe(0.75);
    expect(result.current.changeCount).toBe(1);
  });

  it('mount with a bridge announces pageLoaded, requests state and marks ready', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };

    const { result, unmount } = renderHook(() => useParameterControls());

    expect(result.current.bridgeAvailable).toBe(true);
    expect(window.__pilotReady).toBe(true);
    expect(backend.emitted.map((entry) => entry.message.action)).toEqual([
      'pageLoaded',
      'requestState',
      'listPresets',
    ]);

    unmount();
    expect(window.__pilotReady).toBe(false);
  });

  it('preset flow: loadPreset sends the wire message, presetList/presetError update state', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const { result } = renderHook(() => useParameterControls());

    // The page asks for the list on mount; here we simulate the host's answer.

    act(() => {
      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'presetList',
        presets: ['Init Preset', 'Glass Bells'],
        current: 'Init Preset',
      });
    });

    expect(result.current.presetState).toEqual({
      presets: ['Init Preset', 'Glass Bells'],
      current: 'Init Preset',
    });
    expect(result.current.presetError).toBeNull();

    act(() => {
      result.current.loadPreset('Glass Bells');
    });

    const last = backend.emitted.at(-1);
    expect(last.message).toEqual({ action: 'loadPreset', name: 'Glass Bells' });

    act(() => {
      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'presetError',
        operation: 'loadPreset',
        detail: 'preset not found: No Existe',
      });
    });

    expect(result.current.presetError).toEqual({
      operation: 'loadPreset',
      detail: 'preset not found: No Existe',
    });

    act(() => {
      result.current.savePreset('Pad Nocturno');
    });

    expect(backend.emitted.at(-1).message).toEqual({ action: 'savePreset', name: 'Pad Nocturno' });
  });

  it('handleChange without a drag lands as "end", and as "change" inside one', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const { result } = renderHook(() => useParameterControls());

    const morphXSeed = result.current.parameters.morphX;

    act(() => {
      result.current.handleChange('masterLevel', 0.25);
    });

    act(() => {
      result.current.handleGesture('morphX', 'begin');
    });
    act(() => {
      result.current.handleChange('morphX', 0.5);
    });
    act(() => {
      result.current.handleGesture('morphX', 'end');
    });
    act(() => {
      result.current.handleChange('morphX', 0.6);
    });

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
    const { result } = renderHook(() => useParameterControls());

    const versionBefore = result.current.snapshotVersion;

    act(() => {
      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'syncAllParams',
        version: 3,
        parameterCount: 2,
        parameters: [
          { id: 'masterLevel', value: 0.9, real: 0.9, text: '90%' },
          { id: 'ghostParameter', value: 0.1, real: 0.1, text: '?' }, // unknown -> ignored
        ],
      });
    });

    expect(result.current.parameters.masterLevel).toBe(0.9);
    expect(result.current.parameters.morphX).toBeDefined(); // untouched
    expect(result.current.snapshotVersion).toBe(versionBefore + 1);
  });

  it('applies a native parameterChanged for known ids only', () => {
    const backend = makeBackend();
    window.__JUCE__ = { backend };
    const { result } = renderHook(() => useParameterControls());

    act(() => {
      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'parameterChanged', id: 'morphY', value: 0.2, real: -0.6, text: '-0.60',
      });
      backend.dispatchFromNative(NATIVE_TO_JS_EVENT_ID, {
        action: 'parameterChanged', id: 'ghostParameter', value: 0.8, real: 0.8, text: '?',
      });
    });

    expect(result.current.parameters.morphY).toBe(0.2);
  });
});
