/**
 * The KEYS screen, mounted for real: the shared keyboard is a DOM component and
 * the host reaches into it, so the two things it must guarantee are pinned here.
 *
 *   1. `#mod-wheel-container .kbd-wheel-slider` exists — that selector is how
 *      the host's selftest reads the mod wheel after injecting CC1 natively;
 *   2. mounting the keyboard does NOT push the masterLevel slider out of first
 *      place: the host reads `document.querySelector('input[type=range]')` and
 *      the wheels are range inputs too. El orden del documento (el fader va en
 *      la ficha GLOBAL, las ruedas en la franja de abajo) es lo que las deja
 *      detrás, y esta es la regresión que lo rompería.
 */

import { afterEach, describe, expect, it, vi } from 'vitest';

import { contractSummary, defaultNormalizedState, describeControl } from '../src/contracts/parameters.js';
import { SCREEN_PARAMETER_IDS } from '../src/contracts/screens.js';
import { BANDS } from '../src/contracts/sections.js';
import { createPanel } from '../src/ui/panel.js';
import { mountKeyboard } from '../src/ui/keyboard.js';

/** Las mismas bandas que monta app.js, con sus controles resueltos. */
const bands = BANDS.map((band) => band.map((section) => ({
  ...section,
  controls: section.ids.map(describeControl).filter(Boolean),
})));

function mountPanelWithKeyboard(callbacks = {}) {
  const panel = createPanel({ bands, baselineId: 'masterLevel' });

  document.body.append(panel.element);

  panel.paint({
    parameters: defaultNormalizedState(SCREEN_PARAMETER_IDS),
    summary: contractSummary(SCREEN_PARAMETER_IDS),
    contractErrors: [],
    changeCount: 0,
    bridgeAvailable: true,
    snapshotVersion: 0,
  });

  const keyboard = mountKeyboard({ root: panel.keysRoot, bridgeAvailable: true, callbacks });

  return { panel, keyboard };
}

const modWheelSlider = () => document.querySelector('#mod-wheel-container .kbd-wheel-slider');

afterEach(() => {
  document.body.innerHTML = '';
});

describe('KEYS / shared keyboard', () => {
  it('renders the wheel slider the host selftest reads', () => {
    mountPanelWithKeyboard();

    expect(modWheelSlider()).not.toBeNull();
    // 0..127, the scale the shared Wheel uses (the host divides by 127).
    expect(modWheelSlider().max).toBe('127');
  });

  it('keeps masterLevel as the FIRST range input of the document', () => {
    const { keyboard } = mountPanelWithKeyboard();

    expect(keyboard.hasKeybed()).toBe(true);

    const first = document.querySelector('input[type=range]');

    expect(first.id).toBe('masterLevel');
  });

  it('applies the host MIDI view to the wheel without echoing it back', () => {
    const onModWheel = vi.fn();
    const { keyboard } = mountPanelWithKeyboard({ onModWheel });

    // The native side injected CC1 = 64; the bridge reports 64/127 ≈ 0.504.
    keyboard.setMidiState({ pitchBend: 0, modWheel: 64 / 127 });

    expect(Number(modWheelSlider().value)).toBe(64);
    // Host-driven feedback is the SILENT path: no user-input callback.
    expect(onModWheel).not.toHaveBeenCalled();
  });

  it('routes key presses to the store callbacks', () => {
    const onNoteOn = vi.fn();
    mountPanelWithKeyboard({ onNoteOn });

    const key = document.querySelector('#piano-keyboard .kbd-white-key');
    expect(key).not.toBeNull();

    key.dispatchEvent(new window.PointerEvent('pointerdown', { bubbles: true }));

    expect(onNoteOn).toHaveBeenCalledTimes(1);
    expect(onNoteOn.mock.calls[0][0]).toBe(Number(key.dataset.note));
  });
});
