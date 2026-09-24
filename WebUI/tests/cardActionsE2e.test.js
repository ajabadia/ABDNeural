/**
 * Recorrido E2E de las ACCIONES DE FICHA: del botón que hay en el lienzo al cable
 * del host, y de vuelta al DOM. Los dos modos de la página.
 *
 * Ninguno de los tests que ya existen recorre este camino entero, y cada uno deja
 * un hueco distinto:
 *
 *   - tests/panel.test.js monta el panel con un espía en `onAction`: fija el botón
 *     (etiqueta, `disabled` según host, clic -> handler) pero NO que el handler
 *     exista en la página montada ni que salga nada por el cable;
 *   - tests/paramStore.test.js llama `store.randomize()` directamente: cubre el
 *     store, no el cableado `app.js` -> store;
 *   - tests/appContract.test.js lee el TEXTO de src/app.js: si la línea está pero
 *     la página no la ejecuta (por ejemplo, el botón no llega al documento), sigue
 *     verde.
 *
 * Aquí se arranca la página DE VERDAD —`src/app.js`, el mismo módulo que carga
 * index.html, con su store, su panel y su teclado—, con el backend falso de JUCE
 * en `window.__JUCE__`, y se pulsa el botón que está en el documento. Sin host el
 * botón queda deshabilitado y el clic no finge un sorteo; con host el clic pone
 * `{action:'randomize'}` en `nativeEvent` (una ACCIÓN, no una edición de parámetro:
 * el sorteo lo hace el procesador, que tiene los rangos y los congelados), y los
 * valores que el procesador mueve vuelven por `event` y repintan la página.
 *
 * La página decide el modo UNA sola vez, dentro de `store.start()` y a partir de
 * la presencia de `window.__JUCE__` (ver src/bridge/bridgeCore.js), así que cada
 * modo necesita su propio arranque del módulo: vitest cachea los imports, de ahí
 * el `vi.resetModules()` antes de cada uno.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import {
  JS_TO_NATIVE_EVENT_ID,
  NATIVE_TO_JS_EVENT_ID,
  PAGE_LOADED_EVENT_ID,
} from '../src/bridge/bridgeCore.js';

/**
 * El backend que JUCE inyecta en `window.__JUCE__`, con la misma superficie:
 * `emitEvent(eventId, message)` para lo que sale de la página y
 * `addEventListener(eventId, fn) -> id` / `removeEventListener([eventId, id])`
 * para lo que entra. Mismo contrato que tests/bridgeCore.test.js y
 * tests/paramStore.test.js.
 */
function installFakeJuce() {
  const listeners = new Map();
  const emitted = [];
  let nextId = 1;

  const backend = {
    emitEvent(eventId, message) {
      emitted.push({ eventId, message });
    },
    addEventListener(eventId, listener) {
      const id = nextId++;
      if (!listeners.has(eventId)) listeners.set(eventId, []);
      listeners.get(eventId).push({ id, listener });
      return id;
    },
    removeEventListener([eventId, id]) {
      const list = listeners.get(eventId) ?? [];
      const index = list.findIndex((entry) => entry.id === id);
      if (index >= 0) list.splice(index, 1);
    },
  };

  window.__JUCE__ = { backend };

  return {
    emitted,
    /** Todo lo que la página mandó al host, en orden (canal incluido). */
    sent: () => emitted.map(({ eventId, message }) => [eventId, message.action]),
    /** Lo que salió por el canal normal de la página (no el handshake). */
    actionsOnNativeChannel: () => emitted
      .filter((entry) => entry.eventId === JS_TO_NATIVE_EVENT_ID)
      .map((entry) => entry.message),
    /** El host hablando a la página (emitEventIfBrowserIsVisible). */
    fromHost(message) {
      for (const { listener } of [...(listeners.get(NATIVE_TO_JS_EVENT_ID) ?? [])])
        listener(message);
    },
  };
}

/**
 * Arranca la página como la arranca el host: `#app` ya en el documento, y el
 * módulo de entrada importado DESPUÉS (lee el root al cargarse). El backend hay
 * que instalarlo ANTES: el modo se decide en el primer `start()`.
 *
 * @returns {Promise<HTMLButtonElement>} el botón RANDOM del documento
 */
async function bootPage() {
  document.body.innerHTML = '<div id="app"></div>';

  vi.resetModules();
  await import('../src/app.js');

  return document.querySelector('[data-action="randomize"]');
}

/** El estado NORMALIZADO de la página, tal como lo lee el host: el pie. */
function readFooterState() {
  return JSON.parse(document.querySelector('.panel-footer code').textContent);
}

/** La línea de cambios del pie (`Parameter updates: N`). */
function readFooterUpdates() {
  return document.querySelector('.panel-footer span').textContent;
}

