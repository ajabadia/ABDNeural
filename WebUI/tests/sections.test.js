/**
 * Reparto del lienzo y su ENCAJE, medido.
 *
 * El ticket 8.2 pide que los 70 parámetros quepan en un solo lienzo, sin cajón ni
 * pestañas. "Caben" no es una impresión: aquí se cuenta con los mismos números
 * que usa la CSS (GEOMETRY/CANVAS de sections.js) y se comprueba que la altura
 * total no pasa de la del lienzo. Si una ficha crece una fila de más, este test
 * lo dice antes de que nadie lo vea recortado en WebView2.
 *
 * Además, anti-drift entre las dos fuentes de la geometría: la CSS declara las
 * mismas medidas como custom properties, así que cambiar una sin la otra falla.
 */

import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { describe, expect, it } from 'vitest';

import { PARAMETERS, getDescriptor } from '../src/contracts/parameters.js';
import { GENERAL_PARAMETER_IDS } from '../src/contracts/screens.js';
import {
  BANDS,
  CANVAS,
  GEOMETRY,
  SECTIONS,
  SECTION_ACTIONS,
  SECTION_PARAMETER_IDS,
  SECTION_VISUALS,
  canvasHeight,
  cardHeight,
  rowsOf,
} from '../src/contracts/sections.js';

const here = dirname(fileURLToPath(import.meta.url));
const stylesheet = readFileSync(join(here, '../src/styles/main.css'), 'utf8');

/** Valor de una custom property en px declarada en main.css. */
function cssPixels(name) {
  const match = new RegExp(`--${name}:\\s*([0-9.]+)px`).exec(stylesheet);

  return match ? Number(match[1]) : NaN;
}

describe('sections / cobertura del contrato', () => {
  it('cubre los 70 parámetros del contrato, sin repetir ninguno', () => {
    expect(SECTION_PARAMETER_IDS).toHaveLength(70);
    expect(new Set(SECTION_PARAMETER_IDS).size).toBe(70);
  });

  it('cada id del lienzo existe en el contrato generado', () => {
    for (const id of SECTION_PARAMETER_IDS) {
      expect(getDescriptor(id), `el contrato no contiene "${id}"`).not.toBeNull();
    }
  });

  it('cubre TODOS los ids del contrato (ninguno se queda sin control)', () => {
    // La lista de referencia es la del propio contrato, no una copia a mano.
    const missing = PARAMETERS
      .map((descriptor) => descriptor.id)
      .filter((id) => !SECTION_PARAMETER_IDS.includes(id));

    expect(missing).toEqual([]);
  });

  it('lleva los 11 ids que el selftest del host comprueba', () => {
    for (const id of GENERAL_PARAMETER_IDS) {
      expect(SECTION_PARAMETER_IDS, `falta "${id}"`).toContain(id);
    }
  });
});

describe('sections / bandas', () => {
  it('cada banda llena el ancho del lienzo, sin huecos', () => {
    for (const band of BANDS) {
      const used = band.reduce((total, section) => total + section.span, 0);

      expect(used, `banda ${band.map((section) => section.id).join(' + ')}`).toBe(CANVAS.lanes);
    }
  });

  it('ninguna ficha pasa de dos filas de celdas', () => {
    for (const section of SECTIONS) {
      expect(rowsOf(section), `ficha "${section.id}"`).toBeLessThanOrEqual(2);
    }
  });

  it('ningún carril de una ficha queda vacío', () => {
    for (const section of SECTIONS) {
      // Con 2 filas, la última puede quedar a medias, pero nunca sobran 2 filas.
      const capacity = rowsOf(section) * section.columns;

      expect(section.ids.length, `ficha "${section.id}"`).toBeGreaterThan(capacity - section.columns);
    }
  });
});

