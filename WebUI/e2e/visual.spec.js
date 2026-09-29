/**
 * Regresion visual del LIENZO: una referencia por ficha (patron ABDMS2000).
 *
 * Que falta sin esto: el resto de la suite afirma que las cosas EXISTEN y que
 * dicen lo que deben (el DOM, los mensajes al worklet, el timbre), pero no que
 * se PINTEN bien. Un `min-width: 0` que se cae, un token de color que se
 * invierte, una etiqueta que se sale de su ficha o un `padding` que empuja el
 * boton fuera: nada de eso rompe una asercion de texto, y aun asi el lienzo queda
 * mal. Aqui cada ficha tiene una foto de referencia y se compara con el umbral
 * del `playwright.config.js` (100 px y 0.2 por pixel).
 *
 * DECISIONES, todas medidas y no supuestas:
 *
 *   - EL VIEWPORT ES EL TAMANO DE DISENO, y sale del SSOT: `CANVAS` de
 *     `src/contracts/sections.js`, no un numero escrito aqui. El lienzo es de
 *     diseno fijo y `mountFitStage` lo escala con `transform` para caber entero
 *     en el editor; con un viewport mayor la escala no seria 1 y la referencia
 *     seria una foto REESCALADA (borrosa, y dependiente del viewport del que
 *     regenero). Con el viewport igual al diseno la escala es exactamente 1 y la
 *     captura es 1:1. El spec lo EXIGE antes de capturar (ver `lienzoEnEscala1`):
 *     si el diseno cambia y el ajuste deja de ser identidad, falla con un
 *     mensaje que lo dice, en vez de dejar nueve referencias reescaladas.
 *   - SIN AUDIO, con estado de fabrica. El escenario tiene que ser el mismo en
 *     cada corrida: con el motor arrancado el medidor, el anillo y el LCD se
 *     mueven con el reloj del `AudioContext` y la foto seria distinta cada vez.
 *     Cada test recibe un contexto limpio, asi que tampoco hay memoria local de
 *     una prueba anterior ni tema recordado. (El UNICO bloque que se sale de esto
 *     es el de las agujas, al final del fichero, que captura con el motor
 *     arrancado y explica como se hace determinista.)
 *   - LA LISTA DE FICHAS SALE DEL CONTRATO (`SECTIONS`), no de una lista escrita
 *     a mano: una ficha nueva nace con su referencia que falta (y el test
 *     falla diciendo que la ha creado, que es el aviso de "miralo antes de
 *     aceptarlo"). Al reves, una ficha que desapareciera del DOM haria fallar su
 *     `toHaveCount(1)`: el fallo de un selector no puede quedarse en verde.
 *   - ADEMAS del lienzo entero. Las nueve fichas por separado cazarian un cambio
 *     de pintura DENTRO de una ficha, pero no un cambio de reparto (que ficha
 *     cae en otra banda, un alto que estira la fila de al lado): las fotos
 *     seguirian siendo identicas y el lienzo, otro. Una foto de la pagina
 *     entera cubre ese hueco por 1 archivo.
 *   - `animations: 'disabled'`: la pagina tiene una animacion de un solo tiro
 *     (el destello de la fila de ruta al saltar). Congelada a su estado final,
 *     que es donde esta en reposo.
 *
 * El tema claro se compara una sola vez y con la pagina entera: son los MISMOS
 * tokens con otros valores, asi que una referencia por ficha y por tema serian
 * dieciocho archivos para cazar lo que una sola foto ya dice.
 *
 * REGENERAR (solo tras un cambio INTENCIONAL de pintura):
 *   npx playwright test e2e/visual.spec.js --update-snapshots
 *
 * Las referencias se generaron en Chromium/Windows y hay que compararlas en
 * Windows: la pagina usa las fuentes del sistema (`system-ui`, sin webfont) y su
 * rasterizado cambia entre sistemas operativos. Un baseline de Windows sobre un
 * runner Linux fallaria por la fuente, no por el codigo.
 */

import { expect, test } from '@playwright/test';

import { CANVAS, SECTIONS } from '../src/contracts/sections.js';
import { ensureAudioClock } from './support/audioClock.js';
import { PROBE_URL } from './support/probePage.js';

/** La ficha del lienzo con ese id (el `data-section-id` que pinta ui/panel.js). */
const cardOf = (page, sectionId) => page.locator(`[data-section-id="${sectionId}"]`);

