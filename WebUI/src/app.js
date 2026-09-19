/**
 * NEURONiK WebUI — arranque.
 *
 * JS vainilla sobre WebView2: store (contrato + bridge) + panel DOM + teclado
 * compartido. No hay framework, y el armazón del piloto React (que sí lo tenía)
 * no se porta: lo que se porta es su lógica, ya en src/contracts, src/bridge,
 * src/wasm y src/audio.
 *
 * El orden de arranque importa y está fijado por dos consumidores externos:
 *
 *   1. el panel se monta ANTES de `store.start()`, porque el host mide
 *      "panel in DOM" y "page ready" (`window.__pilotReady`) por separado;
 *   2. el teclado se monta DESPUÉS de `start()`, cuando ya se sabe si hay host
 *      (de eso depende su etiqueta LIVE/LOCAL), y con los contenedores ya en el
 *      documento — `createKeyboard` los busca con `getElementById`.
 *
 * Audio: la página puede sonar de dos maneras y NUNCA a la vez (ver
 * src/audio/policy.js). Dentro de un host el audio es del plugin y aquí no hay
 * control de audio; en un navegador, SOUND ON arranca el motor WASM y el estado
 * se empuja al worklet por un solo camino.
 */

// Tema y widgets de la SSOT compartida, más el CSS de esta carpeta.
import '@abdsynths/shared/styles/tokens.css';
import '@abdsynths/shared/styles/components/widgets.css';
import './styles/main.css';

import { createParameterStore } from './contracts/paramStore.js';
import { describeControl, getDescriptor } from './contracts/parameters.js';
import { SCREEN_PARAMETER_IDS } from './contracts/screens.js';
import { BANDS, SECTION_ACTIONS, SECTION_VISUALS } from './contracts/sections.js';
import { createPanel } from './ui/panel.js';
import { createVisual } from './ui/visuals.js';
import { mountKeyboard } from './ui/keyboard.js';
import { audioOwnerFor } from './audio/policy.js';
import {
  isAudioEngineReady,
  onAudioEngineChange,
  panicWorklet,
  pushEngineToWorklet,
  pushMidiToWorklet,
  pushModelsToWorklet,
  pushParamsToWorklet,
  startAudioEngine,
} from './audio/audioWorkletEngine.js';
import { engineIndexFromNormalized } from './wasm/audioParams.js';

/**
 * The baseline parameter keeps a native range input ON PURPOSE: the host's
 * --selftest drives `document.querySelector('input[type=range]')` and that must
 * be masterLevel, so it is the FIRST control of the BRIDGE screen.
 */
const BASELINE_PARAMETER_ID = 'masterLevel';

const root = document.getElementById('app');
const store = createParameterStore({ ids: SCREEN_PARAMETER_IDS });

/**
 * Las bandas del lienzo con sus controles ya resueltos contra el contrato. El
 * reparto (qué ids van en cada ficha) es de `contracts/sections.js`; aquí solo se
 * convierte en view-models. Un id que no esté en el contrato se descarta en vez
 * de pintar un control con límites inventados (el store lo reporta como error).
 */
const bands = BANDS.map((band) => band.map((section) => {
  const controls = section.ids.map(describeControl).filter(Boolean);
  // La vista pide SUS parámetros, no todos los de la ficha (y avisa si el catálogo
  // y el contrato se separan: se pinta con lo que haya, pero se nota en consola).
  const visualSpec = section.visual ? SECTION_VISUALS[section.visual] : null;
  const visualControls = visualSpec
    ? visualSpec.parameterIds.map(describeControl).filter(Boolean)
    : [];

  if (visualSpec && visualControls.length !== visualSpec.parameterIds.length)
    console.warn(`visual "${visualSpec.id}": el catalogo pide id(s) que el contrato no tiene`);

  return {
    ...section,
    controls,
    // Las acciones de la ficha tambien se resuelven aqui, contra su catalogo, para
    // que el panel reciba el boton ya masticado (mismo trato que los controles).
    action: section.action ? SECTION_ACTIONS[section.action] ?? null : null,
    // Y una vista puede necesitar algo que NO es un parametro: las ranuras de modelo
    // A-D piden al host cargar un fichero (la pagina no tiene sistema de ficheros).
    // El handler viaja por aqui para que el panel siga sin saber que dibuja cada una.
    visual: visualSpec
      ? createVisual(visualSpec.id, visualControls, {
        onLoad: (slot) => store.loadModel(slot),
      })
      : null,
  };
}));

