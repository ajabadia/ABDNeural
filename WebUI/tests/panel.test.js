/**
 * Rehearsal of the host's `--selftest` against the real panel DOM.
 *
 * The C++ check drives `document.querySelector('input[type=range]')` and parses
 * `document.querySelector('.panel-footer code').textContent` as JSON, expecting
 * a set of ids to be present with numeric values. Those two selectors are the
 * page's public contract with the host, and they are pinned here so a UI change
 * cannot break the E2E silently (it would only fail inside WebView2, minutes
 * later, as a timed-out selftest).
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import { AUDIO_OWNER } from '../src/audio/policy.js';
import { describeControl } from '../src/contracts/parameters.js';
import { GENERAL_PARAMETER_IDS, SCREEN_PARAMETER_IDS } from '../src/contracts/screens.js';
import { createPanel } from '../src/ui/panel.js';
import { contractSummary, defaultNormalizedState, describePilotControls } from '../src/contracts/parameters.js';

/** Exactly the ids the host's GENERAL check asserts. */
const HOST_GENERAL_IDS = [
  'engineType',
  'envAttack', 'envDecay', 'envSustain', 'envRelease',
  'unisonDetune', 'unisonSpread',
  'randomStrength',
  'freezeResonator', 'freezeFilter', 'freezeEnvelopes',
];

function makeState(overrides = {}) {
  return {
    parameters: defaultNormalizedState(SCREEN_PARAMETER_IDS),
    summary: contractSummary(SCREEN_PARAMETER_IDS),
    contractErrors: [],
    changeCount: 0,
    bridgeAvailable: false,
    snapshotVersion: 0,
    ...overrides,
  };
}

function mountPanel(handlers = {}) {
  const panel = createPanel({
    bridgeControls: describePilotControls(['masterLevel', 'morphX']),
    generalControls: GENERAL_PARAMETER_IDS.map(describeControl).filter(Boolean),
    handlers,
  });

  document.body.append(panel.element);

  return panel;
}

afterEach(() => {
  document.body.innerHTML = '';
});

describe('panel / host selftest contract', () => {
  it('the FIRST input[type=range] of the document is masterLevel', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint(state);

    const slider = document.querySelector('input[type=range]');

    expect(slider).not.toBeNull();
    expect(slider.id).toBe('masterLevel');
    expect(slider.dataset.parameterId).toBe('masterLevel');
    // The wire carries NORMALISED 0..1, and the slider shows exactly that
    // (masterLevel is 0..1 with no skew, so the two scales coincide).
    expect(Number(slider.value)).toBeCloseTo(state.parameters.masterLevel, 5);
  });

  it('paints a native snapshot onto the baseline slider (NATIVE -> JS)', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint({ ...state, parameters: { ...state.parameters, masterLevel: 0.25 } });

    expect(Number(document.querySelector('input[type=range]').value)).toBeCloseTo(0.25, 5);
    expect(document.querySelector('[data-parameter-readout="masterLevel"]').textContent)
      .toContain('25%');
  });

  it('the footer <code> is JSON with every GENERAL id as a number', () => {
    const panel = mountPanel();
    panel.paint(makeState());

    const code = document.querySelector('.panel-footer code');
    expect(code).not.toBeNull();

    const parsed = JSON.parse(code.textContent);

    for (const id of HOST_GENERAL_IDS) {
      expect(parsed, `falta "${id}" en el JSON del footer`).toHaveProperty(id);
      expect(typeof parsed[id], `"${id}" no es numerico`).toBe('number');
    }

    expect(HOST_GENERAL_IDS).toEqual(GENERAL_PARAMETER_IDS);
  });

  it('a native envAttack edit lands in the footer as normalised 0.5', () => {
    const panel = mountPanel();
    const state = makeState();

    panel.paint({ ...state, parameters: { ...state.parameters, envAttack: 0.5 } });

    expect(JSON.parse(document.querySelector('.panel-footer code').textContent).envAttack)
      .toBeCloseTo(0.5, 6);
  });

  it('reports the bridge status and the contract errors', () => {
    const panel = mountPanel();

    panel.paint(makeState({ bridgeAvailable: true, snapshotVersion: 4, changeCount: 7 }));
    expect(document.querySelector('.status').textContent).toContain('snapshot #4');
    expect(document.querySelector('.panel-footer span').textContent).toContain('updates: 7');

    panel.paint(makeState({ contractErrors: ['"masterLevel" out of normalised range: 1.5'] }));
    expect(document.querySelector('.panel-footer span').textContent).toContain('contract errors');
  });
});

describe('panel / audio ownership (policy)', () => {
  it('inside a host it is a READOUT: there is no SOUND ON to press', () => {
    const panel = mountPanel();

    panel.paintAudio({ owner: AUDIO_OWNER.NATIVE });

    const button = document.querySelector('.audio-start');

    expect(button.hidden).toBe(true);
    expect(document.querySelector('.audio-mode__label').textContent).toContain('nativo');
  });

  it('in local mode it offers SOUND ON and reports the engine', () => {
    const onStartSound = vi.fn();
    const panel = mountPanel({ onStartSound });

    panel.paintAudio({ owner: AUDIO_OWNER.WORKLET });

    const button = document.querySelector('.audio-start');

    expect(button.hidden).toBe(false);
    button.click();
    expect(onStartSound).toHaveBeenCalledTimes(1);

    panel.paintAudio({ owner: AUDIO_OWNER.WORKLET, status: 'ready', sampleRate: 48000 });
    expect(document.querySelector('.audio-mode__detail').textContent).toContain('48.0 kHz');
    expect(button.hidden).toBe(true);

    panel.paintAudio({ owner: AUDIO_OWNER.WORKLET, status: 'error', error: 'sin device' });
    expect(button.hidden).toBe(false);
    expect(button.textContent).toBe('REINTENTAR');
    expect(document.querySelector('.audio-mode__detail').textContent).toContain('sin device');
  });
});

describe('panel / tabs', () => {
  it('exposes the KEYS tab with data-tab="keys" (what the host clicks)', () => {
    const panel = mountPanel();

    expect(document.querySelector('[data-tab="keys"]')).not.toBeNull();
    expect(panel.getActiveTab()).toBe('bridge');
  });

  it('switches screens without unmounting them (the host reads by selector)', () => {
    const panel = mountPanel();

    const keysScreen = document.querySelector('[data-screen="keys"]');
    const bridgeScreen = document.querySelector('[data-screen="bridge"]');

    document.querySelector('[data-tab="keys"]').click();
    expect(panel.getActiveTab()).toBe('keys');
    expect(keysScreen.hidden).toBe(false);
    expect(bridgeScreen.hidden).toBe(true);

    // Still queryable while hidden: the selftest reads the mod wheel there.
    expect(document.querySelector('[data-screen="keys"]')).not.toBeNull();
  });

  it('the PANIC button calls the handler', () => {
    const onPanic = vi.fn();
    mountPanel({ onPanic });

    document.querySelector('.keys-panic').click();

    expect(onPanic).toHaveBeenCalledTimes(1);
  });
});
