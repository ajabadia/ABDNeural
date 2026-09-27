/**
 * Resumen de las cuatro rutas de la matriz de modulación.
 *
 * Es la vista que queda en el LIENZO cuando los 12 controles se mudan al cajón:
 * una línea por ruta con fuente → destino → cantidad, leída del mismo snapshot que
 * los controles. No es un control (no tiene parámetro propio ni gesto), así que no
 * ocupa celda y no entra en el encaje — igual que la curva ADSR.
 *
 * Lo que NO hace, a propósito: marcar una ruta que el motor activo no consume. Eso
 * lo dice el propio desplegable dentro del cajón (opción deshabilitada + motivo), y
 * duplicar aquí esa regla sería una segunda copia del gating. El resumen dice lo que
 * el preset pide; el cajón dice lo que el motor puede hacer con ello.
 */

import { displayText, realFromNormalized } from '../contracts/paramValue.js';
import { choiceIndexFromNormalized } from '../contracts/paramValue.js';
import { ENV1_SOURCE, ENV2_SOURCE } from './envelopeViews.js';
import { latestTelemetry } from '../bridge/telemetry.js';

/** Campos de una ruta, en el orden en que se leen (== el orden de los ids). */
const FIELDS = ['Source', 'Destination', 'Amount'];

/**
 * @param {object} options
 * @param {object[]} options.controls  los 12 view-models de la matriz, en orden de
 *   lectura (fuente, destino, cantidad por ruta): de ahí salen los 4 grupos.
 * @returns {{ element: HTMLElement, rows: object[], paint: Function }}
 */
