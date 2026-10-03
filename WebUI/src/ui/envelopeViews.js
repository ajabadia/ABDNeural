/**
 * Las DOS vistas de la ficha ENVOLVENTES, del mismo catálogo pero con cuerpos
 * distintos:
 *
 *   - `envelope-curves` — el LIENZO: las dos curvas (ENV 1 y ENV 2),
 *     cada una con debajo las rutas que la matriz tiene asignadas a su fuente
 *     (fuente → destino y amount). Sustituye a la única curva ADSR de la ficha
 *     antigua: las dos envolventes van juntas y cada una enseña SU cableado.
 *   - `envelope-blocks` — el CAJÓN: un bloque por envolvente con SU curva
 *     encima de sus cuatro knobs (patrón de las rutas de la matriz: una fila
 *     por envolvente, distintivo incluido).
 *
 * Ninguna de las dos es un control (no tiene parámetro propio ni gesto), así
 * que no ocupan celda y no entran en el encaje — igual que la curva que
 * sustituyen y el resumen de la matriz. Las curvas pintan con la vista
 * compartida `createEnvelopeCurve` (`@abdsynths/shared/components`); su
 * geometria y el detalle del gesto editable viven en el paquete.
 *
 * Las rutas NO se copian a mano: se leen del PROPIO estado de la matriz (los
 * doce ids mod1..mod4). El catálogo `envelope-routes` declara ESO (los ids de
 * los slots), y la vista cruza: fuente == 6 -> ENV 1, == 7 -> ENV 2, y pintan
 * TODAS las rutas activas (una envolvente puede modular dos destinos a la
 * vez). Si la matriz lo cambia —preset, edición, automatización— la vista lo
 * pinta con el mismo snapshot que las celdas del cajón. Sin hardcodeo: lo que
 * se pinta es lo que el cable lleva.
 */

import { choiceIndexFromNormalized, displayText, realFromNormalized } from '../contracts/paramValue.js';
// La curva ADSR es de la FAMILIA compartida desde el 2026-09-29 (migrada de
// src/ui/envelopeCurve.js, borrado: cero copia local). El mapeo
// normalizado->real sigue siendo de aqui (realFromNormalized, el skew del
// contrato); la vista compartida lo recibe por su `toReal` (default
// identidad: pintar desde valores REALES es su contrato).
import { createEnvelopeCurve } from '@abdsynths/shared/components';

/** Ids de los slots de la matriz (orden del reparto): la vista los cruza sola. */
export const MATRIX_ROUTE_IDS = [
  'mod1Source', 'mod1Destination', 'mod1Amount',
  'mod2Source', 'mod2Destination', 'mod2Amount',
  'mod3Source', 'mod3Destination', 'mod3Amount',
  'mod4Source', 'mod4Destination', 'mod4Amount',
];

/** Fuentes ENV de la matriz (índices de choice en getModSources()). */
/**
 * Indice de OPCION de las fuentes ENV en el contrato generado (choices:
 * Off, LFO 1, LFO 2, Pitch Bend, Mod Wheel, Aftertouch, ENV 1, ENV 2). Lo
 * consumen las rutas del lienzo/cajón (cruce fuente → slots) y el panel
 * (barra de nivel del cajón de la MATRIZ): un solo número, una sola verdad.
 */
export const ENV1_SOURCE = 6;
export const ENV2_SOURCE = 7;

/**
 * El amount de una ruta como PORCENTAJE con signo (+100%): es la profundidad
 * con la que la ruta modula, y la ficha la lee como "cuánta ENV llega aquí".
 * El formateo genérico no sirve (el amount es -1..1, y `displayText` solo
 * porcentúa 0..1: pintaría "1.00").
 */
export function formatEnvAmount(realAmount) {
  const value = Number.isFinite(realAmount) ? realAmount : 0;

  return `${value >= 0 ? '+' : ''}${Math.round(value * 100)}%`;
}

/**
 * Las rutas de la matriz que tocan a UNA envolvente, desde el snapshot
 * normalizado y los view-models de los doce campos.
 *
 * @param {object} controlsById  los view-models de la matriz por id
 * @param {object} parameters    snapshot normalizado del store
 * @param {number} sourceIndex   6 (ENV 1) o 7 (ENV 2)
 * @returns {{ slot: number, destination: object, destinationNormalized: number,
 *   amount: object, amountNormalized: number }[]}
 */
