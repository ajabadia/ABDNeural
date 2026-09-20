/**
 * El ajuste del lienzo al viewport: cálculo puro (escala acotada y centrado)
 * y montaje con resize. El fallo que motiva esto: la ventana del editor es
 * más baja que el diseño y SIN ajuste el pie y la franja de teclado quedan
 * fuera de vista ("no se distinguen las teclas").
 */

import { describe, expect, it, vi } from 'vitest';

import { computeFit, mountFitStage } from '../src/ui/fitStage.js';

describe('fitStage / computeFit', () => {
  const design = { designWidth: 1440, designHeight: 990 };

  it('limita por ancho cuando la ventana es estrecha y alta', () => {
    const fit = computeFit({ viewportWidth: 720, viewportHeight: 1200, ...design });

    expect(fit.scale).toBeCloseTo(0.5);
    expect(fit.offsetX).toBe(0);
    expect(fit.offsetY).toBeCloseTo((1200 - 990 * 0.5) / 2);
  });

  it('limita por alto cuando la ventana es ancha y baja (el caso del editor)', () => {
    // ~1424x780 CSS reales: el diseño de 990 NO cabía y el teclado se cortaba.
    const fit = computeFit({ viewportWidth: 1424, viewportHeight: 780, ...design });

    expect(fit.scale).toBeCloseTo(780 / 990);
    expect(fit.offsetY).toBe(0);
    expect(fit.offsetX).toBeCloseTo((1424 - 1440 * (780 / 990)) / 2);
  });

  it('encaja exacto cuando el viewport comparte proporción', () => {
    const fit = computeFit({ viewportWidth: 2880, viewportHeight: 1980, ...design });

    expect(fit.scale).toBeCloseTo(2);
    expect(fit.offsetX).toBe(0);
    expect(fit.offsetY).toBe(0);
  });

  it('acota la escala por arriba (3x, como el zoom nativo) y centra el sobrante', () => {
    const fit = computeFit({ viewportWidth: 5760, viewportHeight: 3960, ...design });

    expect(fit.scale).toBe(3);
    expect(fit.offsetX).toBeCloseTo((5760 - 1440 * 3) / 2);
    expect(fit.offsetY).toBeCloseTo((3960 - 990 * 3) / 2);
  });

  it('acota la escala por abajo (0.25) y no produce offsets negativos', () => {
    const fit = computeFit({ viewportWidth: 120, viewportHeight: 100, ...design });

    expect(fit.scale).toBe(0.25);
    expect(fit.offsetX).toBe(0);
    expect(fit.offsetY).toBe(0);
  });
});

describe('fitStage / mountFitStage', () => {
  function makeFakeViewport() {
    const listeners = new Map();

    return {
      innerWidth: 1424,
      innerHeight: 780,
      listeners,
      addEventListener: (name, fn) => listeners.set(name, fn),
      removeEventListener: (name, fn) => {
        if (listeners.get(name) === fn) listeners.delete(name);
      },
      emit: (name) => listeners.get(name)?.(),
    };
  }

  it('aplica escala y centrado al lienzo y refresca con cada resize', () => {
    const stage = document.createElement('div');
    const viewport = makeFakeViewport();

    mountFitStage(stage, { width: 1440, height: 990, viewport });

    expect(stage.style.transformOrigin).toBe('top left');
    expect(stage.style.transform).toContain('scale(0.7');
    const scaledWidth = 1440 * (780 / 990);
    expect(stage.style.marginLeft).toBe(`${(1424 - scaledWidth) / 2}px`);
    expect(stage.style.marginTop).toBe('0px');

    // Resize: recalcula (ventana estrecha y alta -> manda el ancho).
    viewport.innerWidth = 720;
    viewport.innerHeight = 1200;
    viewport.emit('resize');
    expect(stage.style.transform).toContain('scale(0.5)');
    expect(stage.style.marginTop).not.toBe('0px');
  });

  it('el detach quita el listener: un resize posterior ya no re-aplica', () => {
    const stage = document.createElement('div');
    const viewport = makeFakeViewport();
    const removeSpy = vi.spyOn(viewport, 'removeEventListener');

    const detach = mountFitStage(stage, { width: 1440, height: 990, viewport });
    detach();

    expect(removeSpy).toHaveBeenCalledWith('resize', expect.any(Function));
    expect(viewport.listeners.get('resize')).toBeUndefined();

    const transformBefore = stage.style.transform;
    viewport.innerWidth = 4000;
    viewport.emit('resize');
    expect(stage.style.transform).toBe(transformBefore);
  });
});
