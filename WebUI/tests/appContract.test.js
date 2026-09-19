/**
 * Contract guard for the WebUI entry point (src/app.js).
 *
 * Two things the host depends on and that are easy to break while moving code
 * around:
 *
 *   1. the ORDER of the boot: the panel must be in the DOM before the store
 *      announces the page, because the host times "panel in DOM" and
 *      "page ready" (`window.__pilotReady`) separately and reports both;
 *   2. the UI stays framework free — that was the whole point of the vanilla
 *      decision (ROADMAP, Fase 8), and it is what keeps the bundle at a
 *      fraction of the React pilot's.
 *
 * The DOM-level contract with the host (first range input = masterLevel, footer
 * <code> as JSON, `data-tab="keys"`) is pinned in tests/panel.test.js, against
 * the real DOM instead of against the source text.
 */

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

const here = dirname(fileURLToPath(import.meta.url));
const read = (...segments) => readFileSync(join(here, ...segments), 'utf8');

describe('WebUI entry contract', () => {
  const app = read('../src/app.js');
  const panel = read('../src/ui/panel.js');
  const html = read('../index.html');

  it('keeps masterLevel as the baseline parameter the host selftest drives', () => {
    expect(app).toContain("BASELINE_PARAMETER_ID = 'masterLevel'");
    // The input itself is a native range input (see buildBaselineControl).
    expect(panel).toContain("slider.type = 'range'");
    expect(panel).toContain('slider.min = \'0\'');
    expect(panel).toContain('slider.max = \'1\'');
  });

  it('mounts the panel BEFORE announcing the page to the host', () => {
    // Anchor on the CALLS (with their semicolon): the module doc mentions
    // `store.start()` in prose, and indexOf would find that first.
    const mounted = app.indexOf('root.append(panel.element);');
    const started = app.indexOf('store.start();');

    expect(mounted).toBeGreaterThan(-1);
    expect(started).toBeGreaterThan(-1);
    expect(mounted).toBeLessThan(started);
  });

  it('the store owns every screen id (bridge + GENERAL)', () => {
    expect(app).toContain('createParameterStore({ ids: SCREEN_PARAMETER_IDS })');
  });

  it('mounts the shared keyboard with the MIDI senders of the store', () => {
    expect(app).toContain("import { mountKeyboard } from './ui/keyboard.js'");
    expect(app).toContain('onNoteOn: store.sendMidiNoteOn');
    expect(app).toContain('onNoteOff: store.sendMidiNoteOff');
    expect(app).toContain('onPitchBend: store.sendMidiPitchBend');
    expect(app).toContain('onModWheel: store.sendMidiModWheel');
    expect(app).toContain('onPanic: store.sendMidiPanic');
  });

  it('feeds controls normalised values and pushes normalised edits back', () => {
    expect(app).toContain('parameterStore.handleChange(id, Number(slider.value))');
    expect(app).toContain("parameterStore.handleGesture(id, 'begin')");
    expect(app).toContain("parameterStore.handleGesture(id, 'end')");
  });

  it('drives the wheels from the host MIDI view (host-driven feedback)', () => {
    expect(app).toContain('keyboard.setMidiState(state.midiState)');
  });

  it('index.html loads app.js as a module and has no framework root', () => {
    expect(html).toContain('<script type="module" src="./src/app.js"></script>');
    expect(html).not.toContain('react');
  });

  it('is framework free: no React anywhere in the entry point', () => {
    expect(app).not.toMatch(/from\s+['"]react/);
    expect(app).not.toMatch(/createRoot|renderHook|useState/);
    expect(panel).not.toMatch(/from\s+['"]react/);
  });

  it('takes its theming from the shared SSOT, not from local copies', () => {
    expect(app).toContain("'@abdsynths/shared/styles/tokens.css'");
    expect(app).toContain("'@abdsynths/shared/styles/components/widgets.css'");
  });
});
