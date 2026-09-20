/**
 * Telemetry channel (native -> JS) consumer.
 * ===========================================
 *
 * The store hands each frame here (`pushTelemetryFrame`); this module keeps
 * the LAST frame and fans it out once per frame to subscribers (the future
 * spectral visualizer, scope and modulation ring). Frames are deliberately
 * OUTSIDE the store's setState cycle: they arrive ~15x per second.
 *
 * The `seq` contract: frames carry the native sequence number; when it is
 * absent (0/falsy), the consumer assigns a monotonically increasing one.
 * A frame with an out-of-order seq is still accepted (we keep the last one
 * by arrival order), which the protocol test pins as tolerant behaviour.
 */

import { describe, expect, it, vi } from 'vitest';

import {
  latestTelemetry,
  onTelemetry,
  pushTelemetryFrame,
  resetTelemetry,
} from '../src/bridge/telemetry.js';

const FRAME = {
  action: 'telemetryFrame',
  seq: 3,
  spectral: new Array(64).fill(0.5),
  envelopes: [0.1, 0.9],
  lfos: [0.25, 0.75],
  modulation: new Array(28).fill(0),
  morph: [0.3, 0.6],
};

describe('telemetry consumer', () => {
  it('guarda el ultimo frame y lo expone en latestTelemetry', () => {
    resetTelemetry();
    pushTelemetryFrame({ ...FRAME, seq: 1 });
    pushTelemetryFrame({ ...FRAME, seq: 2 });
    expect(latestTelemetry().seq).toBe(2);
  });

  it('asigna seq monotonica cuando el frame no la trae', () => {
    resetTelemetry();
    pushTelemetryFrame({ ...FRAME, seq: 0 });
    pushTelemetryFrame({ ...FRAME, seq: 0 });
    expect(latestTelemetry().seq).toBe(2);
  });

  it('descarta frames malformados (spectral ausente o distinto de 64)', () => {
    resetTelemetry();
    const notify = vi.fn();
    onTelemetry(notify);

    pushTelemetryFrame(null);
    pushTelemetryFrame({ ...FRAME, seq: 5, spectral: undefined });
    pushTelemetryFrame({ ...FRAME, seq: 6, spectral: new Array(63).fill(0) });

    expect(latestTelemetry()).toBeNull();
    expect(notify).not.toHaveBeenCalled();
  });

  it('reparte una sola llamada por frame a cada suscriptor', () => {
    resetTelemetry();
    const a = vi.fn();
    const b = vi.fn();
    onTelemetry(a);
    onTelemetry(b);

    pushTelemetryFrame({ ...FRAME, seq: 9 });

    expect(a).toHaveBeenCalledTimes(1);
    expect(b).toHaveBeenCalledTimes(1);
    expect(a.mock.calls[0][0].seq).toBe(9);
  });

  it('unsubscribe deja de repartir', () => {
    resetTelemetry();
    const a = vi.fn();
    const off = onTelemetry(a);
    off();

    pushTelemetryFrame({ ...FRAME, seq: 10 });
    expect(a).not.toHaveBeenCalled();
  });

  it('normaliza los arrays secundarios ausentes a vacios', () => {
    resetTelemetry();
    pushTelemetryFrame({ action: 'telemetryFrame', spectral: new Array(64).fill(0) });
    const f = latestTelemetry();
    expect(f.envelopes).toEqual([]);
    expect(f.lfos).toEqual([]);
    expect(f.modulation).toEqual([]);
    expect(f.morph).toEqual([]);
  });
});
