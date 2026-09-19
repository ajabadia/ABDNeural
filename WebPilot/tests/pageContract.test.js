/**
 * Contract guard for the React wrappers: the page and the host must agree on
 * which control renders each pilot parameter, and the baseline slider that the
 * host's --selftest drives must stay a native <input type="range">.
 *
 * This runs against the SOURCE (not the build) so it fails at test time, not
 * inside WebView2.
 */

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

const here = dirname(fileURLToPath(import.meta.url));
const read = (...segments) =>
  readFileSync(join(here, ...segments), 'utf8');

describe('pilot page contract', () => {
  const page = read('../app/page.jsx');

  it('keeps masterLevel as the native baseline input the host selftest drives', () => {
    expect(page).toContain("BASELINE_PARAMETER_ID = 'masterLevel'");

    const baselineBlock = page.slice(
      page.indexOf('BASELINE_PARAMETER_ID'),
      page.indexOf('control.kind === \'choice\''),
    );

    expect(baselineBlock).toContain('type="range"');
  });

  it('renders the other parameters through the shared wrappers', () => {
    expect(page).toContain('ParamSlider');
    expect(page).toContain('ParamChoice');
  });

  it('does NOT start the worklet inside a host (audio policy of 8.1)', () => {
    // Inside the plugin the audio is the plugin's; the page must not spin up a
    // second engine. The mirror of this rule for the vanilla UI lives in
    // WebUI/src/audio/policy.js.
    expect(page).toContain('AUDIO: NATIVO');
    expect(page).toContain('if (insideHost) return;');
    expect(page).toContain('audioControl(bridgeAvailable)');
  });

  it('feeds wrappers normalised state and pushes normalised changes', () => {
    // The state value goes straight into the wrapper; edits come back the same way.
    expect(page).toContain('value={normalized}');
    expect(page).toContain('handleChange(control.id');
  });
});

describe('wrapper module contract', () => {
  const controls = read('../lib/controls.jsx');

  it('wraps the shared family, not custom reimplementations', () => {
    expect(controls).toContain("from '@abdsynths/shared/components'");
    expect(controls).toContain('useSharedControl(Knob');
    expect(controls).toContain('useSharedControl(Slider');
    expect(controls).toContain('useSharedControl(Toggle');
  });

  it('destroys the imperative control on unmount (no DOM/listener leaks)', () => {
    expect(controls).toContain('control.destroy()');
  });
});
