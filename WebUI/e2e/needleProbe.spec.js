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
 *   3. las dos envolventes MIDEN cosas distintas: el boton de ADSR empuja
 *      sustains 0.8 (amp) y 0.2 (filtro) por el canal de VoiceParams, y las
 *      needles tienen que acabar ahi y no en el 0.7 de los defaults de C++;
 *   4. nota OFF: las cuatro se OCULTAN (la cola baja del suelo del dibujo).
 *
 * La pagina se sirve desde el servidor de DEV (vite, puerto 5237, segunda
 * entrada de playwright.config.js) a proposito: es una pagina de PRUEBA y no
 * tiene que entrar en WebUI/dist (lo que embebe el plugin es lo que se envia).
 */

import { expect, test } from '@playwright/test';

import { ensureAudioClock } from './support/audioClock.js';
import { PROBE_URL } from './support/probePage.js';

/** Las cuatro agujas, leidas de los paths con la escala del arnes. */
const readNeedles = (page) => page.evaluate(() => window.__needles());

const allHidden = (needles) => Object.values(needles).every((needle) => !needle.visible);
const allVisible = (needles) => Object.values(needles).every((needle) => needle.visible);

/**
 * El corte de "pinta nivel" y el margen de las comparaciones de nivel.
 *
 * El margen no es cosmetico, y no es por gusto: el nivel no se lee del motor, se
 * DERIVA del `y` que la aguja escribe en el DOM, y ese `y` va con dos decimales
 * (`toFixed(2)`). El sustain del filtro lo empuja ESTA MISMA pagina a 0.2, asi
 * que y = 37.20 y el nivel sale (46 - 37.2) / 44 = 0.19999999999999993: una ULP
 * por DEBAJO de 0.2. Un `toBeGreaterThan(0.2)` caia ahi. El fallo no lo arregla ni
 * el motor ni la pagina: es la asercion pidiendo mas de lo que el propio test
 * acaba de pedir, y el numero que recibia lo decia sin dejar lugar a dudas.
 *
 * MEDIDO el 2026-10-02: fallo con ese numero exacto en el
 * NEURONiK_WebUiNeedleProbeE2e del check de gemelos, y 3/3 verde en las tres
 * vueltas siguientes. Salta cuando la lectura cae sobre el sustain ya asentado,
 * que es lo que pasa con la maquina cargada; por eso el `poll` de `settled` no
 * lo evita: espera a que las needles sean visibles y esten en el sustain, y en
 * ese momento ya estan en el valor que luego se va a medir.
 *
 * El margen es el MISMO que usan las dos aserciones de gemelidad de abajo (no
 * dos reglas para lo mismo), y no se come nada de lo que este test comprueba:
 * entre "pinta nivel" y "no pinta nada" hay un salto de 0.2 a 0, y 0.18 sigue de
 * sobra por encima de 0. Los valores concretos estan fijados despues (amp > 0.6,
 * filtro < 0.4, separacion > 0.3).
 */
const NIVEL_MIN = 0.2;      // el corte de "pinta nivel"
const TOLERANCIA = 0.02;    // el margen, en los dos sentidos

/**
 * Las cuatro visibles Y las dos envolventes ya en su SUSTAIN (amp 0.8, filtro
 * 0.2). Importa la segunda mitad: el primer frame visible es el pico del
 * attack, donde las dos needles valen ~1.0 y no distinguen nada — leer ahi dio
 * un fallo rojo el dia que se escribio esta asercion, y el motor estaba bien.
 */
const settled = (needles) => allVisible(needles)
  && needles.canvasAmp.level > 0.6
  && needles.canvasFilter.level < 0.4;

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

  // La nota entra por el MISMO mensaje que manda el teclado de la pagina. El
  // ADSR va ANTES: el motor lo toma en el updateParameters() del bloque que
  // sigue al push, asi que una nota sonada antes veria los defaults de C++.
  await page.locator('#voice-adsr').click();
  await page.locator('#note-on').click();

  // Sostenido: las CUATRO visibles (el meter trae envelopes=[amp, filter] y las
  // dos vistas pintan su aguja) y ya asentadas en su sustain.
  await expect.poll(() => readNeedles(page).then(settled), { timeout: 20_000, intervals: [200, 400, 800] })
    .toBe(true);

  const needles = await readNeedles(page);

  // Nivel alto (el sustain de amp vive en 0.8 tras el push del ADSR: una aguja
  // pintando la cola del attack no pasa este corte). El corte lleva la tolerancia
  // porque el nivel del filtro llega en 0.19999999999999993: ver NIVEL_MIN.
  for (const [name, needle] of Object.entries(needles)) {
    expect(needle.level, `nivel de ${name}`).toBeGreaterThan(NIVEL_MIN - TOLERANCIA);
  }

  // GEMELIDAD entre vistas: misma envolvente, mismo frame, mismo setLevel —
  // el desacuerdo solo podria salir del re-parseo del path (redondeo del 'd').
  expect(Math.abs(needles.canvasAmp.level - needles.blocksAmp.level)).toBeLessThanOrEqual(TOLERANCIA);
  expect(Math.abs(needles.canvasFilter.level - needles.blocksFilter.level)).toBeLessThanOrEqual(TOLERANCIA);

  // Y AHORA SI: las dos envolventes MIDEN lo que la pagina les pidio, no lo
  // que el motor traia de C++. Los sustains empujados son 0.8 (amp) y 0.2
  // (filtro). Sin el canal `neuronik:voice` las cuatro needles se quedarian
  // juntas en el 0.7 de los defaults de C++ y el poll de arriba no saldria
  // nunca: esta es la asercion que la limitacion de esta pagina prohibia.
  expect(needles.canvasAmp.level, 'sustain de amp pedido 0.8').toBeGreaterThan(0.6);
  expect(needles.canvasFilter.level, 'sustain de filtro pedido 0.2').toBeLessThan(0.4);
  expect(needles.canvasAmp.level - needles.canvasFilter.level,
         'las dos envolventes tienen que separarse').toBeGreaterThan(0.3);

  await page.locator('#note-off').click();

  // Release: la cola baja del suelo del dibujo y las cuatro se ocultan.
  await expect.poll(() => readNeedles(page).then(allHidden), { timeout: 20_000, intervals: [300, 600, 1_000] })
    .toBe(true);
});
