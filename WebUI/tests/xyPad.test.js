/**
 * El pad XY de la ficha MODELOS: wiring de NEURONiK sobre el XYPad compartido
 * (`@abdsynths/shared`, con SUS tests propios para el contrato del componente).
 * Aquí se prueba lo que añade esta página:
 *
 *   - un gesto del pad son DOS gestos coordinados (morphX/morphY) con fase
 *     completa begin/change/end hacia el store;
 *   - una edición sin gesto (teclado) viaja como `end` y SOLO el eje que cambió;
 *   - paint repinta sin eco y no pega con el dedo durante un drag;
 *   - los nombres A–D llegan a las esquinas desde `state.models` (misma fuente
 *     que las ranuras, misma convención EMPTY);
 *   - la vista compuesta (pad + ranuras) sale de la fábrica como UNA vista.
 */

import { beforeEach, describe, expect, it, vi } from 'vitest';

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { createVisual } from '../src/ui/visuals.js';
import { createXyPad } from '../src/ui/xyPad.js';

import { MOD_DESTINATIONS } from '../generated/parameters.generated.js';

const here = dirname(fileURLToPath(import.meta.url));
const read = (...segments) => readFileSync(join(here, ...segments), 'utf8');

/** El bloque de una regla de la hoja de estilos (selector -> sus declaraciones). */
function cssBlock (styles, selector) {
  const start = styles.indexOf(`${selector} {`);
  if (start < 0) return '';

  return styles.slice(start, styles.indexOf('}', start));
}

function makeHost() {
  const host = document.createElement('div');
  document.body.appendChild(host);
  return host;
}

/** jsdom no tiene layout: se le da al pad un rect determinista. */
function stubRect(padElement, width = 200, height = 100) {
  padElement.getBoundingClientRect = () =>
    ({ left: 0, top: 0, right: width, bottom: height, width, height });
}

function pointer(type, x, y) {
  return new PointerEvent(type, { clientX: x, clientY: y, bubbles: true, pointerId: 1 });
}

