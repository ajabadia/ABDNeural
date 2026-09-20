/**
 * El pad XY del bloque MODEL: morphX/morphY dibujados, con los nombres de los
 * modelos A–D en las esquinas (A arriba-izquierda ... D abajo-derecha, igual
 * que pinta el XYPad nativo con `setModelNames`).
 *
 * El COMPONENTE es el compartido (`@abdsynths/shared` XYPad: superficie
 * absoluta, y=1 arriba, setValue silencioso para snapshots, esquinas con
 * `setCorners`). Aquí solo vive el wiring de NEURONiK:
 *
 *   - morphX/morphY son DOS parámetros del contrato que el pad edita a la vez,
 *     así que un gesto suyo son DOS gestos coordinados (uno por eje) con su
 *     fase completa: `begin` con el valor actual, `change` en cada movimiento,
 *     `end` al soltar. Un paso de teclado no abre gesto: viaja como `end`,
 *     que es como el store cierra un toggle.
 *   - los nombres vienen por el puente (`state.models`, la misma fuente de las
 *     ranuras): esquina con nombre = ranura cargada; ranura vacía o `EMPTY`,
 *     esquina oculta.
 *   - mientras se arrastra, un snapshot del host NO mueve el pulgar: no se
 *     pelea con el dedo del usuario (el valor de verdad llega al soltar).
 */

import { XYPad } from '@abdsynths/shared/components';

import { displayableName } from './modelSlots.js';

const MORPH_IDS = { x: 'morphX', y: 'morphY' };

/**
 * @param {object} [options]
 * @param {(id: 'morphX'|'morphY', normalized: number,
 *          phase: 'begin'|'change'|'end') => void} [options.onEdit]
 *   empuja una edición al store (`pushParameter`): la fábrica la inyecta para
 *   que el panel siga sin saber qué dibuja cada vista.
 * @returns {{ element: HTMLElement, pad: object, paint: Function, destroy: Function }}
 *   `pad` es la instancia compartida (handle de inspección para tests).
 */
export function createXyPad({ onEdit = null } = {}) {
  const element = document.createElement('div');
  element.className = 'xy-pad';

  let dragging = false;

  const pad = new XYPad(element, {
    x: 0,
    y: 0,
    width: 200,
    height: 150,
    onDragStart() {
      dragging = true;
      const { x, y } = pad.getValue();

      onEdit?.(MORPH_IDS.x, x, 'begin');
      onEdit?.(MORPH_IDS.y, y, 'begin');
    },
    onChange({ x, y }) {
      if (dragging) {
        onEdit?.(MORPH_IDS.x, x, 'change');
        onEdit?.(MORPH_IDS.y, y, 'change');
      } else {
        // Edición sin gesto (teclado): solo el eje que de verdad cambió respecto
        // a lo que el cable ya sabe (`last` lo actualizan también los snapshots).
        if (x !== last.x) onEdit?.(MORPH_IDS.x, x, 'end');
        if (y !== last.y) onEdit?.(MORPH_IDS.y, y, 'end');
      }

      last = { x, y };
    },
    onDragEnd() {
      dragging = false;
      const { x, y } = pad.getValue();

      onEdit?.(MORPH_IDS.x, x, 'end');
      onEdit?.(MORPH_IDS.y, y, 'end');
      last = { x, y };
    },
  });

  // Lo que el cable ya sabe: el valor de arranque del pad y, desde entonces,
  // también lo que llegue pintado (los snapshots nativos no hay que reenviarlos).
  let last = pad.getValue();

  let cornerKey = null;

  function paintCorners(models) {
    const names = [0, 1, 2, 3].map((slot) => {
      const entry = Array.isArray(models)
        ? models.find((candidate) => candidate?.slot === slot)
        : null;

      return displayableName(entry) ?? '';
    });

    // Repintar solo si el reparto de nombres cambió: `paint` corre con cada
    // snapshot y reconstruir cuatro spans sería basura de DOM por tick.
    const key = names.join('\u0000');

    if (key === cornerKey) return;

    cornerKey = key;
    pad.setCorners(names);
  }

  return {
    element,
    pad,

    /**
     * Repinta desde el snapshot: valor de los dos morph (silencioso: nada de
     * eco) y nombres de ranura en las esquinas.
     */
    paint(parameters, state) {
      paintCorners(state?.models ?? null);

      if (dragging) return;

      const value = {
        x: parameters?.morphX ?? 0,
        y: parameters?.morphY ?? 0,
      };

      pad.setValue(value);
      last = value;
    },

    destroy() {
      pad.destroy();
      element.textContent = '';
    },
  };
}
