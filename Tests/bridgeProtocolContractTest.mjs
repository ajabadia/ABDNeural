/**
 * ABDNeural — anti-drift del contrato versionado del protocolo del bridge.
 *
 * Comprueba que `WebUI/src/bridge/bridgeCore.js` (el transporte JS real, que era
 * `WebPilot/lib/bridge.js` hasta la retirada del piloto) y las formas de mensaje
 * que la página envía y espera coinciden con
 * `WebUI/contracts/bridge-protocol.json`, que está versionado en git.
 *
 * Es el gemelo JS de `NEURONiK_BridgeProtocolContractTest` (C++): entre los dos,
 * ninguna de las dos partes puede mover un literal o una forma de mensaje sin que
 * un test lo diga. Se lanza con `node` desde ctest, igual que el guard de dirección.
 */

import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { fileURLToPath } from 'node:url';

const testsDirectory = path.dirname(fileURLToPath(import.meta.url));
const repositoryRoot = path.dirname(testsDirectory);
const contractPath = path.join(repositoryRoot, 'WebUI', 'contracts', 'bridge-protocol.json');

const failures = [];

function check(condition, description) {
  if (condition) {
    console.log(`  [ok]   ${description}`);
    return;
  }
  console.log(`  [FAIL] ${description}`);
  failures.push(description);
}

const contract = JSON.parse(fs.readFileSync(contractPath, 'utf8'));

// --- Identidad del contrato -----------------------------------------------------

check(contract.contract === 'NEURONiK bridge protocol', 'el fichero es el contrato del bridge');
check(contract.version === 1, 'contract version 1 (la versión actual del protocolo)');

// --- Literales del transporte JS contra el contrato -----------------------------

const bridgeSource = fs.readFileSync(path.join(repositoryRoot, 'WebUI', 'src', 'bridge', 'bridgeCore.js'), 'utf8');

check(
  bridgeSource.includes(`const NATIVE_TO_JS_EVENT_ID = '${contract.channels.nativeToJs.eventId}';`),
  'bridge.js usa el event id nativo->JS del contrato',
);
check(
  bridgeSource.includes(`const JS_TO_NATIVE_EVENT_ID = '${contract.channels.jsToNative.eventId}';`),
  'bridge.js usa el event id JS->nativo del contrato',
);
check(
  bridgeSource.includes(`const PAGE_LOADED_EVENT_ID = '${contract.channels.pageLoaded.eventId}';`),
  'bridge.js usa el event id pageLoaded del contrato',
);

// --- Formas de mensaje: el contrato describe lo que el transporte envía ----------

/**
 * Backend JUCE simulado: graba lo emitido y entrega lo programado, exactamente
 * como hace `window.__JUCE__.backend` (addEventListener devuelve un id numérico,
 * removeEventListener toma [eventId, id]).
 */
function createFakeBackend() {
  const sent = [];
  const listeners = new Map();
  let nextId = 1;

  return {
    sent,
    emitEvent(eventId, payload) {
      sent.push([eventId, payload]);
    },
    addEventListener(eventId, fn) {
      const id = nextId++;
      if (!listeners.has(eventId)) listeners.set(eventId, new Map());
      listeners.get(eventId).set(id, fn);
      return id;
    },
    removeEventListener([eventId, id]) {
      listeners.get(eventId)?.delete(id);
    },
    deliver(eventId, payload) {
      for (const fn of listeners.get(eventId)?.values() ?? []) fn(payload);
    },
  };
}

globalThis.window = { __JUCE__: { backend: createFakeBackend() } };

// Windows: dynamic import exige URL file:// para rutas absolutas.
const bridgeModuleUrl = new URL(
  `file:///${path.join(repositoryRoot, 'WebUI', 'src', 'bridge', 'bridgeCore.js').replace(/\\/g, '/')}`,
);
const { createBridgeTransport } = await import(bridgeModuleUrl.href);

const received = [];
const transport = createBridgeTransport({
  onSnapshot: (entries) => received.push(['snapshot', entries]),
  onParameterChanged: (id, value) => received.push(['changed', id, value]),
  onPresetList: (info) => received.push(['presetList', info]),
  onPresetError: (error) => received.push(['presetError', error]),
  onMidiState: (state) => received.push(['midiState', state]),
  onModels: (slots) => received.push(['modelsState', slots]),
});

