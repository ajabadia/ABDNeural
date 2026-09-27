/**
 * E2E visual de la AGUJA en MODO NAVEGADOR (WebUI/needle-probe).
 *
 * Que mira (lo que ningun otro test ve): la aguja de las curvas ADSR en el
 * navegador vive del METER del worklet (onWorkletEnvelopeLevels), no del canal
 * bridge que alimenta la direccion AGUJA del arnes en el plugin. Este spec
 * carga la pagina de prueba —que monta las DOS vistas de produccion (curvas del
 * lienzo y bloques del cajon: las CUATRO agujas)—, arranca el motor WASM de
 * verdad, manda una nota por el MISMO mensaje que el teclado de la pagina y
 * afirma el ciclo completo sobre el 'd' del path, con la MISMA escala que el
 * arnes (viewBox 100x48, PAD 2):
 *
 *   1. sin motor, las cuatro agujas OCULTAS (el DOM existe, nadie las pinta);
 *   2. con nota, las cuatro VISIBLES con nivel alto y GEMELAS entre vistas
 *      (misma envolvente, mismo frame: |lienzo - cajon| pequeno);
 *   3. nota OFF: las cuatro se OCULTAN (la cola baja del suelo del dibujo).
 *
 * La pagina se sirve desde el servidor de DEV (vite, puerto 5237, segunda
 * entrada de playwright.config.js) a proposito: es una pagina de PRUEBA y no
 * tiene que entrar en WebUI/dist (lo que embebe el plugin es lo que se envia).
 */

import { expect, test } from '@playwright/test';

const PROBE_URL = process.env.NEEDLE_PROBE_URL ?? 'http://localhost:5237/needle-probe/';

/**
 * Espera a que el reloj del servicio de audio avance (mismo guard que
 * localMode.spec.js: el primer arranque tarda ~4 s y puede clavarse).
 * @returns {Promise<number>} intentos consumidos; 0 = este entorno no procesa audio.
 */
async function ensureAudioClock(page, attempts = 24) {
  return page.evaluate(async (tries) => {
    for (let attempt = 1; attempt <= tries; attempt += 1) {
      const context = new AudioContext();

      await context.resume().catch(() => {});

      const before = context.currentTime;
      await new Promise((resolve) => setTimeout(resolve, 250));
      const advanced = context.currentTime - before;

      await context.close();

      if (advanced > 0) return attempt;
    }

    return 0;
  }, attempts);
}

/** Las cuatro agujas, leidas de los paths con la escala del arnes. */
const readNeedles = (page) => page.evaluate(() => window.__needles());

const allHidden = (needles) => Object.values(needles).every((needle) => !needle.visible);
const allVisible = (needles) => Object.values(needles).every((needle) => needle.visible);

test.beforeEach(async ({ page }) => {
  await page.goto(PROBE_URL);
  await page.waitForFunction(() => window.__probeReady === true);
});

test('sin motor, las cuatro agujas existen pero estan OCULTAS', async ({ page }) => {
  const needles = await readNeedles(page);

  expect(Object.keys(needles)).toEqual(['canvasAmp', 'canvasFilter', 'blocksAmp', 'blocksFilter']);
  expect(allHidden(needles), 'sin frames nadie pinta una aguja').toBe(true);
});

test('SOUND ON + nota: las cuatro agujas se VEN, pintan nivel y son GEMELAS entre vistas', async ({ page }) => {
  const attempts = await ensureAudioClock(page);

  test.skip(!attempts, 'el reloj de audio de Chromium no avanza en este entorno (sin salida de audio)');

  await page.locator('#start').click();
  await expect(page.locator('#status')).toHaveAttribute('data-status', 'ready', { timeout: 20_000 });

  // La nota entra por el MISMO mensaje que manda el teclado de la pagina.
  await page.locator('#note-on').click();

  // Sostenido: las CUATRO visibles (el meter trae envelopes=[amp, filter] y las
  // dos vistas pintan su aguja).
  await expect.poll(() => readNeedles(page).then(allVisible), { timeout: 20_000, intervals: [200, 400, 800] })
    .toBe(true);

  const needles = await readNeedles(page);

  // Nivel alto (el sustain de amp vive en 0.7: una aguja pintando la cola del
  // attack no pasa este corte).
  for (const [name, needle] of Object.entries(needles)) {
    expect(needle.level, `nivel de ${name}`).toBeGreaterThan(0.2);
  }

  // GEMELIDAD entre vistas: misma envolvente, mismo frame, mismo setLevel —
  // el desacuerdo solo podria salir del re-parseo del path (redondeo del 'd').
  expect(Math.abs(needles.canvasAmp.level - needles.blocksAmp.level)).toBeLessThanOrEqual(0.02);
  expect(Math.abs(needles.canvasFilter.level - needles.blocksFilter.level)).toBeLessThanOrEqual(0.02);

  // NOTA (hallazgo de esta pagina, no una asercion): aqui NO se exige amp !=
  // filtro. En modo local el worklet solo canaliza GlobalParams (matriz, LFOs,
  // FX) y el morph — no hay canal de VoiceParams/ADSR —, asi que las dos
  // envolventes viven en los defaults de C++ (sustain 0.7/0.7) y los knobs de
  // envolvente de la pagina no llegan al motor local. El feed de la aguja sigue
  // siendo REAL (el meter lee _neuronikGetEnvelopeLevels del motor); lo que no
  // existe todavia es la plomeria de ADSR al worklet.

  await page.locator('#note-off').click();

  // Release: la cola baja del suelo del dibujo y las cuatro se ocultan.
  await expect.poll(() => readNeedles(page).then(allHidden), { timeout: 20_000, intervals: [300, 600, 1_000] })
    .toBe(true);
});
