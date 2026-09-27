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
 *   - local mode keeps the page functional with no __JUCE__ at all;
 *   - MODO LOCAL: las ranuras de modelo se recuerdan entre cargas (localStorage), y
 *     con host la memoria ni se lee (manda modelsState);
 *   - OLVIDAR una ranura la deja EMPTY en el estado Y la saca de la memoria, y avisa
 *     por el mismo canal que un fallo de carga (con su propio tono).
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import {
  PILOT_PARAMETER_IDS,
  defaultNormalizedState,
  getDescriptor,
  toNormalized,
} from '../src/contracts/parameters.js';
import { createParameterStore } from '../src/contracts/paramStore.js';
import { emptyLocalModels } from '../src/audio/localModels.js';
import {
  clearLocalModelCache,
  readCachedModelTexts,
  saveCachedModelText,
} from '../src/audio/localModelCache.js';
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
    // presetError, midiNoteState, modelsState, modelError, midiCcState y
    // telemetryFrame.
    expect(backend.listenerCount(NATIVE_TO_JS_EVENT_ID)).toBe(9);

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
/**
 * MODO LOCAL: la ruta por defecto de la MATRIZ al eje temporal del pad.
 *
 * Fuera del host el motor WASM arranca con la matriz del contrato (los slots 3 y 4
 * en Off), asi que `_neuronikGetMod(morphZ)` vale 0 y el anillo del pad no se mueve
 * aunque el LFO este corriendo. El store SIEMBRA esa ruta en local - LFO 2 ->
 * Morph Z al 100%, en el primer slot LIBRE - y solo si nadie lo ha tocado: con
 * host manda el APVTS y una ruta del usuario no se pisa.
 */
describe('store / ruta local a Morph Z (el anillo del pad sin host)', () => {
  const MATRIX_IDS = [
    'mod1Source', 'mod1Destination', 'mod1Amount',
    'mod2Source', 'mod2Destination', 'mod2Amount',
    'mod3Source', 'mod3Destination', 'mod3Amount',
    'lfo2RateHz', 'lfo2Depth', 'morphZ',
  ];

  // Los indices de choice NUNCA se escriben a mano: salen de la tabla del contrato
  // (crecer la tabla re-mapearia un indice literal).
  const normalizedChoice = (id, label) => {
    const { choices } = getDescriptor(id);
    return choices.indexOf(label) / (choices.length - 1);
  };

  const localStore = () => createParameterStore({ ids: MATRIX_IDS });

  afterEach(() => {
    delete window.__JUCE__;
  });

  it('siembra LFO 2 -> Morph Z en el primer slot LIBRE (sin tocar las ENV)', () => {
    const store = localStore();
    store.start();

    // Los slots 1 y 2 llevan las rutas de las envolventes del contrato: la siembra
    // usa el 3 para no cambiar el sonido del legado.
    expect(store.getState().parameters.mod1Source).toBe(normalizedChoice('mod1Source', 'ENV 1'));
    expect(store.getState().parameters.mod2Source).toBe(normalizedChoice('mod2Source', 'ENV 2'));

    expect(store.seedLocalMorphZRoute()).toBe(true);

    const { parameters, contractErrors } = store.getState();
    expect(parameters.mod3Source).toBe(normalizedChoice('mod3Source', 'LFO 2'));
    expect(parameters.mod3Destination).toBe(normalizedChoice('mod3Destination', 'Morph Z'));
    // El amount es -1..1 y la ruta pide el barrido entero (el arco da la vuelta).
    expect(parameters.mod3Amount).toBe(toNormalized(getDescriptor('mod3Amount'), 1));
    // Sembrar es un estado legal: el contrato no protesta.
    expect(contractErrors).toEqual([]);

    store.dispose();
  });

  it('no pisa una ruta ya asignada (ni del usuario ni de un snapshot nativo)', () => {
    const store = localStore();
    store.start();

    store.pushParameter('mod3Source', normalizedChoice('mod3Source', 'LFO 1'), 'end');

    expect(store.seedLocalMorphZRoute()).toBe(false);
    expect(store.getState().parameters.mod3Source)
      .toBe(normalizedChoice('mod3Source', 'LFO 1'));
    expect(store.getState().parameters.mod3Destination).toBe(0);

    store.dispose();
  });

  it('con host no siembra nada: la matriz la manda el APVTS', () => {
    window.__JUCE__ = { backend: makeBackend() };

    const store = localStore();
    store.start();

    expect(store.getState().bridgeAvailable).toBe(true);
    expect(store.seedLocalMorphZRoute()).toBe(false);
    expect(store.getState().parameters.mod3Source).toBe(0);

    store.dispose();
  });

  it('un store que no posee los ids de la matriz no escribe a medias', () => {
    // El store minimo (los PILOT) no tiene la MATRIZ: sembrar ahi solo generaria
    // contractErrors de ids desconocidos, asi que la siembra se rechaza entera.
    const store = createParameterStore();
    store.start();

    expect(store.seedLocalMorphZRoute()).toBe(false);
    expect(store.getState().contractErrors).toEqual([]);

    store.dispose();
  });
});

