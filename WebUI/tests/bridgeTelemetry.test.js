/**
 * Dispatch de telemetryFrame en bridgeCore: el frame llega al handler onTelemetry
 * intacto, y se desuscribe en dispose(). Arnés: window.__JUCE__ falso con la
 * misma API que inyecta JUCE (backend.addEventListener -> id numerico,
 * removeEventListener([eventId, id])).
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import { createBridgeTransport } from '../src/bridge/bridgeCore.js';

function installFakeJuce() {
  const listeners = new Map();
  let nextId = 1;
  const carrier = {
    addEventListener(eventId, listener) {
      const set = listeners.get(eventId) ?? new Set();
      const id = nextId++;
      set.add({ id, listener });
      listeners.set(eventId, set);
      return id;
    },
    removeEventListener(entry) {
      const [eventId, id] = entry;
      const set = listeners.get(eventId);
      if (!set) return;
      for (const item of set) if (item.id === id) set.delete(item);
    },
    emit(eventId, message) {
      for (const { listener } of listeners.get(eventId) ?? []) listener(message);
    },
  };
  window.__JUCE__ = { backend: carrier };
  return carrier;
}

const FRAME = {
  action: 'telemetryFrame',
  seq: 7,
  spectral: new Array(64).fill(0.5),
  envelopes: [0.1, 0.9],
  lfos: [0.25, 0.75],
  modulation: new Array(28).fill(0),
  morph: [0.3, 0.6],
};

describe('bridgeCore: telemetryFrame', () => {
  afterEach(() => {
    delete window.__JUCE__;
  });

  it('reparte el frame al handler onTelemetry', () => {
    const carrier = installFakeJuce();
    const onTelemetry = vi.fn();
    createBridgeTransport({ onTelemetry });

    carrier.emit('event', FRAME);
    expect(onTelemetry).toHaveBeenCalledTimes(1);
    expect(onTelemetry.mock.calls[0][0]).toBe(FRAME);
  });

  it('ignora frames malformados (sin spectral)', () => {
    const carrier = installFakeJuce();
    const onTelemetry = vi.fn();
    createBridgeTransport({ onTelemetry });

    carrier.emit('event', { action: 'telemetryFrame', seq: 1 });
    expect(onTelemetry).not.toHaveBeenCalled();
  });

  it('desuscribe el listener en disconnect', () => {
    const carrier = installFakeJuce();
    const onTelemetry = vi.fn();
    const bridge = createBridgeTransport({ onTelemetry });

    bridge.dispose();
    carrier.emit('event', FRAME);
    expect(onTelemetry).not.toHaveBeenCalled();
  });
});
