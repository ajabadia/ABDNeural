/**
 * La curva ADSR de las envolventes (ENV 1 y ENV 2). El destino de cada una
 * NO vive aqui: lo declara su ruta en la MATRIZ.
 *
 * Es una VISTA, no un control: no tiene parámetro propio, solo dibuja la
 * envolvente a partir de sus cuatro controles (`envAttack`/... o
 * `filterAttack`/...). Por eso vive fuera de `controls.js`: la familia
 * compartida no la puede dar porque no sabe de qué parámetros se trata, y el
 * panel no la conoce porque su contrato es genérico (recibe `visual.element` y
 * lo pinta, igual que recibe celdas).
 *
 * 8.3 separó FILTRO/ENVOLVENTES y con ello nació la de AMP; la ficha
 * ENVOLVENTES de hoy pinta DOS (una por ADSR, sobre su bloque de controles del
 * cajón). La fábrica se PARAMETRIZA por prefijo de id y por identidad (dataset,
 * etiqueta), de modo que las dos son el mismo dibujo y el mismo comportamiento
 * — dos copias no: una sola vista, dos instancias.
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

/** Orden de los tramos y sufijo de id de cada uno dentro del prefijo. */
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

/**
 * `d` de la AGUJA de nivel: una linea horizontal a la altura del valor que la
 * envolvente esta sacando AHORA (frame.envelopes[amp|filter], 0..1). El frame
 * trae el NIVEL, no la fase dentro de la curva, asi que una linea a lo ancho es
 * la representacion honesta: donde esta la señal, no donde "iria" el trazo.
 * Por debajo de `NEEDLE_FLOOR` se considera silencio y la aguja se esconde.
 */
export const NEEDLE_FLOOR = 0.004;

export function envelopeNeedlePath(level, viewbox = ENVELOPE_VIEWBOX) {
  const top = PAD;
  const bottom = viewbox.height - PAD;
  const clamped = Math.min(1, Math.max(0, Number(level) || 0));
  const y = bottom - clamped * (bottom - top);

  return `M0,${y.toFixed(2)} L${viewbox.width},${y.toFixed(2)}`;
}

/**
 * Identidades de las dos ADSR: prefijo de id en el contrato y dataset visual.
 * SSOT de qué es cada envolvente — la ficha y las vistas leen de aquí, nadie
 * repite los ids a mano.
 */
export const ENVELOPE_IDENTITIES = [
  {
    id: 'amp',
    prefix: 'env',
    dataset: 'amp-envelope',
    title: 'ENV 1 · amplitud',
    aria: 'Curva ADSR de la envolvente de amplitud',
  },
  {
    id: 'filter',
    prefix: 'filter',
    dataset: 'filter-envelope',
    title: 'Curva de la envolvente',
    aria: 'Curva ADSR de la envolvente del filtro',
  },
];

/** La identidad de un prefijo de id (`envAttack` -> `env`). */
export function envelopeIdentityForPrefix(prefix) {
  return ENVELOPE_IDENTITIES.find((identity) => identity.prefix === prefix) ?? null;
}

const SVG_NS = 'http://www.w3.org/2000/svg';

/**
 * Construye la vista de UNA curva.
 *
 * @param {object} options
 * @param {object[]} options.controls  view-models del contrato: los cuatro del prefijo
 * @param {string} options.prefix      prefijo de id (`env` | `filter`)
 * @param {string} [options.dataset]   valor de `data-visual` (amp-envelope | filter-envelope)
 * @param {string} [options.label]     etiqueta bajo el dibujo
 * @param {string} [options.title]     tooltip de la vista
 * @param {string} [options.aria]      etiqueta accesible del SVG
 * @param {{width?: number, height?: number}} [options.viewbox]
 * @returns {{ element: HTMLElement, paint: Function, parameterIds: string[], destroy: Function }}
 */
export function createEnvelopeCurve({
  controls,
  prefix = 'env',
  dataset = 'amp-envelope',
  label = 'AMP ENV',
  title = 'Curva de la envolvente (los cuatro controles de su bloque)',
  aria = 'Curva ADSR de la envolvente de amplitud',
  viewbox = ENVELOPE_VIEWBOX,
}) {
  /** Los cuatro tramos por nombre (`attack` -> control de `envAttack`). */
  const bySegment = ENVELOPE_SEGMENTS.map((segment) => ({
    segment,
    control: controls.find((control) => control.id === `${prefix}${segment[0].toUpperCase()}${segment.slice(1)}`),
  }));

  // OJO con la clase: NO lleva `.cell`. Una celda de `.cell` es un PARÁMETRO, y
  // hay tests (y el informe de paridad) que cuentan 70. La vista ocupa una pista de
  // la rejilla por CSS, pero no se disfraza de control.
  const element = document.createElement('div');
  element.className = 'card__visual';
  element.dataset.visual = dataset;
  element.title = title;

  const svg = document.createElementNS(SVG_NS, 'svg');
  svg.setAttribute('viewBox', `0 0 ${viewbox.width} ${viewbox.height}`);
  svg.setAttribute('preserveAspectRatio', 'none');
  svg.setAttribute('role', 'img');
  svg.setAttribute('aria-label', aria);

  const area = document.createElementNS(SVG_NS, 'path');
  area.setAttribute('class', 'envelope-curve__area');

  const line = document.createElementNS(SVG_NS, 'path');
  line.setAttribute('class', 'envelope-curve__line');

  // Aguja de nivel (telemetria en vivo): oculta hasta que llega el primer
  // frame con nivel por encima del suelo.
  const needle = document.createElementNS(SVG_NS, 'path');
  needle.setAttribute('class', 'envelope-curve__level');
  needle.dataset.visible = 'false';

  svg.append(area, line, needle);

  const caption = document.createElement('span');
  caption.className = 'cell__label';
  caption.textContent = label;

  element.append(svg, caption);

  /** Nivel actual de la envolvente (0..1, del frame de telemetria). */
  function setLevel(level) {
    const value = typeof level === 'number' && Number.isFinite(level) ? level : 0;

    if (value <= NEEDLE_FLOOR) {
      needle.dataset.visible = 'false';
      return;
    }

    needle.dataset.visible = 'true';
    needle.setAttribute('d', envelopeNeedlePath(value, viewbox));
  }

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
    setLevel,
    parameterIds: bySegment.filter(({ control }) => control).map(({ control }) => control.id),
    destroy() {
      element.textContent = '';
    },
  };
}