/**
 * EL CONMUTADOR de la ruta local del pad (ficha MATRIZ): la siembra se puede
 * apagar y cambiar de LFO sin entrar al cajon, y el valor inicial es la siembra
 * misma (encendida, con LFO 2) para que arrancar sin ella sea una decision del
 * usuario y no un cambio de comportamiento.
 *
 * Lo que fija el test: el estado inicial sale del CONTRATO y su lista de LFO
 * tambien (no una constante escrita a mano), OFF devuelve la fila a VIRGEN (los
 * defaults del propio contrato de sus tres ids) y remember el LFO elegido, un
 * LFO que la tabla no lista se rechaza entero sin dejar la fila a medias, con
 * host no se toca nada, y un gesto se aplica con UN solo aviso (pintada y motor
 * local se enteran de la misma verdad, sin un frame con la fila cambiada y el
 * conmutador sin la suya).
 */
describe('store / el conmutador de la ruta local del pad', () => {
  // El store con la MATRIZ y sin host. Los ids y el helper de choice son los del
  // bloque de la siembra, replicados aqui porque cada describe tiene los suyos
  // (y porque el conmutador necesita la fila 3 Y el store entero, no el minimo).
  const MATRIX_IDS = [
    'mod1Source', 'mod1Destination', 'mod1Amount',
    'mod2Source', 'mod2Destination', 'mod2Amount',
    'mod3Source', 'mod3Destination', 'mod3Amount',
    'lfo2RateHz', 'lfo2Depth', 'morphZ',
  ];

  // Los indices de choice NUNCA se escriben a mano: salen de la tabla del contrato.
  const normalizedChoice = (id, label) => {
    const { choices } = getDescriptor(id);
    return choices.indexOf(label) / (choices.length - 1);
  };

  const localStore = () => createParameterStore({ ids: MATRIX_IDS });

  afterEach(() => {
    delete window.__JUCE__;
  });

  it('arranca con la siembra de fabrica: encendida y con el LFO que sembraba', () => {
    const store = localStore();
    store.start();

    const { localMorphRoute } = store.getState();

    expect(localMorphRoute.enabled).toBe(true);
    // El LFO no esta escrito a mano en la vista ni en el test: sale de la
    // siembra (LOCAL_MORPH_Z_ROUTE), que es la SSOT de la ruta.
    expect(localMorphRoute.source).toBe('LFO 2');
    // Y la lista de LFO sale de la TABLA de fuentes del contrato.
    expect(localMorphRoute.sources).toEqual(['LFO 1', 'LFO 2']);

    store.dispose();
  });

  it('apagarla devuelve la fila a VIRGEN y la siembra deja de hacer nada', () => {
    const store = localStore();
    store.start();

    store.seedLocalMorphZRoute();

    expect(store.getState().parameters.mod3Source)
      .toBe(normalizedChoice('mod3Source', 'LFO 2'));

    expect(store.setLocalMorphRoute({ enabled: false })).toBe(true);

    const { parameters, localMorphRoute } = store.getState();

    // VIRGEN = los defaults del contrato de los tres ids de la fila.
    expect(parameters.mod3Source).toBe(0);
    expect(parameters.mod3Destination).toBe(0);
    expect(parameters.mod3Amount).toBe(defaultNormalizedState(['mod3Amount']).mod3Amount);
    expect(localMorphRoute.enabled).toBe(false);
    // El LFO elegido SE GUARDA: apagar no lo olvida.
    expect(localMorphRoute.source).toBe('LFO 2');

    // Y con el conmutador apagado, sembrar es no sembrar.
    const otro = localStore();
    otro.start();
    otro.setLocalMorphRoute({ enabled: false });
    expect(otro.seedLocalMorphZRoute()).toBe(false);
    expect(otro.getState().parameters.mod3Source).toBe(0);

    store.dispose();
    otro.dispose();
  });

  it('encenderla con otro LFO escribe ESA fila (y es un gesto, no una siembra)', () => {
    const store = localStore();
    store.start();

    expect(store.setLocalMorphRoute({ source: 'LFO 1' })).toBe(true);

    const { parameters, localMorphRoute } = store.getState();

    expect(parameters.mod3Source).toBe(normalizedChoice('mod3Source', 'LFO 1'));
    expect(parameters.mod3Destination).toBe(normalizedChoice('mod3Destination', 'Morph Z'));
    expect(parameters.mod3Amount).toBe(toNormalized(getDescriptor('mod3Amount'), 1));
    expect(localMorphRoute).toMatchObject({ enabled: true, source: 'LFO 1' });

    // Y el gesto EXPLICITO escribe una fila que ya tuviera dueno (a diferencia de
    // la siembra, que solo pisa slots virgenes).
    store.pushParameter('mod3Source', normalizedChoice('mod3Source', 'ENV 1'), 'end');
    expect(store.setLocalMorphRoute({ source: 'LFO 2' })).toBe(true);
    expect(store.getState().parameters.mod3Source)
      .toBe(normalizedChoice('mod3Source', 'LFO 2'));

    store.dispose();
  });

  it('un LFO que la tabla no lista se rechaza entero (no a medias)', () => {
    const store = localStore();
    store.start();

    store.seedLocalMorphZRoute();

    expect(store.setLocalMorphRoute({ source: 'LFO 9' })).toBe(false);

    const { parameters, localMorphRoute } = store.getState();

    // Ni el estado ni la fila se mueven: un rechazo no es medio gesto.
    expect(localMorphRoute.source).toBe('LFO 2');
    expect(parameters.mod3Source).toBe(normalizedChoice('mod3Source', 'LFO 2'));
    expect(store.getState().contractErrors).toEqual([]);

    store.dispose();
  });

  it('con host no hace nada: la matriz es del APVTS', () => {
    window.__JUCE__ = { backend: makeBackend() };

    const store = localStore();
    store.start();

    expect(store.getState().bridgeAvailable).toBe(true);
    expect(store.setLocalMorphRoute({ enabled: false })).toBe(false);
    expect(store.setLocalMorphRoute({ source: 'LFO 1' })).toBe(false);
    expect(store.getState().parameters.mod3Source).toBe(0);
    expect(store.getState().localMorphRoute.enabled).toBe(true);

    store.dispose();
  });

  it('un gesto = UN aviso (la fila y el conmutador en la misma verdad)', () => {
    const store = localStore();
    store.start();
    store.seedLocalMorphZRoute();

    let avisos = 0;

    store.subscribe(() => { avisos += 1; });

    // `subscribe()` pinta al instante (contrato del store): ese primer aviso es
    // del alta, no del gesto, asi que el contador arranca aqui.
    avisos = 0;

    store.setLocalMorphRoute({ enabled: false });

    expect(avisos).toBe(1);
    expect(store.getState().localMorphRoute.enabled).toBe(false);

    avisos = 0;
    store.setLocalMorphRoute({ enabled: true, source: 'LFO 1' });

    expect(avisos).toBe(1);

    store.dispose();
  });
});

