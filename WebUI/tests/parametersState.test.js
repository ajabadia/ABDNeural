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
