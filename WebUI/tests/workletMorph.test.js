/**
 * El pad XY y las ranuras en MODO LOCAL (sin host):
 *
 *   1. pushMorphToWorklet cruza el mensaje `neuronik:morph` con (x, y, z) — el
 *      camino que hace que el pad llegue al motor WASM (neuronikSetVoiceMorph);
 *   2. el store en local: loadModel(slot, { requestLocalFile }) pide el fichero
 *      en vez de devolver false, loadLocalModel parsea y actualiza `models`,
 *      y seedLocalModels solo siembra sin bridge;
 *   3. los errores del parser se pintan en modelError (el mismo canal del host).
 *
 * El AudioContext se fakea como en audioEngine.test.js; lo que se prueba es el
 * cable, no Web Audio.
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import {
  getWorkletMorph,
  isAudioEngineReady,
  pushMorphToWorklet,
  startAudioEngine,
} from '../src/audio/audioWorkletEngine.js';
import { createParameterStore } from '../src/contracts/paramStore.js';
import { emptyLocalModels } from '../src/audio/localModels.js';

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

  start() { this.started = true; }

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
    lastNode = this;
  }

  connect() {}
  disconnect() {}
}

function installFakeWebAudio() {
  window.AudioWorkletNode = FakeAudioWorkletNode;

  window.AudioContext = class {
    constructor() {
      this.sampleRate = 48000;
      this.state = 'running';
      this.destination = {};
      this.audioWorklet = { addModule: async () => {} };
      this.resumed = false;
    }

    async resume() { this.resumed = true; }
    async close() { this.closed = true; }
  };

  vi.stubGlobal('fetch', async () => ({ arrayBuffer: async () => new ArrayBuffer(16) }));
}

/**
 * Ready SIN carrera: emite en sondeo hasta que el motor llega a ready.
 * Un unico emit puede perderse (el listener de waitForReady se registra
 * despues de que el nodo existe); repetirlo es inocuo — las emisiones
 * tardias caen en un port que ya no espera nada.
 */
async function fakeWorkletReady() {
  await vi.waitFor(() => {
    lastNode?.port.emit({ type: 'neuronik:ready' });   // emit ya cruza { data }
    expect(isAudioEngineReady()).toBe(true);
  });
}

function validModelText(name = 'CZ-BASS1') {
  return JSON.stringify({
    name,
    format: 2,
    amplitudes: Array.from({ length: 64 }, (_, i) => (i === 0 ? 1 : 0)),
    frequencyOffsets: new Array(64).fill(0),
  });
}

afterEach(async () => {
  const { teardownAudioEngine } = await import('../src/audio/audioWorkletEngine.js');
  await teardownAudioEngine();
  delete window.AudioWorkletNode;
  delete window.AudioContext;
  lastNode = null;
});

describe('worklet / pushMorphToWorklet', () => {
  it('sin motor devuelve false y no revienta', () => {
    expect(pushMorphToWorklet(0.2, 0.8)).toBe(false);
  });

  it('con motor listo cruza neuronik:morph con (x, y, z)', async () => {
    installFakeWebAudio();
    const startPromise = startAudioEngine();
    await vi.waitFor(() => expect(lastNode).not.toBeNull());
    await fakeWorkletReady();
    await startPromise;

    expect(pushMorphToWorklet(0.25, 0.75, 0.5)).toBe(true);

    const morph = lastNode.port.posted.find((message) => message.type === 'neuronik:morph');
    expect(morph).toEqual({ type: 'neuronik:morph', x: 0.25, y: 0.75, z: 0.5 });
  });

  it('recuerda el ultimo morph (re-apply al cambiar de motor)', async () => {
    installFakeWebAudio();
    const startPromise = startAudioEngine();
    await vi.waitFor(() => expect(lastNode).not.toBeNull());
    await fakeWorkletReady();
    await startPromise;

    pushMorphToWorklet(0.1, 0.9, 0.3);

    // FASE 11.4: el estado recuerda tambien los volumenes de capa (z2/z3,
    // default 0: reposo en las capas extra).
    expect(getWorkletMorph()).toEqual({ x: 0.1, y: 0.9, z: 0.3, z2: 0, z3: 0 });
  });
});

describe('store / ranuras en modo local', () => {
  function localStore() {
    const store = createParameterStore({ ids: ['morphX', 'morphY', 'morphZ', 'masterLevel'] });
    store.start();
    return store;
  }

  it('sin host y sin handler, loadModel sigue devolviendo false', () => {
    const store = localStore();

    expect(store.loadModel(0)).toBe(false);
  });

  it('sin host y con handler, loadModel pide el fichero local', () => {
    const store = localStore();
    const requested = [];
    const requestLocalFile = (slot) => requested.push(slot);

    expect(store.loadModel(2, { requestLocalFile })).toBe(true);
    expect(requested).toEqual([2]);
  });

  it('seedLocalModels siembra las ranuras vacias sin bridge', () => {
    const store = localStore();

    expect(store.seedLocalModels(emptyLocalModels())).toBe(true);
    expect(store.getState().models).toHaveLength(4);
    expect(store.getState().models.every((model) => model.name === 'EMPTY')).toBe(true);
  });

  it('loadLocalModel parsea, fija la ranura y actualiza models', async () => {
    const store = localStore();
    store.seedLocalModels(emptyLocalModels());

    const file = new File([validModelText()], 'CZ-BASS1.neuronikmodel');
    expect(await store.loadLocalModel(file, 1)).toBe(true);

    const slot = store.getState().models.find((model) => model.slot === 1);
    expect(slot.name).toBe('CZ-BASS1');
    expect(slot.isValid).toBe(true);
    expect(slot.amplitudes).toHaveLength(64);
  });

  it('loadLocalModel pinta el error del parser en modelError', async () => {
    const store = localStore();
    store.seedLocalModels(emptyLocalModels());

    const file = new File(['no es json'], 'roto.neuronikmodel');
    expect(await store.loadLocalModel(file, 0)).toBe(false);
    expect(store.getState().modelError?.detail).toContain('no es JSON');
    expect(store.getState().modelError?.slot).toBe(0);
  });

  it('loadLocalModel sin ranuras sembradas no finge nada', async () => {
    const store = localStore();

    const file = new File([validModelText()], 'x.neuronikmodel');
    expect(await store.loadLocalModel(file, 0)).toBe(false);
  });
});
