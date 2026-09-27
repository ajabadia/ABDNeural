/**
 * El lienzo: cabecera, tres bandas de fichas, franja de interpretación y pie.
 *
 * Es UN SOLO LIENZO, sin pestañas de parámetros (ticket 8.2): los 70 controles
 * están repartidos en siete fichas que siguen la agrupación del panel nativo, y
 * todas caben a la vez en la superficie de diseño (1440x900; el encaje se mide en
 * tests/sections.test.js). DOM vainilla, sin framework: el panel pinta el estado
 * que recibe y posee su propio estado de vista (si la franja de teclado está
 * plegada). Nunca lee el store, así que se puede probar solo (tests/panel.test.js).
 *
 * Tres contratos con el host viven aquí y NO se pueden "simplificar":
 *
 *   1. el PRIMER `input[type=range]` del documento es el fader de masterLevel: lo
 *      conduce el `--selftest` del host en las dos direcciones, así que se
 *      construye como range nativo y va en la primera ficha (GLOBAL & MASTER),
 *      por delante de las ruedas de la franja de teclado;
 *   2. `footer.panel-footer code` lleva el estado NORMALIZADO como JSON: el host
 *      lo parsea para comprobar que los ids de una ficha están en la página;
 *   3. la franja de teclado publica `data-tab="keys"` (ver contracts/screens.js).
 *
 * El panel tampoco decide QUÉ opciones valen para el motor activo: solo reparte el
 * snapshot y deja que cada celda se reevalúe (ver el gating en ui/controls.js).
 */

import { AUDIO_OWNER, audioOwnerLabel } from '../audio/policy.js';
import { latestTelemetry } from '../bridge/telemetry.js';
import { choiceIndexFromNormalized, displayText, realFromNormalized } from '../contracts/paramValue.js';
import { KEYS_TAB } from '../contracts/screens.js';
import { createParameterControl } from './controls.js';
// Cajon compartido de la familia (contenido estable: sin re-render al abrir).
import { createDrawer } from '@abdsynths/shared/components';
import { ENV1_SOURCE, ENV2_SOURCE } from './envelopeViews.js';
import { ThemeSwitcher } from '@abdsynths/shared/components';

/**
 * @param {object} options
 * @param {object[][]} options.bands       bandas -> fichas -> controles (view-models)
 * @param {string} options.baselineId      id del control base del host (masterLevel)
 * @param {object} [options.handlers]
 * @param {(id: string, normalized: number) => void} [options.handlers.onChange]
 * @param {(id: string, phase: 'begin'|'end') => void} [options.handlers.onGesture]
 * @param {(id: string) => void} [options.handlers.onAction]         acciones de ficha (RANDOM)
 * @param {() => void} [options.handlers.onPanic]       PANIC de la franja
 * @param {() => void} [options.handlers.onStartSound]  SOUND ON (solo modo local)
 * @returns {{ element: HTMLElement, keysRoot: HTMLElement, drawers: Map, paint: Function, paintAudio: Function, toggleKeys: Function, destroy: Function }}
 */
