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
 *
 * ANILLO MORPH-Z (FASE 10.3, el tercer eje del Neuron): un aro SVG circular
 * centrado sobre el pad — overlay propio de esta página, el componente
 * compartido no se toca (su contrato lo pinean SUS tests). Convenciones:
 *
 *   - el ángulo ES el valor: 0 en las 12 (top), horario, 1 = círculo completo
 *     (z es un eje CÍCLICO temporal — LFO2 lo anima en bucle — no un sweep de
 *     knob de 270°);
 *   - un drag del aro es UN gesto de morphZ con fase completa (begin/change/
 *     end), coordinado igual que los ejes del pad; los hit-areas son solo los
 *     trazos (`pointer-events: stroke`): el interior sigue siendo del pad;
 *   - teclado: role=slider con flechas (±0.01), PageUp/Down (±0.1) y
 *     Home/End (0/1), cada paso viaja como `end` y SOLO si cambió;
 *   - paint pinta el aro sin eco y, durante un drag del aro, un snapshot del
 *     host no lo mueve (misma regla que el pulgar del pad).
 */

import { XYPad } from '@abdsynths/shared/components';

import { displayableName } from './modelSlots.js';

const MORPH_IDS = { x: 'morphX', y: 'morphY', z: 'morphZ' };

const Z_STEP = 0.01;
const Z_PAGE = 0.1;

/** aro SVG: track + hit (trazo ancho invisible) + fill con dash por pathLength */
function ringSvg() {
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', '0 0 100 100');
  svg.innerHTML = `
    <circle class="zring-track" cx="50" cy="50" r="47"></circle>
    <circle class="zring-hit"   cx="50" cy="50" r="47"></circle>
    <circle class="zring-fill"  cx="50" cy="50" r="47" pathLength="100"
            transform="rotate(-90 50 50)"></circle>`;
  return svg;
}

/**
 * @param {object} [options]
 * @param {(id: 'morphX'|'morphY'|'morphZ', normalized: number,
 *          phase: 'begin'|'change'|'end') => void} [options.onEdit]
 *   empuja una edición al store (`pushParameter`): la fábrica la inyecta para
 *   que el panel siga sin saber qué dibuja cada vista.
 * @returns {{ element: HTMLElement, pad: object, paint: Function, destroy: Function }}
 *   `pad` es la instancia compartida (handle de inspección para tests).
 */
export function createXyPad({ onEdit = null } = {}) {
  const element = document.createElement('div');
  element.className = 'xy-pad';
  // Consultable como vista de ficha (el espectral y las ranuras lo hacen).
  element.dataset.visual = 'model-xy';

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

  // ------------------------- anillo morph-Z -------------------------------

  let zValue = 0;
  let zDragging = false;

  const ring = document.createElement('div');
  ring.className = 'xy-pad__zring';
  ring.tabIndex = 0;
  ring.setAttribute('role', 'slider');
  ring.setAttribute('aria-label', 'Morph Z');
  ring.setAttribute('aria-valuemin', '0');
  ring.setAttribute('aria-valuemax', '1');
  ring.setAttribute('aria-valuenow', '0');
  ring.setAttribute('aria-valuetext', 'Morph Z 0%');

  const svg = ringSvg();
  ring.appendChild(svg);
  element.appendChild(ring);

  const zFill = svg.querySelector('.zring-fill');

  function renderZ() {
    zFill.setAttribute('stroke-dasharray', `${zValue * 100} 100`);
    ring.setAttribute('aria-valuenow', `${zValue}`);
    ring.setAttribute('aria-valuetext', `Morph Z ${Math.round(zValue * 100)}%`);
  }

  /** silencioso (snapshots) o con eco (ediciones propias) */
  function setZ(value, notify = false) {
    zValue = Math.min(1, Math.max(0, Number.isFinite(value) ? value : 0));
    renderZ();

    if (notify) onEdit?.(MORPH_IDS.z, zValue, 'end');
  }

  /** ángulo -> valor: 0 en las 12, horario, 1 = vuelta completa. */
  function applyRingPointer(event) {
    const rect = svg.getBoundingClientRect();
    const dx = event.clientX - (rect.left + rect.width / 2);
    const dy = event.clientY - (rect.top + rect.height / 2);

    let v = Math.atan2(dx, -dy) / (2 * Math.PI);
    if (v < 0) v += 1;

    setZ(v);
    onEdit?.(MORPH_IDS.z, zValue, 'change');
  }

  svg.addEventListener('pointerdown', (event) => {
    zDragging = true;
    try { svg.setPointerCapture(event.pointerId); } catch { /* jsdom */ }
    ring.focus({ preventScroll: true });

    // `begin` anuncia el gesto con el valor ACTUAL (protocolo del store);
    // el salto angular de este pointerdown viaja como `change`.
    onEdit?.(MORPH_IDS.z, zValue, 'begin');
    applyRingPointer(event);
  });

  svg.addEventListener('pointermove', (event) => {
    if (zDragging) applyRingPointer(event);
  });

  const endZDrag = () => {
    if (!zDragging) return;
    zDragging = false;
    onEdit?.(MORPH_IDS.z, zValue, 'end');
  };

  svg.addEventListener('pointerup', endZDrag);
  svg.addEventListener('pointercancel', endZDrag);

  ring.addEventListener('keydown', (event) => {
    const moves = {
      ArrowUp: Z_STEP,
      ArrowRight: Z_STEP,
      ArrowDown: -Z_STEP,
      ArrowLeft: -Z_STEP,
      PageUp: Z_PAGE,
      PageDown: -Z_PAGE,
    };

    if (event.key in moves) {
      setZ(zValue + moves[event.key]);
    } else if (event.key === 'Home') {
      setZ(0);
    } else if (event.key === 'End') {
      setZ(1);
    } else {
      return;
    }

    // Paso de teclado = edición cerrada (`end`), solo si cambió de verdad
    // (lastZ se actualiza aquí: un segundo End ya no viaja).
    if (zValue !== lastZ) {
      lastZ = zValue;
      onEdit?.(MORPH_IDS.z, zValue, 'end');
    }
    event.preventDefault();
  });

  // Lo que el cable ya sabe del eje z (dedupe del teclado / sin eco de paint).
  let lastZ = zValue;

  renderZ();

  // ------------------------------ retorno ---------------------------------

  return {
    element,
    pad,

    /**
     * Repinta desde el snapshot: valor de los dos morph (silencioso: nada de
     * eco), el del anillo z, y nombres de ranura en las esquinas.
     */
    paint(parameters, state) {
      paintCorners(state?.models ?? null);

      if (!zDragging) {
        setZ(parameters?.morphZ ?? 0);
        lastZ = zValue;
      }

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