describe('xyPad / wiring de morphX-morphY', () => {
  let host;

  beforeEach(() => { host = makeHost(); });

  it('monta el XYPad compartido dentro de su contenedor', () => {
    const view = createXyPad({});
    host.append(view.element);

    expect(view.element.querySelector('.abd-xypad__pad')).not.toBeNull();
    // y=1 arriba: el valor de partida es la esquina del modelo A (morph a 0).
    expect(view.pad.getValue()).toEqual({ x: 0, y: 0 });

    view.destroy();
    expect(host.querySelector('.abd-xypad')).toBeNull();
  });

  it('un paso de teclado viaja como END y solo en el eje que cambió', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    view.element.querySelector('.abd-xypad__pad').dispatchEvent(
      new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }));

    // x = 0 + step (0.01); morphY no cambió: no viaja.
    expect(onEdit.mock.calls).toEqual([
      ['morphX', 0.01, 'end'],
    ]);

    view.destroy();
  });

  it('un drag son dos gestos coordinados: begin/change/end por eje', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    const padElement = view.element.querySelector('.abd-xypad__pad');
    stubRect(padElement);
    padElement.dispatchEvent(pointer('pointerdown', 0, 0));
    padElement.dispatchEvent(pointer('pointermove', 100, 50));
    padElement.dispatchEvent(pointer('pointerup', 100, 50));

    // begin anuncia el gesto con el valor ACTUAL (como handleGesture); el
    // pointerdown salta a la esquina (0, 1) en la misma tick que el begin.
    expect(onEdit.mock.calls).toEqual([
      ['morphX', 0, 'begin'],
      ['morphY', 0, 'begin'],
      ['morphX', 0, 'change'],
      ['morphY', 1, 'change'],
      ['morphX', 0.5, 'change'],
      ['morphY', 0.5, 'change'],
      ['morphX', 0.5, 'end'],
      ['morphY', 0.5, 'end'],
    ]);

    view.destroy();
  });

  it('arrastre fino: Shift = movimiento relativo a 1/10, sin salto al entrar ni al soltar', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    const padElement = view.element.querySelector('.abd-xypad__pad');
    stubRect(padElement, 200, 100);

    // Drag normal a (0.5, 0.5): salto absoluto de siempre.
    padElement.dispatchEvent(pointer('pointerdown', 100, 50));
    expect(view.pad.getValue()).toEqual({ x: 0.5, y: 0.5 });

    // Primer Shift+move: ANCLA el modo fino (valor actual con puntero actual) —
    // cero salto al entrar, aunque la posición del puntero haya cambiado.
    padElement.dispatchEvent(new PointerEvent('pointermove', {
      clientX: 115, clientY: 50, bubbles: true, shiftKey: true, pointerId: 1 }));
    expect(view.pad.getValue()).toEqual({ x: 0.5, y: 0.5 });

    // +30 px de arrastre fino: 0.15 normalizado * 0.1 = +0.015 (relativo),
    // NO 0.725 (que sería el salto absoluto del compartido).
    padElement.dispatchEvent(new PointerEvent('pointermove', {
      clientX: 145, clientY: 50, bubbles: true, shiftKey: true, pointerId: 1 }));
    expect(view.pad.getValue().x).toBeCloseTo(0.515, 5);
    expect(view.pad.getValue().y).toBeCloseTo(0.5, 5);

    // Volver al ancla: valor base exacto (relativo al anclaje, no compone).
    padElement.dispatchEvent(new PointerEvent('pointermove', {
      clientX: 115, clientY: 50, bubbles: true, shiftKey: true, pointerId: 1 }));
    expect(view.pad.getValue().x).toBeCloseTo(0.5, 5);

    // Soltar y mover SIN shift: salto absoluto normal (el gesto fino murió).
    padElement.dispatchEvent(pointer('pointerup', 130, 50));
    padElement.dispatchEvent(pointer('pointerdown', 0, 0));
    padElement.dispatchEvent(pointer('pointerup', 0, 0));
    expect(view.pad.getValue()).toEqual({ x: 0, y: 1 });

    // El fine drag es un GESTO normal para el store: begin/change/end completos.
    const phases = onEdit.mock.calls.filter(([id]) => id === 'morphX').map(([, , phase]) => phase);
    expect(phases[0]).toBe('begin');
    expect(phases).toContain('change');
    expect(phases.at(-1)).toBe('end');

    view.destroy();
  });

  it('sin Shift el salto absoluto pasa intacto (el fino no interfiere)', () => {
    const view = createXyPad({});
    host.append(view.element);

    const padElement = view.element.querySelector('.abd-xypad__pad');
    stubRect(padElement, 200, 100);

    // pointerdown + move SIN shift: el compartido aplica su salto absoluto.
    padElement.dispatchEvent(pointer('pointerdown', 40, 20));
    padElement.dispatchEvent(pointer('pointermove', 180, 80));
    expect(view.pad.getValue().x).toBeCloseTo(0.9, 5);
    expect(view.pad.getValue().y).toBeCloseTo(0.2, 5);

    view.destroy();
  });

  it('paint repinta sin eco y no pega con el dedo durante un drag', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    view.paint({ morphX: 0.25, morphY: 0.75 }, {});

    expect(view.pad.getValue()).toEqual({ x: 0.25, y: 0.75 });
    expect(onEdit).not.toHaveBeenCalled();           // setValue silencioso

    // Drag en marcha: un snapshot del host no mueve el pulgar.
    const padElement = view.element.querySelector('.abd-xypad__pad');
    stubRect(padElement);
    padElement.dispatchEvent(pointer('pointerdown', 200, 0));
    view.paint({ morphX: 0, morphY: 0 }, {});

    expect(view.pad.getValue()).toEqual({ x: 1, y: 1 });

    view.destroy();
  });

  it('el readout X/Y % vive FUERA del pad (fila espejo) y sigue los tres caminos de render', () => {
    const view = createXyPad({});
    host.append(view.element);

    const mirror = view.element.querySelector('.xy-pad__readout-row span');
    const internal = view.element.querySelector('.abd-xypad__readout');

    expect(mirror).not.toBeNull();
    expect(internal).not.toBeNull();

    // Arranque: el espejo nace con el texto del readout interno.
    expect(mirror.textContent).toBe(internal.textContent);

    // 1) setValue silencioso (los snapshots): render() repinta el interno y el
    //    espejo lo sigue — sin gesto, sin eco al store.
    view.paint({ morphX: 0.25, morphY: 0.75 }, {});
    expect(mirror.textContent).toBe(internal.textContent);
    expect(mirror.textContent).toContain('25%');
    expect(mirror.textContent).toContain('75%');

    // 2) teclado (edición sin gesto): el render del pad actualiza el espejo.
    view.element.querySelector('.abd-xypad__pad').dispatchEvent(
      new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }));
    expect(mirror.textContent).toBe(internal.textContent);
    expect(mirror.textContent).toContain('26%');

    view.destroy();
  });

  it('los nombres A–D llegan a las esquinas desde state.models', () => {
    const view = createXyPad({});
    host.append(view.element);
    const paintModels = (models) => view.paint({ morphX: 0, morphY: 0 }, { models });
    const corner = (pos) =>
      view.element.querySelector(`[data-corner="${pos}"]`)?.textContent ?? null;

    paintModels([
      { slot: 0, name: 'Piano', isValid: true },
      { slot: 1, name: 'EMPTY', isValid: true },
      { slot: 2, name: 'Bell', isValid: false },
      { slot: 3, name: 'Glass', isValid: true },
    ]);

    expect(corner('tl')).toBe('Piano');   // ranura A cargada
    expect(corner('tr')).toBeNull();      // EMPTY = ranura vacía: esquina oculta
    expect(corner('bl')).toBe('Bell');    // divergente se MARCA, no se oculta
    expect(corner('br')).toBe('Glass');

    // El estado de las esquinas es el MISMO que el de las ranuras del cajón:
    // data-divergent (el color lo pone la CSS de la página con --color-warning).
    const divergentCorner = view.element.querySelector('[data-corner="bl"]');
    expect(divergentCorner.dataset.divergent).toBe('true');
    expect(divergentCorner.title).toContain('no ha podido cargar');
    expect(view.element.querySelector('[data-corner="tl"]').dataset.divergent).toBe('false');
    expect(view.element.querySelector('[data-corner="br"]').dataset.divergent).toBe('false');

    // Sin modelos (modo local, snapshot sin modelsState): sin esquinas.
    paintModels(null);
    expect(view.element.querySelector('.abd-xypad__corner')).toBeNull();

    view.destroy();
  });
});

