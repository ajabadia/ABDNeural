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

/** Campos de una ruta, en el orden en que se leen (== el orden de los ids). */
const FIELDS = ['Source', 'Destination', 'Amount'];

/**
 * @param {object} options
 * @param {object[]} options.controls  los 12 view-models de la matriz, en orden de
 *   lectura (fuente, destino, cantidad por ruta): de ahí salen los 4 grupos.
 * @returns {{ element: HTMLElement, rows: object[], paint: Function }}
 */
export function createModSummary({ controls }) {
  // Cuatro rutas de tres campos: la agrupación sale del PROPIO orden declarado en
  // SECTION_VISUALS['mod-summary'].parameterIds (y un test exige que coincida con
  // las rutas del cajón), en vez de repetir aquí la lista de ids.
  const routes = [];

  for (let index = 0; index < controls.length; index += FIELDS.length)
    routes.push(controls.slice(index, index + FIELDS.length));

  const element = document.createElement('div');
  element.className = 'mod-summary';

  const rows = routes.map((route, routeIndex) => {
    const row = document.createElement('div');
    row.className = 'mod-summary__row';
    row.dataset.slot = String(routeIndex + 1);

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

    element.append(row);

    return { row, painted };
  });

  return {
    element,
    rows,

    /** Repinta las cuatro rutas desde el snapshot normalizado del store. */
    paint(parameters) {
      for (const { painted } of rows) {
        for (const { control, element: span } of painted) {
          const normalized = parameters[control.id] ?? 0;

          span.textContent = displayText(control, realFromNormalized(control, normalized));
        }
      }
    },
  };
}
