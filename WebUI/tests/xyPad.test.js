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

describe('xyPad / compuesto en la vista model-slots', () => {
  let host;

  beforeEach(() => { host = makeHost(); });

  it('createVisual monta pad + ranuras como UNA vista', () => {
    const onLoad = vi.fn();
    const onEdit = vi.fn();
    const view = createVisual('model-slots', [], { onLoad, onEdit });
    host.append(view.element);

    expect(view.element.className).toBe('model-block');
    expect(view.element.querySelectorAll('.model-slots__row')).toHaveLength(4);
    expect(view.element.querySelector('.abd-xypad__pad')).not.toBeNull();

    // Cada mitad lee lo suyo del mismo paint: ranuras del puente (habilitadas
    // con host) y esquinas del pad.
    view.paint({ morphX: 0.5, morphY: 0.5 }, {
      bridgeAvailable: true,
      models: [
        { slot: 0, name: 'Piano', isValid: true },
        { slot: 1, name: 'EMPTY', isValid: true },
        { slot: 2, name: 'EMPTY', isValid: true },
        { slot: 3, name: 'EMPTY', isValid: true },
      ],
    });

    expect(view.element.querySelector('[data-corner="tl"]').textContent).toBe('Piano');
    expect(view.element.querySelector('[data-slot="0"] .model-slots__load').disabled).toBe(false);

    view.element.querySelector('[data-slot="0"] .model-slots__load').click();
    expect(onLoad).toHaveBeenCalledWith(0);

    // La edición del pad sale por onEdit hacia el store (el y no cambia: solo x).
    view.element.querySelector('.abd-xypad__pad').dispatchEvent(
      new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }));
    expect(onEdit.mock.calls).toEqual([['morphX', 0.51, 'end']]);

    // La semantica de destroy de las vistas es VACIAR (el elemento lo cuelga el
    // panel): lo que no puede quedar es el pad compartido ni una ranura viva.
    view.destroy();
    expect(host.querySelector('.abd-xypad')).toBeNull();
    expect(view.element.querySelector('.model-slots__row')).toBeNull();
  });
});
