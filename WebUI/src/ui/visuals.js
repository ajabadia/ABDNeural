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
 * Una vista es DATO de ficha (no tiene parámetro propio ni celda): la página
 * compone, el panel solo le da sitio y estado. Los handlers que NO son dibujo
 * (cargar una ranura, editar el pad) viajan por `options`, que es quien los
 * conecta con el store.
 *
 * Las vistas no son lo mismo por dentro y no se disimula: la curva y el resumen
 * son DIBUJOS (se repintan con los parámetros); las ranuras PIDEN (su botón lanza
 * una carga en el host); el pad EDITA (dos morphs con gesto completo). Por eso
 * reciben handlers por `options` y por eso `paint` recibe el estado entero, del
 * que cada una lee lo suyo.
 */

import { createEnvelopeCurve } from './envelopeCurve.js';
import { createSpectral } from './spectral.js';
import { createModelSlots } from './modelSlots.js';
import { createModSummary } from './modSummary.js';
import { createXyPad } from './xyPad.js';

/**
 * @param {string} visualId  id del catálogo SECTION_VISUALS
 * @param {object[]} controls  view-models de los parámetros que pide esa vista
 * @param {object} [options]
 * @param {(slot: number) => void} [options.onLoad]  carga de una ranura de modelo
 *   (`model-slots`): la ejecuta el host, así que la vista la recibe por aquí y no
 *   la pide al panel, que no sabe qué dibuja.
 * @param {(id: 'morphX'|'morphY', normalized: number,
 *          phase: 'begin'|'change'|'end') => void} [options.onEdit]  edición del
 *   pad XY (`model-slots`): el store la cierra con su protocolo de gestos.
 * @param {(notify: Function) => Function} [options.onTelemetry]  canal de
 *   telemetría en vivo (`model-slots`): lo consume el espectral; es pintura,
 *   no estado, así que va por su propio camino y no por el paint de snapshots.
 * @returns {{ element: HTMLElement, paint: Function, destroy?: Function }|null}
 *   null cuando el catálogo declara una vista que nadie construye (el llamador lo
 *   avisa en consola en vez de pintar un hueco vacío).
 */
export function createVisual(visualId, controls, options = {}) {
  if (visualId === 'amp-envelope') return createEnvelopeCurve({ controls });

  // Las 4 rutas de la matriz: sus 12 controles viven en el cajón, así que en el
  // lienzo va el resumen (y el cajón da el detalle).
  if (visualId === 'mod-summary') return createModSummary({ controls });

  // La ficha MODELOS A–D es COMPUESTA: el pad XY dibujado (morphX/morphY con los
  // nombres en las esquinas) encima de las cuatro ranuras. El bloque MODEL del
  // panel nativo era exactamente esto: un pad y sus cuatro cargas. Ambas mitades
  // leen lo suyo del MISMO paint (parámetros + modelsState).
  if (visualId === 'model-slots') {
    const slots = createModelSlots({ onLoad: options.onLoad ?? null });
    const pad = createXyPad({ onEdit: options.onEdit ?? null });
    const spectral = createSpectral({ onFrame: options.onTelemetry ?? null });

    const element = document.createElement('div');
    element.className = 'model-block';
    element.append(pad.element, spectral.element, slots.element);

    return {
      element,
      paint(parameters, state) {
        pad.paint(parameters, state);
        // El espectral NO pinta con snapshots: sus barras viven en el canal de
        // telemetría y un snapshot no debe congelarlas.
        slots.paint(parameters, state);
      },
      destroy() {
        spectral.destroy();
        pad.destroy();
        slots.destroy();
      },
    };
  }

  return null;
}
