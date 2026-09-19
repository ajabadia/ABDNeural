/**
 * La curva ADSR de la ficha FILTRO & ENVOLVENTE.
 *
 * La matemática se prueba SIN DOM (los puntos se calculan en unidades reales y la
 * función es pura), y el pintor se prueba con jsdom pero sin medidas: jsdom no
 * calcula layout, así que aquí se comprueba el `d` del trazo, no el encaje — ese
 * lo vigila `tests/sections.test.js` con los números de la geometría.
 *
 * Lo que se fija:
 *   - la forma: sube al pico, baja al sostenido, meseta, y vuelve a cero;
 *   - los tiempos SIEMPRE se comprimen (1 ms y 5 s se ven los dos);
 *   - el sostén manda la altura de la meseta;
 *   - ningún valor degenerado (0 s, sostenido 0 o 1) rompe el trazo;
 *   - el pintor lee el estado NORMALIZADO del store, como cualquier celda.
 */

import { afterEach, describe, expect, it } from 'vitest';

import { describeControl } from '../src/contracts/parameters.js';
import { ENVELOPE_VIEWBOX, createEnvelopeCurve, envelopePoints } from '../src/ui/envelopeCurve.js';

const ENV_IDS = ['envAttack', 'envDecay', 'envSustain', 'envRelease'];
const ENV_CONTROLS = ENV_IDS.map(describeControl);

/** Estado normalizado del store para los cuatro de la envolvente. */
function envelopeState({ attack, decay, sustain, release }) {
  const toNormalized = (id, real) => {
    const control = ENV_CONTROLS.find((candidate) => candidate.id === id);

    return (real - control.min) / (control.max - control.min);
  };

  return {
    envAttack: toNormalized('envAttack', attack),
    envDecay: toNormalized('envDecay', decay),
    envSustain: toNormalized('envSustain', sustain),
    envRelease: toNormalized('envRelease', release),
  };
}

afterEach(() => {
  document.body.textContent = '';
});

describe('curva ADSR / matemática', () => {
  it('la forma sube, baja, aguanta y suelta', () => {
    const points = envelopePoints({ attack: 0.05, decay: 0.2, sustain: 0.6, release: 0.4 });

    expect(points).toHaveLength(5);

    const [start, peak, decayEnd, sustainEnd, end] = points;
    const { height } = ENVELOPE_VIEWBOX;

    expect(start.x).toBe(0);
    expect(start.y).toBeCloseTo(height - 2, 5);          // sale de cero (abajo)
    expect(peak.y).toBeCloseTo(2, 5);                    // pico arriba
    expect(peak.x).toBeGreaterThan(0);

    // Decay llega AL SOSTÉN (el 60% de la altura, medida desde abajo)
    expect(decayEnd.y).toBeGreaterThan(peak.y);
    expect(decayEnd.y).toBeLessThan(start.y);

    // Meseta plana: mismo alto, más a la derecha
    expect(sustainEnd.y).toBeCloseTo(decayEnd.y, 5);
    expect(sustainEnd.x).toBeGreaterThan(decayEnd.x);

    // Release vuelve a cero
    expect(end.y).toBeCloseTo(start.y, 5);
    expect(end.x).toBeGreaterThan(sustainEnd.x);
  });

  it('el sostén manda la altura de la meseta', () => {
    const low = envelopePoints({ attack: 0.1, decay: 0.1, sustain: 0.1, release: 0.1 });
    const high = envelopePoints({ attack: 0.1, decay: 0.1, sustain: 0.9, release: 0.1 });

    // y crece hacia abajo: más sostenido = meseta más ARRIBA = y menor
    expect(high[2].y).toBeLessThan(low[2].y);
    expect(high[3].y).toBeLessThan(low[3].y);
  });

  it('los tiempos se comprimen: 1 ms y 5 s caben los dos', () => {
    const { width } = ENVELOPE_VIEWBOX;
    const points = envelopePoints({ attack: 0.001, decay: 0.001, sustain: 0.5, release: 5 });

    const [, peak, , , end] = points;

    // Ataque de 1 ms: no es cero, se ve
    expect(peak.x).toBeGreaterThan(1);
    // Y el conjunto no desborda el viewBox, por muy largo que sea un tramo
    expect(end.x).toBeLessThanOrEqual(width);
    // El release, 5000 veces más largo, ocupa mucho más ancho que el ataque
    expect(end.x - points[3].x).toBeGreaterThan(peak.x * 3);
  });

  it('un ataque de 5 s y otro de 1 ms dan trazos DISTINTOS (no se aplana)', () => {
    const fast = envelopePoints({ attack: 0.001, decay: 1, sustain: 0.5, release: 1 });
    const slow = envelopePoints({ attack: 5, decay: 1, sustain: 0.5, release: 1 });

    expect(slow[1].x).toBeGreaterThan(fast[1].x);
  });

  it('ningún valor degenerado rompe el trazo', () => {
    for (const values of [
      { attack: 0, decay: 0, sustain: 0, release: 0 },
      { attack: 0, decay: 0, sustain: 1, release: 0 },
      { attack: 5, decay: 5, sustain: 1, release: 5 },
      { attack: Number.NaN, decay: 0.1, sustain: 0.5, release: 0.1 },
    ]) {
      const points = envelopePoints(values);

      for (const point of points) {
        expect(Number.isFinite(point.x)).toBe(true);
        expect(Number.isFinite(point.y)).toBe(true);
        expect(point.x).toBeGreaterThanOrEqual(0);
        expect(point.x).toBeLessThanOrEqual(ENVELOPE_VIEWBOX.width);
      }
    }
  });
});

describe('curva ADSR / pintor', () => {
  it('es una VISTA, no una celda de parámetro', () => {
    const curve = createEnvelopeCurve({ controls: ENV_CONTROLS });

    expect(curve.element.dataset.visual).toBe('amp-envelope');
    expect(curve.element.classList.contains('cell')).toBe(false);
    expect(curve.element.querySelector('svg')).not.toBeNull();
    expect(curve.parameterIds).toEqual(ENV_IDS);
  });

  it('pinta desde el estado NORMALIZADO y cambia con él', () => {
    const curve = createEnvelopeCurve({ controls: ENV_CONTROLS });
    const line = () => curve.element.querySelector('.envelope-curve__line').getAttribute('d');

    curve.paint(envelopeState({ attack: 0.01, decay: 0.1, sustain: 0.7, release: 0.5 }));
    const byDefault = line();

    expect(byDefault).toMatch(/^M0\.00,/);

    curve.paint(envelopeState({ attack: 2, decay: 0.1, sustain: 0.2, release: 0.5 }));
    const byLongAttack = line();

    expect(byLongAttack).not.toBe(byDefault);

    // El área rellena cierra el mismo trazo
    const area = curve.element.querySelector('.envelope-curve__area').getAttribute('d');

    expect(area.endsWith('Z')).toBe(true);
    expect(area.startsWith(byLongAttack)).toBe(true);
  });

  it('sin estado pinta los defaults del contrato (nunca un trazo vacío)', () => {
    const curve = createEnvelopeCurve({ controls: ENV_CONTROLS });

    curve.paint({});

    expect(curve.element.querySelector('.envelope-curve__line').getAttribute('d')).toMatch(/^M0\.00,/);
  });
});