describe('sections / encaje en el lienzo', () => {
  it('los 70 controles caben en el alto del lienzo', () => {
    const total = canvasHeight();

    expect(total, `alto calculado ${total}px vs lienzo ${CANVAS.height}px`)
      .toBeLessThanOrEqual(CANVAS.height);
  });

  it('deja aire suficiente para no depender del redondeo del navegador', () => {
    // La primera cuenta dejó fuera bordes, huecos de fila y relleno del armazón y
    // el lienzo desbordaba 33 px en un motor real. Con todo dentro, sobra margen.
    expect(CANVAS.height - canvasHeight()).toBeGreaterThanOrEqual(8);
  });

  it('el lienzo no desborda ni con la franja de teclado desplegada', () => {
    const displayed = canvasHeight();

    // La franja YA cuenta entera (== su alto): plegarla solo libera espacio.
    expect(displayed).toBeLessThanOrEqual(CANVAS.height);
    expect(displayed - GEOMETRY.keys - GEOMETRY.bandGap).toBeLessThan(CANVAS.height);
  });

  it('las fichas CON celdas miden lo mismo (rejilla regular)', () => {
    // Las dos fichas SIN celdas quedan fuera: la de cajon (sus controles viven en el
    // panel lateral) y la de MODELOS (sus cuatro ranuras son del motor, no del APVTS,
    // asi que no tienen id). Ninguna aporta filas: su alto lo pone la banda al
    // estirarlas y el encaje lo marca su companera de banda.
    const cellCards = SECTIONS.filter((section) => section.ids.length > 0 && !section.drawer);
    const cellLessCards = SECTIONS.filter((section) => section.ids.length === 0 || section.drawer);
    const heights = new Set(cellCards.map(cardHeight));

    expect(cellLessCards.map((section) => section.id)).toEqual(['models', 'modMatrix']);
    expect(cellCards.length).toBe(SECTIONS.length - cellLessCards.length);
    expect(heights.size).toBe(1);
  });

  it('la ficha de MODELOS son cuatro ranuras del MOTOR, no celdas del APVTS', () => {
    const card = SECTIONS.find((section) => section.visual === 'model-slots');
    const visual = SECTION_VISUALS['model-slots'];

    expect(card?.id).toBe('models');
    expect(visual).toBeTruthy();

    // La ficha no tiene celdas; su vista alimenta el pad con los morph que ya
    // pinta la ficha OSCILADOR (mismo contrato, ninguna celda nueva)...
    expect(card.ids).toEqual([]);
    expect(visual.parameterIds).toEqual(['morphX', 'morphY']);
    for (const id of visual.parameterIds)
      expect(SECTION_PARAMETER_IDS).toContain(id);
    // ...y declara el cuerpo que cierra su banda (el pad dibujado, 8.3).
    expect(visual.minBodyHeight).toBeGreaterThan(0);
    // ...asi que no aporta filas al encaje (el lienzo sigue midiendo lo mismo).
    expect(rowsOf(card)).toBe(0);
    expect(canvasHeight()).toBeLessThanOrEqual(CANVAS.height);
    // Una columna: la vista llena el cuerpo de la ficha, no reparte celdas.
    expect(card.columns).toBe(1);

    // Y las 70 celdas siguen siendo 70: una ranura no es una mas.
    expect(SECTION_PARAMETER_IDS).toHaveLength(70);
    expect(SECTION_PARAMETER_IDS).not.toContain('models');
    expect(SECTION_PARAMETER_IDS).not.toContain('model-slots');

    // Comparte banda con el LFO y la matriz: es el unico hueco de carriles que
    // quedaba, y el reparto de esa banda es una decision, no un accidente.
    const band = BANDS.find((candidate) => candidate.includes(card));

    expect(band.map((section) => section.id)).toEqual(['lfo', 'models', 'modMatrix']);
  });

  it('la ficha de cajon no aporta filas al lienzo y su banda la estira', () => {
    const drawerSection = SECTIONS.find((section) => section.drawer);

    expect(drawerSection?.id).toBe('modMatrix');
    // Cero filas: sus 12 celdas no empujan el encaje.
    expect(rowsOf(drawerSection)).toBe(0);
    expect(cardHeight(drawerSection)).toBeLessThan(cardHeight(SECTIONS[0]));
    // Pero sus ids SI son del reparto: el store y el recuento siguen viendo 70.
    expect(drawerSection.ids).toHaveLength(12);
    expect(SECTION_PARAMETER_IDS).toContain('mod1Source');

    // Y su banda la cierra la ficha MODELOS desde 8.3: el pad dibujado pide mas
    // cuerpo (minBodyHeight) que las dos filas del LFO.
    const band = BANDS.find((candidate) => candidate.includes(drawerSection));

    expect(band.reduce((total, section) => total + section.span, 0)).toBe(CANVAS.lanes);
    expect(Math.max(...band.map(cardHeight))).toBe(cardHeight(SECTIONS.find((s) => s.id === 'models')));
  });

  it('las rutas del cajon son, en orden, los ids de la ficha', () => {
    const section = SECTIONS.find((candidate) => candidate.drawer);

    expect(section.drawer.groups.flat()).toEqual(section.ids);
  });

  it('el resumen del lienzo se alimenta de los mismos ids que el cajon, en el mismo orden', () => {
    const section = SECTIONS.find((candidate) => candidate.drawer);
    const visual = SECTION_VISUALS[section.visual];

    expect(section.visual).toBe('mod-summary');
    // El resumen agrupa sus controles de tres en tres (fuente, destino, cantidad):
    // si el orden de `parameterIds` cambiara, pintaria las rutas cruzadas.
    expect(visual.parameterIds).toEqual(section.ids);
    expect(visual.parameterIds.length).toBe(section.drawer.groups.length * 3);
  });

  it('la curva ADSR vive en la celda LIBRE de su ficha (no cambia el encaje)', () => {
    const card = SECTIONS.find((section) => section.visual === 'amp-envelope');
    const visual = SECTION_VISUALS['amp-envelope'];

    expect(card?.id).toBe('filterEnv');
    expect(visual).toBeTruthy();

    // Se alimenta de parametros que la ficha YA pinta (no inventa ids)
    for (const id of visual.parameterIds)
      expect(card.ids).toContain(id);

    // Y ocupa un hueco real: ids + vista <= celdas de la rejilla. Esta es la razon
    // de que anadir la curva no mueva ni una fila del lienzo.
    expect(card.ids.length + 1).toBeLessThanOrEqual(card.columns * rowsOf(card));

    // No es una celda de parametro: el lienzo sigue teniendo 70
    expect(SECTION_PARAMETER_IDS).not.toContain('amp-envelope');
    expect(SECTION_PARAMETER_IDS).toHaveLength(70);
  });

  it('la accion RANDOM es accion, no parametro, y no ocupa celda', () => {
    const action = SECTION_ACTIONS.randomize;

    expect(action.id).toBe('randomize');
    expect(action.label).toBe('RANDOM');
    expect(action.title).toBeTruthy();

    // Vive en la ficha GLOBAL & MASTER (accion de estado, no de una seccion de timbre)
    const globalCard = SECTIONS.find((section) => section.action === 'randomize');

    expect(globalCard?.id).toBe('global');

    // No es un id del APVTS: no aparece en las celdas ni en el encaje
    expect(SECTION_PARAMETER_IDS).not.toContain('randomize');
    expect(SECTION_PARAMETER_IDS).toHaveLength(70);
  });
});