export function createPanel({ bands, baselineId, handlers = {}, onTelemetry = null, lcdSlot = null }) {
  const controls = [];
  // Mapa id -> control para consumidores de TELEMETRIA (el anillo de modulacion
  // pinta sobre el knob, no escribe el parametro): su acceso es por id y su
  // ciclo de vida es el del panel.
  const knobsById = new Map();
  // Vistas de ficha (curva ADSR...): no son controles, pero se repintan con el
  // mismo snapshot, asi que el panel les pasa el estado igual que a las celdas.
  const visuals = [];
  // Botones de accion (no son parametros): se deshabilitan sin host, porque el
  // sorteo lo hace el procesador y sin plugin no hay APVTS que sortear.
  const actionButtons = [];
  // Celdas cuya disponibilidad depende de OTRO parametro (el motor): se reevaluan
  // con el mismo snapshot que pinta los valores, en una pasada aparte.
  const gatedControls = [];
  // Cajones laterales, uno por ficha que declara `drawer` (ver contracts/sections.js).
  // Se crean al montar y se quedan: abrir y cerrar es una clase CSS, no reconstruir.
  const drawers = new Map();
  // La DECLARACION del cajon de cada ficha (su `drawer` del contrato), para lo que
  // el paint necesita saber del contrato y no del nodo: hoy, que cuenta su
  // distintivo vivo (`liveBadge`).
  const drawerSpecs = new Map();
  // id -> view-model. Lo pide ese mismo distintivo (para leer un `choice` hacen
  // falta sus `options`); el repintado normal va por el array `controls`.
  const controlsById = new Map();
  let baselineSlider = null;
  let baselineControl = null;
  // Parametros del ULTIMO snapshot: los necesita el repintado por frame de las
  // barras ENV del cajón de la MATRIZ (la decisión de fila vive en el snapshot,
  // el nivel en el frame).
  let lastParameters = {};
  // Chips de la franja de estado de GLOBAL & MASTER (los llena buildCard).
  let globalStripItems = [];
  const baselineReadouts = new Map();
  // Resalte de slot pedido por una vista (rutas de ENVOLVENTES -> matriz). Es un
  // gesto, no estado: nace con openDrawerRoute y MUERE con el cierre del cajon
  // (onClose del mueble compartido). No lo reevalua el paint a proposito: la
  // verdad del slot la dice su contenido (los selects del cajon), no un borde.
  let highlightedSlot = null;
  // De donde vino el salto de ruta (rutas del lienzo de ENVOLVENTES / bloques
  // IR A LA RUTA de su cajon). Mismo trato que el resalte: un GESTO, no estado
  // — nace en openDrawerRoute y se CONSUME una unica vez al cerrar la matriz.
  // El mueble compartido no encadena retornos (un open() no cierra al hermano,
  // solo comparten velo), asi que la vuelta la ejecuta el dueño de los cajones:
  // el panel reabre ENVOLVENTES al cerrarse la matriz.
  let routeReturn = null;
  // La fila del LCD (ver el bloque de abajo: solo existe con lcdSlot).
  let lcdRowSlot = null;

  /** Limpia el resalte del cajon dado (o del que lo tenga). */
  function clearRouteHighlight(sectionId) {
    if (highlightedSlot && sectionId && highlightedSlot.sectionId !== sectionId) return;

    highlightedSlot = null;

    for (const drawer of drawers.values())
      for (const row of drawer.body.querySelectorAll('.drawer-slot'))
        row.dataset.slotHighlight = 'false';
  }

  const element = document.createElement('section');
  element.className = 'panel';

  // NOTA: `lcdRowSlot` y la clase .panel--has-lcd se deciden antes de construir
  // la cabecera (el append del LCD va entre header y canvas, al final).

  // --- cabecera -------------------------------------------------------------

  const header = document.createElement('header');
  header.className = 'panel-header';

  const title = document.createElement('h1');
  title.textContent = 'NEURONiK';

  // Audio ownership (policy de src/audio/policy.js). Dentro del plugin esto es
  // una LECTURA, no un control: el audio es del host y aquí no hay motor que
  // arrancar, así que no hay botón.
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

  // Selector de temas (infraestructura compartida; los temas son de la suite:
  // 'dark' es el :root y 'light' el bloque de tokens claro). SIN persistencia:
  // cada carga arranca oscuro, asi selftest y paridad nunca heredan el estado
  // de una prueba manual. El menu "View" de navegacion es otra pieza (ROADMAP).
  new ThemeSwitcher(audioRow, {
    themes: [
      { id: 'dark', label: 'Dark' },
      { id: 'light', label: 'Light' },
    ],
    root: document.documentElement,
  });

  const status = document.createElement('p');
  status.className = 'status';

  const contractLine = document.createElement('p');
  contractLine.className = 'contract-line';

  header.append(title, audioRow, status, contractLine);

  // --- fila del LCD (opcional) ----------------------------------------------
  // El LCD superior del synth: la página lo crea (ui/lcdTop.js) y el panel solo
  // le da SU fila — DEBAJO del título (la cabecera) y ENCIMA de las bandas,
  // justo por encima de la banda del motor (OSCILADOR · RESONADOR · FILTRO).
  // SIN LCD la fila no existe: los gap del grid cuentan por pista (no por
  // hijo) y una fila vacía metería un hueco de banda que el contrato no cuenta.
  // Con LCD el panel marca .panel--has-lcd y su CSS declara la quinta fila.
  if (lcdSlot) {
    const lcdRow = document.createElement('div');
    lcdRow.className = 'lcd-row';
    lcdRow.append(lcdSlot);
    lcdRowSlot = lcdRow;
    element.classList.add('panel--has-lcd');
  }

  // --- lienzo: bandas de fichas --------------------------------------------

  const canvas = document.createElement('div');
  canvas.className = 'canvas';

  bands.forEach((band, bandIndex) => {
    const bandElement = document.createElement('div');
    bandElement.className = 'band';
    bandElement.dataset.bandIndex = String(bandIndex);

    for (const section of band) {
      bandElement.append(buildCard(section, {
        actionButtons,
        baselineId,
        controls,
        knobsById,
        drawers,
        drawerSpecs,
        gatedControls,
        visuals,
        handlers,
        // Los chips de la franja de GLOBAL (los llena buildCard, los repinta paint).
        onGlobalStrip: (items) => { globalStripItems = items; },
        onBaseline: (built) => {
          baselineSlider = built.slider;
          baselineControl = built.control;
        },
        // Cierre real de un cajon (✕/velo/ESC): el resalte muere y, si este
        // cajon fue el DESTINO de un salto de ruta con retorno pendiente, el
        // panel REABRE el origen — "vuelve a ENVOLVENTES al cerrar". Un cierre
        // silencioso (close(true), el que hace el propio salto con el origen)
        // no llega aqui: el mueble solo avisa por closeDrawer() de usuario.
        onDrawerClosed: (sectionId) => {
          clearRouteHighlight(sectionId);

          const returnTo = routeReturn;
          routeReturn = null; // consumo UNICO: cerrar otra vez ya no vuelve

          const back = returnTo && drawers.get(returnTo);
          if (returnTo && back) back.open();
        },
        readouts: baselineReadouts,
      }));
    }

    canvas.append(bandElement);
  });

  // El indice id -> view-model DEL CONTRATO (los que traen `options`): lo pide lo
  // que se pinta FUERA de la celda, porque la celda ya montada no guarda el
  // descriptor. Hoy lo consume el distintivo vivo del cajon (lee un `choice`).
  for (const band of bands)
    for (const section of band)
      for (const control of section.controls) controlsById.set(control.id, control);

  // --- franja de interpretación --------------------------------------------
  // El teclado compartido viene de src/ui/keyboard.js y append su strip dentro de
  // keysRoot; el panel solo le da sitio y las herramientas de la franja.

  const performance = document.createElement('div');
  performance.className = 'performance';

  const keysToolbar = document.createElement('div');
  keysToolbar.className = 'keys-toolbar';

  const panicButton = document.createElement('button');
  panicButton.type = 'button';
  panicButton.className = 'keys-panic';
  panicButton.textContent = 'PANIC';
  panicButton.addEventListener('click', () => handlers.onPanic?.());

  // El host pulsa este botón antes de leer la rueda de modulación: el atributo es
  // el anclaje, no una pestaña (ver el porqué del nombre en contracts/screens.js).
  const keysToggle = document.createElement('button');
  keysToggle.type = 'button';
  keysToggle.className = 'keys-toggle';
  keysToggle.dataset.tab = KEYS_TAB;
  keysToggle.setAttribute('aria-expanded', 'true');
  keysToggle.textContent = 'TECLADO';
  keysToggle.addEventListener('click', () => toggleKeys());

  const keysStatus = document.createElement('span');
  keysStatus.className = 'keys-status';

  keysToolbar.append(panicButton, keysToggle, keysStatus);

  const keysRoot = document.createElement('div');
  keysRoot.id = 'keys-root';

  performance.append(keysToolbar, keysRoot);

  // --- pie (el host parsea este <code> como JSON) ---------------------------

  const footer = document.createElement('footer');
  footer.className = 'panel-footer';

  const footerText = document.createElement('span');
  const footerState = document.createElement('code');

  footer.append(footerText, footerState);

  element.append(header, ...(lcdRowSlot ? [lcdRowSlot] : []), canvas, performance, footer);

  let keysCollapsed = false;

  /** Pliega/despliega la franja de teclado (estado de VISTA: no toca el store). */
  function toggleKeys() {
    keysCollapsed = !keysCollapsed;
    performance.classList.toggle('performance--collapsed', keysCollapsed);
    keysToggle.setAttribute('aria-expanded', String(!keysCollapsed));
  }

  /** Repinta desde un snapshot del store. Barato: solo escribe valores. */
  function paint(state) {
    const { parameters, snapshotVersion, bridgeAvailable } = state;

    lastParameters = parameters;

    status.textContent = bridgeAvailable
      ? `bridge: conectado al host (WebView2) · snapshot #${snapshotVersion}`
      : 'bridge: modo local (sin host)';

    contractLine.textContent = contractLineFor(state);

    for (const control of controls) control.setNormalized(parameters[control.id] ?? 0);

    // Gating por motor: la lista se reevalua con ESTE snapshot. Si el snapshot no
    // trae el motor, no se toca: no se inventa cual esta activo.
    for (const control of gatedControls) {
      const engine = parameters[control.engineParameter];

      if (engine !== undefined) control.setEngine(engine);
    }

    // El distintivo VIVO de los cajones que lo declaran (la MATRIZ: rutas
    // ASIGNADAS) se reescribe con el MISMO snapshot que pinta las celdas, y con
    // `setHeader` del mueble compartido — que no reconstruye nada, solo el dato.
    // Un cajon sin `liveBadge` (los inventarios: 2 ADSR, 4 LFO, 4 RANURAS, 8
    // GLOBAL) conserva el literal de su ficha: no cambia con el uso.
    for (const [sectionId, spec] of drawerSpecs) {
      const badge = liveDrawerBadge(spec, controlsById, parameters);

      if (badge !== null) drawers.get(sectionId)?.setHeader({ badge });
    }

    // ¿Qué filas del cajón de la MATRIZ vienen de una ENVOLVENTE? La verdad la
    // dice el snapshot (los índices de opción del Select de fuente, 0 = Off),
    // leído del MISMO snapshot que pinta las celdas — sin duplicar el cruce.
    paintEnvLevels(parameters);

    // Franja de estado de GLOBAL & MASTER: mismo displayText que las celdas, el
    // MISMO snapshot (cada chip lleva el item que buildCard le asignó).
    for (const { item, value } of globalStripItems) {
      const control = item.control;
      const normalized = parameters[control.id] ?? 0;
      let text = displayText(control, realFromNormalized(control, normalized));

      // MIDI suma su toggle: "Omni" + Thru = "Omni · THRU" (la verdad del canal
      // de entrada y del eco al host en un solo chip).
      if (item.extra) {
        const thruOn = choiceIndexFromNormalized(item.extra, parameters[item.extra.id] ?? 0) > 0;

        if (thruOn) text += ' · THRU';
      }

      value.textContent = text;
    }

    // Las vistas se repintan con el MISMO snapshot: los parámetros (curva ADSR,
    // resumen de la matriz) y el estado entero, porque las ranuras de modelo viven
    // fuera del APVTS (`state.models`) y se habilitan según haya host.
    for (const visual of visuals) visual.paint(parameters, state);

    if (baselineSlider && baselineControl) {
      const normalized = parameters[baselineControl.id] ?? 0;

      // Nunca se pelea con el dedo del usuario: mientras el fader tiene el foco,
      // el valor es suyo.
      if (document.activeElement !== baselineSlider) baselineSlider.value = String(normalized);

      paintReadout(baselineReadouts.get(baselineControl.id), baselineControl, normalized);
    }

    keysStatus.textContent = bridgeAvailable ? 'MIDI → PLUGIN LIVE' : 'LOCAL MODE';
    keysStatus.classList.toggle('keys-status--local', !bridgeAvailable);

    for (const button of actionButtons)
      button.disabled = !bridgeAvailable;

    footerText.textContent = `Parameter updates: ${state.changeCount}`
      + (state.contractErrors.length > 0
          ? ` · contract errors: ${state.contractErrors.join(', ')}`
          : '');

    // EL contrato del host: estado normalizado como JSON (ver la cabecera).
    footerState.textContent = JSON.stringify(parameters);
  }

  /**
   * Indicadores de actividad por voz junto a la fila de audio. Es PINTURA en
   * vivo (app.js lo alimenta del meter del worklet), no estado del snapshot:
   * el estado del motor viaja por paintAudio, las voces por su propio camino
   * — mismo patrón que la aguja de las curvas o el anillo morphZ.
   */
  /** El host (Standalone) declara 8 voces; el meter puede cantar más. */
  const MAX_VOICES_UI = 8;

  const voiceMeter = document.createElement('div');
  voiceMeter.className = 'voice-meter';
  voiceMeter.hidden = true;
  voiceMeter.setAttribute('role', 'img');
  for (let index = 0; index < MAX_VOICES_UI; index += 1) {
    const led = document.createElement('span');
    led.className = 'voice-meter__led';
    led.dataset.led = String(index + 1);
    voiceMeter.append(led);
  }
  audioRow.append(voiceMeter);

  /**
   * Pinta el número de voces activas (0..N). Un número mayor que los leds se
   * muestra en el aria-label (la polifonía real del motor) sin inventar leds.
   */
  function setVoiceMeter(count) {
    const voices = Math.max(0, Number.isFinite(count) ? Math.floor(count) : 0);

    voiceMeter.hidden = voices === 0;
    voiceMeter.setAttribute('aria-label', `${voices} ${voices === 1 ? 'voz activa' : 'voces activas'}`);

    for (const led of voiceMeter.children) {
      const index = Number(led.dataset.led);
      led.dataset.active = index <= voices ? 'true' : 'false';
    }
  }

  /**
   * Nivel de envolvente de las filas del cajón de la MATRIZ cuya fuente es
   * ENV 1/ENV 2. La DECISIÓN (qué filas) corre con cada snapshot (`paint`);
   * el NIVEL lo repinta el canal de telemetría a ~15 Hz llamando a esta misma
   * función SIN parámetros — el nivel de la última decisión, fresco por frame.
   * Orden del par: envelopes=[amp, filter] → ENV 1, ENV 2 (contrato puente).
   */
  function paintEnvLevels(parameters = null) {
    const matrixDrawer = drawers.get('modMatrix');
    const sourceIds = ['mod1Source', 'mod2Source', 'mod3Source', 'mod4Source'];
    const envByOptionIndex = new Map([[ENV1_SOURCE, 0], [ENV2_SOURCE, 1]]);

    for (const row of matrixDrawer?.body.querySelectorAll('.drawer-slot') ?? []) {
      const envLevel = row.querySelector('.drawer-slot__env-level');

      if (!envLevel) continue;

      if (parameters !== null) {
        const sourceControl = controlsById.get(sourceIds[Number(row.dataset.slot) - 1]);
        const value = sourceControl ? parameters[sourceControl.id] : undefined;
        const optionIndex = sourceControl && value !== undefined
          ? choiceIndexFromNormalized(sourceControl, value)
          : -1;

        envLevel.dataset.envelope = String(envByOptionIndex.get(optionIndex) ?? -1);
      }

      const envelope = Number(envLevel.dataset.envelope);
      const live = envelope >= 0;

      envLevel.dataset.live = String(live);

      if (live) {
        const frame = latestTelemetry();
        const level = frame?.envelopes?.[envelope] ?? 0;

        envLevel.style.setProperty('--env-level', String(Math.min(1, Math.max(0, level))));
      }
    }
  }

  // El nivel por frame es PINTURA fuera del ciclo de estado (mismo camino que
  // las agujas): se suscribe aquí y muere con el panel. Solo con host: en modo
  // local los frames no existen y la decisión vive en el paint de snapshots.
  const stopEnvLevels = typeof onTelemetry === 'function'
    ? onTelemetry(() => paintEnvLevels(lastParameters))
    : null;

  /**
   * Línea de audio: quién posee el motor y (solo en modo local) cómo arrancarlo.
   * La alimenta app.js desde la política + el estado del propio motor.
   */
  function paintAudio({ owner, status: audioState = 'idle', sampleRate = 0, error = null }) {
    audioLabel.textContent = audioOwnerLabel(owner);

    const localMode = owner === AUDIO_OWNER.WORKLET;

    if (!localMode) {
      audioDetail.textContent = '· sin control en la página';
      audioButton.hidden = true;
      return;
    }

    switch (audioState) {
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
        audioDetail.textContent = `· ${error ?? audioState}`;
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
    for (const control of controls) control.destroy();
    knobsById.clear();
    for (const visual of visuals) visual.destroy?.();
    for (const drawer of drawers.values()) drawer.destroy();
    stopEnvLevels?.();
    controls.length = 0;
    visuals.length = 0;
    gatedControls.length = 0;
    drawerSpecs.clear();
    controlsById.clear();
    drawers.clear();
    highlightedSlot = null;
    routeReturn = null;
    globalStripItems = [];
    element.textContent = '';
  }

  /**
   * Abre el cajón de UNA sección en un SLOT concreto y lo resalta. Es la mitad
   * receptora del gesto "pulsa una ruta de ENVOLVENTES y vete a la matriz": la
   * vista pide (setRouteOpener), el panel ejecuta — es el dueño de los cajones
   * y de las filas RUTA n.
   *
   * El resalte PERSISTE hasta que otro paint lo reevalúe: un paint con la ruta
   * ya no asignada a ENV lo limpia (el slot se queda marcado solo mientras el
   * estado dice lo mismo que el botón que trajo aquí).
   *
   * @param {string} sectionId  'modMatrix' hoy; el método es genérico a propósito
   * @param {number} slot  el RUTA n (1..4) a resaltar
   * @param {{ returnTo?: string }} [options]  cajón a reabrir cuando ESTE se
   *   cierre ('envelopes' desde los botones IR A LA RUTA del cajón de
   *   ENVOLVENTES). Se consume UNA vez: el siguiente cierre ya no vuelve.
   * @returns {boolean} true si el cajón existía y se abrió
   */
  function openDrawerRoute(sectionId, slot, { returnTo = null } = {}) {
    const drawer = drawers.get(sectionId);

    if (!drawer) return false;

    // Un salto nuevo SUSTITUYE al anterior (resalte y retorno): primero se
    // limpia TODO. El retorno viejo muere ANTES de cerrar el cajon de origen:
    // el mueble compartido no tiene cierre silencioso (close() SIEMPRE avisa
    // por onClose), asi que ese cierre consumiria un retorno vivo y reabriria
    // lo que estamos cerrando. Con null en el camino, su onClose es un no-op.
    clearRouteHighlight();
    routeReturn = null;

    // El cajón de origen ABIERTO (IR A LA RUTA vive en el cajón de ENVOLVENTES)
    // lo cierro AQUI: no es un cierre de usuario y no tiene nada que limpiar.
    // Al volver, lo reabre el handler de cierre de la matriz.
    for (const [otherId, other] of drawers)
      if (otherId !== sectionId && other.isOpen()) other.close();

    routeReturn = returnTo;

    highlightedSlot = { sectionId, slot: Number(slot) || 0 };

    for (const row of drawer.body.querySelectorAll('.drawer-slot'))
      row.dataset.slotHighlight = String(Number(row.dataset.slot) === highlightedSlot.slot);

    drawer.open();

    return true;
  }

  return {
    element,
    keysRoot,
    drawers,
    knobsById,
    paint,
    paintAudio,
    toggleKeys,
    openDrawerRoute,
    setVoiceMeter,
    destroy,
    isKeysCollapsed: () => keysCollapsed,
  };
}

/**
 * Icono lápiz de los triggers de cajón. Espejo EXACTO de
 * `ABDSharedAssets/icons/edit.svg` (la SSOT del catálogo): inline y no `<img>`
 * porque el inline hereda `currentColor` y el icono se tiñe con el acento del
 * tema, igual que el texto del botón. Cuando el paquete compartido publique un
 * módulo de iconos inline, este espejo se sustituye por el import.
 */
const PENCIL_ICON_SVG = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"'
  + ' width="10" height="10" fill="none" stroke="currentColor" stroke-width="2"'
  + ' stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'
  + '<path d="M17 3a2.85 2.83 0 1 1 4 4L7.5 20.5 2 22l1.5-5.5Z"/><path d="m15 5 4 4"/></svg>';

/**
 * Una ficha: cabecera con título/subtítulo y rejilla de celdas.
 *
 * Una ficha de CAJÓN (section.drawer) se pinta en dos sitios a la vez: en el lienzo
 * queda la cabecera con el botón y su vista resumen, y sus celdas se montan dentro
 * del cajón lateral, agrupadas por ruta. Los ids son los mismos — lo que cambia es
 * dónde vive cada celda (ver contracts/sections.js).
 */
function buildCard(section, context) {
  const drawer = section.drawer ? drawerFor(section, context) : null;

  const card = document.createElement('div');
  card.className = 'card';
  card.dataset.sectionId = section.id;
  card.style.gridColumn = `span ${section.span}`;

  if (drawer) card.classList.add('card--drawer');

  const heading = document.createElement('div');
  heading.className = 'card__heading';

  const cardTitle = document.createElement('h2');
  cardTitle.textContent = section.title;

  const cardSubtitle = document.createElement('span');
  cardSubtitle.className = 'card__subtitle';
  cardSubtitle.textContent = section.subtitle ?? '';

  heading.append(cardTitle, cardSubtitle);

  // Accion de ficha (RANDOM): no ocupa celda, asi que no entra en el encaje.
  if (section.action) {
    const actionButton = document.createElement('button');
    actionButton.type = 'button';
    actionButton.className = 'card__action';
    actionButton.dataset.action = section.action.id;
    actionButton.textContent = section.action.label;
    actionButton.title = section.action.title ?? '';
    actionButton.disabled = true;   // hasta que la pintura diga si hay host
    actionButton.addEventListener('click', () => context.handlers.onAction?.(section.action.id));

    context.actionButtons.push(actionButton);
    heading.append(actionButton);
  }

  // Abridor del cajón: es una acción de VISTA (como plegar el teclado), no del
  // host, así que no pasa por SECTION_ACTIONS ni por el store.
  if (drawer) {
    const trigger = document.createElement('button');
    trigger.type = 'button';
    trigger.className = 'card__action';
    trigger.dataset.drawerTrigger = section.id;
    trigger.textContent = section.drawer.trigger;
    // Icono lápiz del catálogo compartido (mismo trazo en toda la suite). El
    // texto visible queda corto (EDIT) y la etiqueta accesible completa.
    trigger.insertAdjacentHTML('beforeend', PENCIL_ICON_SVG);
    trigger.setAttribute('aria-label', `${section.drawer.trigger} ${section.title}`);
    trigger.title = `${section.title}: se edita en el cajón lateral`;
    trigger.addEventListener('click', () => drawer.open());

    heading.append(trigger);
  }

  const body = document.createElement('div');
  body.className = 'card__body';

  // La rejilla de columnas la impone el reparto SOLO en el lienzo: una ficha de
  // cajon deja que su vista resumen llene el cuerpo (su ancho no reparte celdas)
  // ...salvo que tenga FRONTAL (caja LFO): sus controles viven en el lienzo y
  // necesitan su rejilla (la clase card--drawer la fuerza a una columna).
  if (! drawer || section.drawer.frontal) {
    body.style.gridTemplateColumns = `repeat(${section.columns}, minmax(0, 1fr))`;
    if (drawer) body.classList.add('card__body--frontal');
  }

  // Vista de DETALLE del cajón (MODELOS A–D): no son celdas del APVTS, así que
  // el bucle de controles no las toca — la página monta la vista (con sus
  // handlers al store, los MISMOS que el interface) y aquí solo se cuelga y se
  // repinta con el mismo snapshot que el lienzo.
  if (drawer?.body && section.drawerVisual) {
    drawer.body.append(section.drawerVisual.element);
    context.visuals.push(section.drawerVisual);
  }

  // Destino de cada celda: el cuerpo de la ficha, o su hueco en el cajón. Un
  // cajón CON `groups` (la matriz) monta una fila por ruta; SIN `groups`
  // (GLOBAL & MASTER) apila las celdas en una columna con distintivo n1..nN.
  // CON `blocks` (ENVOLVENTES) las celdas caen dentro del bloque de SU
  // envolvente, que ya montó la vista del cajón (curva encima, knobs debajo).
  // `claimBlocks()` VACIA los contenedores antes: la vista puede sobrevivir al
  // panel (la suite la construye una vez y monta varias) y sin eso los knobs
  // se acumularian entre montajes.
  const slotOf = drawer
    ? (section.drawer.groups
        ? buildSlotRows(section, drawer.body)
        : section.drawer.blocks
          ? section.drawerVisual?.claimBlocks?.() ?? new Map()
          : buildSlotColumn(
              // FRONTAL primero (caja LFO): esos controles viven en el lienzo
              // y el cajon no recibe copia (patron masterLevel en GLOBAL).
              section.ids.filter(
                (id) => !section.drawer.frontal?.includes(id) && id !== context.baselineId,
              ),
              drawer.body,
            ))
    : null;

  for (const control of section.controls) {
    // El control BASE del host (masterLevel) sigue siendo un range NATIVO: es la
    // mitad del contrato de 8.1 paso 2c y el ÚNICO caso donde un control no sale
    // de la familia compartida. Ocupa la primera celda de su ficha, así que es el
    // `input[type=range]` que el host encuentra ANTES que las ruedas del teclado.
    if (control.id === context.baselineId) {
      const built = buildBaselineControl(control, context.readouts);

      body.append(built.wrapper);
      context.onBaseline({ slider: built.slider, control });
      continue;
    }

    const cell = createParameterControl(control, context.handlers);

    context.controls.push({ id: control.id, setNormalized: cell.setNormalized, destroy: cell.destroy });

    // Solo los knobs traen anillo: un choice/toggle no suma telemetria pintable.
    if (typeof cell.setModulation === 'function')
      context.knobsById.set(control.id, cell);

    if (cell.engineParameter && cell.setEngine)
      context.gatedControls.push({ engineParameter: cell.engineParameter, setEngine: cell.setEngine });

    (slotOf?.get(control.id) ?? body).append(cell.element);
  }

  // Vista de la ficha (curva ADSR): ocupa la celda que sobra y se repinta con el
  // mismo snapshot. El panel no sabe QUE dibuja: solo le da sitio y estado — por eso
  // comprueba que le llega una vista MONTADA (app.js resuelve el catalogo) y no un
  // id suelto, que es el error facil de cometer al montar el panel desde un test.
  if (section.visual?.element && typeof section.visual.paint === 'function') {
    context.visuals.push(section.visual);
    body.append(section.visual.element);
  }

  // FRANJA de estado de GLOBAL & MASTER: tempo / MIDI / aleatorio SIN abrir el
  // cajón (los tres controles viven dentro). Es PINTURA del snapshot — lectura
  // de los view-models que la ficha ya tiene, con el mismo displayText que las
  // celdas: cero fuentes nuevas, cero parámetros extra en el recuento.
  if (section.id === 'globalFull') {
    const strip = document.createElement('div');
    strip.className = 'global-strip';

    const byId = Object.fromEntries(section.controls.map((control) => [control.id, control]));
    const items = [
      { key: 'tempo', label: 'TEMPO', control: byId.masterBPM },
      { key: 'midi', label: 'MIDI', control: byId.midiChannel, extra: byId.midiThru },
      { key: 'random', label: 'RANDOM', control: byId.randomStrength },
    ].filter((item) => item.control);

    const stripItems = items.map((item) => {
      const chip = document.createElement('span');
      chip.className = 'global-strip__chip';
      chip.dataset.key = item.key;

      const label = document.createElement('span');
      label.className = 'global-strip__label';
      label.textContent = item.label;

      const value = document.createElement('span');
      value.className = 'global-strip__value';

      chip.append(label, value);
      strip.append(chip);

      return { item, value };
    });

    // buildCard es función de MÓDULO: el estado vivo del panel (lo que paint
    // repinta) le llega por context — mismo camino que onBaseline.
    context.onGlobalStrip(stripItems);

    // Franja clicable: lleva al cajón donde viven los controles reales (mismo
    // patrón del resumen de la matriz: el frontal resume, el cajón edita).
    strip.role = 'button';
    strip.tabIndex = 0;
    strip.setAttribute('aria-label', 'GLOBAL & MASTER: abrir el cajón');
    strip.title = 'Tempo, MIDI y aleatorio se editan en el cajón';
    strip.addEventListener('click', () => drawer?.open());
    strip.addEventListener('keydown', (event) => {
      if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        drawer?.open();
      }
    });

    body.append(strip);
  }

  card.append(heading, body);

  return card;
}

