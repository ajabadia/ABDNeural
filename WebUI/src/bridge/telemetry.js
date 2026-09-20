/**
 * Telemetry channel (native -> JS) consumer.
 * ===========================================
 *
 * The host polls the processor's visual data and emits `telemetryFrame`
 * messages (~15 Hz, value-diffed: an idle synth emits nothing). The store
 * hands each frame here; this module keeps the LAST frame and fans it out to
 * subscribers (the future spectral visualizer, scope and the XY pad's
 * modulation ring).
 *
 * Frames are NOT app state: they arrive ~15x per second and re-rendering the
 * whole panel per frame would be wasteful, so they live OUTSIDE the store's
 * setState cycle. Subscribers get exactly one call per frame.
 */

let last = null;
let seq = 0;
const subscribers = new Set();

/** Store-side entry point: validate minimally, remember, fan out. */
export function pushTelemetryFrame(frame) {
  if (!frame || !Array.isArray(frame.spectral) || frame.spectral.length !== 64) return;

  last = {
    seq: Number(frame.seq) || ++seq,
    spectral: frame.spectral,
    envelopes: Array.isArray(frame.envelopes) ? frame.envelopes : [],
    lfos: Array.isArray(frame.lfos) ? frame.lfos : [],
    modulation: Array.isArray(frame.modulation) ? frame.modulation : [],
    morph: Array.isArray(frame.morph) ? frame.morph : [],
  };

  for (const notify of subscribers) notify(last);
}

/** Latest frame, or null before the first one (e.g. before the page was bridged). */
export function latestTelemetry() {
  return last;
}

/** Subscribe to frames; returns the unsubscribe function. */
export function onTelemetry(notify) {
  subscribers.add(notify);
  return () => subscribers.delete(notify);
}

/** Test hook: reset every piece of module state. */
export function resetTelemetry() {
  last = null;
  seq = 0;
  subscribers.clear();
}
