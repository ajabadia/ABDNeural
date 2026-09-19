/**
 * NEURONiK WebUI — arranque.
 *
 * JS vainilla sobre WebView2: store (contrato + bridge) + panel DOM + teclado
 * compartido. No hay framework, y el armazón del piloto React (que sí lo tenía)
 * no se porta: lo que se porta es su lógica, ya en src/contracts y src/bridge.
 *
 * El orden de arranque importa y está fijado por dos consumidores externos:
 *
 *   1. el panel se monta ANTES de `store.start()`, porque el host mide
 *      "panel in DOM" y "react ready" por separado (`window.__pilotReady`);
 *   2. el teclado se monta DESPUÉS de `start()`, cuando ya se sabe si hay host
 *      (de eso depende su etiqueta LIVE/LOCAL), y con los contenedores ya en el
 *      documento — `createKeyboard` los busca con `getElementById`.
 */

// Tema y widgets de la SSOT compartida, más el CSS de esta carpeta.
import '@abdsynths/shared/styles/tokens.css';
import '@abdsynths/shared/styles/components/widgets.css';
import './styles/main.css';

import { createParameterStore } from './contracts/paramStore.js';
import { describeControl } from './contracts/parameters.js';
import { GENERAL_PARAMETER_IDS, SCREEN_PARAMETER_IDS } from './contracts/screens.js';
import { createPanel } from './ui/panel.js';
import { mountKeyboard } from './ui/keyboard.js';

/**
 * The baseline parameter keeps a native range input ON PURPOSE: the host's
 * --selftest drives `document.querySelector('input[type=range]')` and that must
 * be masterLevel, so it is the FIRST control of the BRIDGE screen.
 */
const BASELINE_PARAMETER_ID = 'masterLevel';

const root = document.getElementById('app');
const store = createParameterStore({ ids: SCREEN_PARAMETER_IDS });

let paint = () => {};

if (root) {
  const panel = createPanel({
    bridgeControls: [describeControl(BASELINE_PARAMETER_ID)].filter(Boolean),
    generalControls: GENERAL_PARAMETER_IDS.map(describeControl).filter(Boolean),
    handlers: { onPanic: () => store.sendMidiPanic() },
  });

  root.append(panel.element);

  bindBaseline(panel.element.querySelector(`#${BASELINE_PARAMETER_ID}`), store);

  // Connect (or fall into local mode) once the panel is in the DOM: the host
  // times the page startup against `window.__pilotReady`, which start() sets.
  store.start();

  const keyboard = mountKeyboard({
    root: panel.keysRoot,
    bridgeAvailable: store.getState().bridgeAvailable,
    callbacks: {
      onNoteOn: store.sendMidiNoteOn,
      onNoteOff: store.sendMidiNoteOff,
      onPitchBend: store.sendMidiPitchBend,
      onModWheel: store.sendMidiModWheel,
      onPanic: store.sendMidiPanic,
    },
  });

  paint = (state) => {
    panel.paint(state);
    // Host-driven feedback: the plugin's external MIDI view moves the wheels.
    keyboard.setMidiState(state.midiState);
  };
}

// subscribe() paints immediately, so the panel never renders a blank frame.
store.subscribe(paint);

/**
 * Wire the baseline slider: normalised value on the wire, real units in the
 * readout, and the gesture protocol around the drag (begin/change/end).
 */
function bindBaseline(slider, parameterStore) {
  if (!slider) return;

  const id = slider.dataset.parameterId;

  slider.addEventListener('pointerdown', () => {
    parameterStore.handleGesture(id, 'begin');
  });

  slider.addEventListener('input', () => {
    parameterStore.handleChange(id, Number(slider.value));
  });

  for (const eventName of ['pointerup', 'pointercancel', 'blur']) {
    slider.addEventListener(eventName, () => {
      parameterStore.handleGesture(id, 'end');
    });
  }
}
