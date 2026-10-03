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
import { compruebaPaqueteCompartido, mensajeDelGuard } from './support/sharedPackage.js';

/** La ficha del lienzo con ese id (el `data-section-id` que pinta ui/panel.js). */
const cardOf = (page, sectionId) => page.locator(`[data-section-id="${sectionId}"]`);

/**
 * ABRIR LA PAGINA, Y FALLAR PRONTO SI NO ABRE.
 *
 * Las dos esperas de este spec —la navegacion y la bandera de arranque— llevan
 * un timeout CORTO y PROPIO, no el del test. El del test son 60 s, y el fallo
 * que llega con el es `Test timeout of 60000ms exceeded`: no dice que fallo, ni
 * donde, ni por que. Con diecisiete tests, eso son hasta 17 minutos de runner
 * para aprender lo mismo diecisiete veces.
 *
 * Ademas separa lo que eran DOS fallos distintos y que antes de aqui salian
 * como el mismo:
 *
 *   - que el `goto` no termine es que no hay servidor, o no hay documento;
 *   - que cargue y la bandera no llegue es que la pagina arranco y la app NO.
 *
 * El segundo es el fallo frecuente de verdad, y el que mas dice si se lee bien,
 * porque cuando la app no arranca casi siempre hay un `pageerror` detras —el
 * paquete compartido sin un componente, un import que no resuelve, el .wasm que
 * no se bajo—. Ese error ya se estaba recogiendo en `pageErrors`, y antes NO
 * SE VEIA NUNCA: el `beforeEach` reventaba el primero y el `afterEach` no
 * llegaba a contarlo. El diagnostico estaba ahi y lo tumbaba el timeout.
 *
 * Los numeros no son inventados. En una corrida buena el arranque va en torno
 * al segundo: los tests de esta suite tardan entre 1.8 y 6.0 s COMPLETOS, con la
 * foto dentro. 15 s para navegar y 10 s para arrancar son un orden de magnitud
 * de margen sobre eso, y siguen siendo tres veces mas rapidos que el timeout
 * del test, que es justo lo que se pedia.
 */
const NAVEGACION_MS = 15_000;
const ARRANQUE_MS = 10_000;

/**
 * Los `pageerror` recogidos, o un aviso claro de que no hubo ninguno: que no
 * haya errores tambien es informacion, y "no dice nada" se lee distinto de
 * "no fallo por un error de pagina".
 */
function diagnostico(pageErrors, original) {
  const errores = (pageErrors ?? []).slice(0, 3);
  const listado = errores.length > 0
    ? errores.map((e) => `  - ${e}`).join('\n')
    : '  (la pagina no ha lanzado ningun pageerror: el fallo es de otra cosa)';

  const cola = original?.message ? `\nY el goto fallo con: ${original.message}` : '';
  return `Errores de la pagina:\n${listado}${cola}`;
}

/**
 * Navega y espera a la bandera de arranque, con timeouts propios y un mensaje
 * que diga QUE fallo. `arranca` es la funcion que devuelve `true` cuando la
 * pagina esta lista; `pageErrors` es el array del `beforeEach`.
 */