/**
 * El lienzo montado y EN ESCALA 1. Un viewport mayor que el diseno no esta
 * prohibido (el editor se puede agrandar), pero entonces la foto seria
 * reescalada: por eso se mide la escala en vez de suponerla.
 */
async function lienzoEnEscala1(page) {
  const transform = await page.evaluate(() => document.getElementById('app')?.style.transform ?? '');

  expect(
    transform,
    'el lienzo deberia estar en escala 1 (identidad, sin transform): si el tamano de diseno ha '
    + 'cambiado, el viewport de este spec tiene que seguir a CANVAS y las referencias regenerarse',
  ).toBe('');
}

test.describe('NEURONiK Visual Regression - el lienzo', () => {
  // El viewport IGUAL al tamano de diseno (el SSOT de sections.js).
  test.use({ viewport: { width: CANVAS.width, height: CANVAS.height } });

  let pageErrors = [];

  test.beforeEach(async ({ page }) => {
    pageErrors = [];
    page.on('pageerror', (error) => pageErrors.push(error.message));

    await page.goto('/');
    await page.waitForFunction(() => window.__pilotReady === true);
    await lienzoEnEscala1(page);
  });

  test.afterEach(() => {
    // Una pagina que lanzo un error puede haber dejado el lienzo a medias, y una
    // foto de eso REGENERARIA la referencia del fallo. Primero el error.
    expect(pageErrors, 'la pagina no debe lanzar errores').toEqual([]);
  });

  test('el lienzo entero', async ({ page }) => {
    await expect(page).toHaveScreenshot('lienzo.png', { animations: 'disabled' });
  });

  for (const section of SECTIONS) {
    test(`ficha ${section.id}`, async ({ page }) => {
      const card = cardOf(page, section.id);

      // Contar ANTES de comparar: un selector que no encuentra la ficha no puede
      // quedarse en verde comparando el vacio.
      await expect(card).toHaveCount(1);
      await expect(card).toHaveScreenshot(`${section.id}.png`, { animations: 'disabled' });
    });
  }

  // Los CAJONES. Una foto por ficha NO los cubre, y despues del rack FX (cajon
  // de EFECTOS, 2026-09-29) hay superficies nuevas que se pueden pintar mal sin
  // que ninguna referencia se entere: un modulo con el tema de la familia
  // equivocado, un hueco con tres knobs muertos, un texto que se sale.
  //
  // La lista sale del CONTRATO (`section.drawer`), como la de las fichas: un
  // cajon nuevo nace con su referencia que falta, y el fallo lo dice.
  for (const section of SECTIONS.filter((candidate) => candidate.drawer)) {
    test(`cajon ${section.id}`, async ({ page }) => {
      await page.locator(`[data-drawer-trigger="${section.id}"]`).click();

      // Contar ANTES de comparar, por el mismo motivo que las fichas: un
      // selector que no encuentra el cajon no puede quedarse en verde.
      const drawer = page.locator(`#drawer-${section.id}`);

      await expect(drawer).toHaveCount(1);
      await expect(drawer).toHaveScreenshot(`cajon-${section.id}.png`, { animations: 'disabled' });
    });
  }

  test('el lienzo entero con el tema claro', async ({ page }) => {
    await page.locator('.abd-theme-switcher__btn', { hasText: 'Light' }).click();
    await expect(page.locator('html')).toHaveAttribute('data-theme', 'light');
    await expect(page).toHaveScreenshot('lienzo-tema-claro.png', { animations: 'disabled' });
  });
});

/**
 * LAS AGUJAS SOSTENIDAS (needle-probe): el unico bloque de esta suite que
 * necesita AUDIO, y por eso rompe la regla de "sin audio, con estado de fabrica"
 * de arriba. Se sostiene porque lo que se captura esta ASENTADO:
 *
 *   - las dos envolventes con sustains pushed por la pagina (0.8 la de amp, 0.2
 *     la de filtro) y las dos vistas repintadas con ESE MISMO estado, asi que
 *     cada aguja tiene que caer en la altura del sustain de su propia curva —una
 *     foto a media convergencia no diria nada;
 *   - el `d` de la aguja sale de `envelopeNeedlePath`, que escribe la Y con
 *     `toFixed(2)`: en el sustain los ultimos bits del float no llegan al
 *     pixel, asi que la foto es estable entre corridas;
 *   - la DOBLE lectura antes de disparar (dos muestras consecutivas iguales a dos
 *     decimales) no deja que una referencia se genere con la envolvente todavia
 *     moviendose;
 *   - se fotografian los DOS elementos de las vistas, no la pagina entera: el
 *     texto del estado lleva los Hz del dispositivo de audio (44100 en un
 *     portatil, 48000 en la bancada) y haria la referencia dependiente de la
 *     maquina.
 *
 * `needleProbe.spec.js` afirma lo mismo en numero (amp > 0.6, filtro < 0.4): alli
 * la asercion es que el VALOR llegue al motor, aqui lo que se fija es que se
 * PINTE en su sitio. Las dos hacen falta: un motor correcto con la aguja mal
 * escalada sale verde en el otro.
 */