beforeEach(() => {
  window.__pilotReady = undefined;
});

afterEach(() => {
  document.body.innerHTML = '';
  delete window.__JUCE__;
});

describe('E2E / acción de ficha RANDOM (sin host)', () => {
  it('el botón está deshabilitado y el clic no finge un sorteo', async () => {
    const button = await bootPage();

    // Sin backend no hay host al otro lado, y el modo es el local.
    expect(window.__JUCE__).toBeUndefined();
    expect(button).not.toBeNull();
    // Y es el botón de SU ficha: la acción viaja pegada a la tarjeta que la
    // declara (globalFull), no suelta por el lienzo.
    expect(button.closest('.card').dataset.sectionId).toBe('globalFull');
    expect(document.querySelector('.keys-status').textContent).toBe('LOCAL MODE');

    // Sin APVTS que sortear el botón no se puede pulsar: lo decide la PINTURA
    // (bridgeAvailable), no el clic.
    expect(button.disabled).toBe(true);

    const before = readFooterState();
    const updatesBefore = readFooterUpdates();

    button.click();
    button.click();

    // Un botón deshabilitado no dispara su listener: el estado de la página (el
    // JSON que el propio host parsea) y el contador de cambios quedan intactos, y
    // la página no se inventa un backend para poder decir "he sorteado".
    expect(readFooterState()).toEqual(before);
    expect(readFooterUpdates()).toBe(updatesBefore);
    expect(window.__JUCE__).toBeUndefined();
  });
});

describe('E2E / acción de ficha RANDOM (con host)', () => {
  it('el clic saca {action:randomize} por el cable y el sorteo vuelve pintando la página', async () => {
    const host = installFakeJuce();
    const button = await bootPage();

    // El arranque se anuncia al host y pide el estado. Dos canales, y el orden
    // importa: `pageLoaded` va por SU event id (el host lo bindea aparte para no
    // confundirlo con una edición), y el resto por `nativeEvent`.
    expect(host.sent()).toEqual([
      [PAGE_LOADED_EVENT_ID, 'pageLoaded'],
      [JS_TO_NATIVE_EVENT_ID, 'requestState'],
      [JS_TO_NATIVE_EVENT_ID, 'listPresets'],
    ]);
    expect(window.__pilotReady).toBe(true);

    expect(button).not.toBeNull();
    expect(button.disabled).toBe(false);

    host.emitted.length = 0;
    host.fromHost({
      action: 'syncAllParams',
      version: 1,
      parameterCount: 1,
      parameters: [{ id: 'randomStrength', value: 0.7, real: 0.7, text: '70%' }],
    });

    const before = readFooterState();
    expect(before.randomStrength).toBeCloseTo(0.7, 6);

    button.click();

    // La acción sale como ACCIÓN y sola: el sorteo es del procesador (él tiene los
    // rangos y los congelados), así que la página no manda ninguna edición de
    // parámetro ni se inventa valores.
    expect(host.actionsOnNativeChannel()).toEqual([{ action: 'randomize' }]);

    // El procesador sortea y contesta moviendo parámetros por el camino normal de
    // `parameterChanged`: la página los recibe y se repinta. Mismo cable, la otra
    // dirección — el lazo se cierra.
    host.fromHost({ action: 'parameterChanged', id: 'randomStrength', value: 0.42, real: 0.42, text: '42%' });
    host.fromHost({ action: 'parameterChanged', id: 'oscLevel', value: 0.83, real: 0.83, text: '83%' });

    const after = readFooterState();
    expect(after.randomStrength).toBeCloseTo(0.42, 6);
    expect(after.oscLevel).toBeCloseTo(0.83, 6);
    expect(after.randomStrength).not.toBe(before.randomStrength);

    // Y el repintado llega al DOM, no sólo al JSON del pie: la lectura visible de
    // la celda del parámetro que el procesador movió.
    const readout = document.querySelector('[data-parameter-id="randomStrength"] .cell__readout');
    expect(readout).not.toBeNull();
    expect(readout.textContent).toBe('42%');
  });

  it('el sorteo no toca ningún parámetro por su cuenta (el estado sólo cambia cuando el host contesta)', async () => {
    const host = installFakeJuce();
    const button = await bootPage();

    const before = readFooterState();
    button.click();

    // Se pidió el sorteo... y nada más: sin respuesta del host, la página no
    // adelanta un estado que no conoce (ni un solo id movido).
    expect(host.actionsOnNativeChannel().some((message) => message.action === 'randomize')).toBe(true);
    expect(readFooterState()).toEqual(before);
    expect(readFooterUpdates()).toContain('Parameter updates: 0');
  });
});
