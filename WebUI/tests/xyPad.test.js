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

import { createVisual } from '../src/ui/visuals.js';
import { createXyPad } from '../src/ui/xyPad.js';

import { MOD_DESTINATIONS } from '../generated/parameters.generated.js';

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

    const phases = onEdit.mock.calls.map(([, , phase]) => phase);
    expect(phases).toEqual(['begin', 'change', 'end']);

    const change = onEdit.mock.calls.find(([, , phase]) => phase === 'change');
    expect(change[0]).toBe('morphZ');
    expect(change[1]).toBeCloseTo(0.25, 5);

    view.destroy();
  });

  it('el interior NO es del aro: los hits solo capturan el trazo (el pad sigue intacto)', () => {
    const view = createXyPad({});
    host.append(view.element);

    const hit = view.element.querySelector('.zring-hit');
    expect(hit).not.toBeNull();
    // la regla CSS del paquete: pointer-events solo en el trazo
    expect(hit.style.pointerEvents).toBe('');

    view.destroy();
  });

  it('teclado: flechas ±0.01, PageUp/Down ±0.1, Home/End 0/1 — como END y solo si cambió', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    const ring = view.element.querySelector('.xy-pad__zring');

    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }));
    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'PageUp', bubbles: true }));

    // 0.01 + 0.1 = 0.11: dos pasos, dos END
    expect(onEdit.mock.calls).toEqual([
      ['morphZ', 0.01, 'end'],
      ['morphZ', 0.11, 'end'],
    ]);

    onEdit.mockClear();
    // End -> 1; y un segundo End no viaja (no cambió: dedupe honesto)
    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'End', bubbles: true }));
    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'End', bubbles: true }));
    expect(onEdit.mock.calls).toEqual([['morphZ', 1, 'end']]);

    // Home -> 0
    ring.dispatchEvent(new KeyboardEvent('keydown', { key: 'Home', bubbles: true }));
    expect(onEdit.mock.calls[1]).toEqual(['morphZ', 0, 'end']);

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

  it('setZMod pinta el arco base->efectivo y no viaja al store', () => {
    const onEdit = vi.fn();
    const view = createXyPad({ onEdit });
    host.append(view.element);

    view.paint({ morphX: 0, morphY: 0, morphZ: 0.25 }, {});
    view.setZMod(0.5); // efectivo 0.75: arco de 0.25 a 0.75

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dashoffset')).toBe('-25');
    expect(mod.getAttribute('stroke-dasharray')).toBe('50 100');
    // telemetría es pintura: ningún gesto hacia el store
    expect(onEdit).not.toHaveBeenCalled();

    view.destroy();
  });

  it('contribución negativa: el arco corre hacia atrás (efectivo < base)', () => {
    const view = createXyPad({});
    host.append(view.element);

    view.paint({ morphX: 0, morphY: 0, morphZ: 0.25 }, {});
    view.setZMod(-0.25); // efectivo 0

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dashoffset')).toBe('0');
    expect(mod.getAttribute('stroke-dasharray')).toBe('25 100');

    view.destroy();
  });

  it('el efectivo se clamp a 0..1 (la misma matemática de la voz)', () => {
    const view = createXyPad({});
    host.append(view.element);

    view.paint({ morphX: 0, morphY: 0, morphZ: 0.9 }, {});
    view.setZMod(0.5); // efectivo clamp a 1: arco de 0.9 a 1

    const mod = view.element.querySelector('.zring-mod');
    expect(mod.getAttribute('stroke-dashoffset')).toBe('-90');
    expect(mod.getAttribute('stroke-dasharray')).toBe('10 100');

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
    expect(mod.getAttribute('stroke-dasharray')).toBe('50 100');

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
