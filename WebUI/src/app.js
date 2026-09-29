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
// Los temas de los modulos de efecto, por FAMILIA. Van en la hoja compartida y no
// en `main.css` a proposito: son los MISMOS tokens que el registro `fxTheme.js`
// declara, y las dos mitades estan atadas por el test de ABDSharedAssets. Copiar
// los once colores aqui seria la tercera copia, y la que nadie actualiza.
import '@abdsynths/shared/styles/components/fx.css';
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
  pushVoiceToWorklet,
  getWorkletMorph,
  startAudioEngine,
} from './audio/audioWorkletEngine.js';
import { emptyLocalModels } from './audio/localModels.js';
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
 * SUSCRIPCIONES DE LA PAGINA (el circuito del unsubscribe).
 *
 * Todos los canales devuelven su remover —`store.onUserEdit`,
 * `store.onTelemetry`, `store.subscribe` y los `onWorklet*` del motor— y sin
 * llamarlos la pagina se los lleva puestos. En una pestaña del navegador no se
 * nota: el documento se tira entero y los suscriptores con el. En WebView2 SI:
 * los canales son estado de MODULO (viven mas alla del documento), asi que un
 * `pagehide` sin limpiar deja la `paint` de la pagina ya cerrada apuntando al
 * store, y la siguiente que monte el plugin se suma a la cola. `subscribeForPage`
 * es el unico sitio por el que pasa una suscripcion: la que se quede fuera es
 * una fuga silenciosa.
 */
const pageSubscriptions = [];

/** Anota el remover de una suscripcion para el apagado de pagina. */
function subscribeForPage(unsubscribe) {
  // `undefined` = el canal no devolvio remover (hoy ninguno); se acepta igual
  // para no mentir con un `if` en cada punto de suscripcion.
  if (typeof unsubscribe === 'function') pageSubscriptions.push(unsubscribe);
}

/**
 * APAGAR la pagina: devuelve los canales que tomo. Idempotente por construccion
 * —`splice(0)` deja la lista VACIA—, asi que un `pagehide` repetido no hace
 * nada y la pagina apagada no vuelve a pintar por un frame que llegara tarde.
 */
function teardownPage() {
  for (const unsubscribe of pageSubscriptions.splice(0)) {
    try {
      unsubscribe();
    } catch (error) {
      console.warn('suscripcion que no se pudo cerrar', error);
    }
  }
}

