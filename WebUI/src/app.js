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
import { createLcdTop } from './ui/lcdTop.js';
import { pushTelemetryFrame } from './bridge/telemetry.js';
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
  onWorkletEnvelopeLevels,
  onWorkletMorphZ,
  onWorkletVoices,
  panicWorklet,
  pushEngineToWorklet,
  pushMidiToWorklet,
  pushModelsToWorklet,
  pushMorphToWorklet,
  pushParamsToWorklet,
  getWorkletMorph,
  startAudioEngine,
} from './audio/audioWorkletEngine.js';
import { readModelFile, emptyLocalModels } from './audio/localModels.js';
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
 * Carga LOCAL de modelos (modo navegador, sin host): el input de fichero
 * es el dialogo que en el plugin abre el host nativo. Se fabrica UNA vez
 * y queda oculto; `store.loadModel(slot)` lo pide cuando el modo local lo
 * necesita (el store entrega el modelo al worklet por el canal que ya
 * existe — `neuronik:models` — y pinta la ficha con su `models`).
 */
const localModelInput = document.createElement('input');
localModelInput.type = 'file';
localModelInput.accept = '.neuronikmodel,application/json';
localModelInput.hidden = true;
let localModelSlot = -1;

localModelInput.addEventListener('change', async () => {
  const file = localModelInput.files?.[0] ?? null;
  localModelInput.value = '';   // poder re-elegir el MISMO fichero

  if (file && localModelSlot >= 0)
    await store.loadLocalModel(file, localModelSlot);
  localModelSlot = -1;
});
root?.append(localModelInput);

/**
 * View-models de los parámetros que pide la vista de DETALLE de un cajón (los
 * mismos `describeControl` que el interface: si el catálogo y el contrato se
 * separan, se pinta con lo que hay y se nota en consola).
 */
function drawerVisualControls(visualSpec) {
  return visualSpec.parameterIds.map(describeControl).filter(Boolean);
}

/**
 * Espectral SILENTE para los frames que el meter local entra al canal de
 * telemetría (solo lleva envelopes=[amp, filter]): el canal exige las 64
 * bandas (contrato del frame del puente) y los consumidores del espectral
 * pintan su reposo — igual que hoy, que en local no reciben frames.
 */
const SILENT_SPECTRAL_FRAME = new Array(64).fill(0);

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

  // El spec del cajón se resuelve antes de las rutas: `routeControls` decide con
  // él (el cajón de ENVOLVENTES también las cruza) y `drawerVisual` necesita las
  // rutas ya resueltas. Declararlo aquí y no más abajo evita el TDZ del `const`.
  const drawerVisualSpec = section.drawer?.visual
    ? SECTION_VISUALS[section.drawer.visual]
    : null;

  // Las vistas de ENVOLVENTES (lienzo y cajón) cruzan las rutas de la MATRIZ:
  // los doce ids de mod1..mod4 viajan como view-models aparte. El lienzo pinta
  // las rutas bajo cada curva; el cajón resuelve con ellas el slot de IR A LA
  // RUTA en el momento del clic (ver ui/envelopeViews.js).
  const routeControls = visualSpec?.id === 'envelope-curves' || drawerVisualSpec?.id === 'envelope-blocks'
    ? ['mod1Source', 'mod1Destination', 'mod1Amount',
       'mod2Source', 'mod2Destination', 'mod2Amount',
       'mod3Source', 'mod3Destination', 'mod3Amount',
       'mod4Source', 'mod4Destination', 'mod4Amount']
      .map(describeControl).filter(Boolean)
    : [];

  // El detalle del cajón (MODELOS: espectral + ranuras) es una vista del MISMO
  // catálogo que el interface, con los MISMOS handlers (store, puente, morfeo):
  // mismas variables, cero estado duplicado. El panel la cuelga en el cajón y
  // la repinta con el snapshot del lienzo.
  const drawerVisual = drawerVisualSpec
    ? createVisual(drawerVisualSpec.id, drawerVisualControls(drawerVisualSpec), {
      onLoad: (slot) => store.loadModel(slot, {
        requestLocalFile: (nextSlot) => { localModelSlot = nextSlot; localModelInput.click(); },
      }),
      onEdit: (id, value, phase) => store.pushParameter(id, value, phase),
      onTelemetry: store.onTelemetry,
      // ENVOLVENTES: la vista del cajón reparte los knobs de la ficha dentro
      // de SU bloque (curva encima); el panel le pregunta por `blockOf`. Los
      // doce de la matriz alimentan IR A LA RUTA (mismo salto que el lienzo).
      ids: section.ids,
      routeControls,
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
        onLoad: (slot) => store.loadModel(slot, {
          requestLocalFile: (nextSlot) => { localModelSlot = nextSlot; localModelInput.click(); },
        }),
        // El pad de la ficha MODELOS edita morphX/morphY: un gesto suyo son DOS
        // gestos coordinados (uno por eje) con fase completa. pushParameter es
        // la primitiva; handleChange solo sabe cerrar UN id.
        onEdit: (id, value, phase) => store.pushParameter(id, value, phase),
        // El espectral de la ficha MODELOS se cuelga del canal de telemetria
        // (pintura en vivo, fuera del ciclo setState).
        onTelemetry: store.onTelemetry,
        // ENVOLVENTES: las rutas de la matriz que pinta debajo de cada curva y
        // el canal de frames para la AGUJA de nivel (envelopes=[amp, filter]).
        routeControls,
      })
      : null,
  };
}));

