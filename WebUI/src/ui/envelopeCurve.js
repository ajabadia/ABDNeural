/**
 * La curva ADSR de la ficha FILTRO & ENVOLVENTE.
 *
 * Es una VISTA, no un control: no tiene parámetro propio, solo dibuja la
 * envolvente de amplitud a partir de los cuatro que ya están en el lienzo
 * (`envAttack`/`envDecay`/`envSustain`/`envRelease`). Por eso vive fuera de
 * `controls.js`: la familia compartida no la puede dar porque no sabe de qué
 * parámetros se trata, y el panel no la conoce porque su contrato es genérico
 * (recibe `visual.element` y lo pinta, igual que recibe celdas).
 *
 * Ocupa la celda libre de su ficha (11 controles en una rejilla de 6x2), así que
 * añadirla NO cambia la geometría: el encaje de `tests/sections.test.js` sigue
 * contando la misma rejilla.
 *
 * Dos decisiones que no son obvias:
 *
 *   - **Los tiempos se comprimen con √**: el rango real va de 1 ms a 5 s (y el
 *     `skew` de los parámetros ya es 0.5, es decir logarítmico). En escala lineal
 *     un ataque de 1 ms sería un trazo de medio píxel. La raíz cuadrada mantiene
 *     legibles los dos extremos sin mentir sobre el orden;
 *   - **el tramo de sostenido tiene ancho propio** (no es un tiempo): sin él, un
 *     `sustain` sin ataque ni release parecería una meseta de ancho cero.
 */

import { realFromNormalized } from '../contracts/paramValue.js';

/** Orden de los tramos y sufijo de id de cada uno dentro de `env*`. */
export const ENVELOPE_SEGMENTS = ['attack', 'decay', 'sustain', 'release'];

/** Geometría del viewBox. El CSS estira el SVG a la celda. */
export const ENVELOPE_VIEWBOX = { width: 100, height: 48 };

const PAD = 2;                 // margen en px del viewBox, para que el trazo no se corte
const SUSTAIN_SHARE = 0.18;    // ancho del tramo de sostenido, en fracción del total
const MIN_SEGMENT_SHARE = 0.05; // ninguna rampa se queda en nada (1 ms tiene que verse)

/**
 * Puntos de la curva, en coordenadas del viewBox (y crece hacia ABAJO, como el SVG).
 *
 * @param {{attack: number, decay: number, sustain: number, release: number}} values
 *        tiempos en SEGUNDOS y sostenido 0..1, en unidades reales.
 * @param {{width?: number, height?: number}} [viewbox]
 * @returns {{x: number, y: number}[]}  start, pico, fin de decay, fin de sostenido, fin de release
 */
export function envelopePoints(values, viewbox = ENVELOPE_VIEWBOX) {
  const { width, height } = viewbox;
  const top = PAD;
  const bottom = height - PAD;
  const y = (level) => bottom - Math.min(1, Math.max(0, level)) * (bottom - top);

  const attack = Math.max(0, Number(values.attack) || 0);
  const decay = Math.max(0, Number(values.decay) || 0);
  const release = Math.max(0, Number(values.release) || 0);
  const sustain = Math.min(1, Math.max(0, Number(values.sustain) || 0));

  // √ normalizado: el mayor de los tres tiempos ocupa (1 - sostenido) del ancho.
  const longest = Math.max(attack, decay, release, 1e-6);
  const rampSpace = width * (1 - SUSTAIN_SHARE);
  const shareOf = (seconds) => Math.max(MIN_SEGMENT_SHARE, Math.sqrt(seconds / longest)) * rampSpace;

  const shares = [attack, decay, release].map(shareOf);
  const total = shares[0] + shares[1] + shares[2];
  const scale = total > rampSpace ? rampSpace / total : 1;   // nunca desborda el viewBox

  const xAttack = shares[0] * scale;
  const xDecay = xAttack + shares[1] * scale;
  const xSustain = xDecay + width * SUSTAIN_SHARE;
  const xRelease = xSustain + shares[2] * scale;

  return [
    { x: 0, y: y(0) },
    { x: xAttack, y: y(1) },
    { x: xDecay, y: y(sustain) },
    { x: xSustain, y: y(sustain) },
    { x: xRelease, y: y(0) },
  ];
}

/** `d` del trazo, con dos decimales (un `d` estable no repinta por ruido de float). */
export function envelopeLinePath(points) {
  const at = (point) => `${point.x.toFixed(2)},${point.y.toFixed(2)}`;

  return `M${at(points[0])} ` + points.slice(1).map((point) => `L${at(point)}`).join(' ');
}

/** `d` del área rellena bajo la curva (cierra por la base del viewBox). */
export function envelopeAreaPath(points, viewbox = ENVELOPE_VIEWBOX) {
  const base = (viewbox.height - PAD).toFixed(2);

  return `${envelopeLinePath(points)} L${points[points.length - 1].x.toFixed(2)},${base} L0.00,${base} Z`;
}

const SVG_NS = 'http://www.w3.org/2000/svg';

/**
 * Construye la vista de la curva.
 *
 * @param {object} options
 * @param {object[]} options.controls  view-models del contrato: los cuatro `env*`
 * @param {{width?: number, height?: number}} [options.viewbox]
 * @returns {{ element: HTMLElement, paint: Function, parameterIds: string[], destroy: Function }}
 */
export function createEnvelopeCurve({ controls, viewbox = ENVELOPE_VIEWBOX }) {
  /** Los cuatro tramos por nombre (`attack` -> control de `envAttack`). */
  const bySegment = ENVELOPE_SEGMENTS.map((segment) => ({
    segment,
    control: controls.find((control) => control.id === `env${segment[0].toUpperCase()}${segment.slice(1)}`),
  }));

  // OJO con la clase: NO lleva `.cell`. Una celda de `.cell` es un PARÁMETRO, y
  // hay tests (y el informe de paridad) que cuentan 70. La vista ocupa una pista de
  // la rejilla por CSS, pero no se disfraza de control.
  const element = document.createElement('div');
  element.className = 'card__visual';
  element.dataset.visual = 'amp-envelope';
  element.title = 'Curva de la envolvente de amplitud (los cuatro controles de la ficha)';

  const svg = document.createElementNS(SVG_NS, 'svg');
  svg.setAttribute('viewBox', `0 0 ${viewbox.width} ${viewbox.height}`);
  svg.setAttribute('preserveAspectRatio', 'none');
  svg.setAttribute('role', 'img');
  svg.setAttribute('aria-label', 'Curva ADSR de la envolvente de amplitud');

  const area = document.createElementNS(SVG_NS, 'path');
  area.setAttribute('class', 'envelope-curve__area');

  const line = document.createElementNS(SVG_NS, 'path');
  line.setAttribute('class', 'envelope-curve__line');

  svg.append(area, line);

  const label = document.createElement('span');
  label.className = 'cell__label';
  label.textContent = 'AMP ENV';

  element.append(svg, label);

  /** Repinta desde los valores NORMALIZADOS del snapshot del store. */
  function paint(parameters = {}) {
    const values = {};

    for (const { segment, control } of bySegment)
      values[segment] = control
        ? realFromNormalized(control, parameters[control.id] ?? 0)
        : 0;

    const points = envelopePoints(values, viewbox);

    line.setAttribute('d', envelopeLinePath(points));
    area.setAttribute('d', envelopeAreaPath(points, viewbox));
  }

  paint();   // estado inicial: los defaults del contrato

  return {
    element,
    paint,
    parameterIds: bySegment.filter(({ control }) => control).map(({ control }) => control.id),
    destroy() {
      element.textContent = '';
    },
  };
}
