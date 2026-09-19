/**
 * Guardas de la selección de parámetros y de los anclajes del host.
 *
 * La lista de GENERAL se pincha LITERAL a propósito: el `--selftest` del host
 * comprueba esos ids exactos en la página, así que cambiar la lista es cambiar el
 * contrato E2E y tiene que ser una edición consciente (que falle aquí, no dentro
 * de WebView2).
 *
 * El anclaje `[data-tab="keys"]` sigue existiendo aunque el lienzo ya no tenga
 * pestañas: el host lo pulsa antes de leer la rueda de modulación. Se publica
 * desde contracts/screens.js para que el host y esta suite usen el MISMO selector.
 */

import { describe, expect, it } from 'vitest';

import { getDescriptor } from '../src/contracts/parameters.js';
import {
  GENERAL_PARAMETER_IDS,
  KEYS_TAB,
  KEYS_TAB_SELECTOR,
  SCREEN_PARAMETER_IDS,
} from '../src/contracts/screens.js';
import { SECTION_PARAMETER_IDS } from '../src/contracts/sections.js';

/** Espejo de los ids de WebPilotHost.cpp / BridgeSelftest.h. */
const HOST_GENERAL_IDS = [
  'engineType',
  'envAttack', 'envDecay', 'envSustain', 'envRelease',
  'unisonDetune', 'unisonSpread',
  'randomStrength',
  'freezeResonator', 'freezeFilter', 'freezeEnvelopes',
];

describe('ids de GENERAL', () => {
  it('coincide con los que comprueba el selftest del host', () => {
    expect(GENERAL_PARAMETER_IDS).toEqual(HOST_GENERAL_IDS);
  });

  it('todos existen en el contrato generado', () => {
    for (const id of GENERAL_PARAMETER_IDS) {
      expect(getDescriptor(id), `el contrato no contiene "${id}"`).not.toBeNull();
    }
  });
});

describe('SCREEN_PARAMETER_IDS', () => {
  it('es el reparto del lienzo, sin duplicados', () => {
    expect(SCREEN_PARAMETER_IDS).toEqual(SECTION_PARAMETER_IDS);
    expect(new Set(SCREEN_PARAMETER_IDS).size).toBe(SCREEN_PARAMETER_IDS.length);
  });

  it('contiene todos los ids de GENERAL (el host los busca en el estado)', () => {
    for (const id of GENERAL_PARAMETER_IDS) expect(SCREEN_PARAMETER_IDS).toContain(id);
  });

  it('cubre los 70 del contrato', () => {
    expect(SCREEN_PARAMETER_IDS).toHaveLength(70);
  });
});

describe('anclajes del host', () => {
  it('la franja de teclado se publica como [data-tab="keys"]', () => {
    expect(KEYS_TAB).toBe('keys');
    expect(KEYS_TAB_SELECTOR).toBe('[data-tab="keys"]');
  });
});
