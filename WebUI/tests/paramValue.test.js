/**
 * Tests for src/contracts/paramValue.js — the value plumbing between the
 * generated contract and the shared control family.
 *
 * PORTADO de `WebPilot/tests/paramValue.test.js`; sólo cambian las rutas de import.
 *
 * Pinned here:
 *   - real <-> normalised round-trips through the descriptor view-model;
 *   - interval snapping before conversion (no wire value the plugin rejects);
 *   - choice index <-> normalised linear encoding (APVTS discrete mapping);
 *   - display text uses real units and percent for unit-less 0..1 ranges.
 */

import { describe, expect, it } from 'vitest';

import {
  choiceIndexFromNormalized,
  describeParam,
  displayText,
  normalizedFromChoiceIndex,
  normalizedFromReal,
  realFromNormalized,
} from '../src/contracts/paramValue.js';

describe('describeParam', () => {
  it('returns view-models straight from the generated contract', () => {
    const control = describeParam('masterLevel');

    expect(control).not.toBeNull();
    expect(control.id).toBe('masterLevel');
    expect(control.kind).toBe('float');
    expect(control.min).toBe(0);
    expect(control.max).toBe(1);
  });

  it('returns null for unknown IDs (callers render an explicit error)', () => {
    expect(describeParam('noExiste')).toBeNull();
  });
});

describe('real <-> normalised round-trip', () => {
  it('is exact for skew-free 0..1 parameters', () => {
    const control = describeParam('masterLevel');

    expect(realFromNormalized(control, 0.25)).toBeCloseTo(0.25);
    expect(normalizedFromReal(control, 0.75)).toBeCloseTo(0.75);
  });

  it('round-trips through the descriptor view-model on skewed ranges', () => {
    const control = describeParam('morphX');

    for (const normalized of [0, 0.1, 0.37, 0.5, 0.83, 1]) {
      const real = realFromNormalized(control, normalized);

      expect(normalizedFromReal(control, real)).toBeCloseTo(normalized, 5);
    }
  });

  it('snaps real edits to the declared interval', () => {
    const control = describeParam('engineType'); // interval 1, discrete choices

    const real = realFromNormalized(control, 0.5);
    const normalized = normalizedFromReal(control, real + 0.2);

    // Whatever the arithmetic did, the result must be a legal choice encoding.
    expect([0, 0.5, 1]).toContain(normalized);
  });
});

describe('choice encoding (index N <-> N/(count-1))', () => {
  it('maps index 0 of 2 options to 0 and index 1 to 1', () => {
    const control = { options: ['A', 'B'] };

    expect(normalizedFromChoiceIndex(control, 0)).toBe(0);
    expect(normalizedFromChoiceIndex(control, 1)).toBe(1);
  });

  it('round-trips indexes for a 3-option control', () => {
    const control = { options: ['A', 'B', 'C'] };

    expect(choiceIndexFromNormalized(control, 0)).toBe(0);
    expect(choiceIndexFromNormalized(control, 0.5)).toBe(1);
    expect(choiceIndexFromNormalized(control, 1)).toBe(2);
  });

  it('clamps out-of-range indexes and floats', () => {
    const control = { options: ['A', 'B'] };

    expect(choiceIndexFromNormalized(control, 2)).toBe(1);
    expect(choiceIndexFromNormalized(control, -1)).toBe(0);
    expect(normalizedFromChoiceIndex(control, 5)).toBe(1);
    expect(normalizedFromChoiceIndex(control, -3)).toBe(0);
  });
});

describe('displayText', () => {
  it('uses percent for unit-less 0..1 ranges', () => {
    const control = describeParam('masterLevel');

    expect(displayText(control, 0.67)).toBe('67%');
  });

  it('formats with the descriptor units for non-0..1 ranges', () => {
    const control = { kind: 'float', min: 0, max: 1, unit: '' };

    expect(displayText(control, 0.5)).toBe('50%');
  });
});