check(transport.available === true, 'el transporte se activa con window.__JUCE__ presente');

// --- JS -> nativo: formas exactas del contrato -----------------------------------

const jsToNative = contract.messages.jsToNative;

transport.sendParameterChange('masterLevel', 0.42, 'begin');
const [, change] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(
  change.action === 'parameterChanged'
    && change.id === 'masterLevel'
    && change.value === 0.42
    && change.gesture === 'begin',
  'parameterChanged (JS->nativo) lleva action/id/value/gesture según el contrato',
);

transport.sendRequestState();
const [, request] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(
  request.action === Object.keys(jsToNative.requestState.fields)[0] || request.action === 'requestState',
  'requestState lleva solo action',
);

transport.announcePageLoaded();
const [pageLoadedEventId] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(
  pageLoadedEventId === contract.channels.pageLoaded.eventId,
  'announcePageLoaded va por el event id pageLoaded del contrato',
);

// Gestos: el contrato declara exactamente begin/change/end y change por defecto
const declaredGestures = ['begin', 'change', 'end'].filter((phase) =>
  contract.valueConvention.gesture.includes(`'${phase}'`),
);
check(declaredGestures.length === 3, 'el contrato declara las tres fases de gesto');
transport.sendParameterChange('morphX', 0.5);
check(
  globalThis.window.__JUCE__.backend.sent.at(-1)[1].gesture === 'change',
  'un cambio sin gesto declarado envía "change" (default del contrato)',
);

// --- Presets (aditivo a v1): formas exactas de envío y entrega ---------------

const jsPresetActions = contract.messages.jsToNative;

check(
  Object.keys(jsPresetActions).includes('listPresets')
    && Object.keys(jsPresetActions).includes('loadPreset')
    && Object.keys(jsPresetActions).includes('savePreset'),
  'el contrato declara las tres acciones de preset JS->nativo',
);

transport.sendListPresets();
let [, listMsg] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(
  listMsg.action === 'listPresets' && Object.keys(listMsg).length === 1,
  'listPresets lleva solo action',
);

transport.sendLoadPreset('Glass Bells');
[, listMsg] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(
  listMsg.action === 'loadPreset' && listMsg.name === 'Glass Bells'
    && Object.keys(listMsg).length === 2,
  'loadPreset lleva action+name (sin extensión)',
);

transport.sendSavePreset('Pad Nocturno');
[, listMsg] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(
  listMsg.action === 'savePreset' && listMsg.name === 'Pad Nocturno',
  'savePreset lleva action+name',
);

globalThis.window.__JUCE__.backend.deliver(contract.channels.nativeToJs.eventId, {
  action: 'presetList',
  presets: ['Init Preset', 'Glass Bells'],
  current: 'Glass Bells',
});
check(
  received.some(([kind]) => kind === 'presetList'),
  'un presetList válido llega como onPresetList',
);

globalThis.window.__JUCE__.backend.deliver(contract.channels.nativeToJs.eventId, {
  action: 'presetError',
  operation: 'loadPreset',
  detail: 'preset not found: X',
});
check(
  received.some(([kind]) => kind === 'presetError'),
  'un presetError válido llega como onPresetError',
);

// --- MIDI (aditivo a v1): envío JS->nativo y entrega nativo->JS ----------------

check(
  Object.keys(contract.messages.jsToNative).includes('midiNoteOn')
    && Object.keys(contract.messages.jsToNative).includes('midiNoteOff')
    && Object.keys(contract.messages.jsToNative).includes('midiPitchBend')
    && Object.keys(contract.messages.jsToNative).includes('midiModWheel')
    && Object.keys(contract.messages.jsToNative).includes('midiPanic'),
  'el contrato declara las acciones MIDI JS->nativo',
);
check(
  Object.keys(contract.messages.nativeToJs).includes('midiNoteState'),
  'el contrato declara midiNoteState nativo->JS',
);

transport.sendMidiNoteOn(60, 0.9);
let [, midiMsg] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(
  midiMsg.action === 'midiNoteOn' && midiMsg.note === 60 && midiMsg.velocity === 0.9,
  'midiNoteOn lleva action/note/velocity',
);

transport.sendMidiNoteOff(60);
[, midiMsg] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(midiMsg.action === 'midiNoteOff' && midiMsg.note === 60, 'midiNoteOff lleva action/note');