/**
 * MODO LOCAL: la memoria de las ranuras (localStorage).
 *
 * El plugin vuelve a sus cuatro ranuras porque el PRESET lleva la ruta del fichero
 * (`modelPath<slot>`); en el navegador no hay preset ni sistema de ficheros, asi que
 * sin memoria cada recarga empezaba con las cuatro EMPTY y habia que volver a buscar
 * los mismos ficheros a mano.
 *
 * Lo que fija el test: lo guardado es el TEXTO del fichero (no el objeto parseado, que
 * seria un dialecto derivado), una ranura recordada vuelve por el MISMO parser que un
 * fichero recien elegido, con host no se toca nada (manda `modelsState`), una entrada
 * corrupta se descarta en vez de llevarse por delante el arranque, y sin almacen la
 * carga funciona igual (la memoria es un extra, no un requisito) con su aviso.
 */
describe('store / memoria local de las ranuras (localStorage)', () => {
  const modelJson = (name = 'CZ-BASS1') => JSON.stringify({
    name,
    format: 2,
    amplitudes: Array.from({ length: 64 }, (_, i) => (i === 0 ? 1 : 0.25 / (i + 1))),
    frequencyOffsets: new Array(64).fill(0),
  });

  const fileOf = (text) => new File([text], 'CZ-BASS1.neuronikmodel');

  /** El arranque local de app.js: store sin host + las cuatro ranuras vacias. */
  const localStore = () => {
    const store = createParameterStore();
    store.start();
    store.seedLocalModels(emptyLocalModels());
    return store;
  };

  beforeEach(() => {
    clearLocalModelCache();
  });

  afterEach(() => {
    clearLocalModelCache();
    delete window.__JUCE__;
  });

  it('recuerda la ranura cargada y la repuebla en la siguiente carga', async () => {
    const store = localStore();
    const text = modelJson();

    expect(await store.loadLocalModel(fileOf(text), 1)).toBe(true);

    // Lo guardado es el FICHERO tal cual, no el modelo parseado.
    expect(readCachedModelTexts()).toEqual({ 1: text });

    store.dispose();

    // "Recargar": store nuevo, sembrado en vacio. La memoria es lo unico que queda.
    const reloaded = localStore();
    expect(reloaded.restoreLocalModels()).toBe(1);

    const { models, modelError } = reloaded.getState();
    expect(modelError).toBe(null);
    expect(models[1]).toMatchObject({ slot: 1, name: 'CZ-BASS1', isValid: true });
    expect(models[1].amplitudes).toEqual(JSON.parse(text).amplitudes);
    // Y solo esa ranura: la memoria es por ranura, las otras tres siguen EMPTY.
    expect(models.filter((model) => model.name === 'EMPTY')).toHaveLength(3);

    reloaded.dispose();
  });

  it('cargar otra vez la misma ranura recuerda la ULTIMA, no las dos', async () => {
    const store = localStore();

    await store.loadLocalModel(fileOf(modelJson('CZ-PAD1')), 0);
    await store.loadLocalModel(fileOf(modelJson('CZ-HAMOG')), 0);

    const reloaded = localStore();
    expect(reloaded.restoreLocalModels()).toBe(1);
    expect(reloaded.getState().models[0].name).toBe('CZ-HAMOG');

    reloaded.dispose();
    store.dispose();
  });

  it('sin memoria no toca las ranuras (0, y siguen EMPTY)', () => {
    const store = localStore();

    expect(store.restoreLocalModels()).toBe(0);
    expect(store.getState().models.every((model) => model.name === 'EMPTY')).toBe(true);
    expect(store.getState().modelError).toBe(null);

    store.dispose();
  });

  it('con host no lee la memoria: las ranuras las manda modelsState', () => {
    window.__JUCE__ = { backend: makeBackend() };
    saveCachedModelText(0, modelJson());   // memoria de una sesion local anterior

    const store = createParameterStore();
    store.start();

    expect(store.getState().bridgeAvailable).toBe(true);
    expect(store.restoreLocalModels()).toBe(0);
    // Ni siquiera siembra: la ficha espera el modelsState del host.
    expect(store.getState().models).toBe(null);

    store.dispose();
  });

  it('una entrada corrupta se descarta y las buenas sobreviven (con aviso visible)', () => {
    saveCachedModelText(0, 'esto no es un modelo');
    saveCachedModelText(2, modelJson('CZ-PAD1'));

    const store = localStore();
    expect(store.restoreLocalModels()).toBe(1);

    const { models, modelError } = store.getState();
    expect(models[2].name).toBe('CZ-PAD1');
    // La rota no se pinta a medias: EMPTY y el motivo en la ficha.
    expect(models[0].name).toBe('EMPTY');
    expect(modelError.slot).toBe(0);
    expect(modelError.detail).toMatch(/no se pudo releer/);

    store.dispose();
  });

  it('un fichero que no parsea no se guarda y deja la ranura como estaba', async () => {
    const store = localStore();

    expect(await store.loadLocalModel(fileOf('no soy un modelo'), 0)).toBe(false);

    expect(store.getState().modelError).toMatchObject({ slot: 0 });
    expect(store.getState().models[0].name).toBe('EMPTY');
    expect(readCachedModelTexts()).toEqual({});

    store.dispose();
  });

  it('sin almacen la carga funciona igual y el aviso dice que no se recordara', async () => {
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const store = localStore();

    vi.stubGlobal('localStorage', undefined);

    try {
      expect(await store.loadLocalModel(fileOf(modelJson()), 3)).toBe(true);
    } finally {
      vi.unstubAllGlobals();
    }

    // La ranura SI esta cargada: la memoria es un extra, no un requisito de la carga.
    expect(store.getState().models[3].name).toBe('CZ-BASS1');
    expect(store.getState().modelError).toBe(null);
    expect(warn).toHaveBeenCalledTimes(1);
    expect(warn.mock.calls[0][0]).toMatch(/no se pudo recordar/);

    warn.mockRestore();
    store.dispose();
  });
});