export function envRoutesFromSnapshot(controlsById, parameters, sourceIndex) {
  const routes = [];

  for (let index = 0; index < MATRIX_ROUTE_IDS.length; index += 3) {
    const [sourceId, destinationId, amountId] = MATRIX_ROUTE_IDS.slice(index, index + 3);
    const source = controlsById[sourceId];
    const destination = controlsById[destinationId];
    const amount = controlsById[amountId];

    if (!source || !destination || !amount) continue;

    const sourceNormalized = parameters[sourceId] ?? 0;
    if (choiceIndexFromNormalized(source, sourceNormalized) !== sourceIndex) continue;

    routes.push({
      // El slot (1..4) es el que abre el cajón de la MATRIZ: el mismo número
      // del distintivo RUTA n que pinta el panel.
      slot: index / 3 + 1,
      destination,
      destinationNormalized: parameters[destinationId] ?? 0,
      amount,
      amountNormalized: parameters[amountId] ?? 0,
    });
  }

  return routes;
}

/**
 * La fila de UNA ruta: destino → amount, con el mismo vocabulario del resumen.
 * Es un BOTON: pulsarla abre el cajón de la MATRIZ en su slot (ver
 * `onOpenRoute`; el panel lo cierra tarde porque los cajones son suyos).
 */
function routeRow(route, onOpenRoute) {
  const row = document.createElement('button');
  row.type = 'button';
  row.className = 'env-route';
  row.dataset.envRoute = route.destination.id;
  row.dataset.slot = String(route.slot);
  row.title = `RUTA ${route.slot}: abrir en la MATRIZ DE MODULACIÓN`;
  row.setAttribute('aria-label', `Ruta ${route.slot} de ENV: ${row.title}`);
  row.addEventListener('click', () => onOpenRoute?.(route.slot));

  const destination = document.createElement('span');
  destination.className = 'env-route__destination';
  destination.textContent = displayText(route.destination, realFromNormalized(route.destination, route.destinationNormalized));

  const arrow = document.createElement('span');
  arrow.className = 'env-route__arrow';
  arrow.textContent = '→';

  const amount = document.createElement('span');
  amount.className = 'env-route__amount';
  amount.textContent = formatEnvAmount(realFromNormalized(route.amount, route.amountNormalized));

  row.append(destination, arrow, amount);

  return row;
}

/**
 * Cuerpo del LIENZO: una columna por envolvente (curva + sus rutas debajo).
 *
 * @param {object} options
 * @param {object[]} options.envControls  los view-models de las ocho ADSR (env* y filter*)
 * @param {object[]} options.routeControls  los doce view-models de la matriz
 * @param {(notify: Function) => Function} [options.onTelemetry]  canal de frames
 *   en vivo: frame.envelopes es [amp, filter] (ver ParameterBridgeTest) y cada
 *   nivel pinta la AGUJA de SU curva. Es pintura, no estado: va por su propio
 *   camino, como el espectral y el anillo morphZ.
 * @param {(slot: number) => void} [options.onOpenRoute]  petición de abrir el
 *   cajón de la MATRIZ en el slot (1..4) de una ruta pulsada. Llega TARDE (ver
 *   `setRouteOpener`): las vistas se fabrican antes de que exista el panel.
 * @returns {{ element: HTMLElement, paint: Function, setRouteOpener: Function,
 *   destroy: Function }}
 */
