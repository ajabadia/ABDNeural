/**
 * KEYS screen: the SHARED virtual keyboard (`@abdsynths/midi-keyb`, the same
 * component ABDMS2000 mounts) plus the pitch/mod wheels.
 *
 * Key/wheel input goes out through the bridge MIDI messages; `midiNoteState`
 * coming back is applied by the HOST-DRIVEN feedback API (the silent wheel
 * setters, v0.2) so a hardware/DAW move lands on the page without echoing back
 * as user input.
 *
 * The containers are built here (ids included) because the wheel slider the
 * host's `--selftest` reads, `#mod-wheel-container .kbd-wheel-slider`, only
 * exists once the keyboard has rendered into them.
 */

import { createKeyboard } from '@abdsynths/midi-keyb';
import '@abdsynths/midi-keyb/keyboard.css';

const PITCH_WHEEL_ID = 'pitch-wheel-container';
const MOD_WHEEL_ID = 'mod-wheel-container';
const KEYBED_ID = 'piano-keyboard';

/**
 * Mount the keyboard inside `root`.
 *
 * @param {object} options
 * @param {HTMLElement} options.root             container of the keys screen
 * @param {boolean} [options.bridgeAvailable]    drives the LIVE/LOCAL label only
 * @param {object} options.callbacks
 * @param {(note: number, velocity: number) => void} [options.callbacks.onNoteOn]
 * @param {(note: number) => void} [options.callbacks.onNoteOff]
 * @param {(value: number) => void} [options.callbacks.onPitchBend]
 * @param {(value: number) => void} [options.callbacks.onModWheel]
 * @param {() => void} [options.callbacks.onPanic]
 * @returns {{ setMidiState: (state: object) => void, hasKeybed: () => boolean, destroy: () => void }}
 */
export function mountKeyboard({ root, bridgeAvailable = false, callbacks = {} }) {
  const strip = document.createElement('div');
  strip.className = 'keys-strip';

  const pitchWheel = document.createElement('div');
  pitchWheel.id = PITCH_WHEEL_ID;

  const keybed = document.createElement('div');
  keybed.id = KEYBED_ID;

  const modWheel = document.createElement('div');
  modWheel.id = MOD_WHEEL_ID;

  strip.append(pitchWheel, keybed, modWheel);

  const note = document.createElement('p');
  note.className = 'keys-note';
  note.textContent = 'QWERTY plays (A W S E D…), Z/X shifts octave, Space = panic.'
    + (bridgeAvailable ? '' : ' — LOCAL MODE: notes stay on this page.');

  root.append(strip, note);

  // The containers must exist BEFORE createKeyboard: it renders the wheels into
  // them at construction time (and never again).
  const keyboard = createKeyboard({
    containerId: KEYBED_ID,
    wheelPitchId: PITCH_WHEEL_ID,
    wheelModId: MOD_WHEEL_ID,
    panicBtnId: null,
    onNoteOn: (noteNumber, velocity) => callbacks.onNoteOn?.(noteNumber, velocity),
    onNoteOff: (noteNumber) => callbacks.onNoteOff?.(noteNumber),
    onPitchBend: (value) => callbacks.onPitchBend?.(value),
    onModWheel: (value) => callbacks.onModWheel?.(value),
    onPanic: () => callbacks.onPanic?.(),
  });

  const keybedRendered = document.querySelector(`#${KEYBED_ID} .kbd-white-key`) !== null;

  return {
    /**
     * Host-driven feedback: mirror the plugin's external MIDI view on the
     * wheels. Held notes are NOT re-highlighted on purpose — the shared
     * keyboard keeps its own press state and a repaint from telemetry would
     * fight the user's finger.
     */
    setMidiState(state) {
      if (!keybedRendered) return;

      keyboard.setPitchBend?.(state?.pitchBend ?? 0);
      keyboard.setModWheel?.(state?.modWheel ?? 0);
    },

    hasKeybed: () => keybedRendered,

    destroy() {
      keyboard.destroy();
      root.textContent = '';
    },
  };
}