/**
 * Columna del cajón sin rutas (patron GLOBAL & MASTER): una fila por control,
 * con su distintivo numerico. Devuelve el mismo mapa id -> fila que
 * buildSlotRows, para que el bucle de controles no sepa en qué variante está.
 */
function buildSlotColumn(ids, host) {
  const slots = new Map();

  for (const [index, id] of ids.entries()) {
    const row = document.createElement('div');
    row.className = 'drawer-slot drawer-slot--column';
    row.dataset.slot = String(index + 1);

    const badge = document.createElement('span');
    badge.className = 'drawer-slot__badge';
    badge.textContent = `n${index + 1}`;

    row.append(badge);
    host.append(row);

    slots.set(id, row);
  }

  return slots;
}

/** El cajón de una ficha (uno solo por ficha, aunque se pinte desde dos sitios). */
function drawerFor(section, context) {
  const existing = context.drawers.get(section.id);

  if (existing) return existing;

  context.drawerSpecs.set(section.id, section.drawer);

  const drawer = createDrawer({
    id: `drawer-${section.id}`,
    title: section.title,
    badge: section.drawer.badge,
    // El resalte de ruta (ENVOLVENTES -> matriz) es un GESTO, no estado: con el
    // cajón cerrado muere, para que al reabrir por su EDIT no herede un borde
    // viejo. onClose del mueble compartido dispara UNA vez por cierre real.
    onClose: () => context.onDrawerClosed?.(section.id),
  });

  context.drawers.set(section.id, drawer);

  return drawer;
}