function createCanvasCurves({ envControls, routeControls, onTelemetry }) {
  const controlsById = Object.fromEntries(envControls.concat(routeControls).map((control) => [control.id, control]));

  const element = document.createElement('div');
  element.className = 'env-curves';

  const columns = [];

  for (const identity of [
    // Identidad, NO papel: el destino de cada envolvente lo declara la RUTA
    // que la matriz le tenga asignada — la que se pinta debajo. Antes ponia
    // «ENV 1 · AMP» / «ENV 2 · FILTER» y el rotulo mentia en cuanto la matriz
    // reencaminaba la fuente (dato precableado en el texto).
    { prefix: 'env', dataset: 'amp-envelope', label: 'ENV 1', title: 'Curva de la envolvente 1', aria: 'Curva ADSR de la envolvente 1' },
    { prefix: 'filter', dataset: 'filter-envelope', label: 'ENV 2', title: 'Curva de la envolvente 2', aria: 'Curva ADSR de la envolvente 2' },
  ]) {
    const curve = createEnvelopeCurve({
      controls: envControls,
      prefix: identity.prefix,
      dataset: identity.dataset,
      label: identity.label,
      title: identity.title,
      aria: identity.aria,
      // El caption con la CLASE LOCAL de etiqueta de celda: el mismo texto y
      // el mismo estilo que cuando la curva vivia aqui.
      captionClass: 'cell__label',
      // El skew del contrato: la vista compartida pinta desde valores REALES
      // y el normalizado->real es de aqui (como cuando la curva era local).
      toReal: realFromNormalized,
    });

    const column = document.createElement('div');
    column.className = 'env-curves__column';
    column.dataset.envelope = identity.prefix;
    column.append(curve.element);

    const routes = document.createElement('div');
    routes.className = 'env-curves__routes';
    column.append(routes);

    element.append(column);
    columns.push({ prefix: identity.prefix, curve, routes });
  }

  // Quien abre el cajón de la matriz. Se conecta TARDE: la vista se fabrica
  // antes del panel (el encaje de app.js), pero el cajón es del panel.
  let openRoute = null;

  // El frame trae [amp, filter] en ESE orden (contrato del puente): indice 0 ->
  // ENV 1 (columna 'env'), indice 1 -> ENV 2 (columna 'filter'). Sin canal o sin
  // nivel numerico, la aguja de esa curva simplemente no aparece.
  const stopTelemetry = typeof onTelemetry === 'function'
    ? onTelemetry((frame) => {
      const envelopes = Array.isArray(frame?.envelopes) ? frame.envelopes : [];

      columns[0]?.curve.setLevel(envelopes[0]);
      columns[1]?.curve.setLevel(envelopes[1]);
    })
    : null;

  return {
    element,

    /** Repinta curvas y rutas desde el MISMO snapshot que las celdas. */
    paint(parameters) {
      for (const column of columns) {
        column.curve.paint(parameters);

        // En un giro se vacía el contenedor y se rellenan las rutas ACTIVAS:
        // son 0..4 filas de 20 px, no hay nada que desmontar.
        column.routes.textContent = '';

        const sourceIndex = column.prefix === 'env' ? ENV1_SOURCE : ENV2_SOURCE;
        const routes = envRoutesFromSnapshot(controlsById, parameters ?? {}, sourceIndex);

        if (routes.length === 0) {
          const empty = document.createElement('span');
          empty.className = 'env-route env-route--empty';
          empty.textContent = 'sin ruta en la matriz';
          column.routes.append(empty);
          continue;
        }

        for (const route of routes) column.routes.append(routeRow(route, (slot) => openRoute?.(slot)));
      }
    },

    /**
     * La curva de UNA envolvente por prefijo (`env` | `filter`): el feed del
     * worklet (aguja en modo navegador, app.js) la busca asi. En plugin la
     * aguja vive del canal bridge y nadie llama aqui.
     */
    needleFor(prefix) {
      return columns.find((column) => column.prefix === prefix)?.curve ?? null;
    },

    /**
     * Conecta quién abre el cajón de la matriz (el panel, via app.js). Un
     * repintado no la toca: es wiring, no estado.
     */
    setRouteOpener(opener) {
      openRoute = typeof opener === 'function' ? opener : null;
    },

    destroy() {
      stopTelemetry?.();
      for (const column of columns) column.curve.destroy();
      element.textContent = '';
    },
  };
}

