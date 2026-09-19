/**
 * Fábrica de vistas de ficha: convierte el catálogo (`SECTION_VISUALS`) en la vista
 * ya montada, con los view-models de los parámetros que la alimentan.
 *
 * Vive en su propio módulo, y no dentro de app.js, porque la usan DOS consumidores:
 * la página (app.js) y la suite del panel (tests/panel.test.js). Cuando la fábrica
 * estaba escrita dentro del test, el harness montaba una curva ADSR para cualquier
 * vista declarada; al añadir el resumen de la matriz, el test contó dos curvas y
 * falló señalando al sitio equivocado. Una sola fábrica, un solo comportamiento.
 *
 * Una vista es DATO de ficha (no tiene parámetro propio ni gesto, así que no ocupa
 * celda): la página compone, el panel solo le da sitio y estado.
 *
 * Las tres vistas no son lo mismo por dentro y no se disimula: la curva y el resumen
 * son DIBUJOS (se repintan con los parámetros), y las ranuras de modelo además
 * PIDEN (su botón lanza una carga en el host). Por eso reciben el handler por
 * `options` y por eso `paint` recibe el estado entero, del que cada una lee lo suyo.
 */

import { createEnvelopeCurve } from './envelopeCurve.js';
import { createModelSlots } from './modelSlots.js';
import { createModSummary } from './modSummary.js';

/**
 * @param {string} visualId  id del catálogo SECTION_VISUALS
 * @param {object[]} controls  view-models de los parámetros que pide esa vista
 * @param {object} [options]
 * @param {(slot: number) => void} [options.onLoad]  carga de una ranura de modelo
 *   (`model-slots`): la ejecuta el host, así que la vista la recibe por aquí y no
 *   la pide al panel, que no sabe qué dibuja.
 * @returns {{ element: HTMLElement, paint: Function, destroy?: Function }|null}
 *   null cuando el catálogo declara una vista que nadie construye (el llamador lo
 *   avisa en consola en vez de pintar un hueco vacío).
 */
export function createVisual(visualId, controls, options = {}) {
  if (visualId === 'amp-envelope') return createEnvelopeCurve({ controls });

  // Las 4 rutas de la matriz: sus 12 controles viven en el cajón, así que en el
  // lienzo va el resumen (y el cajón da el detalle).
  if (visualId === 'mod-summary') return createModSummary({ controls });

  // Las ranuras A–D: no son parámetros (por eso `controls` viene vacío), lo que
  // necesitan es el camino de carga, que es del store/host.
  if (visualId === 'model-slots') return createModelSlots({ onLoad: options.onLoad ?? null });

  return null;
}
