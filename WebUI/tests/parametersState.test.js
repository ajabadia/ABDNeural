/**
 * Tests for the contract-state helpers in src/contracts/parameters.js: default
 * state seeding, state validation (the UI makes these errors visible) and the
 * divergent-parameter report. The value math (to/fromNormalized) is already
 * covered by tests/paramValue.test.js.
 *
 * PORTADO de `WebPilot/tests/parametersState.test.js`; sólo cambia la ruta del import.
 */

import { describe, expect, it } from 'vitest';

import {
  PILOT_PARAMETER_IDS,
  contractSummary,
  defaultState,
  describeControl,
  divergentParameters,
  getDescriptor,
  validateState,
} from '../src/contracts/parameters.js';

describe('defaultState', () => {
  it('seeds floats from the contract default value', () => {
    const state = defaultState(['masterLevel']);
    const descriptor = getDescriptor('masterLevel');

    expect(state.masterLevel).toBe(descriptor.defaultValue);
  });

  it('seeds choices from the contract default choice index', () => {
    const state = defaultState(['engineType']);
    const descriptor = getDescriptor('engineType');

    expect(descriptor.kind).toBe('choice');
    expect(state.engineType).toBe(descriptor.defaultChoiceIndex);
    expect(Number.isInteger(state.engineType)).toBe(true);
  });

  it('skips unknown ids instead of inventing values', () => {
    const state = defaultState(['notARealParameter']);

    expect(state).toEqual({});
  });
});

describe('validateState', () => {
  it('passes a state seeded by defaultState for the contract screen', () => {
    const ids = PILOT_PARAMETER_IDS;

    expect(validateState(ids, defaultState(ids))).toEqual([]);
  });

  it('reports unknown parameters', () => {
    expect(validateState(['ghost'], {})).toEqual(['unknown parameter "ghost"']);
  });

  it('reports missing values', () => {
    expect(validateState(['masterLevel'], {})).toEqual(['missing value for "masterLevel"']);
  });

  it('reports out-of-range floats and invalid choice indexes', () => {
    const errors = validateState(
      ['masterLevel', 'engineType'],
      { masterLevel: 42, engineType: 99 },
    );

    expect(errors.some((e) => e.includes('masterLevel') && e.includes('out of range'))).toBe(true);
    expect(errors.some((e) => e.includes('engineType') && e.includes('invalid choice index'))).toBe(true);
  });
});

describe('describeControl / divergences', () => {
  it('exposes the descriptor view-model a generic control needs', () => {
    const control = describeControl('masterLevel');
    const descriptor = getDescriptor('masterLevel');

    expect(control.id).toBe('masterLevel');
    expect(control.label).toBe(descriptor.name);
    expect(control.min).toBe(descriptor.minValue);
    expect(control.max).toBe(descriptor.maxValue);
    expect(control.defaultValue).toBe(descriptor.defaultValue);
  });

  it('returns null for unknown ids so callers can report instead of render', () => {
    expect(describeControl('ghost')).toBeNull();
  });

  it('lists divergent parameters from the contract dspStatus', () => {
    const divergent = divergentParameters();

    expect(divergent.length).toBeGreaterThan(0);
    expect(divergent.every((d) => d.dspStatus !== 'implemented')).toBe(true);
  });

  it('summarises the contract screen against the contract', () => {
    const summary = contractSummary();

    expect(summary.screen.total).toBe(PILOT_PARAMETER_IDS.length);
    expect(summary.screen.implemented + summary.screen.divergent).toBe(summary.screen.total);
  });
});

describe('ENV wiring defaults (preset nuevo)', () => {
  // El cableado ENV 1/ENV 2 por matriz (8.3) vive en el CONTRATO generado:
  // un regenerado que mueva estos defaults rompe presets nuevos en silencio
  // (ENV 2 muda con amount 0). Pin contra el artefacto, no contra constantes.
  it('ENV 1 arranca cableada al nivel del oscilador (fuente 6 -> destino 1)', () => {
    const d = getDescriptor('mod1Source');

    expect(d.defaultChoiceIndex).toBe(6); // 6 = ENV 1 en getModSources()
  });

  it('ENV 2 arranca cableada al cutoff del filtro (fuente 7 -> destino 10)', () => {
    const d = getDescriptor('mod2Source');

    expect(d.defaultChoiceIndex).toBe(7); // 7 = ENV 2
  });

  it('el knob de profundidad de la envolvente del filtro nace audible (1.0)', () => {
    const d = getDescriptor('filterEnvAmount');

    expect(d.defaultValue).toBe(1);
    expect(d.defaultNormalized).toBe(1);
  });

  it('defaultState siembra los tres y el estado pasa la validacion', () => {
    const ids = ['mod1Source', 'mod2Source', 'filterEnvAmount'];
    const state = defaultState(ids);

    expect(state.mod1Source).toBe(6);
    expect(state.mod2Source).toBe(7);
    expect(state.filterEnvAmount).toBe(1);
    expect(validateState(ids, state)).toEqual([]);
  });
});