describe('xyPad / las dos vistas de MODELOS (mudanza al centro, 8.3)', () => {
  let host;

  beforeEach(() => { host = makeHost(); });

  it('model-xy: SOLO el pad en el lienzo (la ficha del centro)', () => {
    const onEdit = vi.fn();
    const view = createVisual('model-xy', [], { onEdit });
    host.append(view.element);

    // La vista del lienzo es el pad pelado: ni ranuras ni espectral.
    expect(view.element.dataset.visual).toBe('model-xy');
    expect(view.element.querySelectorAll('.model-slots__row')).toHaveLength(0);
    expect(view.element.querySelector('.abd-xypad__pad')).not.toBeNull();

    // El mismo paint del interface mueve las esquinas y edita los morph...
    view.paint({ morphX: 0.5, morphY: 0.5 }, { bridgeAvailable: false });
    view.element.querySelector('.abd-xypad__pad').dispatchEvent(
      new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }));
    expect(onEdit.mock.calls).toEqual([['morphX', 0.51, 'end']]);

    view.destroy();
    expect(host.querySelector('.abd-xypad')).toBeNull();
  });

  it('model-slots: espectral + ranuras para el CAJON (mismos handlers)', () => {
    const onLoad = vi.fn();
    const onEdit = vi.fn();
    const view = createVisual('model-slots', [], { onLoad, onEdit });
    host.append(view.element);

    expect(view.element.className).toBe('model-block model-block--drawer');
    expect(view.element.querySelectorAll('.model-slots__row')).toHaveLength(4);
    // El pad vive en el LIENZO: el detalle del cajon no monta otro.
    expect(view.element.querySelector('.abd-xypad__pad')).toBeNull();

    // Las ranuras leen su estado del mismo paint del puente.
    view.paint({ morphX: 0.5, morphY: 0.5 }, {
      bridgeAvailable: true,
      models: [
        { slot: 0, name: 'Piano', isValid: true },
        { slot: 1, name: 'EMPTY', isValid: true },
        { slot: 2, name: 'EMPTY', isValid: true },
        { slot: 3, name: 'EMPTY', isValid: true },
      ],
    });

    expect(view.element.querySelector('[data-slot="0"] .model-slots__name').textContent).toBe('Piano');
    expect(view.element.querySelector('[data-slot="0"] .model-slots__load').disabled).toBe(false);

    view.element.querySelector('[data-slot="0"] .model-slots__load').click();
    expect(onLoad).toHaveBeenCalledWith(0);

    // La semantica de destroy de las vistas es VACIAR (el elemento lo cuelga el
    // panel): no queda una ranura viva.
    view.destroy();
    expect(view.element.querySelector('.model-slots__row')).toBeNull();
  });
});

