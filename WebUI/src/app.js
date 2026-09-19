/**
 * NEURONiK WebUI — arranque.
 *
 * Va en JS vainilla sobre el store (src/contracts/paramStore.js) y los controles
 * compartidos de `@abdsynths/shared`; no hay framework. Esta pantalla es el
 * andamiaje de la Fase 8: monta el store, muestra el estado del bridge y del
 * contrato, y deja el control base que el host usa para medir el arranque.
 * El reparto por pestañas (GENERAL, RESONATOR, FILTER/ENV, FX, LFO/MOD, BROWSER)
 * es el ticket 8.2.
 */

// Tema y widgets de la SSOT compartida, más el CSS de esta carpeta.
import '@abdsynths/shared/styles/tokens.css';
import '@abdsynths/shared/styles/components/widgets.css';
import './styles/main.css';

import { createParameterStore } from './contracts/paramStore.js';
import {
  describeParam,
  displayText,
  realFromNormalized,
} from './contracts/paramValue.js';

/**
 * The baseline parameter keeps a native range input ON PURPOSE: the host's
 * --selftest drives `document.querySelector('input[type=range]')` and that
 * must be masterLevel. Everything else renders through the shared family.
 */
const BASELINE_PARAMETER_ID = 'masterLevel';

const root = document.getElementById('app');
const store = createParameterStore();

const baselineControl = describeParam(BASELINE_PARAMETER_ID);

let paint = () => {};

if (root) {
  const { header, status, summary, readout, slider } = buildLayout(baselineControl);

  paint = (state) => {
    status.textContent = state.bridgeAvailable
      ? 'bridge: conectado al host (WebView2)'
      : 'bridge: modo local (sin host)';

    summary.textContent = contractLine(state.summary);

    const normalized = state.parameters[BASELINE_PARAMETER_ID] ?? 0;

    if (document.activeElement !== slider) slider.value = String(normalized);

    readout.textContent =
      `${baselineControl.label}: ${displayText(baselineControl, realFromNormalized(baselineControl, normalized))}`;
  };

  bindBaseline(slider, baselineControl, store);
  root.append(header, status, summary, createBaselineCard(baselineControl, slider, readout));

  store.subscribe(paint);
} else {
  store.subscribe(() => {});
}

store.start();

/** `70 parameters · 62 wired to the DSP · …` — from the generated contract. */
function contractLine(summary) {
  return `${summary.total} parameters · ${summary.implemented} wired to the DSP · `
    + `${summary.uiOnly} UI only · ${summary.notRouted} not routed`;
}

/** Header + status line + contract summary (the caller appends them in order). */
function buildLayout(control) {
  const header = document.createElement('div');
  header.className = 'webui-header';

  const title = document.createElement('h1');
  title.textContent = `NEURONiK · ${control.group}`;

  header.append(title);

  const status = document.createElement('p');
  status.className = 'webui-status';

  const summary = document.createElement('p');
  summary.className = 'webui-status';

  const slider = document.createElement('input');
  slider.type = 'range';
  slider.id = BASELINE_PARAMETER_ID;
  slider.dataset.parameterId = BASELINE_PARAMETER_ID;
  slider.min = '0';
  slider.max = '1';
  slider.step = '0.001';

  const readout = createReadout();

  return { header, status, summary, readout, slider };
}

function createReadout() {
  const readout = document.createElement('span');
  readout.className = 'webui-readout';
  readout.dataset.parameterReadout = BASELINE_PARAMETER_ID;

  return readout;
}

function createBaselineCard(control, slider, readout) {
  const card = document.createElement('section');
  card.className = 'webui-card';

  const heading = document.createElement('h2');
  heading.textContent = 'CONTRATO · control base';

  const row = document.createElement('div');
  row.className = 'webui-row';

  const label = document.createElement('label');
  label.htmlFor = BASELINE_PARAMETER_ID;
  label.textContent = control.label;

  row.append(label, readout);
  card.append(heading, row, slider);

  const note = document.createElement('p');
  note.className = 'webui-note';
  note.textContent =
    'Andamiaje de la Fase 8: store + bridge + contrato generado, sin framework. '
    + 'Las pestañas de paridad (GENERAL, RESONATOR, FILTER/ENV, FX, LFO/MOD, BROWSER) llegan en el ticket 8.2.';

  card.append(note);

  return card;
}

/**
 * Wire the baseline slider: normalised value on the wire, real units in the
 * readout, and the gesture protocol around the drag (begin/change/end).
 */
function bindBaseline(slider, control, parameterStore) {
  slider.addEventListener('pointerdown', () => {
    parameterStore.handleGesture(control.id, 'begin');
  });

  slider.addEventListener('input', () => {
    parameterStore.handleChange(control.id, Number(slider.value));
  });

  for (const eventName of ['pointerup', 'pointercancel', 'blur']) {
    slider.addEventListener(eventName, () => {
      parameterStore.handleGesture(control.id, 'end');
    });
  }
}