let paint = () => {};
let engineSnapshot = { status: 'idle', error: null, sampleRate: 0, voices: 0 };

// Anillos de modulacion (nivel de modulo): el callback del meter local corre
// desde DENTRO de createPanel (via pushTelemetryFrame) y el primer frame llega
// antes de que el bloque de montaje llegue a su linea — si vive solo ahi, el
// frame revienta un TDZ (medido en consola: modRings is not defined).
let modRings = null;
let lcdEngineIndex = -1;

// La vista del pad XY (ficha MODELOS) queda a mano para el feed del worklet:
// en standalone (sin bridge) es el meter del worklet quien pinta su anillo z
// con la modulacion del destino 28; en plugin lo hace la telemetria nativa.
const canvasMorphZView = bands
  .flat()
  .find((section) => section.visual?.element?.dataset.visual === 'model-xy')
  ?.visual ?? null;

// Las vistas de ENVOLVENTES quedan a mano para conectarlas TARDE con el panel:
// sus rutas (lienzo) e IR A LA RUTA (cajón) piden abrir el cajón de la MATRIZ en
// su slot, pero se fabrican ANTES del panel (orden de arranque fijado por el
// selftest del host) y el cajón es del panel. setRouteOpener lo cierra.
const canvasEnvCurvesView = bands
  .flat()
  .find((section) => section.visual?.element?.classList?.contains('env-curves'))
  ?.visual ?? null;

const drawerEnvBlocksView = bands
  .flat()
  .find((section) => section.drawerVisual?.element?.classList?.contains('env-blocks'))
  ?.drawerVisual ?? null;

// El RESUMEN de la matriz en el lienzo: sus filas son botones con el mismo
// gesto que las rutas de ENVOLVENTES (abrir la matriz en SU slot).
const canvasModSummaryView = bands
  .flat()
  .find((section) => section.visual?.element?.classList?.contains('mod-summary'))
  ?.visual ?? null;

