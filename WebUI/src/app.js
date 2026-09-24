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

// Tema, widgets y fondo de la SSOT compartida, más el CSS de esta carpeta.
import '@abdsynths/shared/styles/tokens.css';
import '@abdsynths/shared/styles/components/widgets.css';
import '@abdsynths/shared/styles/components/backgrounds.css';
import './styles/main.css';

import { createParameterStore } from './contracts/paramStore.js';
import { describeControl, getDescriptor } from './contracts/parameters.js';
import { SCREEN_PARAMETER_IDS } from './contracts/screens.js';
import { BANDS, CANVAS, SECTION_ACTIONS, SECTION_VISUALS } from './contracts/sections.js';
import { createPanel } from './ui/panel.js';
import { createModulationRings } from './ui/modulationRings.js';
import { createVisual } from './ui/visuals.js';
import { mountKeyboard } from './ui/keyboard.js';
import { mountFitStage } from '@abdsynths/shared/components';
import { audioOwnerFor } from './audio/policy.js';
import {
  isAudioEngineReady,
  onAudioEngineChange,
  onWorkletMorphZ,
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
 * View-models de los parámetros que pide la vista de DETALLE de un cajón (los
 * mismos `describeControl` que el interface: si el catálogo y el contrato se
 * separan, se pinta con lo que hay y se nota en consola).
 */
function drawerVisualControls(visualSpec) {
  return visualSpec.parameterIds.map(describeControl).filter(Boolean);
}

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

  // El detalle del cajón (MODELOS: espectral + ranuras) es una vista del MISMO
  // catálogo que el interface, con los MISMOS handlers (store, puente, morfeo):
  // mismas variables, cero estado duplicado. El panel la cuelga en el cajón y
  // la repinta con el snapshot del lienzo.
  const drawerVisualSpec = section.drawer?.visual
    ? SECTION_VISUALS[section.drawer.visual]
    : null;
  const drawerVisual = drawerVisualSpec
    ? createVisual(drawerVisualSpec.id, drawerVisualControls(drawerVisualSpec), {
      onLoad: (slot) => store.loadModel(slot),
      onEdit: (id, value, phase) => store.pushParameter(id, value, phase),
      onTelemetry: store.onTelemetry,
    })
    : null;

  return {
    ...section,
    controls,
    // Las acciones de la ficha tambien se resuelven aqui, contra su catalogo, para
    // que el panel reciba el boton ya masticado (mismo trato que los controles).
    action: section.action ? SECTION_ACTIONS[section.action] ?? null : null,
    // Y una vista puede necesitar algo que NO es un parametro: las ranuras de modelo
    // A-D piden al host cargar un fichero (la pagina no tiene sistema de ficheros).
    // El handler viaja por aqui para que el panel siga sin saber que dibuja cada una.
    drawerVisual,
    visual: visualSpec
      ? createVisual(visualSpec.id, visualControls, {
        onLoad: (slot) => store.loadModel(slot),
        // El pad de la ficha MODELOS edita morphX/morphY: un gesto suyo son DOS
        // gestos coordinados (uno por eje) con fase completa. pushParameter es
        // la primitiva; handleChange solo sabe cerrar UN id.
        onEdit: (id, value, phase) => store.pushParameter(id, value, phase),
        // El espectral de la ficha MODELOS se cuelga del canal de telemetria
        // (pintura en vivo, fuera del ciclo setState).
        onTelemetry: store.onTelemetry,
      })
      : null,
  };
}));

let paint = () => {};
let engineSnapshot = { status: 'idle', error: null, sampleRate: 0, voices: 0 };

// La vista del pad XY (ficha MODELOS) queda a mano para el feed del worklet:
// en standalone (sin bridge) es el meter del worklet quien pinta su anillo z
// con la modulacion del destino 28; en plugin lo hace la telemetria nativa.
const canvasMorphZView = bands
  .flat()
  .find((section) => section.visual?.element?.dataset.visual === 'model-xy')
  ?.visual ?? null;

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

  // Anillos de modulacion: el frame de telemetria suma sobre cada destino;
  // el mapa de knobs lo alimenta la misma coleccion que pinta los snapshots.
  const modRings = createModulationRings(panel.knobsById);

  // Anillo morphZ del pad en standalone: el meter del worklet trae la
  // contribucion del destino 28 (en plugin la trae el frame de telemetria
  // nativo, que visuals.js consume del canal bridge). Mismo destino, dos caminos.
  onWorkletMorphZ((mod) => canvasMorphZView?.setZMod(mod));

  const renderAudio = () => panel.paintAudio({ owner, ...engineSnapshot });

  root.append(panel.element);

  // El lienzo es de diseño FIJO (CANVAS) y el editor es redimensionable: escalar
  // para caber entero — sin esto, una ventana baja corta el pie y la franja de
  // teclado (el fallo "no se distinguen las teclas": el keybed estaba FUERA).
  mountFitStage(root, { width: CANVAS.width, height: CANVAS.height });

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

// Telemetria (nativa -> web): los frames NO son estado (a ~15 Hz no pasan por
// setState); alimentan directamente la pintura en tiempo real. Hoy, el anillo
// de modulacion; el espectral y el scope se cuelgan del mismo canal.
store.onTelemetry((frame) => modRings.handleFrame(frame));

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
