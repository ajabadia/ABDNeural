/**
 * The page shell: tabs, screens and the state footer.
 *
 * Vanilla DOM, no framework. The panel owns its LOCAL view state (the active
 * tab) and paints the shared state it is handed; it never reads the store
 * directly, so it is testable on its own (tests/panel.test.js).
 *
 * Two contracts with the host's `--selftest` live here and must not be
 * "simplified" away:
 *
 *   1. the FIRST `input[type=range]` of the document is the masterLevel slider
 *      (the host drives it in both directions; tests/appContract.test.js and
 *      tests/panel.test.js pin it);
 *   2. `footer.panel-footer code` carries the NORMALISED state as JSON — the
 *      host parses it to check that a screen's ids really are on the page.
 */

import { AUDIO_OWNER, audioOwnerLabel } from '../audio/policy.js';
import { displayText, realFromNormalized } from '../contracts/paramValue.js';
import { TABS } from '../contracts/screens.js';

/**
 * @param {object} options
 * @param {object[]} options.bridgeControls   view-models of the BRIDGE screen
 * @param {object[]} options.generalControls  view-models of the GENERAL screen
 * @param {object} [options.handlers]
 * @param {() => void} [options.handlers.onPanic]       KEYS toolbar PANIC button
 * @param {() => void} [options.handlers.onStartSound]  SOUND ON (local mode only)
 * @returns {{ element: HTMLElement, keysRoot: HTMLElement, paint: Function, paintAudio: Function, selectTab: Function, destroy: Function }}
 */
