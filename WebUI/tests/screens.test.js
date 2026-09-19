/**
 * Screen selection guards: every id a screen claims must exist in the generated
 * contract, and the store must own each of them exactly once.
 *
 * The GENERAL list is pinned literally on purpose: the host's `--selftest`
 * asserts those exact ids are on the page, so changing the list is a change to
 * the E2E contract and has to be a conscious edit (fail here, not inside
 * WebView2).
 */

import { describe, expect, it } from 'vitest';

import { PILOT_PARAMETER_IDS, getDescriptor } from '../src/contracts/parameters.js';
import { GENERAL_PARAMETER_IDS, SCREEN_PARAMETER_IDS, TABS } from '../src/contracts/screens.js';

/** Mirrors the ids in WebPilotHost.cpp's selftestStartGeneralCheck(). */
const HOST_GENERAL_IDS = [
  'engineType',
  'envAttack', 'envDecay', 'envSustain', 'envRelease',
  'unisonDetune', 'unisonSpread',
  'randomStrength',
  'freezeResonator', 'freezeFilter', 'freezeEnvelopes',
];

describe('GENERAL screen ids', () => {
  it('matches the ids the host selftest asserts', () => {
    expect(GENERAL_PARAMETER_IDS).toEqual(HOST_GENERAL_IDS);
  });

  it('every id exists in the generated contract', () => {
    for (const id of GENERAL_PARAMETER_IDS) {
      expect(getDescriptor(id), `contrato no contiene "${id}"`).not.toBeNull();
    }
  });
});

describe('SCREEN_PARAMETER_IDS', () => {
  it('keeps the pilot ids first and has no duplicates', () => {
    expect(SCREEN_PARAMETER_IDS.slice(0, PILOT_PARAMETER_IDS.length)).toEqual(PILOT_PARAMETER_IDS);
    expect(new Set(SCREEN_PARAMETER_IDS).size).toBe(SCREEN_PARAMETER_IDS.length);
  });

  it('covers both screens (engineType lives in both lists)', () => {
    for (const id of [...PILOT_PARAMETER_IDS, ...GENERAL_PARAMETER_IDS])
      expect(SCREEN_PARAMETER_IDS).toContain(id);
  });
});

describe('TABS', () => {
  it('exposes KEYS as data-tab="keys"', () => {
    const keys = TABS.find((tab) => tab.id === 'keys');

    expect(keys).toBeDefined();
    expect(keys.tab).toBe('keys');
  });
});
