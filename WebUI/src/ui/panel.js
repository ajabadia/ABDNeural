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
// Los defaults del contrato generado (linea defaultNormalized de cada
// parametro): la referencia del modo `touched` del distintivo vivo.
import { defaultNormalizedState, describeControl, toNormalized } from '../contracts/parameters.js';
import { KEYS_TAB } from '../contracts/screens.js';
import { createParameterControl } from './controls.js';
// Cajon compartido de la familia (contenido estable: sin re-render al abrir).
import { createDrawer } from '@abdsynths/shared/components';
import { ENV1_SOURCE, ENV2_SOURCE } from './envelopeViews.js';
// La verdad de "ranura cargada" es LA MISMA que pinta la vista del cajon de
// MODELOS (displayableName), y los defaults del contrato (touched) son los que
// siembra el store: importados, no reescritos.
import { MODEL_SLOT_LABELS, displayableName as displayableModelName } from './modelSlots.js';

/**
 * Ranuras que expone el motor (`getNumModelSlots() == 4`, cuatro huecos en
 * `modelNames`): el total del distintivo vivo de MODELOS. Si el motor publicara
 * otra cosa, la vista model-slots ya pinta el desajuste; aqui el total se
 * deriva de las etiquetas para no declarar el 4 dos veces.
 */
const MODEL_SLOT_COUNT = MODEL_SLOT_LABELS.length;
/**
 * Margen del modo `touched`: un valor a menos de medio paso minimo del default
 * es "en default" (los floats pasan por el mapeo de la NormalisableRange, y el
 * redondeo del cable puede devolver 0.7000001). Medio paso de la rejilla de
 * 1/4096 del wire es generoso con el redondeo y estricto con el gesto.
 */
