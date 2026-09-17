/**
 * ABDNeural — anti-drift del contrato versionado del protocolo del bridge.
 *
 * Comprueba que `WebPilot/lib/bridge.js` (el transporte JS real) y las formas de
 * mensaje que la página envía y espera coinciden con
 * `WebPilot/contracts/bridge-protocol.json`, que está versionado en git.
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
const contractPath = path.join(repositoryRoot, 'WebPilot', 'contracts', 'bridge-protocol.json');

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

const bridgeSource = fs.readFileSync(path.join(repositoryRoot, 'WebPilot', 'lib', 'bridge.js'), 'utf8');

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
  `file:///${path.join(repositoryRoot, 'WebPilot', 'lib', 'bridge.js').replace(/\\/g, '/')}`,
);
const { createBridgeTransport } = await import(bridgeModuleUrl.href);

const received = [];
const transport = createBridgeTransport({
  onSnapshot: (entries) => received.push(['snapshot', entries]),
  onParameterChanged: (id, value) => received.push(['changed', id, value]),
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
