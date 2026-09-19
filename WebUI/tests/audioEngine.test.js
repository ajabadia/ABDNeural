/**
 * The ported worklet engine (src/audio/audioWorkletEngine.js).
 *
 * Two things matter here and neither is a formality:
 *
 *   1. inside a host, `startAudioEngine()` must NOT even build an AudioContext —
 *      that is the "two engines sounding" scenario the audio policy exists to
 *      prevent, and the only way to be sure is to watch the constructor;
 *   2. in a browser the lifecycle must still work: ready handshake, sample rate,
 *      the message shapes the worklet understands, and teardown.
 *
 * The AudioContext is faked (jsdom has none, and a real one would need a device):
 * what is under test is the state machine and the wire shapes, not Web Audio.
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import {
  audioEngineState,
  isAudioEngineReady,
  onAudioEngineChange,
  panicWorklet,
  pushEngineToWorklet,
  pushMidiToWorklet,
  pushModelsToWorklet,
  pushParamsToWorklet,
  startAudioEngine,
  teardownAudioEngine,
} from '../src/audio/audioWorkletEngine.js';

let lastNode = null;

class FakePort {
  constructor() {
    this.listeners = [];
    this.posted = [];
  }

  addEventListener(type, fn) {
    if (type === 'message') this.listeners.push(fn);
  }

  removeEventListener(type, fn) {
    this.listeners = this.listeners.filter((listener) => listener !== fn);
  }

  postMessage(message) {
    this.posted.push(message);
  }

  /** Simulate the worklet talking back. */
  emit(data) {
    for (const fn of [...this.listeners]) fn({ data });
  }
}

class FakeAudioWorkletNode {
  constructor(context, name, options) {
    this.context = context;
    this.name = name;
    this.options = options;
    this.port = new FakePort();
    this.connected = false;
    lastNode = this;
  }

  connect() { this.connected = true; }
  disconnect() { this.connected = false; }
}

function installFakeWebAudio() {
  window.AudioWorkletNode = FakeAudioWorkletNode;

  window.AudioContext = class {
    constructor() {
      this.sampleRate = 48000;
      this.destination = {};
      this.audioWorklet = { addModule: async () => {} };
      this.resumed = false;
    }

    async resume() { this.resumed = true; }
    async close() { this.closed = true; }
  };

  vi.stubGlobal('fetch', async () => ({ arrayBuffer: async () => new ArrayBuffer(16) }));
}

afterEach(async () => {
  await teardownAudioEngine();
  lastNode = null;
  delete window.__JUCE__;
  delete window.AudioWorkletNode;
  delete window.AudioContext;
  vi.unstubAllGlobals();
  vi.restoreAllMocks();
});

describe('audio engine / policy', () => {
  it('refuses to start inside a host and never builds an AudioContext', async () => {
    const constructed = vi.fn();

    installFakeWebAudio();
    window.AudioContext = class { constructor() { constructed(); } };
    window.__JUCE__ = { backend: {} };

    const state = await startAudioEngine();

    expect(state.status).toBe('blocked');
    expect(state.error).toContain('motor nativo');
    expect(constructed).not.toHaveBeenCalled();
    expect(isAudioEngineReady()).toBe(false);
  });

  it('reports unsupported when the runtime has no AudioWorklet', async () => {
    const state = await startAudioEngine();

    expect(state.status).toBe('unsupported');
    expect(state.error).toContain('AudioWorkletNode');
  });
});

describe('audio engine / browser lifecycle', () => {
  it('reaches ready, reports the sample rate and announces the state', async () => {
    installFakeWebAudio();

    const seen = [];
    onAudioEngineChange((state) => seen.push(state.status));

    const starting = startAudioEngine();

    await vi.waitFor(() => expect(lastNode).not.toBeNull());
    expect(audioEngineState.status).toBe('loading');

    lastNode.port.emit({ type: 'neuronik:ready' });

    const state = await starting;

    expect(state.status).toBe('ready');
    expect(state.sampleRate).toBe(48000);
    expect(isAudioEngineReady()).toBe(true);
    expect(seen).toContain('ready');
    // The .wasm crossed through processorOptions (the only channel that works
    // from a static export), with the context's real sample rate.
    expect(lastNode.options.processorOptions.sampleRate).toBe(48000);
  });

  it('speaks the worklet message shapes the DSP understands', async () => {
    installFakeWebAudio();

    const starting = startAudioEngine();
    await vi.waitFor(() => expect(lastNode).not.toBeNull());
    lastNode.port.emit({ type: 'neuronik:ready' });
    await starting;

    expect(pushParamsToWorklet({ masterLevel: 1 })).toBe(true);
    const params = lastNode.port.posted.at(0);
    expect(params.type).toBe('neuronik:params');
    // masterLevel is GlobalParams field 0 (see wasm/audioParams.js).
    expect(params.fields).toContainEqual([0, 1]);

    expect(pushEngineToWorklet(1)).toBe(true);
    expect(lastNode.port.posted.at(-1)).toEqual({ type: 'neuronik:engine', index: 1 });

    const slots = [{ slot: 0, isValid: true, amplitudes: [], frequencyOffsets: [] }];
    expect(pushModelsToWorklet(slots)).toBe(true);
    expect(lastNode.port.posted.at(-1)).toEqual({ type: 'neuronik:models', slots });

    expect(pushMidiToWorklet({ kind: 'noteOn', note: 60, velocity: 0.9 })).toBe(true);
    expect(lastNode.port.posted.at(-1)).toEqual({
      type: 'neuronik:midi', kind: 'noteOn', note: 60, velocity: 0.9,
    });

    expect(panicWorklet()).toBe(true);
    expect(lastNode.port.posted.at(-1)).toEqual({ type: 'neuronik:panic' });
  });

  it('teardown returns to idle and every sender goes quiet', async () => {
    installFakeWebAudio();

    const starting = startAudioEngine();
    await vi.waitFor(() => expect(lastNode).not.toBeNull());
    lastNode.port.emit({ type: 'neuronik:ready' });
    await starting;

    await teardownAudioEngine();

    expect(audioEngineState.status).toBe('idle');
    expect(isAudioEngineReady()).toBe(false);
    expect(pushParamsToWorklet({ masterLevel: 1 })).toBe(false);
    expect(pushEngineToWorklet(0)).toBe(false);
    expect(pushModelsToWorklet([])).toBe(false);
    expect(pushMidiToWorklet({ kind: 'noteOn', note: 60 })).toBe(false);
    expect(panicWorklet()).toBe(false);
  });
});
