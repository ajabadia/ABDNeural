/**
 * Contract guard for the WebUI entry point: the page and the host must agree on
 * which control the host's `--selftest` drives, and the UI must stay framework
 * free (the whole point of the vanilla decision in ROADMAP Fase 8).
 *
 * Like the pilot's pageContract test, this runs against the SOURCE so a
 * regression fails at test time and not inside WebView2.
 */

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

const here = dirname(fileURLToPath(import.meta.url));
const read = (...segments) => readFileSync(join(here, ...segments), 'utf8');

describe('WebUI entry contract', () => {
  const app = read('../src/app.js');
  const html = read('../index.html');

  it('keeps masterLevel as the native baseline input the host selftest drives', () => {
    expect(app).toContain("BASELINE_PARAMETER_ID = 'masterLevel'");

    const baselineBlock = app.slice(
      app.indexOf('BASELINE_PARAMETER_ID'),
      app.indexOf('function createReadout'),
    );

    expect(baselineBlock).toContain("slider.type = 'range'");
  });

  it('mounts the store and announces the page', () => {
    expect(app).toContain("import { createParameterStore } from './contracts/paramStore.js'");
    expect(app).toContain('createParameterStore()');
    expect(app).toContain('store.start()');
  });

  it('feeds controls normalised values and pushes normalised edits back', () => {
    expect(app).toContain('state.parameters[BASELINE_PARAMETER_ID]');
    expect(app).toContain('parameterStore.handleChange(control.id, Number(slider.value))');
    expect(app).toContain("parameterStore.handleGesture(control.id, 'begin')");
  });

  it('index.html loads app.js as a module and has no framework root', () => {
    expect(html).toContain('<script type="module" src="./src/app.js"></script>');
    expect(html).not.toContain('react');
  });

  it('is framework free: no React anywhere in the entry point', () => {
    expect(app).not.toMatch(/from\s+['"]react/);
    expect(app).not.toMatch(/createRoot|renderHook|useState/);
  });

  it('takes its theming from the shared SSOT, not from local copies', () => {
    expect(app).toContain("'@abdsynths/shared/styles/tokens.css'");
    expect(app).toContain("'@abdsynths/shared/styles/components/widgets.css'");
  });
});