describe('sections / geometría compartida con la CSS', () => {
  it('la CSS declara las mismas medidas que el reparto', () => {
    expect(cssPixels('abd-canvas-w')).toBe(CANVAS.width);
    expect(cssPixels('abd-canvas-h')).toBe(CANVAS.height);
    expect(cssPixels('abd-cell-h')).toBe(GEOMETRY.cell);
    expect(cssPixels('abd-keys-h')).toBe(GEOMETRY.keys);
    expect(cssPixels('abd-footer-h')).toBe(GEOMETRY.footer);
    expect(cssPixels('abd-top-h')).toBe(GEOMETRY.top);
    expect(cssPixels('abd-band-gap')).toBe(GEOMETRY.bandGap);
    expect(cssPixels('abd-card-header-h')).toBe(GEOMETRY.cardHeader);
    expect(cssPixels('abd-card-row-gap')).toBe(GEOMETRY.cardRowGap);
    expect(cssPixels('abd-card-border')).toBe(GEOMETRY.cardBorder);
    expect(cssPixels('abd-panel-pad')).toBe(GEOMETRY.panelPadding);
    expect(cssPixels('abd-knob-size')).toBe(GEOMETRY.knob);
    expect(cssPixels('abd-card-pad') * 2).toBe(GEOMETRY.cardPadding);
  });
});
