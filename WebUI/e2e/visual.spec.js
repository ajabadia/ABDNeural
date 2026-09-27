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
 *     una prueba anterior ni tema recordado.
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

  test('el lienzo entero con el tema claro', async ({ page }) => {
    await page.locator('.abd-theme-switcher__btn', { hasText: 'Light' }).click();
    await expect(page.locator('html')).toHaveAttribute('data-theme', 'light');
    await expect(page).toHaveScreenshot('lienzo-tema-claro.png', { animations: 'disabled' });
  });
});
