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
  it('cubre el contrato: 70 celdas propias, sin repetir ninguna', () => {
    // FASE 10 (camino B): 72 del APVTS = 70 con celda en el reparto (morphZ
    // entra al cajon de MODELOS) + 2 que el pad `model-xy` edita con un gesto
    // coordinado (morphX/morphY dejaron de ser celdas).
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
    // Un control tambien puede ser una VISTA: el pad de MODELOS edita
    // morphX/morphY (camino B), asi que cuentan como cubiertos.
    const covered = new Set([
      ...SECTION_PARAMETER_IDS,
      ...Object.values(SECTION_VISUALS).flatMap((visual) => visual.parameterIds),
    ]);
    const missing = PARAMETERS
      .map((descriptor) => descriptor.id)
      .filter((id) => !covered.has(id));

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
    // Las fichas SIN filas quedan fuera: las dos de vistas (MODELOS: sus cuatro
    // ranuras son del motor; la de cajon de la MATRIZ: sus controles viven en el
    // panel lateral) y las dos de cajon (GLOBAL & MASTER: solo su control base;
    // LFO: los suyos viven en el FRONTAL, que SI aporta filas y va aparte).
    const cellCards = SECTIONS.filter((section) => section.ids.length > 0 && !section.drawer);
    const cellLessCards = SECTIONS.filter((section) => section.ids.length === 0 || section.drawer);
    const twoRow = cellCards.filter((section) => rowsOf(section) === 2);
    const heights = new Set(twoRow.map(cardHeight));

    expect(cellLessCards.map((section) => section.id)).toEqual(['models', 'lfo', 'modMatrix', 'globalFull']);
    expect(cellCards.length).toBe(SECTIONS.length - cellLessCards.length);
    // Todas las de dos filas miden lo mismo; FILTRO (una fila) es la excepcion
    // declarada de la separacion 8.3 y es MAS BAJA.
    expect(heights.size).toBe(1);
    const filter = SECTIONS.find((section) => section.id === 'filter');
    expect(rowsOf(filter)).toBe(1);
    expect(cardHeight(filter)).toBeLessThan(cardHeight(twoRow[0]));
  });

  it('la ficha de MODELOS: en el CENTRO, solo el pad; las ranuras en su cajon', () => {
    const card = SECTIONS.find((section) => section.id === 'models');
    const padVisual = SECTION_VISUALS[card.visual];
    const drawerVisual = SECTION_VISUALS[card.drawer.visual];

    // La vista del lienzo es SOLO el pad (morphX/morphY con las esquinas A-D);
    // el detalle (espectral + ranuras) vive en el cajon (mismos handlers).
    expect(card.visual).toBe('model-xy');
    expect(padVisual).toBeTruthy();
    expect(card.drawer.visual).toBe('model-slots');
    expect(drawerVisual).toBeTruthy();

    // MORPH-Z vive aqui (FASE 10): ficha con cajon => su celda SOLO en el
    // cajon, bajo la vista de ranuras; el lienzo se queda con el pad.
    expect(card.ids).toEqual(['morphZ']);
    expect(padVisual.parameterIds).toEqual(['morphX', 'morphY']);
    // morphX/morphY (camino B) ya no son celdas: existen en el contrato y el
    // pad es su control.
    for (const id of drawerVisual.parameterIds)
      expect(getDescriptor(id)).not.toBeNull();
    // ...y su cuerpo (150) es el que cierra su banda.
    expect(padVisual.minBodyHeight).toBeGreaterThan(0);
    expect(rowsOf(card)).toBe(0);
    expect(canvasHeight()).toBeLessThanOrEqual(CANVAS.height);
    // Una columna: la vista llena el cuerpo de la ficha, no reparte celdas.
    expect(card.columns).toBe(1);

    // 70 celdas propias (morphZ incluido; morphX/morphY viven en el pad):
    // una ranura no es una mas.
    expect(SECTION_PARAMETER_IDS).toHaveLength(70);
    expect(SECTION_PARAMETER_IDS).not.toContain('models');
    expect(SECTION_PARAMETER_IDS).not.toContain('model-slots');

    // La mudanza al centro (8.3): entre FILTRO & ENVOLVENTE y EFECTOS, con los
    // carriles que reparten sus vecinos (5 + 2 + 5). Es una decision, no un
    // accidente.
    const band = BANDS.find((candidate) => candidate.includes(card));

    expect(band.map((section) => section.id)).toEqual(['envelopes', 'models', 'fx']);
    expect(band.map((section) => section.span)).toEqual([5, 2, 5]);
  });

  it('la MATRIZ: cajon por rutas, resumen en el lienzo, banda con GLOBAL', () => {
    const drawerSection = SECTIONS.find((section) => section.id === 'modMatrix');
    const models = SECTIONS.find((section) => section.id === 'models');

    // Cero filas: sus 12 celdas no empujan el encaje.
    expect(rowsOf(drawerSection)).toBe(0);
    expect(cardHeight(drawerSection)).toBeLessThan(cardHeight(SECTIONS[0]));
    // Pero sus ids SI son del reparto: el store y el recuento siguen viendo 70.
    expect(drawerSection.ids).toHaveLength(12);
    expect(SECTION_PARAMETER_IDS).toContain('mod1Source');

    // Banda del fondo: LFO + matriz + global (la caja LFO entra con los
    // carriles que GLOBAL deja al ceder dos).
    const band = BANDS.find((candidate) => candidate.includes(drawerSection));

    expect(band.map((section) => section.id)).toEqual(['lfo', 'modMatrix', 'globalFull']);
    expect(band.reduce((total, section) => total + section.span, 0)).toBe(CANVAS.lanes);
    // El alto de la banda lo manda el LFO: su FRONTAL (2 filas) es lo mas alto
    // (la matriz y el global son armazon de cajon).
    const lfo = SECTIONS.find((section) => section.id === 'lfo');
    expect(Math.max(...band.map(cardHeight))).toBe(cardHeight(lfo));
    expect(models).toBeTruthy();
  });

  it('las rutas del cajon de la MATRIZ son, en orden, los ids de la ficha', () => {
    const section = SECTIONS.find((candidate) => candidate.id === 'modMatrix');

    expect(section.drawer.groups.flat()).toEqual(section.ids);
  });

  it('el resumen del lienzo se alimenta de los mismos ids que el cajon, en el mismo orden', () => {
    const section = SECTIONS.find((candidate) => candidate.id === 'modMatrix');
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

    expect(card?.id).toBe('envelopes');
    expect(visual).toBeTruthy();

    // Se alimenta de parametros que la ficha YA pinta (no inventa ids)
    for (const id of visual.parameterIds)
      expect(card.ids).toContain(id);

    // Y ocupa un hueco real: ids + vista <= celdas de la rejilla. Esta es la razon
    // de que anadir la curva no mueva ni una fila del lienzo.
    expect(card.ids.length + 1).toBeLessThanOrEqual(card.columns * rowsOf(card));

    // No es una celda de parametro: el reparto sigue teniendo 70
    expect(SECTION_PARAMETER_IDS).not.toContain('amp-envelope');
    expect(SECTION_PARAMETER_IDS).toHaveLength(70);
  });

  it('la caja LFO: frontal rate+depth en el lienzo, el resto en el cajon', () => {
    const lfo = SECTIONS.find((section) => section.id === 'lfo');

    expect(lfo.drawer).toBeTruthy();
    expect(lfo.drawer.trigger).toBe('EDIT');
    // El frontal es SUBCONJUNTO de ids (mismas variables, no duplicado) y son
    // los cuatro primeros: rate y depth de cada LFO.
    expect(lfo.drawer.frontal).toEqual(['lfo1RateHz', 'lfo1Depth', 'lfo2RateHz', 'lfo2Depth']);
    for (const id of lfo.drawer.frontal)
      expect(lfo.ids).toContain(id);
    expect(lfo.ids.slice(0, 4)).toEqual(lfo.drawer.frontal);
    // El resto (forma, sync, division) vive SOLO en el cajon.
    expect(lfo.ids.slice(4)).toEqual(['lfo1Waveform', 'lfo1SyncMode', 'lfo1RhythmicDivision', 'lfo2Waveform', 'lfo2SyncMode', 'lfo2RhythmicDivision']);
    // El frontal SI empuja el encaje: 4 controles en 2 columnas = 2 filas
    // (la rejilla 2x2 de rate+depth, un LFO por fila).
    expect(rowsOf(lfo)).toBe(2);
    expect(lfo.ids.length).toBe(10); // 4 frontal (incluidos) + 6 de cajon
  });

  it('la separacion FILTRO / ENVOLVENTES reparte la antigua ficha sin perder ids', () => {
    const filter = SECTIONS.find((section) => section.id === 'filter');
    const envelopes = SECTIONS.find((section) => section.id === 'envelopes');

    // Los 11 ids de la antigua FILTRO & ENVOLVENTE, repartidos sin duplicados.
    const union = [...filter.ids, ...envelopes.ids];
    expect(new Set(union).size).toBe(union.length);
    expect(union.sort()).toEqual([
      'envAttack', 'envDecay', 'envSustain', 'envRelease',
      'filterAttack', 'filterCutoff', 'filterDecay', 'filterEnvAmount',
      'filterRelease', 'filterRes', 'filterSustain',
    ].sort());
    // La profundidad de la env del filtro es del FILTRO (knob de su banda).
    expect(filter.ids).toContain('filterEnvAmount');
    // Las ADSR completas viven en ENVOLVENTES; la curva sigue en su celda libre.
    expect(envelopes.ids).toEqual(['envAttack', 'envDecay', 'envSustain', 'envRelease', 'filterAttack', 'filterDecay', 'filterSustain', 'filterRelease']);
    expect(SECTION_PARAMETER_IDS).toHaveLength(70);
  });

  it('la accion RANDOM es accion, no parametro, y no ocupa celda', () => {
    const action = SECTION_ACTIONS.randomize;

    expect(action.id).toBe('randomize');
    expect(action.label).toBe('RANDOM');
    expect(action.title).toBeTruthy();

    // Vive en la ficha GLOBAL & MASTER (accion de estado, no de una seccion de
    // timbre); desde la mudanza 8.3 esa ficha es 'globalFull' y vive al final.
    const globalCard = SECTIONS.find((section) => section.action === 'randomize');

    expect(globalCard?.id).toBe('globalFull');

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
