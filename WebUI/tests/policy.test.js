/**
 * The audio policy (src/audio/policy.js) — one engine at a time.
 *
 * The rule is decided by ONE signal, `window.__JUCE__` (what JUCE injects inside
 * a host), and the same helper the bridge uses. These tests pin both directions
 * and, above all, that the worklet guard cannot be fooled by a missing argument:
 * inside a host it always refuses.
 */

import { afterEach, describe, expect, it } from 'vitest';

import {
  AUDIO_OWNER,
  AUDIO_STATUS_BLOCKED,
  WORKLET_REFUSED_IN_HOST,
  audioOwnerFor,
  audioOwnerLabel,
  isInsideJuceHost,
  workletAllowed,
  workletRefusalReason,
} from '../src/audio/policy.js';

afterEach(() => {
  delete window.__JUCE__;
});

describe('audio ownership', () => {
  it('maps a live bridge to the native engine and local mode to the worklet', () => {
    expect(audioOwnerFor(true)).toBe(AUDIO_OWNER.NATIVE);
    expect(audioOwnerFor(false)).toBe(AUDIO_OWNER.WORKLET);
  });

  it('labels the owner for the UI', () => {
    expect(audioOwnerLabel(AUDIO_OWNER.NATIVE)).toContain('nativo');
    expect(audioOwnerLabel(AUDIO_OWNER.WORKLET)).toContain('navegador');
  });
});

describe('the worklet guard', () => {
  it('allows the worklet in a plain browser', () => {
    expect(isInsideJuceHost()).toBe(false);
    expect(workletAllowed()).toBe(true);
    expect(workletRefusalReason()).toBeNull();
  });

  it('refuses it inside a host, with the reason', () => {
    window.__JUCE__ = { backend: { emitEvent() {}, addEventListener() { return 1; }, removeEventListener() {} } };

    expect(isInsideJuceHost()).toBe(true);
    expect(workletAllowed()).toBe(false);
    expect(workletRefusalReason()).toBe(WORKLET_REFUSED_IN_HOST);
  });

  it('is decided by the SAME signal the bridge uses (no second detection)', () => {
    // A backend without the full API still means "there is a host": the bridge
    // would attach to it, so the policy must refuse too.
    window.__JUCE__ = { backend: {} };

    expect(isInsideJuceHost()).toBe(true);
  });

  it('exposes a distinct status for a refused start', () => {
    expect(AUDIO_STATUS_BLOCKED).toBe('blocked');
  });
});