/**
 * El distintivo VIVO de un cajón, o `null` si su ficha no declara ninguno.
 *
 * `liveBadge` lista los ids de FUENTE que cuentan (`ids`): una ruta está cuando
 * su fuente no es la primera opción de la lista —"Off", índice 0, que es la
 * convención del contrato generado—, así que el distintivo dice cuántas rutas
 * están ASIGNADAS sobre el total declarado. Una fuente que el snapshot no traiga
 * cuenta como Off: no se inventa una asignación que el cable no lleva.
 */
function liveDrawerBadge(spec, controlsById, parameters) {
  const live = spec.liveBadge;

  if (!live) return null;

  const assigned = live.ids.filter((id) => {
    const control = controlsById.get(id);

    return control ? choiceIndexFromNormalized(control, parameters[id] ?? 0) > 0 : false;
  }).length;

  return `${assigned}/${live.ids.length} ${live.label}`;
}

/**
 * Una fila por ruta del cajón, con su distintivo. Devuelve el mapa id -> fila, que
 * es lo que permite que las celdas caigan en su ruta sin que el bucle de controles
 * sepa nada de la matriz.
 */
function buildSlotRows(section, host) {
  const rows = new Map();

  for (const [index, ids] of section.drawer.groups.entries()) {
    const row = document.createElement('div');
    row.className = 'drawer-slot';
    row.dataset.slot = String(index + 1);

    const badge = document.createElement('span');
    badge.className = 'drawer-slot__badge';
    badge.textContent = `RUTA ${index + 1}`;

    // Nivel de envolvente en vivo: SI la fila acabara con fuente ENV 1/ENV 2,
    // aqui pende su barra (se activa en paint). Montarla siempre evita pedirle
    // a paint decidir DOM por frame: paint solo reevalua data-live.
    const envLevel = document.createElement('span');
    envLevel.className = 'drawer-slot__env-level';
    envLevel.dataset.live = 'false';
    row.append(envLevel);

    row.append(badge);
    host.append(row);

    for (const id of ids) rows.set(id, row);
  }

  return rows;
}