async function abreYEsperaArranque(page, url, arranca, pageErrors) {
  let response;

  try {
    response = await page.goto(url, { timeout: NAVEGACION_MS });
  } catch (error) {
    throw new Error(
      `la pagina no ha cargado: ${url} no ha respondido en ${NAVEGACION_MS} ms. `
      + 'Si el server de `vite preview` no levanta, esto es lo que se ve, y NO es '
      + 'lo mismo que una pagina que carga y una app que no arranca.\n'
      + diagnostico(pageErrors, error),
    );
  }

  // Un `goto` a un 404 responde con EXITO: `goto` no lanza por un estado de
  // error, solo por no poder llegar. Sin esta linea, un 404 seria un "la app no
  // ha arrancado" que no es de la app.
  if (response && !response.ok())
    throw new Error(`la pagina responde ${response.status()} ${response.statusText()} en ${url}`);

  try {
    await page.waitForFunction(arranca, null, { timeout: ARRANQUE_MS });
  } catch {
    throw new Error(
      `la pagina ha cargado pero la app no ha arrancado en ${ARRANQUE_MS} ms: nadie ha `
      + 'puesto su bandera de listo. El servidor responde y el documento existe, asi '
      + 'que el problema no es de red.\n'
      + diagnostico(pageErrors),
    );
  }
}

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

  // EL PAQUETE COMPARTIDO, o por que esta suite puede estar en verde sin medir.
  //
  // La WebUI se pinta con el mueble de `@abdsynths/shared`, que NO se baja de
  // ningun registro: pnpm lo enlaza al arbol del hermano ABDSharedAssets. Un
  // enlace puede existir y no servir, y entonces las capturas se compararian
  // contra un lienzo al que le falta un componente: la referencia se regenera con
  // el hueco, la prueba pasa, y el hueco entra en el repo como si fuera el
  // diseño. Medido el 2026-09-29: `components/envelopeCurve.js`,
  // `components/envelopeGestures.js`, `components/envelopePad.js` y
  // `styles/components/envelope.css` estan en el disco de ABDSharedAssets y en
  // NINGUN commit, de modo que un clon limpio -el del runner de CI- se queda sin
  // ellos.
  //
  // Va en el `beforeEach` y no en un `beforeAll` a proposito: un hook que lanza
  // marca el fallo de su test, pero los tests siguientes siguen y cada uno
  // falla por su cuenta con su propio `toHaveScreenshot`, que es un informe
  // entero de capturas para explicar que lo que falta es el paquete. Fallando
  // DENTRO del test, ese test no captura nada y el nombre del fallo lo dice.
  // Cuesta unos pocos `require.resolve` por test, nada al lado de Chromium.
  test.beforeEach(async ({ page }) => {
    // El guard primero: si el mueble no esta, no tiene sentido arrancar el
    // navegador para sacar fotos de un lienzo que no es el de este commit.
    const guard = await compruebaPaqueteCompartido();
    expect(guard.ok, mensajeDelGuard(guard)).toBe(true);
    for (const aviso of guard.avisos ?? []) {
      console.warn(`[guard del paquete compartido] ${aviso}`);
    }

    pageErrors = [];
    page.on('pageerror', (error) => pageErrors.push(error.message));

    await abreYEsperaArranque(page, '/', () => window.__pilotReady === true, pageErrors);
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

test.describe('NEURONiK Regresion visual - el arco de los knobs', () => {
  test.use({ viewport: { width: CANVAS.width, height: CANVAS.height } });

  // `pageErrors` es local de cada `describe` a proposito, y no un modulo: un
  // array compartido entre bloques haria que un test leyera los errores de otro.
  let pageErrors = [];

  test.beforeEach(async ({ page }) => {
    pageErrors = [];
    page.on('pageerror', (error) => pageErrors.push(error.message));

    await abreYEsperaArranque(page, '/', () => window.__pilotReady === true, pageErrors);
    await lienzoEnEscala1(page);
  });

  test.afterEach(() => {
    // Mismo motivo que en el resto de la suite: una pagina que ha lanzado un
    // error puede haber dejado los arcos a medias, y el recuento de pixeles
    // de arriba no lo distinguiria de un arco que no pinta.
    expect(pageErrors, 'la pagina no debe lanzar errores').toEqual([]);
  });

  test('todo arco con valor pinta pixeles, y el del origen no pinta ninguno', async ({ page }) => {
    // ── POR QUE ESTE TEST Y NO UNO DE FOTOS ──
    //
    // El arco de valor estuvo roto un tiempo y NADIE se entero, y la razon de
    // que la regresion visual no lo cazara es la unica razon de que este test
    // exista: una referencia ARCHIVA como es la pantalla rota. El arco se
    // dibujaba girado, caia fuera del viewBox, y eso no es "un pixel mas o
    // menos": es la foto correcta de un arco que no esta. Once referencias
    // commiteadas con el hueco y la suite en verde, que es exactamente el modo
    // de fallo que un test de fotos no puede ver por construccion.
    //
    // Este mira lo contrario: no COMPARA con una referencia, comprueba que el
    // arco PINTE. Un arco que no pinta da 0 y el test se va.
    //
    // ── COMO SE CUENTA, Y POR QUE CON DOS FOTOS ──
    //
    // Hay una foto con los arcos como estan y otra con TODOS los arcos ocultos
    // (`display: none`). Los pixeles que cambian entre las dos, dentro de la
    // caja de un arco, son exactamente los pixeles de ESE arco: la pista, el
    // puntero y el resto de la ficha son los mismos en las dos fotos, asi que
    // se restan solos.
    //
    // Un "cuantos pixeles tiene el color del arco" habria sido mas corto, y
    // habria roto con el tema claro y con cualquier cambio de token. Comparar
    // contra si mismo no sabe que color es el acento, y por eso aguanta.
    //
    // Y no hace falta ningun decodificador de PNG en Node: las dos fotos se
    // devuelven a la pagina como data URL y se leen con un canvas. Lo que se
    // compara lo pinta el MISMO navegador que pinta la pantalla, que es lo
    // unico que importa aqui: el bug era CSS contra atributo SVG, y solo un
    // renderizador de verdad lo reproduce.

    const cajas = await page.$$eval('.abd-knob__arc', (arcos) => arcos
      .map((arco) => {
        const r = arco.getBoundingClientRect();

        return {
          // `display: none` lo pone el propio componente cuando el trazo es de
          // largo cero, que en unipolar es el valor 0 y en bipolar el 0.5. Es
          // la verdad de si DEBERIA pintar, y la lee el componente, no el test.
          deberia: arco.style.display !== 'none',
          largo: arco.style.strokeDasharray,
          x: r.x, y: r.y, w: r.width, h: r.height,
        };
      })
      .filter((c) => c.w > 0 && c.h > 0));

    // Sin knobs visibles el test pasaria sin comprobar nada, que es la forma
    // mas silenciosa de tener un test roto. Se dice en el fallo.
    expect(cajas.length, 'no hay ningun arco en el lienzo: el selector cambio').toBeGreaterThan(0);

    const fotoConArcos = (await page.screenshot()).toString('base64');

    await page.$$eval('.abd-knob__arc', (arcos) => {
      for (const arco of arcos) arco.style.display = 'none';
    });
    const fotoSinArcos = (await page.screenshot()).toString('base64');

    const pintados = await page.evaluate(async ({ a, b, cajas: lista }) => {
      const cargar = async (b64) => {
        const img = new Image();
        img.src = `data:image/png;base64,${b64}`;
        await img.decode();
        const canvas = document.createElement('canvas');
        canvas.width = img.naturalWidth;
        canvas.height = img.naturalHeight;
        const ctx = canvas.getContext('2d', { willReadFrequently: true });
        ctx.drawImage(img, 0, 0);
        return { px: ctx.getImageData(0, 0, canvas.width, canvas.height).data, w: canvas.width, h: canvas.height };
      };

      const conArcos = await cargar(a);
      const sinArcos = await cargar(b);

      return lista.map((c) => {
        // Una caja fuera de la foto no se puede contar: recortarla daria un
        // cero que parece "no pinta" y no lo es. Se recortan las que se salen
        // de la foto y se dicen aparte, que es un problema de otro.
        const x0 = Math.max(0, Math.floor(c.x));
        const y0 = Math.max(0, Math.floor(c.y));
        const x1 = Math.min(conArcos.w, Math.ceil(c.x + c.w));
        const y1 = Math.min(conArcos.h, Math.ceil(c.y + c.h));

        if (x1 <= x0 || y1 <= y0) return { ...c, pixeles: -1 };

        let n = 0;
        for (let y = y0; y < y1; y += 1) {
          for (let x = x0; x < x1; x += 1) {
            const i = (y * conArcos.w + x) * 4;
            if (conArcos.px[i] !== sinArcos.px[i]
              || conArcos.px[i + 1] !== sinArcos.px[i + 1]
              || conArcos.px[i + 2] !== sinArcos.px[i + 2])
              n += 1;
          }
        }

        return { ...c, pixeles: n };
      });
    }, { a: fotoConArcos, b: fotoSinArcos, cajas });

    const fuera = pintados.filter((c) => c.pixeles === -1);
    const medidos = pintados.filter((c) => c.pixeles >= 0);

    // Los que TIENEN que pintar y no pintan. Este es el fallo que se busca.
    const mudos = medidos.filter((c) => c.deberia && c.pixeles === 0);

    // Los que NO tienen que pintar y pintan. Comprueba la otra mitad del
    // `display: none` del componente: un arco de largo cero con `linecap: round`
    // pinta el remate, un punto, y en bipolar ese punto cae EN EL CENTRO,
    // justo donde esta la marca del cero, y ahi un punto se lee como un valor.
    const fantasma = medidos.filter((c) => !c.deberia && c.pixeles > 0);

    const detalle = (lista) => lista.map((c) =>
      `  x=${Math.round(c.x)} y=${Math.round(c.y)} ${Math.round(c.w)}x${Math.round(c.h)}`
      + ` largo="${c.largo ?? ''}" pixeles=${c.pixeles}`).join('\n');

    expect(
      mudos.length === 0,
      `${mudos.length} arco(s) con valor NO pintan ni un pixel. Un arco de valor que no `
      + 'pinta es el bug que archivaron las referencias: si la foto queda igual con el arco '
      + 'que sin el, el arco no esta en la pantalla. Revisa que la rotacion de la ventana '
      + `vaya por CSS y no por atributo.\n${detalle(mudos)}`,
    ).toBe(true);

    expect(
      fantasma.length === 0,
      `${fantasma.length} arco(s) sin valor pintan pixeles. deberia ser un trazo de largo `
      + `cero, y con linecap round el remate se ve. En bipolar cae en el centro.\n${detalle(fantasma)}`,
    ).toBe(true);

    // ── LOS QUE NO SE CUENTAN, Y POR QUE NO ES UN FALLO ──
    //
    // Hay arcos en el DOM que caen fuera del lienzo: los de los cajones, que
    // viven a la derecha y solo entran en pantalla al abrir el cajon. Medido:
    // quince, entre x=1458 y x=2003, con el lienzo en 1440. Un recorte de foto
    // no puede contarlos, y contarlos como cero seria inventarse un fallo.
    //
    // Un arco fuera de la foto NO es un arco roto: es un arco que no se esta
    // mirando. Por eso se cuentan aparte y se quedan fuera del veredicto.
    //
    // Lo que si se exige es que quede ALGO medido, para que el test no pueda
    // degradarse a pasar sin comprobar nada —que es como se rompe un test— si
    // el selector o el layout cambian.
    if (fuera.length > 0)
      console.log(`[arcos] ${fuera.length} fuera del lienzo, no contados (cajones cerrados)`);

    expect(
      medidos.length,
      `solo ${medidos.length} arcos medibles de ${pintados.length}: el lienzo no tiene arcos `
      + 'a la vista y este test pasaria sin comprobar nada',
    ).toBeGreaterThan(0);
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

    await abreYEsperaArranque(page, PROBE_URL, () => window.__probeReady === true, pageErrors);
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
