/**
 * Screen selection: which contract ids each screen owns.
 *
 * Data, not markup — the store seeds the state from here and the panel renders
 * from the same source, so a screen cannot drift from what the page holds.
 *
 * `GENERAL_PARAMETER_IDS` mirrors the plugin's own GENERAL tab
 * (`Source/UI/ParameterPanel.cpp`): the same ids the host's `--selftest`
 * asserts are present in the page state, straight from the generated contract
 * (no hand-written ranges).
 */

import { PILOT_PARAMETER_IDS } from './parameters.js';

/** Ids of the plugin's GENERAL tab (the native panel that ships today). */
export const GENERAL_PARAMETER_IDS = [
  'engineType',
  'envAttack', 'envDecay', 'envSustain', 'envRelease',
  'unisonDetune', 'unisonSpread',
  'randomStrength',
  'freezeResonator', 'freezeFilter', 'freezeEnvelopes',
];

/**
 * Every id the store owns. The bridge screen's ids stay first: the bridge tab
 * keeps the declaration order it always had.
 */
export const SCREEN_PARAMETER_IDS = [...new Set([...PILOT_PARAMETER_IDS, ...GENERAL_PARAMETER_IDS])];

/**
 * Provisional tab set. BRIDGE is where the baseline control lives (the host
 * drives it), GENERAL mirrors the native GENERAL tab, KEYS is the shared
 * keyboard. 8.2 replaces this grouping with the native panel's own one
 * (GENERAL, RESONATOR, FILTER/ENV, FX, LFO/MOD, BROWSER) — that is the DoD of
 * the ticket, not a detail to improvise here.
 *
 * Tabs live INSIDE one page on purpose: the embedded snapshot serves by
 * basename, so a second route would collide with index.html (see the 404
 * snapshot bug in the HANDOFF). `data-tab="keys"` is what the host's selftest
 * clicks.
 */
export const TABS = [
  { id: 'bridge', label: 'BRIDGE' },
  { id: 'general', label: 'GENERAL' },
  { id: 'keys', label: 'KEYS', tab: 'keys' },
];