describe('store / OLVIDAR una ranura (el boton de la ficha RANURAS)', () => {
  const modelJson = (name = 'CZ-BASS1') => JSON.stringify({
    name,
    format: 2,
    amplitudes: Array.from({ length: 64 }, (_, i) => (i === 0 ? 1 : 0.25 / (i + 1))),
    frequencyOffsets: new Array(64).fill(0),
  });

  const fileOf = (text) => new File([text], 'CZ-BASS1.neuronikmodel');

  const localStore = () => {
    const store = createParameterStore();
    store.start();
    store.seedLocalModels(emptyLocalModels());
    return store;
  };

  beforeEach(() => {
    clearLocalModelCache();
  });

  afterEach(() => {
    clearLocalModelCache();
    delete window.__JUCE__;
  });

  it('la deja EMPTY y saca su texto de la memoria (un F5 ya no la devuelve)', async () => {
    const store = localStore();

    await store.loadLocalModel(fileOf(modelJson()), 1);
    saveCachedModelText(2, modelJson('CZ-PAD1'));

    expect(store.forgetLocalModel(1)).toBe(true);

    // El estado: la ranura VACIA DE FABRICA, no una entrada borrada a mano.
    expect(store.getState().models[1]).toEqual({
      slot: 1,
      name: 'EMPTY',
      isValid: false,
      amplitudes: new Array(64).fill(0),
      frequencyOffsets: new Array(64).fill(0),
    });
    // Y la memoria: solo esa ranura se va, la otra sigue como estaba.
    expect(readCachedModelTexts()).toEqual({ 2: modelJson('CZ-PAD1') });

    // "Recargar": store nuevo con la MISMA memoria -> la ranura olvidada no vuelve.
    const reloaded = localStore();

    expect(reloaded.restoreLocalModels()).toBe(1);
    expect(reloaded.getState().models[1].name).toBe('EMPTY');
    expect(reloaded.getState().models[2].name).toBe('CZ-PAD1');

    reloaded.dispose();
    store.dispose();
  });

  it('avisa por el MISMO canal que un error de carga, con su propio tono', async () => {
    const store = localStore();

    await store.loadLocalModel(fileOf(modelJson()), 0);
    store.forgetLocalModel(0);

    // Un `modelNotice` al lado del `modelError`: la ficha los pinta en la misma
    // linea y por eso no se mezclan en un solo campo.
    expect(store.getState().modelError).toBe(null);
    expect(store.getState().modelNotice).toMatchObject({ slot: 0, tone: 'ok' });
    expect(store.getState().modelNotice.detail).toMatch(/no sobrevive al F5/);

    store.dispose();
  });

  it('si la memoria no se deja vaciar, avisa de que al recargar volvera (tone warn)', async () => {
    const store = localStore();

    await store.loadLocalModel(fileOf(modelJson()), 0);

    // El almacen se LEE (la ranura esta ahi) pero no se ESCRIBE: el aviso tiene
    // que decirlo, porque el F5 la devuelve.
    const readonly = {
      getItem: globalThis.localStorage.getItem.bind(globalThis.localStorage),
      setItem: () => { throw new Error('QuotaExceededError'); },
      removeItem: () => { throw new Error('SecurityError'); },
    };
    vi.stubGlobal('localStorage', readonly);

    try {
      expect(store.forgetLocalModel(0)).toBe(true);
    } finally {
      vi.unstubAllGlobals();
    }

    // La SESION si queda vacia (el motor deja de usar el modelo ya).
    expect(store.getState().models[0].name).toBe('EMPTY');
    expect(store.getState().modelNotice).toMatchObject({ slot: 0, tone: 'warn' });
    expect(store.getState().modelNotice.detail).toMatch(/al recargar volvera/);

    store.dispose();
  });

  it('una ranura vacia no se olvida (no se finge un gesto) y con host no se toca', async () => {
    const store = localStore();

    expect(store.forgetLocalModel(0)).toBe(false);
    expect(store.getState().modelNotice).toBe(null);
    expect(store.forgetLocalModel(9)).toBe(false);

    store.dispose();

    // Con host las ranuras son del PRESET: su `modelPath<slot>` las devuelve.
    window.__JUCE__ = { backend: makeBackend() };
    const online = createParameterStore();
    online.start();
    saveCachedModelText(0, modelJson());

    expect(online.forgetLocalModel(0)).toBe(false);
    expect(online.getState().modelNotice).toBe(null);
    // Ni la memoria: con host la memoria local ni se lee, luego no se toca.
    expect(readCachedModelTexts()).toEqual({ 0: modelJson() });

    online.dispose();
  });

  it('un gesto de carga posterior limpia el aviso (una linea, un mensaje)', async () => {
    const store = localStore();

    await store.loadLocalModel(fileOf(modelJson()), 0);
    store.forgetLocalModel(0);
    expect(store.getState().modelNotice).not.toBe(null);

    await store.loadLocalModel(fileOf(modelJson('CZ-PAD1')), 0);
    expect(store.getState().modelNotice).toBe(null);

    // Y al reves: un fallo de carga deja obsoleto el "olvidada" anterior.
    store.forgetLocalModel(0);
    await store.loadLocalModel(fileOf('no soy un modelo'), 0);
    expect(store.getState().modelNotice).toBe(null);
    expect(store.getState().modelError).toMatchObject({ slot: 0 });

    store.dispose();
  });

  it('un solo setState: quien escucha ve la ranura vacia y el aviso a la vez', async () => {
    const store = localStore();

    await store.loadLocalModel(fileOf(modelJson()), 2);

    const frames = [];
    store.subscribe((state) => frames.push({
      name: state.models[2].name,
      notice: state.modelNotice?.tone ?? null,
    }));

    store.forgetLocalModel(2);

    // `subscribe()` pinta al instante (contrato del store): ese primer frame es el
    // de antes del gesto. Despues, UN solo frame con la fila ya vacia y el aviso
    // con la verdad nueva: nunca uno con la ranura vacia y el aviso del gesto
    // anterior (el destello de un frame, en rojo para el usuario).
    expect(frames.length).toBe(2);
    expect(frames[1]).toEqual({ name: 'EMPTY', notice: 'ok' });

    store.dispose();
  });
});