/**
 * Cuerpo del CAJÓN: un bloque por envolvente con SU curva encima de sus knobs.
 * Los knobs los monta el panel (siguen siendo celdas del reparto); esta vista
 * pinta las dos curvas y devuelve los mapas que dicen a qué bloque cae cada id.
 *
 * Cada bloque lleva en su barra el boton IR A LA RUTA: el mismo salto que las
 * rutas del lienzo (abre la MATRIZ resaltando el slot), pero resuelto EN EL
 * CLIC contra el ultimo snapshot — el cajon no debe congelar un slot que la
 * matriz puede haber cambiado desde el ultimo paint. Sin ruta para esa
 * envolvente, el boton se deshabilita (y el paint lo reevalua: la verdad la
 * dice el estado, no el primer render).
 *
 * @param {object} options
 * @param {object[]} options.envControls  los view-models de las ocho ADSR
 * @param {string[]} options.ids  los ids de la ficha, en orden de reparto
 * @param {object[]} [options.routeControls]  los doce view-models de la matriz,
 *   para resolver el slot de cada envolvente en el clic
 * @param {Function} [options.onTelemetry]  canal de frames en vivo: el MISMO par
 *   que alimenta el lienzo (frame.envelopes=[amp, filter]) pinta la aguja de
 *   las curvas del cajón. Pintura, no estado: por su propio camino.
 * @returns {{ element: HTMLElement, paint: Function, blockOf: Function,
 *   needleFor: Function, setRouteOpener: Function, destroy: Function }}
 */