// `pagehide`, no `beforeunload`: el primero SIEMPRE llega (navegacion, cierre de
// pestana, cierre del editor del plugin), mientras que `beforeunload` el
// navegador puede suprimirlo del todo y WebView2 no lo garantiza al navegar.
// Ademas no bloquea la salida: aqui no hay audio ni temporizadores que cerrar,
// solo suscriptores que devolver. `persisted` = la pagina se va a la CACHE
// (bfcache) y puede volver: ahi el documento sigue vivo, y quitarle los
// suscriptores la dejaria muda al regresar.
window.addEventListener('pagehide', (event) => {
  if (event.persisted) return;
  teardownPage();
});

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
      // LOS CONTROLES DE LA FICHA, y no solo los de la vista. Una vista de cajon
      // puede necesitar los view-models de los ids de la FICHA: los modulos de
      // EFECTOS tienen que saber que ids del bus posee el store para decidir que
      // huecos tienen bus, y eso no sale de `parameterIds` (que va vacio a
      // proposito, porque una vista no reparte celdas).
      //
      // Sin esto, la vista se fabricaba con CERO controles: ningun modulo se
      // pintaba con bus y las celdas se caian al cuerpo de la ficha por el
      // `?? body` del panel. El fallo es silencioso y total: la pagina se ve
      // entera y con todos los mandos, pero los del bus estan en la rejilla
      // en vez de en su modulo.
      controls,
      onLoad: (slot) => store.loadModel(slot, {
        requestLocalFile: (nextSlot) => { localModelSlot = nextSlot; localModelInput.click(); },
      }),
      // OLVIDAR una ranura: la vacia y saca su texto de la memoria local, para
      // que un F5 no la devuelva. Solo tiene efecto en modo local (con host manda
      // el preset), asi que el store es quien lo dice con su return value, no la
      // vista, que no guarda estado de la memoria.
      onForget: (slot) => store.forgetLocalModel(slot),
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
        // El mismo OLVIDAR que en el cajon: la vista del lienzo de MODELOS es el
        // pad, pero el handler viaja igual para que las dos superficies de la
        // ficha hablen con el store por la misma puerta.
        onForget: (slot) => store.forgetLocalModel(slot),
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
        // El RESUMEN de la matriz: sus filas ENV llevan la barra de nivel del
        // MISMO canal que las barras del cajon (envelopes=[amp, filter]).
        onTelemetry: store.onTelemetry,
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
// Lo único que puede obligar al LCD a rehacer su ÁRBOL. La firma la lee
// del propio lcdTop en vez de compararla aquí, para que app.js no tenga que
// saber de qué depende el menú; se toma al crear, no antes, para que el
// primer paint no destruya y rehaga un árbol recién construido.
let lcdMenuFirma = null;

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
  // Sin `engineType`: el LCD lo lee del store, que es quien lo tiene. La
  // lambda que se le pasaba antes no se llamaba nunca --solo decidia si
  // leerlo--, asi que era una forma de no decidir aqui lo que se decide
  // ahi, y por eso funcionaba por la razon equivocada.
  const lcdTop = createLcdTop({ store });

  lcdMenuFirma = lcdTop.menuSignature();

  // PREVIEW en el LCD de CUALQUIER edit de usuario (celdas, pad, aro, knobs de
  // cajon): el mismo gesto de hardware del original — giras y el LCD ensena el
  // parametro con su nombre y valor del contrato, y vuelve solo al reposo. El
  // canal es de EDITS DE USUARIO: un cambio nativo (host, automatizacion, un
  // preset) no lo dispara — no es tu mano la que gira. showParameterPreview
  // guarda el estado del propio LCD (en EDIT del menu no pisa).
  subscribeForPage(store.onUserEdit((id, normalized) => lcdTop.showParameterPreview(id, normalized)));

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
      // El conmutador de la ruta local del pad (ficha MATRIZ): el store
      // escribe la fila 3 y guarda su estado, con el mismo camino que
      // cualquier otro gesto (el paint y el motor local se enteran por el
      // snapshot).
      onLocalRoute: (change) => store.setLocalMorphRoute(change),
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

  // Esquinas A-D del pad (ficha MODELOS): clic en una ranura CARGADA abre su
  // cajón en esa ranura — el gesto inverso a IR A LA RUTA. Sin retorno: quien
  // abrió fue el LIENZO, cerrar deja el lienzo como estaba (mismo criterio que
  // las rutas del lienzo de ENVOLVENTES). El slot viaja en la numeración de
  // las filas del cajón destino (0-based, la del motor: A=0). El estado de
  // openDrawerRoute ya limpia cualquier resalte/retorno de un salto anterior
  // (no se apilan gestos).
  canvasMorphZView?.setCornerOpener?.((slot) =>
    panel.openDrawerRoute('models', slot));

  // Anillos de modulacion: el frame de telemetria suma sobre cada destino;
  // el mapa de knobs lo alimenta la misma coleccion que pinta los snapshots.
  modRings = createModulationRings(panel.knobsById);

  // Anillo morphZ del pad en standalone: el meter del worklet trae la
  // contribucion del destino 28 (en plugin la trae el frame de telemetria
  // nativo, que visuals.js consume del canal bridge). Mismo destino, dos caminos.
  subscribeForPage(onWorkletMorphZ((mod) => canvasMorphZView?.setZMod(mod)));

  // AGUJA de las curvas ADSR en standalone: el meter del worklet trae los dos
  // niveles (envelopes=[amp, filter], el mismo par que el frame nativo). En
  // plugin la aguja vive del canal bridge; aqui, del meter. Mismo par, dos
  // caminos — y las DOS vistas (lienzo y cajón), que son curvas gemelas.
  subscribeForPage(onWorkletEnvelopeLevels(([amp, filter]) => {
    canvasEnvCurvesView?.needleFor?.('env')?.setLevel(amp);
    canvasEnvCurvesView?.needleFor?.('filter')?.setLevel(filter);
    drawerEnvBlocksView?.needleFor?.('env')?.setLevel(amp);
    drawerEnvBlocksView?.needleFor?.('filter')?.setLevel(filter);
    // El meter local ALIMENTA el canal de telemetría: un solo canal para TODAS
    // las vistas que lean `frame.envelopes` (las barras del cajón de la MATRIZ
    // se repintan solas, como las agujas). Es el mismo par que trae el frame
    // nativo — el puente lo entra por store.onTelemetry, el meter por aquí.
    pushTelemetryFrame({ spectral: SILENT_SPECTRAL_FRAME, envelopes: [amp, filter] });
  }));

  // ACTIVIDAD POR VOZ junto a la fila de audio (standalone): el meter del
  // worklet cuenta las voces activas del motor. En plugin el contador vive en
  // el motor nativo y la pagina no lo mueve — el indicador simplemente no
  // aparece (hidden en 0).
  subscribeForPage(onWorkletVoices((count) => panel.setVoiceMeter(count)));

  const renderAudio = () => panel.paintAudio({ owner, ...engineSnapshot });

  root.append(panel.element);

  // El lienzo es de diseño FIJO (CANVAS) y el editor es redimensionable: escalar
  // para caber entero — sin esto, una ventana baja corta el pie y la franja de
  // teclado (el fallo "no se distinguen las teclas": el keybed estaba FUERA).
  mountFitStage(root, { width: CANVAS.width, height: CANVAS.height });

  // Connect (or fall into local mode) once the panel is in the DOM: the host  // times the page startup against `window.__pilotReady`, which start() sets.
  store.start();

  // MODO LOCAL: la ficha RANURAS necesita el shape de models para pintarse
  // (el host manda modelsState; el navegador se lo siembra a si mismo). Con
  // bridge no se toca: el modelsState del host manda y llegara en su momento.
  if (!store.getState().bridgeAvailable) {
    store.seedLocalModels(emptyLocalModels());
    // Y las ranuras que la pagina RECUERDA (localStorage): en el navegador no hay
    // preset que las traiga, asi que la ultima carga ES el preset. Sin memoria no
    // hace nada (las cuatro siguen EMPTY); con host no se llama a nada de esto.
    store.restoreLocalModels();
    store.setLocalModelReady(true);   // el input de fichero ya vive en el DOM
    // Y una ruta de la MATRIZ a Morph Z (LFO 2 -> Morph Z): sin host el motor
    // nace con la matriz del contrato (slot 3 en Off) y el anillo del pad se
    // queda quieto. Sembrarla aqui es lo que hace que el arco gire con el LFO
    // desde el primer SOUND ON, sin tocar nada a mano.
    store.seedLocalMorphZRoute();
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
  // AL LLEGAR A READY se re-aplica el estado de la pagina: el motor acaba de
  // nacer con el MIRROR por defecto del worklet, y syncEngine() se corta en
  // seco mientras no haya motor (`!isAudioEngineReady()`), asi que este es el
  // unico momento en que la matriz, los modelos y el pad llegan a un motor
  // recien arrancado con SOUND ON. Un repintado de mas no cuesta nada: el
  // guard de lastEngineIndex y el de lastMorph evitan repeticiones.
  //
  // La COPIA no es cosmetica: `engine` es el MISMO objeto vivo del modulo
  // (`audioEngineState`, mutado en el sitio), asi que guardarlo por referencia
  // hacia que `wasReady` fuese SIEMPRE true -su status ya es 'ready' cuando
  // llega el aviso- y el re-sync no disparaba NUNCA. Medido en Chromium real:
  // sin esta copia el worklet arrancaba y procesaba, pero no le llegaba ni un
  // `neuronik:params` (matriz, LFO y pad en los defaults del struct: el anillo
  // del pad no bailaba). Lo caza el E2E de Playwright (e2e/localMode.spec.js).
  subscribeForPage(onAudioEngineChange((engine) => {
    const wasReady = engineSnapshot.status === 'ready';

    engineSnapshot = { ...engine };
    renderAudio();

    if (engine.status === 'ready' && !wasReady) paint(store.getState());
  }));

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

    // El ADSR va por su propio canal (VoiceParams, no GlobalParams): sin esta
    // linea los ocho knobs de envolvente se movian en la pagina y el motor
    // local seguia con los defaults de C++ — la ENV 2 era la ENV 1.
    pushVoiceToWorklet(state.parameters);
  }

  paint = (state) => {
    owner = audioOwnerFor(state.bridgeAvailable);

    panel.paint(state);

    // LCD superior: el reposo/EDIT se repinta con el snapshot (preset cargado,
    // edit nativo...) y el árbol se RECONSTRUYE en cuanto cambia algo de lo
    // que depende: el motor, o el efecto puesto en el hueco 1, que es lo que
    // da nombre a los mandos de la rama EFFECTOS. Poner un chorus en la ficha
    // renombra de SAT DRIVE a CHO RATE sin que nadie avise al LCD, y aquí
    // no hace falta saber cómo se deriva ese nombre para enterarse.
    const lcdFirma = lcdTop.menuSignature();

    if (lcdFirma !== lcdMenuFirma) {
      lcdMenuFirma = lcdFirma;
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
subscribeForPage(store.subscribe(paint));

// Telemetria (nativa -> web): los frames NO son estado (a ~15 Hz no pasan por
// setState); alimentan directamente la pintura en tiempo real. Hoy, el anillo
// de modulacion; el espectral y el scope se cuelgan del mismo canal.
subscribeForPage(store.onTelemetry((frame) => modRings.handleFrame(frame)));

/**
 * El control BASE (masterLevel) ya NO se cablea aqui: desde 2026-09-28 es un
 * Knob del paquete compartido y lo construye `buildBaselineControl`, que recibe
 * los `handlers` del panel y engancha el gesto (begin/end) y el `onChange` por
 * el MISMO camino que cualquier otro control continuo.
 *
 * Antes este sitio tenía su propio cableado a mano —`pointerdown` -> gesto,
 * `input` -> handleChange— porque el fader era un `<input>` nativo y no salía de
 * la familia compartida. Ese trato especial era justo lo que lo separaba del
 * resto; con el knob desaparece, y con el `bindBaseline` desaparece también la
 * segunda mitad del contrato 8.1 2c (el ancla `data-baseline-control` la escribe la celda, y el puente `baselineControl.value` va en ella).
 */