export function createPanel({ bridgeControls, generalControls, handlers = {} }) {
  const baseline = bridgeControls[0] ?? null;
  let baselineSlider = null;
  const readouts = new Map();
  const generalValues = new Map();
  const screens = new Map();
  const tabButtons = [];

  const element = document.createElement('section');
  element.className = 'panel';

  const title = document.createElement('h1');
  title.textContent = 'NEURONiK';

  const status = document.createElement('p');
  status.className = 'status';

  const contractLine = document.createElement('p');
  contractLine.className = 'contract-line';

  // Audio ownership (policy of src/audio/policy.js). In a host this is a
  // READOUT, not a control: the plugin owns the audio and the page cannot start
  // a second engine, so there is no button to press.
  const audioRow = document.createElement('div');
  audioRow.className = 'audio-mode';

  const audioLabel = document.createElement('span');
  audioLabel.className = 'audio-mode__label';

  const audioDetail = document.createElement('span');
  audioDetail.className = 'audio-mode__detail';

  const audioButton = document.createElement('button');
  audioButton.type = 'button';
  audioButton.className = 'audio-start';
  audioButton.textContent = 'SOUND ON';
  audioButton.hidden = true;
  audioButton.addEventListener('click', () => handlers.onStartSound?.());

  audioRow.append(audioLabel, audioDetail, audioButton);

  const tabBar = document.createElement('div');
  tabBar.className = 'tabs';
  tabBar.setAttribute('role', 'tablist');
  tabBar.setAttribute('aria-label', 'Screens');

  // Screens are built detached and appended in order at the end, so the DOM
  // order is explicit and the first range input is guaranteed to be the
  // baseline control.
  for (const tab of TABS) {
    const button = document.createElement('button');
    button.type = 'button';
    button.setAttribute('role', 'tab');
    button.dataset.tab = tab.tab ?? tab.id;
    button.textContent = tab.label;
    button.addEventListener('click', () => selectTab(tab.id));
    tabButtons.push({ button, id: tab.id });
    tabBar.append(button);

    const screen = document.createElement('div');
    screen.className = 'tab-group';
    screen.dataset.screen = tab.id;
    screens.set(tab.id, screen);
  }

  // --- BRIDGE ---------------------------------------------------------------
  const bridgeScreen = screens.get('bridge');

  if (baseline) {
    const built = buildBaselineControl(baseline, readouts);
    baselineSlider = built.slider;
    bridgeScreen.append(built.wrapper, buildReadoutRow(baseline, readouts));
  }

  const bridgeNote = document.createElement('p');
  bridgeNote.className = 'tab-note';
  bridgeNote.textContent =
    'Andamiaje de la Fase 8: aquí solo vive el CONTROL BASE, el que el --selftest del host '
    + 'conduce en las dos direcciones. La paridad de los 70 parámetros con el panel nativo, '
    + 'con knobs y envolvente dibujada, es el ticket 8.2.';
  bridgeScreen.append(bridgeNote);

  // --- GENERAL --------------------------------------------------------------
  const generalScreen = screens.get('general');

  for (const control of generalControls) {
    const row = document.createElement('div');
    row.className = 'param-row';
    row.dataset.parameterId = control.id;

    const label = document.createElement('span');
    label.className = 'param-row__label';
    label.textContent = control.dspStatus === 'implemented' ? control.label : `${control.label} *`;

    const value = document.createElement('span');
    value.className = 'param-row__value';

    row.append(label, value);
    generalScreen.append(row);
    generalValues.set(control.id, value);
  }

  const generalNote = document.createElement('p');
  generalNote.className = 'tab-note';
  generalNote.textContent =
    'Andamiaje de la Fase 8: los valores son los del contrato generado y viven en el estado '
    + 'que el bridge sincroniza. Los widgets de la familia compartida (knob/slider/toggle) '
    + 'llegan en el ticket 8.2; * marca los parámetros que el motor no consume (ver contrato).';
  generalScreen.append(generalNote);

  // --- KEYS -----------------------------------------------------------------
  const keysScreen = screens.get('keys');

  const keysToolbar = document.createElement('div');
  keysToolbar.className = 'keys-toolbar';

  const panicButton = document.createElement('button');
  panicButton.type = 'button';
  panicButton.className = 'keys-panic';
  panicButton.textContent = 'PANIC';
  panicButton.addEventListener('click', () => handlers.onPanic?.());

  const keysStatus = document.createElement('span');
  keysStatus.className = 'keys-status';

  keysToolbar.append(panicButton, keysStatus);

  const keysRoot = document.createElement('div');
  keysRoot.id = 'keys-root';

  keysScreen.append(keysToolbar, keysRoot);

  // --- footer ---------------------------------------------------------------
  const footer = document.createElement('footer');
  footer.className = 'panel-footer';

  const footerText = document.createElement('span');
  const footerState = document.createElement('code');

  footer.append(footerText, footerState);

  element.append(title, audioRow, status, contractLine, tabBar, ...screens.values(), footer);

  let activeTab = TABS[0].id;

  /** Show one screen (all of them stay mounted: the host reads them by selector). */
  function selectTab(id) {
    if (!screens.has(id)) return;

    activeTab = id;

    for (const [screenId, screen] of screens) screen.hidden = screenId !== id;

    for (const { button, id: buttonId } of tabButtons) {
      const isActive = buttonId === id;
      button.classList.toggle('tab--active', isActive);
      button.setAttribute('aria-selected', String(isActive));
    }
  }

  selectTab(activeTab);

  /** Repaint from one state snapshot. Cheap enough to run on every change. */
  function paint(state) {
    const { parameters, snapshotVersion, bridgeAvailable } = state;

    status.textContent = bridgeAvailable
      ? `bridge: conectado al host (WebView2) · snapshot #${snapshotVersion}`
      : 'bridge: modo local (sin host)';

    contractLine.textContent = contractLineFor(state);

    if (baselineSlider && baseline) {
      const normalized = parameters[baseline.id] ?? 0;

      // Never fight the user's finger: while the slider is focused it owns the value.
      if (document.activeElement !== baselineSlider) baselineSlider.value = String(normalized);

      paintReadout(readouts.get(baseline.id), baseline, normalized);
    }

    for (const [id, valueElement] of generalValues) {
      const control = generalControls.find((candidate) => candidate.id === id);
      const normalized = parameters[id];

      valueElement.textContent = control && typeof normalized === 'number'
        ? displayText(control, realFromNormalized(control, normalized))
        : '—';
    }

    keysStatus.textContent = bridgeAvailable ? 'MIDI → PLUGIN LIVE' : 'LOCAL MODE';
    keysStatus.classList.toggle('keys-status--local', !bridgeAvailable);

    footerText.textContent = `Parameter updates: ${state.changeCount}`
      + (state.contractErrors.length > 0
          ? ` · contract errors: ${state.contractErrors.join(', ')}`
          : '');

    // THE host contract: normalised state as JSON (see the module doc).
    footerState.textContent = JSON.stringify(parameters);
  }

  /**
   * Audio line: who owns the engine, and (only in local mode) how to start it.
   * Fed by app.js from the policy + the engine's own state broadcast.
   */
  function paintAudio({ owner, status = 'idle', sampleRate = 0, error = null }) {
    audioLabel.textContent = audioOwnerLabel(owner);

    const localMode = owner === AUDIO_OWNER.WORKLET;

    if (!localMode) {
      audioDetail.textContent = '· sin control en la página';
      audioButton.hidden = true;
      return;
    }

    switch (status) {
      case 'ready':
        audioDetail.textContent = `· ON · ${(sampleRate / 1000).toFixed(1)} kHz`;
        audioButton.hidden = true;
        break;
      case 'loading':
        audioDetail.textContent = '· arrancando…';
        audioButton.hidden = true;
        break;
      case 'error':
        audioDetail.textContent = `· ERROR${error ? `: ${error}` : ''}`;
        audioButton.textContent = 'REINTENTAR';
        audioButton.hidden = false;
        break;
      case 'unsupported':
      case 'blocked':
        audioDetail.textContent = `· ${error ?? status}`;
        audioButton.hidden = true;
        break;
      default:
        audioDetail.textContent = '· sin arrancar';
        audioButton.textContent = 'SOUND ON';
        audioButton.hidden = false;
        break;
    }
  }

  function destroy() {
    element.textContent = '';
  }

  return { element, keysRoot, paint, paintAudio, selectTab, destroy, getActiveTab: () => activeTab };
}

/** Baseline control: a native range input, normalised 0..1 on the wire. */
function buildBaselineControl(control, readouts) {
  const slider = document.createElement('input');
  slider.type = 'range';
  slider.id = control.id;
  slider.dataset.parameterId = control.id;
  slider.min = '0';
  slider.max = '1';
  slider.step = '0.001';

  const label = document.createElement('label');
  label.className = 'control-label';
  label.htmlFor = control.id;
  label.textContent = control.label;

  const readout = document.createElement('span');
  readout.className = 'readout';
  readout.dataset.parameterReadout = control.id;
  readouts.set(control.id, readout);

  const wrapper = document.createElement('div');
  wrapper.className = 'control';
  wrapper.append(label, slider);

  return { wrapper, slider };
}

function buildReadoutRow(control, readouts) {
  const row = document.createElement('div');
  row.className = 'readout-row';
  row.append(readouts.get(control.id));

  return row;
}

function paintReadout(readout, control, normalized) {
  if (!readout) return;

  readout.textContent =
    `${control.label}: ${displayText(control, realFromNormalized(control, normalized))}`;
}

function contractLineFor(state) {
  const { summary } = state;

  return `${summary.total} parameters · ${summary.implemented} wired to the DSP · `
    + `${summary.uiOnly} UI only · ${summary.notRouted} not routed`;
}