function createDrawerBlocks({ envControls, ids, routeControls = [], onTelemetry = null }) {
  const controlsById = Object.fromEntries(envControls.concat(routeControls).map((control) => [control.id, control]));

  const element = document.createElement('div');
  element.className = 'env-blocks';

  const blocks = [];
  const blockOf = new Map();

  // Quien abre el cajón de la matriz (el panel, via app.js): llega TARDE, como
  // en el lienzo — la vista se fabrica antes del panel.
  let openRoute = null;
  let lastParameters = {};

  // La AGUJA del cajon vive del MISMO canal que la del lienzo: el frame trae
  // [amp, filter] en ESE orden (contrato del puente) y cada nivel pinta SU
  // curva. Sin canal o sin nivel numerico, la aguja de esa curva no aparece.
  const stopTelemetry = typeof onTelemetry === 'function'
    ? onTelemetry((frame) => {
      const envelopes = Array.isArray(frame?.envelopes) ? frame.envelopes : [];

      blocks[0]?.curve.setLevel(envelopes[0]);
      blocks[1]?.curve.setLevel(envelopes[1]);
    })
    : null;

  // Identidad, no papel (igual que el lienzo): el destino lo dice la ruta.
  for (const identity of [
    { prefix: 'env', label: 'ENV 1' },
    { prefix: 'filter', label: 'ENV 2' },
  ]) {
    const curve = createEnvelopeCurve({
      controls: envControls,
      prefix: identity.prefix,
      dataset: 'drawer-envelope',
      // El titulo vive en la BARRA del bloque (con el boton): la caption de la
      // curva sobraria dos veces.
      label: '',
      title: `Curva de la envolvente ${identity.label.replace('ENV ', '')}`,
      aria: `Curva ADSR de la envolvente ${identity.label.replace('ENV ', '')}`,
      // El skew del contrato, igual que el lienzo: sin el, la curva se pinta
      // con los valores NORMALIZADOS y su forma cambia.
      toReal: realFromNormalized,
    });

    const bar = document.createElement('div');
    bar.className = 'env-block__bar';

    const title = document.createElement('span');
    title.className = 'env-block__title';
    title.textContent = identity.label;

    const goto = document.createElement('button');
    goto.type = 'button';
    goto.className = 'env-block__goto';
    goto.textContent = 'IR A LA RUTA';
    goto.disabled = true;   // hasta que el primer paint diga si hay ruta
    goto.addEventListener('click', () => {
      // El slot se resuelve AHORA, no cuando se pinto el boton.
      const sourceIndex = identity.prefix === 'env' ? ENV1_SOURCE : ENV2_SOURCE;
      const routes = envRoutesFromSnapshot(controlsById, lastParameters, sourceIndex);

      if (routes.length > 0) openRoute?.(routes[0].slot);
    });

    bar.append(title, goto);

    const block = document.createElement('div');
    block.className = 'env-block';
    block.dataset.envelope = identity.prefix;
    block.append(bar, curve.element);

    // Los destinos de ESTA envolvente, junto a su grafica: la misma fila del
    // lienzo (destino -> amount) y el mismo dato — las rutas que la MATRIZ le
    // tiene asignadas a su fuente (6/7), resueltas contra el snapshot.
    const routeList = document.createElement('div');
    routeList.className = 'env-block__routes';
    block.append(routeList);

    const knobs = document.createElement('div');
    knobs.className = 'env-block__knobs';
    block.append(knobs);

    element.append(block);
    blocks.push({
      prefix: identity.prefix,
      curve,
      routeList,
      knobs,
      goto,
      sourceIndex: identity.prefix === 'env' ? ENV1_SOURCE : ENV2_SOURCE,
    });

    for (const id of ids) {
      if (id.startsWith(identity.prefix)) blockOf.set(id, knobs);
    }
  }

  return {
    element,
    blockOf,

    /**
     * Los contenedores de knobs de los bloques, VACIOS. El panel los reclama
     * UNA vez por montaje antes de colgar las celdas: la vista puede vivir mas
     * que un panel (la suite la construye una vez y monta varias), y sin esta
     * reclamacion los knobs se acumularian entre montajes.
     */
    claimBlocks() {
      for (const block of blocks) block.knobs.textContent = '';

      return blockOf;
    },

    paint(parameters) {
      lastParameters = parameters ?? {};

      for (const block of blocks) {
        block.curve.paint(lastParameters);

        const routes = envRoutesFromSnapshot(controlsById, lastParameters, block.sourceIndex);

        // Un giro: se vacia y se rellena con las rutas ACTIVAS (0..4 filas).
        // El boton IR A LA RUTA sigue: es el salto a la MATRIZ; estas filas
        // dicen A DONDE va.
        block.routeList.textContent = '';

        if (routes.length === 0) {
          const empty = document.createElement('span');
          empty.className = 'env-route env-route--empty';
          empty.textContent = 'sin ruta en la matriz';
          block.routeList.append(empty);
        } else {
          for (const route of routes) block.routeList.append(routeRow(route, (slot) => openRoute?.(slot)));
        }

        if (routes.length > 0) {
          block.goto.disabled = false;
          // Varias rutas para la misma envolvente: el clic abre la primera y el
          // tooltip las lista todas.
          block.goto.title = `RUTA ${routes.map((route) => route.slot).join(', ')}: abrir en la MATRIZ DE MODULACIÓN`;
        } else {
          block.goto.disabled = true;
          block.goto.title = 'Sin ruta de esta envolvente en la matriz';
        }
      }
    },

    /**
     * Conecta quién abre el cajón de la matriz (el panel, via app.js). Un
     * repintado no la toca: es wiring, no estado.
     */
    setRouteOpener(opener) {
      openRoute = typeof opener === 'function' ? opener : null;
    },

    /**
     * La curva de UNA envolvente por prefijo (`env` | `filter`): el feed del
     * worklet (aguja en modo navegador, app.js) la busca asi. En plugin la
     * aguja vive del canal bridge, que ya pinta directo.
     */
    needleFor(prefix) {
      return blocks.find((block) => block.prefix === prefix)?.curve ?? null;
    },

    destroy() {
      stopTelemetry?.();
      for (const block of blocks) block.curve.destroy();
      element.textContent = '';
    },
  };
}

/**
 * Las dos vistas del catálogo (llamadas desde visuals.js).
 *
 * @param {object} options
 * @param {object[]} options.controls  los view-models que pide el catálogo (ocho ADSR
 *   para el lienzo y el cajón; los doce de la matriz solo el lienzo)
 * @param {object[]} [options.routeControls]  los doce de la matriz (lienzo Y cajón:
 *   el cajón los usa para resolver el slot de IR A LA RUTA en el clic)
 * @param {Function} [options.onTelemetry]  canal de frames en vivo (lienzo Y
 *   cajón: la aguja de nivel de cada curva)
 * @param {string[]} [options.ids]  los ids de la ficha (solo cajón: el reparto)
 */
export function createEnvelopeCurves({ controls, routeControls = [], onTelemetry = null }) {
  return createCanvasCurves({ envControls: controls, routeControls, onTelemetry });
}

export function createEnvelopeBlocks({ controls, ids = [], routeControls = [], onTelemetry = null }) {
  return createDrawerBlocks({ envControls: controls, ids, routeControls, onTelemetry });
}