/** Control base: range nativo, normalizado 0..1 en el cable. */
function buildBaselineControl(control, readouts) {
  const slider = document.createElement('input');
  slider.type = 'range';
  slider.id = control.id;
  slider.dataset.parameterId = control.id;
  slider.min = '0';
  slider.max = '1';
  slider.step = '0.001';

  const label = document.createElement('label');
  label.className = 'cell__label';
  label.htmlFor = control.id;
  label.textContent = control.label;

  const readout = document.createElement('span');
  readout.className = 'cell__readout';
  readout.dataset.parameterReadout = control.id;
  readouts.set(control.id, readout);

  // El `data-parameter-id` va SOLO en el slider (es el elemento que el host
  // consulta); en la celda sería un duplicado del mismo id en el documento.
  const wrapper = document.createElement('div');
  wrapper.className = 'cell cell--baseline';
  wrapper.dataset.controlKind = 'slider';
  wrapper.append(label, slider, readout);

  return { wrapper, slider };
}

function paintReadout(readout, control, normalized) {
  if (!readout) return;

  readout.textContent = displayText(control, realFromNormalized(control, normalized));
}

function contractLineFor(state) {
  const { summary } = state;

  return `${summary.total} parameters · ${summary.implemented} wired to the DSP · `
    + `${summary.uiOnly} UI only · ${summary.notRouted} not routed`;
}