test.describe('NEURONiK Visual Regression - las agujas (needle-probe)', () => {
  let pageErrors = [];

  test.beforeEach(async ({ page }) => {
    pageErrors = [];
    page.on('pageerror', (error) => pageErrors.push(error.message));

    await page.goto(PROBE_URL);
    await page.waitForFunction(() => window.__probeReady === true);
  });

  test.afterEach(() => {
    // Mismo motivo que en el lienzo: una pagina con errores puede haber dejado
    // las vistas a medias, y la foto de eso REGENERARIA la referencia.
    expect(pageErrors, 'la pagina no debe lanzar errores').toEqual([]);
  });

  test('las cuatro agujas, SOSTENIDAS en el sustain de su curva', async ({ page }) => {
    const attempts = await ensureAudioClock(page);

    test.skip(!attempts, 'el reloj de audio de Chromium no avanza en este entorno (sin salida de audio)');

    await page.locator('#start').click();
    await expect(page.locator('#status')).toHaveAttribute('data-status', 'ready', { timeout: 20_000 });

    // El ADSR primero (el motor lo toma en el bloque siguiente) y la nota
    // despues: al revés, la foto seria de una nota sonada con los defaults.
    await page.locator('#voice-adsr').click();
    await page.locator('#note-on').click();

    const readNeedles = () => page.evaluate(() => window.__needles());
    // Los valores EXACTOS que define la foto, con un margen de 0.03 (la
    // exponencial se acerca al sustain por debajo y el `d` se redondea a dos
    // decimales): no es solo "visible", es "en la altura que la pagina pidio",
    // que es lo que hace que la referencia signifique algo.
    const settled = (needles) => Object.values(needles).every((needle) => needle.visible)
      && Math.abs(needles.canvasAmp.level - 0.8) < 0.03
      && Math.abs(needles.canvasFilter.level - 0.2) < 0.03;

    await expect.poll(() => readNeedles().then(settled), { timeout: 20_000, intervals: [200, 400, 800] })
      .toBe(true);

    // DOS MUESTRAS CONSECUTIVAS IGUALES antes de disparar. Un `waitForTimeout`
    // fijo aqui no vale: el poll de arriba sale en cuanto el sustain esta dentro
    // del margen, y la envolvente puede converge un frame mas tarde (la
    // referencia se regeneraba con 0.81/0.24 en vez de 0.80/0.20). Se comparan a
    // dos decimales porque es lo que acaba en el `d` de la aguja.
    let previous = null;
    const quiet = async () => {
      const levels = await page.evaluate(() => Object.fromEntries(
        Object.entries(window.__needles()).map(([name, n]) => [name, n.level.toFixed(2)])));

      const same = previous !== null && JSON.stringify(levels) === JSON.stringify(previous);
      previous = levels;
      return same;
    };

    await expect.poll(quiet, { timeout: 20_000, intervals: [300, 500] })
      .toBe(true);

    // `maxDiffPixels: 0`, MAS ESTRICTO que el de la config (20): al contrario que
    // el resto de la suite, aqui el estado de la foto no depende del instante en
    // que se dispara sino del valor al que converge la envolvente, y ese valor
    // entra en el `d` con `toFixed(2)`. Medido: dos corridas seguidas dan 0 pixeles
    // de diferencia con el motor de verdad sonando, y con la foto a media
    // convergencia la referencia se generaba con 0.81/0.24 (un fallo rojo del
    // guard de dos muestras, no del motor). Si algun dia esto se pone rojo de
    // forma intermitente, la causa es el reloj de audio, no la pintura.
    await expect(page.locator('.env-curves'))
      .toHaveScreenshot('aguja-curvas.png', { animations: 'disabled', maxDiffPixels: 0 });
    await expect(page.locator('.env-blocks'))
      .toHaveScreenshot('aguja-bloques.png', { animations: 'disabled', maxDiffPixels: 0 });
  });
});