describe('xyPad / el anillo exterior de morph-Z (FASE 10)', () => {
  let host;

  beforeEach(() => { host = makeHost(); });

  /** stub del rect del svg del aro (jsdom no tiene layout) */
  function stubRingRect(svg, size = 100) {
    svg.getBoundingClientRect = () =>
      ({ left: 0, top: 0, right: size, bottom: size, width: size, height: size });
  }

  it('monta el aro como slider accesible independiente (0..1, dasharray a 0)', () => {
    const view = createXyPad({});
    host.append(view.element);

    const ring = view.element.querySelector('.xy-pad__zring');
    expect(ring).not.toBeNull();
    expect(ring.getAttribute('role')).toBe('slider');
    expect(ring.getAttribute('aria-label')).toBe('Morph Z');
    expect(ring.getAttribute('aria-valuemin')).toBe('0');
    expect(ring.getAttribute('aria-valuemax')).toBe('1');
    expect(ring.getAttribute('aria-valuenow')).toBe('0');

    const fill = ring.querySelector('.zring-fill');
    expect(fill.getAttribute('stroke-dasharray')).toBe('0 100');

    view.destroy();
  });

  it('un drag del aro es UN gesto de morphZ con fase completa (ángulo -> valor)', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    const svg = view.element.querySelector('.xy-pad__zring svg');
    stubRingRect(svg, 100);

    // pointerdown a la derecha del centro: angulo 90° -> z = 0.25
    svg.dispatchEvent(pointer('pointerdown', 100, 50));
    svg.dispatchEvent(pointer('pointerup', 100, 50));

    const phases = onEdit.mock.calls.filter(([id]) => id === 'morphZ').map(([, , phase]) => phase);
    expect(phases).toEqual(['begin', 'change', 'end']);

    const change = onEdit.mock.calls.find(([, , phase]) => phase === 'change');
    expect(change[0]).toBe('morphZ');
    expect(change[1]).toBeCloseTo(0.25, 5);

    // FASE 11.4: el aro mezcla CAPAS — cada fase viaja tambien como
    // morphZ2/morphZ3 (cada capa sigue su propia linea de frames).
    const layerPhases = onEdit.mock.calls.filter(([id]) => id === 'morphZ2').map(([, , phase]) => phase);
    expect(layerPhases).toEqual(['begin', 'change', 'end']);
    expect(onEdit).toHaveBeenCalledWith('morphZ2', expect.any(Number), 'change');
    expect(onEdit).toHaveBeenCalledWith('morphZ3', expect.any(Number), 'change');

    view.destroy();
  });

  /**
   * Quien recibe el puntero sobre el pad lo decide el HIT-TESTING del navegador,
   * no el arbol del DOM: jsdom no tiene layout ni `elementFromPoint`, y un test
   * de manejadores pasa con el pad TAPADO (fue el caso: el contenedor del aro es
   * una caja con `inset` negativo y `pointer-events` por defecto, asi que se
   * comia los gestos del pad en todo su interior; lo destapo el selftest en vivo,
   * direccion MORPH). Aqui se fija la REGLA de la hoja de estilos —que es lo que
   * jsdom si puede leer— y el hit-testing de verdad se mide en la pagina viva
   * con `document.elementFromPoint`.
   */
  it('el interior NO es del aro: su caja deja pasar el puntero y el trazo sigue siendo suyo', () => {
    const styles = read('../src/styles/main.css');
    const container = cssBlock(styles, '.xy-pad__zring');
    const hit = cssBlock(styles, '.xy-pad__zring .zring-hit');

    // la caja del aro es un overlay sobre el pad: transparente al puntero
    expect(container).toMatch(/pointer-events:\s*none/);
    // y el trazo (11 unidades del viewBox, ancho de dedo) sigue agarrable
    expect(hit).toMatch(/pointer-events:\s*stroke/);
    expect(hit).toMatch(/stroke-width:\s*11/);

    // el componente no pinta las reglas en linea: viven en la hoja de estilos
    const view = createXyPad({});
    host.append(view.element);

    expect(view.element.querySelector('.zring-hit').style.pointerEvents).toBe('');

    view.destroy();
  });

  it('teclado: flechas ±0.01, PageUp/Down ±0.1, Home/End 0/1 — como END y solo si cambió', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    const ring = view.element.querySelector('.xy-pad__zring');

    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }));
    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'PageUp', bubbles: true }));

    // 0.01 + 0.1 = 0.11: dos pasos, dos END — en los TRES z (FASE 11.4: el
    // aro mezcla capas, cada paso mueve morphZ2/morphZ3 con el mismo valor).
    expect(onEdit.mock.calls).toEqual([
      ['morphZ', 0.01, 'end'], ['morphZ2', 0.01, 'end'], ['morphZ3', 0.01, 'end'],
      ['morphZ', 0.11, 'end'], ['morphZ2', 0.11, 'end'], ['morphZ3', 0.11, 'end'],
    ]);

    onEdit.mockClear();
    // End -> 1; y un segundo End no viaja (no cambió: dedupe honesto)
    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'End', bubbles: true }));
    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'End', bubbles: true }));
    expect(onEdit.mock.calls).toEqual([
      ['morphZ', 1, 'end'], ['morphZ2', 1, 'end'], ['morphZ3', 1, 'end'],
    ]);

    // Home -> 0 (FASE 11.4: End ya gasto las llamadas [0..2]; Home son [3..5])
    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'Home', bubbles: true }));
    expect(onEdit.mock.calls).toEqual([
      ['morphZ', 1, 'end'], ['morphZ2', 1, 'end'], ['morphZ3', 1, 'end'],
      ['morphZ', 0, 'end'], ['morphZ2', 0, 'end'], ['morphZ3', 0, 'end'],
    ]);

    // aria sigue al valor
    expect(ring.getAttribute('aria-valuenow')).toBe('0');

    view.destroy();
  });

  it('paint pinta el aro sin eco y no pega con el dedo durante un drag del aro', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    const ring = view.element.querySelector('.xy-pad__zring');
    const fill = ring.querySelector('.zring-fill');

    // snapshot con morphZ=0.4: el aro se pinta, no hay eco al store
    view.paint({ morphX: 0, morphY: 0, morphZ: 0.4 }, {});
    expect(fill.getAttribute('stroke-dasharray')).toBe('40 100');
    expect(onEdit).not.toHaveBeenCalled();

    // drag del aro: el snapshot NO mueve el aro (no se pega con el dedo)
    const svg = view.element.querySelector('.xy-pad__zring svg');
    stubRingRect(svg, 100);
    svg.dispatchEvent(pointer('pointerdown', 100, 50)); // z -> 0.25
    expect(fill.getAttribute('stroke-dasharray')).toBe('25 100');

    view.paint({ morphX: 0, morphY: 0, morphZ: 0.8 }, {});
    expect(fill.getAttribute('stroke-dasharray')).toBe('25 100');

    svg.dispatchEvent(pointer('pointerup', 100, 50));

    // al soltar, el snapshot vuelve a mandar
    view.paint({ morphX: 0, morphY: 0, morphZ: 0.8 }, {});
    expect(fill.getAttribute('stroke-dasharray')).toBe('80 100');

    view.destroy();
  });
});