let paint = () => {};
let engineSnapshot = { status: 'idle', error: null, sampleRate: 0, voices: 0 };

if (root) {
  const panel = createPanel({
    bands,
    baselineId: BASELINE_PARAMETER_ID,
    handlers: {
      // Un edit de cualquier celda, en NORMALIZADO (el store cierra la fase: ver
      // el contrato de gestos en src/ui/controls.js).
      onChange: (id, normalized) => store.handleChange(id, normalized),
      onGesture: (id, phase) => store.handleGesture(id, phase),
      // Acciones de ficha: hoy solo RANDOM (el sorteo lo hace el procesador).
      onAction: (id) => {
        if (id === 'randomize') store.randomize();
      },
      // PANIC stops everything the plugin sounds; in local mode the worklet has
      // no panic path of its own in the host, and here it is a no-op without engine.
      onPanic: () => {
        store.sendMidiPanic();
        panicWorklet();
      },
      onStartSound: () => {
        // Refuses by itself inside a host: see src/audio/policy.js.
        startAudioEngine();
      },
    },
  });

  let owner = audioOwnerFor(false);
  let lastEngineIndex = -1;

  const renderAudio = () => panel.paintAudio({ owner, ...engineSnapshot });

  root.append(panel.element);

  bindBaseline(panel.element.querySelector(`#${BASELINE_PARAMETER_ID}`), store);

  // Connect (or fall into local mode) once the panel is in the DOM: the host
  // times the page startup against `window.__pilotReady`, which start() sets.
  store.start();

  const keyboard = mountKeyboard({
    root: panel.keysRoot,
    bridgeAvailable: store.getState().bridgeAvailable,
    callbacks: {
      // Dual path: the bridge when there is a plugin, the worklet when the page
      // is on its own. Only one of the two can be live (audio policy).
      onNoteOn: (note, velocity) => {
        store.sendMidiNoteOn(note, velocity);
        pushMidiToWorklet({ kind: 'noteOn', note, velocity });
      },
      onNoteOff: (note) => {
        store.sendMidiNoteOff(note);
        pushMidiToWorklet({ kind: 'noteOff', note });
      },
      onPitchBend: (value) => {
        store.sendMidiPitchBend(value);
        pushMidiToWorklet({ kind: 'pitchBend', value });
      },
      onModWheel: store.sendMidiModWheel,
      onPanic: () => {
        store.sendMidiPanic();
        panicWorklet();
      },
    },
  });

  // The engine reports on its own channel: its state changes outside store
  // updates (loading, ready, sample rate). Fires immediately with the current one.
  onAudioEngineChange((engine) => {
    engineSnapshot = engine;
    renderAudio();
  });

  /**
   * Page -> worklet sync. ONE path for both page edits and native snapshots, so
   * a preset load or a natively moved parameter reaches the WASM engine too.
   * The engine is pushed separately because switching it rebuilds the DSP
   * (expensive) and only matters when the index actually changes.
   */
  function syncEngine(state) {
    if (!isAudioEngineReady()) return;

    const index = engineIndexFromNormalized(
      state.parameters.engineType ?? 0, getDescriptor('engineType'));

    if (index !== lastEngineIndex) {
      lastEngineIndex = index;
      pushEngineToWorklet(index);

      // Models hang off the concrete engine: the switch rebuilt it, so re-apply
      // what the bridge sent.
      if (state.models) pushModelsToWorklet(state.models);
    }

    pushParamsToWorklet(state.parameters);
  }

  paint = (state) => {
    owner = audioOwnerFor(state.bridgeAvailable);

    panel.paint(state);
    // Host-driven feedback: the plugin's external MIDI view moves the wheels.
    keyboard.setMidiState(state.midiState);
    renderAudio();
    syncEngine(state);
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