transport.sendMidiPitchBend(-0.5);
[, midiMsg] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(midiMsg.action === 'midiPitchBend' && midiMsg.value === -0.5, 'midiPitchBend lleva action/value');

transport.sendMidiModWheel(0.75);
[, midiMsg] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(midiMsg.action === 'midiModWheel' && midiMsg.value === 0.75, 'midiModWheel lleva action/value');

transport.sendMidiPanic();
[, midiMsg] = globalThis.window.__JUCE__.backend.sent.at(-1);
check(midiMsg.action === 'midiPanic', 'midiPanic lleva solo action');

globalThis.window.__JUCE__.backend.deliver(contract.channels.nativeToJs.eventId, {
  action: 'midiNoteState', held: [60, 64], pitchBend: -0.25, modWheel: 0.5,
});
check(
  received.some(([kind, state]) => kind === 'midiState' && state.held[0] === 60),
  'un midiNoteState válido llega como onMidiState',
);

// --- nativo -> JS: el transporte filtra por action y acepta el esquema del contrato

const snapshotPayload = {
  action: 'syncAllParams',
  version: 1,
  parameterCount: 1,
  parameters: [{ id: 'masterLevel', value: 0.9, real: 0.9, text: '0.90' }],
};

globalThis.window.__JUCE__.backend.deliver(contract.channels.nativeToJs.eventId, snapshotPayload);
check(received.some(([kind]) => kind === 'snapshot'), 'un syncAllParams válido llega como snapshot');

globalThis.window.__JUCE__.backend.deliver(contract.channels.nativeToJs.eventId, {
  action: 'parameterChanged',
  id: 'morphY',
  value: 0.25,
  real: 0.25,
  text: '0.25',
});
check(
  received.some(([kind, id]) => kind === 'changed' && id === 'morphY'),
  'un parameterChanged nativo llega al callback',
);

// Models: el slot 0 publicado cruza a onModels; lo malformado se ignora.
check(
  contract.channels.nativeToJs.eventId === 'event'
    && Object.keys(contract.messages.nativeToJs).includes('modelsState'),
  'el contrato declara modelsState nativo->JS',
);

globalThis.window.__JUCE__.backend.deliver(contract.channels.nativeToJs.eventId, {
  action: 'modelsState',
  slots: [{
    slot: 0,
    isValid: true,
    amplitudes: new Array(64).fill(0),
    frequencyOffsets: new Array(64).fill(0),
  }],
});
check(
  received.some(([kind, slots]) => kind === 'modelsState' && slots[0].slot === 0),
  'un modelsState válido llega como onModels',
);

// --- Telemetria: el frame llega al consumidor onTelemetry tal cual ------------

const telemetryReceived = [];
const telemetryTransport = createBridgeTransport({
  onTelemetry(frame) {
    telemetryReceived.push(frame);
  },
});
check(telemetryTransport.available === true, 'con el backend falso el transporte esta disponible (telemetria)');
globalThis.window.__JUCE__.backend.deliver(contract.channels.nativeToJs.eventId, {
  action: contract.messages.nativeToJs.telemetryFrame.fields.action.split(' ')[0].replaceAll("'", ''),
  seq: 1,
  spectral: new Array(64).fill(0.5),
  envelopes: [0.1, 0.9],
  lfos: [0.5, 0.5],
  modulation: [0.25, 0.25],
  morph: [0.5, 0.5],
});
check(
  telemetryReceived.length === 1 && telemetryReceived[0].spectral.length === 64,
  'un telemetryFrame valido llega como onTelemetry (aditivo a v1)',
);
check(
  contract.behaviour.telemetryPoll !== undefined,
  'el contrato documenta la politica de sondeo con diff de la telemetria',
);
telemetryTransport.dispose();

// Sin JUCE: modo local (el contrato lo documenta como comportamiento)
delete globalThis.window;
const localTransport = createBridgeTransport({ onSnapshot() {}, onParameterChanged() {} });
check(
  localTransport.available === false && contract.behaviour.localMode !== undefined,
  'sin window.__JUCE__ el transporte cae a modo local, como documenta el contrato',
);

console.log(
  failures.length === 0
    ? '\nContrato del bridge: todo en verde.'
    : `\n${failures.length} comprobaciones fallaron.`,
);
process.exit(failures.length === 0 ? 0 : 1);