describe('xyPad / anillo morphZ con la modulación en vivo (destino 28)', () => {
  let host;

  beforeEach(() => { host = makeHost(); });

  // Mismo SSOT que producción: el índice de telemetría se lee del contrato.
  const MORPH_Z_TARGET = MOD_DESTINATIONS.findIndex(
    (destination) => destination?.parameterId === 'morphZ');

  function frameWith(contribution) {
    const modulation = new Array(MORPH_Z_TARGET + 1).fill(0);
    modulation[MORPH_Z_TARGET] = contribution;
    return { modulation };
  }

  it('setZMod pinta el arco CON SIGNO desde la base y no viaja al store', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    view.paint({ morphX: 0, morphY: 0, morphZ: 0.25 }, {});
    view.setZMod(0.5); // horario: NACE en la base (0.25), span 50

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dashoffset')).toBe('-25');
    expect(mod.getAttribute('stroke-dasharray')).toBe('50 100');
    // telemetría es pintura: ningún gesto hacia el store
    expect(onEdit).not.toHaveBeenCalled();

    view.destroy();
  });

  it('contribución negativa: el arco corre ANTIHORARIO y TERMINA en la base', () => {
    const view = createXyPad({});
    host.append(view.element);

    view.paint({ morphX: 0, morphY: 0, morphZ: 0.25 }, {});
    view.setZMod(-0.25); // antihorario: de 0.00 a 0.25 (termina en la base)

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dashoffset')).toBe('0');
    expect(mod.getAttribute('stroke-dasharray')).toBe('25 100');

    view.destroy();
  });

  it('la contribucion pasa de 1 sin recorte: el arco envuelve por las 12', () => {
    const view = createXyPad({});
    host.append(view.element);

    view.paint({ morphX: 0, morphY: 0, morphZ: 0.9 }, {});
    view.setZMod(0.5); // sin clamp: 50 guiones desde 0.90 (10 + 40 tras envolver por 0)

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dashoffset')).toBe('-90');
    expect(mod.getAttribute('stroke-dasharray')).toBe('50 100');

    view.destroy();
  });

  it('morphZ a 0 + contribucion negativa: la semionda se VE (envuelve por las 12)', () => {
    const view = createXyPad({});
    host.append(view.element);

    // El caso que mando el recorte viejo al olvido: con la base en reposo, la
    // semionda negativa del LFO es INVISIBLE (el arco quedaba clampeado a 0).
    // Ahora el arco entra en el lado antihorario del aro: termina en la base
    // (las 12) y cubre el lado negativo (0.95 -> 0.00).
    view.paint({ morphX: 0, morphY: 0, morphZ: 0 }, {});
    view.setZMod(-0.95);

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dashoffset')).toBe('-5');
    expect(mod.getAttribute('stroke-dasharray')).toBe('95 100');

    view.destroy();
  });

  it('sin modulación el arco es invisible y el aro base manda', () => {
    const view = createXyPad({});
    host.append(view.element);

    view.paint({ morphX: 0, morphY: 0, morphZ: 0.4 }, {});
    view.setZMod(0);

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dasharray')).toBe('0 100');
    const fill = view.element.querySelector('.zring-fill');
    expect(fill.getAttribute('stroke-dasharray')).toBe('40 100');

    view.destroy();
  });

  it('la telemetría nativa alimenta el aro vía onTelemetry de la vista', () => {
    let notify = null;
    const unsubs = [];
    const view = createVisual('model-xy', [], {
      onTelemetry: (cb) => { notify = cb; return () => unsubs.push(1); },
    });
    host.append(view.element);

    notify(frameWith(0.5));

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dasharray')).toBe('50 100'); // positivo: nace en la base

    view.destroy();
    expect(unsubs.length).toBe(1); // destroy cancela la suscripción
  });

  it('frames sin destino morphZ o sin campo modulation no envenenan el aro', () => {
    let notify = null;
    const view = createVisual('model-xy', [], {
      onTelemetry: (cb) => { notify = cb; return () => {}; },
    });
    host.append(view.element);

    notify({});                    // frame sin modulation
    notify({ modulation: [0.1] }); // índice del destino fuera del array

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dasharray')).toBe('0 100');

    view.destroy();
  });
});
