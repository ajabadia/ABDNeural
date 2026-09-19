/**
 * Who owns the audio engine — the policy, in ONE place.
 *
 * NEURONiK ships the same DSP twice: as the plugin's **native** engine and as
 * the **WASM** module that runs in an AudioWorklet when the page is opened in a
 * browser. Both sound at once is not a supported case: two engine instances
 * fighting over the same parameters produce doubled voices and phase-y FX, and
 * nothing in the protocol stops it.
 *
 * The rule (ROADMAP, Fase 8 / ticket 8.1):
 *
 *   - **inside a JUCE host** (the plugin's editor, or the pilot host bench)
 *     the audio is the PLUGIN's. The page is a remote control: it talks over the
 *     bridge (APVTS) and never instantiates the worklet.
 *   - **in a plain browser** the page owns the audio: `SOUND ON` starts the
 *     AudioWorklet with the WASM build, because there is no plugin underneath.
 *
 * The signal is the same one the bridge uses to decide whether it is live:
 * JUCE injects `window.__JUCE__` when native integration is enabled, and that
 * only happens inside a host. So the policy cannot disagree with the bridge —
 * both call `nativeBackend()`.
 */

import { nativeBackend } from '../bridge/bridgeCore.js';

/** Audio engine owners. `native` = the plugin; `worklet` = this page. */
export const AUDIO_OWNER = {
  NATIVE: 'native',
  WORKLET: 'worklet',
};

/** Engine status extra: the page was told NOT to start (see startAudioEngine). */
export const AUDIO_STATUS_BLOCKED = 'blocked';

/** True when the page is running inside a JUCE host (plugin editor or bench). */
export function isInsideJuceHost() {
  return nativeBackend() !== null;
}

/**
 * Owner for a given bridge state. Pure, so a renderer can derive it from the
 * store snapshot it already holds (`bridgeAvailable`).
 */
export function audioOwnerFor(bridgeAvailable) {
  return bridgeAvailable ? AUDIO_OWNER.NATIVE : AUDIO_OWNER.WORKLET;
}

/** Short label for the UI. */
export function audioOwnerLabel(owner) {
  return owner === AUDIO_OWNER.NATIVE
    ? 'AUDIO: motor nativo del plugin'
    : 'AUDIO: motor local del navegador (WASM)';
}

/** Reason text for a refused start. */
export const WORKLET_REFUSED_IN_HOST =
  'el motor WASM no arranca dentro del plugin: el audio lo pone el motor nativo (politica 8.1)';

/**
 * The guard the worklet engine calls before touching an AudioContext.
 * Returns false (and says why) when a host owns the audio.
 */
export function workletAllowed() {
  return !isInsideJuceHost();
}

export function workletRefusalReason() {
  return workletAllowed() ? null : WORKLET_REFUSED_IN_HOST;
}