export function createModSummary({ controls, onTelemetry = null }) {
  // Cuatro rutas de tres campos: la agrupación sale del PROPIO orden declarado en
  // SECTION_VISUALS['mod-summary'].parameterIds (y un test exige que coincida con
  // las rutas del cajón), en vez de repetir aquí la lista de ids.
  const routes = [];

  for (let index = 0; index < controls.length; index += FIELDS.length)
    routes.push(controls.slice(index, index + FIELDS.length));

  const element = document.createElement('div');
  element.className = 'mod-summary';

  const rows = routes.map((route, routeIndex) => {
    // Fila BOTON: el mismo gesto que las rutas de ENVOLVENTES (envRoute en
    // envelopeViews) — pulsarla abre el cajón de la MATRIZ resaltando SU slot.
    // Quien ejecuta llega TARDE (la vista se fabrica antes del panel): el click
    // llama a `openRoute` si app.js ya lo conectó; sin opener, la fila es texto.
    const row = document.createElement('button');
    row.type = 'button';
    row.className = 'mod-summary__row';
    row.dataset.slot = String(routeIndex + 1);
    row.title = `RUTA ${routeIndex + 1}: abrir en la MATRIZ DE MODULACIÓN`;
    row.addEventListener('click', () => openRoute?.(routeIndex + 1));

    const slot = document.createElement('span');
    slot.className = 'mod-summary__slot';
    slot.textContent = String(routeIndex + 1);

    row.append(slot);

    const painted = route.map((control, fieldIndex) => {
      // La flecha separa fuente de destino: es lo que hace legible la ruta.
      if (fieldIndex === 1) {
        const arrow = document.createElement('span');
        arrow.className = 'mod-summary__arrow';
        arrow.textContent = '→';
        row.append(arrow);
      }

      const value = document.createElement('span');
      value.className = `mod-summary__${FIELDS[fieldIndex].toLowerCase()}`;
      // NO lleva `data-parameter-id`: en esta página ese atributo marca una CELDA de
      // control (el anclaje que cuentan el selftest y la suite), y un resumen es un
      // texto que se pinta, no un control. Marcarlo lo duplicaba en el recuento.
      value.dataset.summaryParameter = control.id;
      row.append(value);

      return { control, element: value };
    });

    // La BARRA de nivel de envolvente, junto al amount: la misma pieza que las
    // filas ENV del cajon (`.drawer-slot__env-level`), aqui en el LIENZO. Viva
    // solo si la fuente de esta ruta es ENV 1/ENV 2 (lo decide paint con el
    // snapshot; sin fuente ENV es un tubo sin fondo, invisible). PREPEND: la
    // primera pista del grid de la fila es la suya (5px), como en el cajon.
    const envLevel = document.createElement('span');
      envLevel.className = 'drawer-slot__env-level';
      envLevel.dataset.live = 'false';
      row.prepend(envLevel);

    element.append(row);

    return { row, painted };
  });

  // Quien abre el cajón de la MATRIZ (el panel, via app.js). Con null, las filas
  // quedan en texto informativo (el click no lleva a ninguna parte): el opener
  // es wiring, no estado — un repintado no lo toca.
  let openRoute = null;
  let lastParameters = {};
  // El ultimo par de niveles visto por la SUSCRIPCION (el frame mas fresco que
  // esta vista ha recibido): paint sin frame a mano usa este, no una lectura
  // global que puede no existir (el canal de la vista es inyectable).
  let lastEnvelopes = null;

  /**
   * La DECISION de qué filas tienen barra viva corre con cada snapshot (la
   * fuente de cada ruta, contra el orden del desplegable): ENV 1 -> el par
   * envelopes[0], ENV 2 -> envelopes[1]. El NIVEL por frame es la misma
   * pintura de telemetría que las agujas y las barras del cajón. Sin canal,
   * la decisión vive igualmente en paint (el nivel se queda en el último
   * frame recibido, como en el cajón).
   */
  function paintEnvLevels(parameters, envelopes = null) {
    rows.forEach(({ row }, index) => {
      const sourceControl = controls[index * FIELDS.length];
      const value = sourceControl ? parameters[sourceControl.id] : undefined;
      const optionIndex = sourceControl && value !== undefined
        ? choiceIndexFromNormalized(sourceControl, value)
        : -1;

      const envLevel = row.querySelector('.drawer-slot__env-level');
      const envelope = optionIndex === ENV1_SOURCE ? 0 : optionIndex === ENV2_SOURCE ? 1 : -1;

      envLevel.dataset.live = String(envelope >= 0);

      if (envelope >= 0) {
        // El nivel viene del FRAME que dispara la pintura (el mismo dato que
        // ven las agujas); sin frame a mano, el ultimo que la vista recibio.
        const levels = envelopes ?? lastEnvelopes ?? [];
        const level = levels[envelope] ?? 0;

        envLevel.style.setProperty('--env-level', String(Math.min(1, Math.max(0, level))));
      }
    });
  }

  // El frame repinta el nivel SIN paint intermedio (el canal es su dueño),
  // igual que las barras del cajón. Muere con la vista.
  const stopTelemetry = typeof onTelemetry === 'function'
    ? onTelemetry((frame) => {
      lastEnvelopes = Array.isArray(frame?.envelopes) ? frame.envelopes : lastEnvelopes;

      return paintEnvLevels(
        lastParameters,
        Array.isArray(frame?.envelopes) ? frame.envelopes : null,
      );
    })
    : null;

  return {
    element,
    rows,

    /** Repinta las cuatro rutas desde el snapshot normalizado del store. */
    paint(parameters) {
      lastParameters = parameters ?? {};

      for (const { painted } of rows) {
        for (const { control, element: span } of painted) {
          const normalized = parameters[control.id] ?? 0;

          span.textContent = displayText(control, realFromNormalized(control, normalized));
        }
      }

      paintEnvLevels(lastParameters);
    },

    /**
     * Conecta quién abre el cajón de la matriz resaltando el slot (app.js, tarde:
     * la vista se fabrica antes del panel). Mismo cable que las rutas de ENVOLVENTES.
     */
    setRouteOpener(opener) {
      openRoute = typeof opener === 'function' ? opener : null;
    },

    destroy() {
      stopTelemetry?.();
    },
  };
}