const TOUCH_EPSILON = 1 / 8192;
import { createRouteBack } from './routeBack.js';
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
  // Chips del LIENZO con el distintivo vivo (0/4 RANURAS, 0/8 GLOBAL): la MISMA
  // verdad que la cabecera del cajon, pero a la vista. Se registran por ficha
  // para que el paint los reescriba con el resultado del MISMO calculo (un dato,
  // dos destinos). Solo los monta quien lo pide con `liveBadge.onCard`.
  const liveBadgeChips = new Map();
  // El CONMUTADOR de la ruta local del pad (ficha MATRIZ, modo local): sus
  // piezas, para que el paint las repinte con el MISMO snapshot que las
  // celdas. `state` es la ultima verdad pintada: el proximo clic ya sabe
  // hacia donde va (la vista no decide el estado, lo refleja).
  const localRoutes = new Map();
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
  // VOLVER A LA RUTA: el boton que reabre la MATRIZ en el MISMO slot mientras
  // el retorno este fresco. Es del panel (el dueño del gesto vive aqui) pero
  // se cuelga del cajon de ORIGEN: es alli donde el usuario mira al decidir
  // "vuelve a lo que estaba editando". Ciclo de vida = el de routeReturn:
  // nace con el salto con retorno, se REANCLA en cada ida y vuelta completa
  // (cerrar la matriz reabre ENVOLVENTES -> anchor) y muere cuando el usuario
  // cierra el cajon por si mismo (el onClose limpia routeReturn y el boton).
  const routeBack = createRouteBack();
  // La fila del LCD (ver el bloque de abajo: solo existe con lcdSlot).
  let lcdRowSlot = null;

  /** Limpia el resalte del cajon dado (o del que lo tenga). */
  function clearRouteHighlight(sectionId) {
    if (highlightedSlot && sectionId && highlightedSlot.sectionId !== sectionId) return;

    highlightedSlot = null;

    for (const drawer of drawers.values()) {
      for (const row of drawer.body.querySelectorAll('.drawer-slot'))
        row.dataset.slotHighlight = 'false';

      // La mitad de vistas (model-slots) pinta sus propias filas: mismas dos
      // familias que openDrawerRoute resalta.
      for (const row of drawer.body.querySelectorAll('[data-slot-visual]'))
        row.dataset.slotHighlight = 'false';
    }
  }

  /**
   * Cuelga el boton VOLVER en el cuerpo del cajon de origen y arma su gesto.
   * El click vuelve a saltar con retorno (reabre la matriz en el slot fresco
   * y deja el retorno vivo), como cualquier IR A LA RUTA. El ciclo de vida lo
   * gobierna el panel: cuelga aqui (una vez) y descolga routeBack.clear().
   */
  function mountRouteBack(sectionId) {
    const origin = drawers.get(sectionId);

    if (!origin || origin.body.contains(routeBack.element)) return;

    routeBack.onBack((slot) => openDrawerRoute('modMatrix', slot, { returnTo: sectionId }));
    origin.body.append(routeBack.element);
  }

  /**
   * El usuario ABRE un cajon por su cuenta (EDIT de ficha, franja de GLOBAL):
   * el retorno pendiente muere AQUI, con la vuelta todavia por cobrar. La
   * vuelta era el cierre de la MATRIZ, y el usuario ya se movio por su pie;
   * abrir el propio cajon de origen TAMBIEN cancela (cerrarlo luego es un
   * cierre propio, el mismo trato que le da el onClose).
   */
  function cancelRouteReturn() {
    routeReturn = null;
    routeBack.clear();
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
        liveBadgeChips,
        localRoutes,
        gatedControls,
        visuals,
        handlers,
        // Los chips de la franja de GLOBAL (los llena buildCard, los repinta paint).
        onGlobalStrip: (items) => { globalStripItems = items; },
        // El usuario abre un cajon por su cuenta: cancela el retorno pendiente.
        onDrawerOpenedByUser: () => { cancelRouteReturn(); },
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
          // El slot fresco se captura ANTES de limpiar el resalte (que lo
          // anula): es el que reancla el boton VOLVER.
          const returnSlot = highlightedSlot?.sectionId === sectionId
            ? highlightedSlot.slot : null;

          clearRouteHighlight(sectionId);

          const returnTo = routeReturn;
          routeReturn = null; // consumo UNICO: cerrar otra vez ya no vuelve

          const back = returnTo && drawers.get(returnTo);
          if (returnTo && back) {
            back.open();
            // La vuelta completa REANCLA el boton en el slot que trajo aqui:
            // el retorno sigue fresco mientras el usuario no cierre el cajon.
            routeBack.anchor(returnSlot);
          } else {
            // Cierre de usuario (o de un cajon sin retorno pendiente): el gesto
            // murio con el boton. El consumo unico de arriba ya vacio
            // routeReturn; esto solo descolga el boton del cajon de origen.
            routeBack.clear();
          }
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

    // El distintivo VIVO de los cajones que lo declaran se reescribe con el
    // MISMO snapshot que pinta las celdas, y con `setHeader` del mueble
    // compartido — que no reconstruye nada, solo el dato. La MATRIZ cuenta
    // rutas ASIGNADAS, MODELOS ranuras cargadas (fuera del APVTS:
    // state.models) y GLOBAL celdas tocadas (defaults del contrato); el modo
    // lo declara cada ficha (ver liveDrawerBadge). Un cajon sin `liveBadge`
    // (2 ADSR, 4 LFO) conserva el literal de su ficha: no cambia con el uso.
    for (const [sectionId, spec] of drawerSpecs) {
      const badge = liveDrawerBadge(spec, controlsById, state);

      if (badge === null) continue;

      drawers.get(sectionId)?.setHeader({ badge });

      // El chip del lienzo (si la ficha lo pide) muestra ESE MISMO dato: no
      // hay segundo calculo que pueda discrepar del distintivo del cajon. La
      // etiqueta accesible se rehace con el dato para que el nombre accesible
      // contenga el texto visible (WCAG 2.5.3), y el title anade el destino.
      const chip = liveBadgeChips.get(sectionId);

      if (chip) {
        chip.textContent = shortLiveBadge(badge);
        chip.setAttribute('aria-label', `${badge}: abrir el cajón de ${chip.dataset.cardTitle}`);
        chip.title = `${chip.dataset.cardTitle}: ${badge} — se edita en el cajón lateral`;
      }
    }

    // EL CONMUTADOR de la ruta local del pad: mismo snapshot, misma verdad. Con
    // host se pinta deshabilitado (la matriz es del APVTS y la siembra local no
    // existe) y en OFF su selector de LFO tambien: se guarda el LFO elegido
    // para cuando se vuelva a encender.
    for (const [sectionId, parts] of localRoutes) {
      const route = state.localMorphRoute;

      if (!route) continue;   // un snapshot sin el campo: no se inventa

      parts.state = route;

      const withHost = state.bridgeAvailable === true;
      const title = withHost
        ? `${sectionId}: solo en MODO LOCAL (sin host) — con plugin manda la matriz del APVTS`
        : `${route.enabled ? 'Quitar' : 'Sembrar'} la ruta del pad (${route.source} → Morph Z) en la fila 3 de la matriz`;

      parts.toggle.setAttribute('aria-pressed', String(route.enabled));
      parts.toggle.classList.toggle('is-on', route.enabled);
      parts.toggle.title = title;
      parts.toggle.disabled = withHost;
      parts.select.disabled = withHost || !route.enabled;
      parts.select.title = title;

      // Las OPCIONES son las que publique el store (la tabla de fuentes del
      // contrato). Se rellenan la primera vez y solo se refrescan si la lista
      // cambia, que seria un contrato nuevo: repintar opciones en cada paint
      // robaria el foco del desplegable.
      const sources = Array.isArray(route.sources) ? route.sources : [];
      const sourceKey = sources.join('|');

      if (parts.sourceKey !== sourceKey) {
        parts.sourceKey = sourceKey;
        parts.select.replaceChildren(...sources.map((label) => new Option(label, label)));
      }

      if (sources.includes(route.source)) parts.select.value = route.source;
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

  const voiceMeter = document.createElement('button');
  voiceMeter.type = 'button';
  voiceMeter.className = 'voice-meter';
  voiceMeter.hidden = true;
  // Clic = PANIC (el MISMO gesto doble del botón de la franja: notas apagadas
  // por el bridge Y pánico al worklet). El tooltip lo dice y cuenta las voces
  // que va a parar — la intención es visible sin abrir la consola. El aria-label
  // lo reescribe setVoiceMeter con el conteo vivo.
  voiceMeter.setAttribute('aria-label', 'PANIC: parar todas las voces');
  voiceMeter.title = 'PANIC: parar todas las voces';
  voiceMeter.addEventListener('click', () => handlers.onPanic?.());
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
    // El nombre accesible y el tooltip describen la ACCION del boton, con el
    // conteo vivo de las voces que va a parar.
    const label = `PANIC: parar ${voices} ${voices === 1 ? 'voz activa' : 'voces activas'}`;

    voiceMeter.setAttribute('aria-label', label);
    voiceMeter.title = label;

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
    ? onTelemetry((frame) => {
      paintEnvLevels(lastParameters);

      // El medidor de voces en PLUGIN: el frame nativo trae `voices` (el MISMO
      // dato que el meter del worklet ensena en local). Sin propiedad, no toca
      // nada — el dueño en local sigue siendo onWorkletVoices (app.js).
      if (frame?.voices !== undefined) setVoiceMeter(frame.voices);
    })
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
    liveBadgeChips.clear();
    controlsById.clear();
    drawers.clear();
    highlightedSlot = null;
    routeReturn = null;
    routeBack.clear();
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
      if (otherId !== sectionId && other.isOpen()) other.close();    routeReturn = returnTo;

    if (returnTo) {
      mountRouteBack(returnTo);
      routeBack.anchor(slot);
    } else {
      // Salto SIN retorno (rutas del lienzo, EDIT): el gesto VOLVER que pudiera
      // quedar de un salto anterior ya no esta pendiente. Sin boton colgado,
      // es un no-op.
      routeBack.clear();
    }

    highlightedSlot = { sectionId, slot: Number(slot) || 0 };

    for (const row of drawer.body.querySelectorAll('.drawer-slot'))
      row.dataset.slotHighlight = String(Number(row.dataset.slot) === highlightedSlot.slot);

    // Resalte por CLASE de vista (no solo por filas de celdas): el cajón de
    // MODELOS no tiene celdas del APVTS — sus cuatro filas son la vista
    // model-slots, en la numeración 0-based del motor (data-slot 0..3). Así el
    // mismo gesto (esquina A-D del pad -> ranura) resalta en ambos mundos.
    for (const row of drawer.body.querySelectorAll('[data-slot-visual]'))
      row.dataset.slotHighlight = String(Number(row.dataset.slotVisual) === highlightedSlot.slot);

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
    routeBack,
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

  // Distintivo VIVO en el lienzo (0/4 RANURAS, 0/8 GLOBAL): el mismo dato que
  // la cabecera del cajon — aqui a la vista, y con el mismo gesto que la franja
  // de GLOBAL & MASTER: pulsarlo ABRE el cajon. Lo piden las fichas con
  // `liveBadge.onCard`; nace con el literal del contrato (el inventario, que es
  // lo unico cierto antes del primer paint) y el paint lo reescribe con la
  // verdad. Va justo antes del EDIT porque es el `margin-left: auto` del
  // primero el que empuja la pareja al borde derecho de la cabecera.
  if (drawer && section.drawer.liveBadge?.onCard) {
    const chip = document.createElement('button');
    chip.type = 'button';
    chip.className = 'card__badge';
    chip.dataset.liveBadge = section.id;
    chip.dataset.cardTitle = section.title;
    chip.textContent = shortLiveBadge(section.drawer.badge);
    chip.setAttribute('aria-label', `${section.drawer.badge}: abrir el cajón de ${section.title}`);
    chip.title = `${section.title}: ${section.drawer.badge} — se edita en el cajón lateral`;
    chip.addEventListener('click', () => {
      context.onDrawerOpenedByUser?.();
      drawer.open();
    });

    context.liveBadgeChips.set(section.id, chip);
    heading.append(chip);
  }

  // EL CONMUTADOR de la ruta local del pad: la fila del pad se enciende y
  // apaga, y se elige que LFO la mueve. Vive en la CABECERA y no en el cuerpo:
  // el cuerpo de la MATRIZ tiene altura FIJA para las cuatro filas del resumen
  // y añadir una linea mas lo descuadra. Con host se ve pero no se toca: el
  // paint lo deshabilita (con un plugin delante no hay nada local que sembrar).
  if (section.localRoute) {
    const tools = document.createElement('div');
    tools.className = 'mod-route';
    tools.dataset.localRoute = section.id;

    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'mod-route__toggle';
    toggle.dataset.localRouteToggle = section.id;
    toggle.textContent = section.localRoute.label;
    toggle.setAttribute('aria-pressed', 'false');
    // Deshabilitado hasta el primer paint (el patron de la accion RANDOM): antes
    // del snapshot no hay estado que invertir, y un clic a ciegas no es un
    // gesto que la vista pueda atribuir.
    toggle.disabled = true;

    // El paint sabe si estaba encendida: la vista no guarda el estado, lo
    // refleja, y el gesto solo dice hacia donde va.
    const parts = { toggle, select: null, state: null, sourceKey: null };

    toggle.addEventListener('click', () => {
      context.handlers.onLocalRoute?.({ enabled: !(parts.state?.enabled ?? false) });
    });

    const select = document.createElement('select');
    select.className = 'mod-route__source synth-select';
    select.dataset.localRouteSource = section.id;
    select.setAttribute('aria-label', 'LFO de la ruta del pad');
    select.addEventListener('change', () =>
      context.handlers.onLocalRoute?.({ source: select.value }));

    parts.select = select;
    tools.append(toggle, select);
    context.localRoutes.set(section.id, parts);
    heading.append(tools);
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
    trigger.addEventListener('click', () => {
      context.onDrawerOpenedByUser?.();
      drawer.open();
    });

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
    strip.addEventListener('click', () => {
      context.onDrawerOpenedByUser?.();
      drawer?.open();
    });
    strip.addEventListener('keydown', (event) => {
      if (event.key === 'Enter' || event.key === ' ') {
        event.preventDefault();
        context.onDrawerOpenedByUser?.();
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

  // La spec viaja con los ids de SU ficha: el distintivo vivo 'touched' los
  // deriva (celdas del cajon, sin el control base) sin duplicarlos en el
  // contrato.
  context.drawerSpecs.set(section.id, { ...section.drawer, ids: section.ids });

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
 * Texto VISIBLE del chip del lienzo: la FRACCION del distintivo vivo ('0/4'),
 * no su rotulo entero ('0/4 RANURAS'). La cabecera de una ficha es una fila
 * FIJA y la de MODELOS ya va justa (su subtitulo se aprieta a cero): con el
 * rotulo completo el chip empujaba el EDIT fuera de la ficha. La palabra se
 * queda en el `title` y en la etiqueta accesible, donde si cabe. La
 * cabecera del cajon —que si tiene sitio— sigue mostrando el rotulo entero.
 *
 * @param {string} badge  el distintivo vivo completo
 * @returns {string} lo que cabe en la cabecera de la ficha
 */
function shortLiveBadge(badge) {
  return String(badge).split(' ')[0];
}

/**
 * El distintivo VIVO de un cajón, o `null` si su ficha no declara ninguno.
 *
 * El MODO lo declara la ficha (`liveBadge.mode`) y el dato lo trae el snapshot:
 *
 *   - `assigned` (la MATRIZ): `ids` lista los ids de FUENTE que cuentan — una
 *     ruta está cuando su fuente no es la primera opción de la lista ("Off",
 *     índice 0, la convención del contrato generado). Una fuente que el
 *     snapshot no traiga cuenta como Off: no se inventa una asignación que el
 *     cable no lleva. (Es el modo por defecto: las fichas que solo traen
 *     `ids` + `label` siguen funcionando igual.)
 *   - `loaded` (MODELOS): cuántas ranuras del MOTOR traen modelo. El dato no
 *     es un parámetro: vive en `state.models` y la verdad es la MISMA que
 *     pinta la vista model-slots (entry con nombre != 'EMPTY', via
 *     displayableName — un nombre con `isValid: false` cuenta como cargada:
 *     el fichero se cargó, lo que falla es el fichero).
 *   - `touched` (GLOBAL): cuántas CELDAS del cajón se han apartado del default
 *     del contrato generado (defaultNormalized). Un id que el snapshot no
 *     traiga cuenta como en default: no se inventa un gesto que no hubo.
 *   - `active` (las fichas CON MOTOR): cuántas celdas consume el motor que está
 *     sonando, con la cobertura que el propio gating usa (ver
 *     `activeCellsBadge`). Si el CONTRATO no declara el selector de motor no
 *     hay distintivo: no se inventa un gating que el contrato no dice.
 */
/**
 * Las CELDAS que cuenta un distintivo vivo: las que la ficha declara en
 * `liveBadge.ids` o, si no, las suyas (drawerFor anexa los ids de la ficha a la
 * spec). masterLevel queda fuera en ambos casos por contrato 8.1 2c: no es celda
 * del cajon, vive en la ficha, y contar en los dos sitios seria contar dos veces
 * lo mismo.
 */
function drawerCellIds(live, spec) {
  if (Array.isArray(live.ids) && live.ids.length > 0) return live.ids;

  return (spec.ids ?? []).filter((id) => id !== 'masterLevel');
}

/**
 * Distintivo VIVO de CELDAS ACTIVAS: cuantas de las celdas de la ficha consume el
 * motor que esta sonando. No duplica cobertura: lee la MISMA que usa el gating
 * de la UI, y solo cambia el punto de vista (de celda a ficha).
 *
 *   - Celda GATEADA (choice con `engineParameter`: los destinos de la MATRIZ):
 *     activa si la OPCION que tiene seleccionada es alcanzable con el motor
 *     activo — la misma regla con la que `setEngine` deshabilita opciones
 *     (`engine !== 'both' && engine !== active`). Un destino de NEUROTIK
 *     seleccionado mientras suena NEURONiK es una celda apagada, y el distintivo
 *     lo dice en vez de contar un destello que no suena.
 *   - Celda normal: la cobertura POR PARAMETRO (`engines`, la que el host deriva
 *     con `engineCoverageFor`). La consumen `both` y el motor activo; `host` la
 *     consume el procesador, asi que cambiar de motor no la apaga y cuenta
 *     siempre; `none` no la consume nadie y nunca cuenta.
 *
 * El motor activo lo dice el MISMO choice que la UI usa para gatear: el
 * `engineParameter` de la celda, o el que nombre la ficha en `live.engine`
 * (engineType por defecto). Un VALOR ausente cuenta como su default del
 * contrato, igual que en los modos `assigned` y `touched` — el store siempre
 * siembra los defaults—; lo que no se inventa es el SELECTOR: si el contrato no
 * lo declara, o su lista de opciones no nombra ningún motor, no hay distintivo
 * (`null`), porque un 'activo' sin motor que lo sostenga sería un número inventado.
 *
 * @returns {string|null} '3/5 ACTIVAS', o null si el snapshot no dice el motor
 */
function activeCellsBadge(live, spec, controlsById, parameters) {
  const ids = drawerCellIds(live, spec);

  if (ids.length === 0) return null;

  // Motor activo por parametro, en memo: varias celdas pueden gatear por el
  // mismo choice y leerlo dos veces seria leer el contrato dos veces.
  const activeEngine = new Map();
  const engineFor = (gateId) => {
    if (!activeEngine.has(gateId)) {
      // El selector de motor se resuelve por CONTRATO si no es celda de la
      // ficha: su cobertura por opcion (`optionEngines`) es del contrato, no
      // de la ficha, y una ficha que no tenga el ENGINE TYPE en su lienzo
      // seguiria teniendo motor (es global, lo elige la ficha OSCILADOR).
      const control = controlsById.get(gateId) ?? describeControl(gateId);
      const index = control
        ? choiceIndexFromNormalized(control, parameters[gateId] ?? 0)
        : -1;
      const name = index >= 0 ? (control.optionEngines ?? [])[index] : undefined;

      // 'both'/'host'/'none' no son motores: un selector de motor que no nombra
      // uno deja el gating sin verdad, y sin verdad no hay distintivo.
      activeEngine.set(gateId, ['neuronik', 'neurotik'].includes(name) ? name : null);
    }

    return activeEngine.get(gateId);
  };

  const gateId = live.engine ?? 'engineType';

  // Sin selector en el contrato no hay distintivo: un 0/N sin motor que lo
  // sostenga significaria "no lo se", que no es lo que dice un distintivo.
  if (engineFor(gateId) === null) return null;

  const active = ids.filter((id) => {
    const control = controlsById.get(id);

    if (!control) return false;

    if (control.engineParameter && (control.optionEngines ?? []).length > 0) {
      const chosen = choiceIndexFromNormalized(control, parameters[id] ?? 0);
      const optionEngine = (control.optionEngines ?? [])[chosen] ?? 'both';
      const running = engineFor(control.engineParameter);

      if (running === null) return false;

      return optionEngine === 'both' || optionEngine === running;
    }

    // `host` la consume el procesador (no es cosa de un motor): sigue viva con
    // los dos motores. `none` no la consume nadie: nunca cuenta.
    return control.engines === 'both'
      || control.engines === 'host'
      || control.engines === engineFor(gateId);
  }).length;

  return `${active}/${ids.length} ${live.label ?? 'ACTIVAS'}`;
}

function liveDrawerBadge(spec, controlsById, state) {
  const live = spec.liveBadge;

  if (!live) return null;

  const parameters = state.parameters;

  if (live.mode === 'loaded') {
    const models = Array.isArray(state.models) ? state.models : [];
    const loaded = models.filter((entry) => displayableModelName(entry) !== null).length;

    return `${loaded}/${MODEL_SLOT_COUNT} ${live.label ?? 'RANURAS'}`;
  }

  if (live.mode === 'active') return activeCellsBadge(live, spec, controlsById, parameters);

  if (live.mode === 'touched') {
    const ids = drawerCellIds(live, spec);
    // Los defaults se resuelven UNA vez para toda la fila, no celda a celda: el
    // distintivo se repinta en cada paint y `defaultNormalizedState` es una
    // llamada por id (mapa + conversiones) que aqui solo depende del contrato.
    const contractDefaults = defaultNormalizedState(ids);

    const touched = ids.filter((id) => {
      const value = parameters[id];

      if (value === undefined) return false;

      const control = controlsById.get(id);
      // El default del contrato puede venir del descriptor (con el skew del
      // mapeo) o del propio generated (defaultNormalized); si no trae ninguno,
      // un parametro se nace a "0 en real" y ese es su default. Nunca
      // undefined: un default ausente contaria TODAS las celdas como tocadas.
      const fallbackDefault = control ? realFromNormalized(control, 0) : 0;
      const def = contractDefaults[id]
        ?? (control ? toNormalized(control, fallbackDefault) : fallbackDefault);

      return Math.abs(value - def) > TOUCH_EPSILON;
    }).length;

    return `${touched}/${ids.length} ${live.label ?? 'GLOBAL'}`;
  }

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