if (root) {
  // LCD SUPERIOR (fila propia del panel, bajo el título): la migración del
  // LcdDisplay + LcdMenuManager + D-pad del interface C++ (retirados en
  // c811b75). La mecánica es del paquete compartido; los datos (árbol
  // dependiente del engineType, reposo preset/estado, pasos de parámetro) son
  // del synth. Conduce los parámetros REALES vía store (pushParameter 'end':
  // gesto de hardware). Su SITIO (entre cabecera y bandas) lo fija el panel:
  // ver lcdSlot en createPanel (panel.js) — aqui se crea ANTES para pasarlo.
  const lcdTop = createLcdTop({
    store,
    engineType: () => store.getState().parameters.engineType ?? 0,
  });

  const panel = createPanel({
    bands,
    baselineId: BASELINE_PARAMETER_ID,
    // Nivel de envolvente en las filas ENV del cajón de la MATRIZ: el MISMO
    // canal pub/sub que las agujas (frames a ~15 Hz, pintura fuera de estado).
    onTelemetry: store.onTelemetry,
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
    // El LCD vive DENTRO del panel: su fila va DEBAJO del título (la cabecera
    // es el título, no una nav-bar) y ENCIMA de las bandas — justo por encima
    // de OSCILADOR · RESONADOR · FILTRO. El panel le da sitio y el ancho
    // completo del lienzo; fuera (anterior) quedaba por encima del título.
    lcdSlot: lcdTop.element,
  });

  let owner = audioOwnerFor(false);
  let lastEngineIndex = -1;
  let lastLocalModels = null;
  let lastMorph = { x: null, y: null, z: null };

  // Rutas de ENVOLVENTES -> MATRIZ: el opener tardío (las vistas se fabricaron
  // antes del panel). Pulsa "Osc Level → +100%" bajo ENV 1 en el lienzo —o IR A
  // LA RUTA en el bloque del cajón— y la matriz abre con su RUTA n resaltada.
  // El retorno al cerrar es SOLO del cajón: IR A LA RUTA vive dentro del cajón
  // de ENVOLVENTES, y al cerrar la matriz el panel REABRE ese cajón (el usuario
  // "se vino desde allí"). Las rutas del lienzo no abrieron ENVOLVENTES — sin
  // returnTo, cerrar la matriz deja el lienzo como estaba.
  canvasEnvCurvesView?.setRouteOpener((slot) => panel.openDrawerRoute('modMatrix', slot));
  drawerEnvBlocksView?.setRouteOpener((slot) =>
    panel.openDrawerRoute('modMatrix', slot, { returnTo: 'envelopes' }));
  canvasModSummaryView?.setRouteOpener((slot) => panel.openDrawerRoute('modMatrix', slot));

  // Anillos de modulacion: el frame de telemetria suma sobre cada destino;
  // el mapa de knobs lo alimenta la misma coleccion que pinta los snapshots.
  modRings = createModulationRings(panel.knobsById);

  // Anillo morphZ del pad en standalone: el meter del worklet trae la
  // contribucion del destino 28 (en plugin la trae el frame de telemetria
  // nativo, que visuals.js consume del canal bridge). Mismo destino, dos caminos.
  onWorkletMorphZ((mod) => canvasMorphZView?.setZMod(mod));

  // AGUJA de las curvas ADSR en standalone: el meter del worklet trae los dos
  // niveles (envelopes=[amp, filter], el mismo par que el frame nativo). En
  // plugin la aguja vive del canal bridge; aqui, del meter. Mismo par, dos
  // caminos — y las DOS vistas (lienzo y cajón), que son curvas gemelas.
  onWorkletEnvelopeLevels(([amp, filter]) => {
    canvasEnvCurvesView?.needleFor?.('env')?.setLevel(amp);
    canvasEnvCurvesView?.needleFor?.('filter')?.setLevel(filter);
    drawerEnvBlocksView?.needleFor?.('env')?.setLevel(amp);
    drawerEnvBlocksView?.needleFor?.('filter')?.setLevel(filter);
    // El meter local ALIMENTA el canal de telemetría: un solo canal para TODAS
    // las vistas que lean `frame.envelopes` (las barras del cajón de la MATRIZ
    // se repintan solas, como las agujas). Es el mismo par que trae el frame
    // nativo — el puente lo entra por store.onTelemetry, el meter por aquí.
    pushTelemetryFrame({ spectral: SILENT_SPECTRAL_FRAME, envelopes: [amp, filter] });
  });

  // ACTIVIDAD POR VOZ junto a la fila de audio (standalone): el meter del
  // worklet cuenta las voces activas del motor. En plugin el contador vive en
  // el motor nativo y la pagina no lo mueve — el indicador simplemente no
  // aparece (hidden en 0).
  onWorkletVoices((count) => panel.setVoiceMeter(count));

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

  // MODO LOCAL: la ficha RANURAS necesita el shape de models para pintarse
  // (el host manda modelsState; el navegador se lo siembra a si mismo). Con
  // bridge no se toca: el modelsState del host manda y llegara en su momento.
  if (!store.getState().bridgeAvailable) {
    store.seedLocalModels(emptyLocalModels());
    store.setLocalModelReady(true);   // el input de fichero ya vive en el DOM
  }

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
      // what the bridge sent — and the pad, which the new engine woke at the
      // struct defaults.
      if (state.models) pushModelsToWorklet(state.models);
      pushMorphToWorklet(getWorkletMorph().x, getWorkletMorph().y, getWorkletMorph().z,
                         getWorkletMorph().z2, getWorkletMorph().z3);
    }

    // MODO LOCAL: los modelos cargados por la pagina y el pad viven en el
    // store, no en el APVTS de un host — este es su unico camino al motor.
    if (!state.bridgeAvailable) {
      if (state.models && state.models !== lastLocalModels) {
        lastLocalModels = state.models;
        pushModelsToWorklet(state.models);
      }

      const morphX = state.parameters.morphX;
      const morphY = state.parameters.morphY;
      const morphZ = state.parameters.morphZ ?? 0;   // default del contrato
      // FASE 11.4: el VOLUMEN de las capas 1 y 2 viaja con el morph.
      const z2 = state.parameters.morphZ2 ?? 0;
      const z3 = state.parameters.morphZ3 ?? 0;
      const nextMorph = { x: morphX, y: morphY, z: morphZ, z2, z3 };

      if (morphX !== undefined && (nextMorph.x !== lastMorph.x
          || nextMorph.y !== lastMorph.y || nextMorph.z !== lastMorph.z
          || nextMorph.z2 !== lastMorph.z2 || nextMorph.z3 !== lastMorph.z3)) {
        lastMorph = nextMorph;
        pushMorphToWorklet(nextMorph.x, nextMorph.y, nextMorph.z, nextMorph.z2, nextMorph.z3);
      }
    }

    pushParamsToWorklet(state.parameters);
  }

  paint = (state) => {
    owner = audioOwnerFor(state.bridgeAvailable);

    panel.paint(state);

    // LCD superior: el reposo/EDIT se repinta con el snapshot (preset cargado,
    // edit nativo...) y el árbol se reconstruye si cambió el engineType.
    const engineIndex = Math.round(state.parameters.engineType ?? 0);

    if (engineIndex !== lcdEngineIndex) {
      lcdEngineIndex = engineIndex;
      lcdTop.rebuild();
    }
    lcdTop.paint();
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
