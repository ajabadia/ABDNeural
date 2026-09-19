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
import { displayText, realFromNormalized } from '../contracts/paramValue.js';
import { KEYS_TAB } from '../contracts/screens.js';
import { createParameterControl } from './controls.js';
import { createDrawer } from './drawer.js';

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
export function createPanel({ bands, baselineId, handlers = {} }) {
  const controls = [];
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
  let baselineSlider = null;
  let baselineControl = null;
  const baselineReadouts = new Map();

  const element = document.createElement('section');
  element.className = 'panel';

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

  const status = document.createElement('p');
  status.className = 'status';

  const contractLine = document.createElement('p');
  contractLine.className = 'contract-line';

  header.append(title, audioRow, status, contractLine);

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
        drawers,
        gatedControls,
        visuals,
        handlers,
        onBaseline: (built) => {
          baselineSlider = built.slider;
          baselineControl = built.control;
        },
        readouts: baselineReadouts,
      }));
    }

    canvas.append(bandElement);
  });

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

  element.append(header, canvas, performance, footer);

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
    for (const visual of visuals) visual.destroy?.();
    for (const drawer of drawers.values()) drawer.destroy();
    controls.length = 0;
    visuals.length = 0;
    gatedControls.length = 0;
    drawers.clear();
    element.textContent = '';
  }

  return {
    element,
    keysRoot,
    drawers,
    paint,
    paintAudio,
    toggleKeys,
    destroy,
    isKeysCollapsed: () => keysCollapsed,
  };
}

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
    trigger.title = `${section.title}: se edita en el cajón lateral`;
    trigger.addEventListener('click', () => drawer.open());

    heading.append(trigger);
  }

  const body = document.createElement('div');
  body.className = 'card__body';

  // La rejilla de columnas la impone el reparto SOLO en el lienzo: una ficha de
  // cajón deja que su vista resumen llene el cuerpo (su ancho no reparte celdas).
  if (! drawer) body.style.gridTemplateColumns = `repeat(${section.columns}, minmax(0, 1fr))`;

  // Destino de cada celda: el cuerpo de la ficha, o la fila de su ruta en el cajón.
  const slotOf = drawer && section.drawer.groups
    ? buildSlotRows(section, drawer.body)
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

  card.append(heading, body);

  return card;
}

/** El cajón de una ficha (uno solo por ficha, aunque se pinte desde dos sitios). */
function drawerFor(section, context) {
  const existing = context.drawers.get(section.id);

  if (existing) return existing;

  const drawer = createDrawer({
    id: `drawer-${section.id}`,
    title: section.title,
    badge: section.drawer.badge,
  });

  context.drawers.set(section.id, drawer);

  return drawer;
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
