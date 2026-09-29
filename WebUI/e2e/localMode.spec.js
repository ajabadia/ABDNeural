/**
 * Smoke E2E del MODO LOCAL, automatizado (Chromium de verdad).
 *
 * Es la version en test del protocolo que hasta hoy se verificaba A MANO y se dejaba
 * escrito en HANDOFF: sin host (`window.__JUCE__` ausente), SOUND ON arranca el motor
 * WASM del navegador, la ficha RANURAS carga un .neuronikmodel, una tecla enciende la
 * voz y el pad mueve morphX/morphY con su anillo bailando. Cada uno de esos pasos deja
 * una huella en el DOM o en los mensajes que la pagina manda al worklet, y eso es lo
 * que se afirma aqui.
 *
 * Lo que SOLO se puede comprobar en un navegador (y por eso no esta en vitest):
 *   - el AudioContext + AudioWorklet arrancan de verdad y el worklet PROCESA;
 *   - el estado de la pagina cruza la frontera (`neuronik:params`, `neuronik:models`,
 *     ...): el agujero que dejo la pagina muda al llegar a `ready`;
 *   - el anillo del pad se mueve con el LFO sembrado en MODO LOCAL;
 *   - la memoria de ranuras sobrevive a un F5 (localStorage real, no un mock).
 *
 * Trampas del entorno, medidas (no supuestas):
 *   1. `--mute-audio` (en playwright.config.js): sin el, el reloj del AudioContext no
 *      avanza y el worklet no procesa nunca;
 *   2. el servicio de audio tarda en arrancar, asi que el test ESPERA a que su reloj
 *      avance (`ensureAudioClock`) antes de pedir audio; si no arranca, los tests que
 *      necesitan procesar se SALTAN con el motivo (no se finge un verde).
 */

import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { expect, test } from '@playwright/test';

import { ensureAudioClock } from './support/audioClock.js';

const here = path.dirname(fileURLToPath(import.meta.url));

/** Un modelo REAL del banco CZ101 (el mismo que carga el smoke a mano). */
const MODEL = path.resolve(here, '../../Assets/Models/CZ-BASS1.neuronikmodel');

/**
 * Espia de la frontera pagina -> worklet: guarda lo que la pagina POSTEA (para saber
 * si su estado sale) y lo que el worklet REPORTA en el meter (voz y modulacion).
 * Se instala ANTES de que la pagina cargue, asi que caza el primer mensaje.
 */
function workletProbe() {
  window.__neuronikSent = [];
  window.__neuronikMeter = [];

  const originalPost = MessagePort.prototype.postMessage;

  MessagePort.prototype.postMessage = function patched(message) {
    window.__neuronikSent.push(message);
    return originalPost.call(this, message);
  };

  const OriginalNode = window.AudioWorkletNode;

  window.AudioWorkletNode = class extends OriginalNode {
    constructor(...args) {
      super(...args);
      this.port.addEventListener('message', (event) => {
        if (event.data?.type === 'neuronik:meter') window.__neuronikMeter.push(event.data);
      });
    }
  };
}

/** Abre el cajon de una ficha (sus controles no viven en el lienzo). */
async function openDrawer(page, sectionId) {
  await page.locator(`[data-drawer-trigger="${sectionId}"]`).click();
}

/** SOUND ON y espera al badge del motor listo. */
async function startLocalEngine(page) {
  await page.locator('.audio-start').click();
  await expect(page.locator('.audio-mode__detail')).toHaveText(/· ON · [\d.]+ kHz/);
}

/**
 * El arco de modulacion del anillo del pad: `start` = dashoffset, `span` = dasharray.
 *
 * El muestreo va DENTRO de la pagina, con `requestAnimationFrame`, y por dos
 * razones. La primera es el coste: antes cada muestra era un `page.evaluate`
 * desde Node, y bajo carga (el E2E roda en paralelo con los otros dos specs y con
 * el resto de ctest) el viaje de ida y vuelta ya no cabia en los 100 ms de
 * espera, asi que en 1,5 s entraban cuatro o cinco muestras en vez de quince.
 *
 * La segunda, y la que motivó el arreglo: la ventana era de RELOJ DE PARED
 * (`Date.now() + ms`) mientras que lo que se mueve —el LFO del motor— corre en
 * el reloj del AudioContext. La asercion decia de facto "en 1,5 s de pared hay
 * mas de cuatro estados distintos", que no es lo que queremos medir: queremos
 * "el anillo se mueve". Con la maquina cargada la ventana se le acababa al
 * LFO, no al anillo, y la prueba caia sin motivo real. Ahora la ventana es una
 * CONDICION con un techo generoso como red de seguridad.
 */
/**
 * Muestrea el arco hasta que aparecen `want` estados DISTINTOS (o se agota el
 * techo). Devuelve una muestra por estado: lo que se compara al final.
 *
 * El lector del arco va DENTRO de cada `evaluate` a proposito: la funcion se
 * serializa y no puede cerrar sobre nada de este modulo.
 *
 * Que devuelva menos de `want` es información, no un fallo mudo: el mensaje de
 * la aserción lo dice ("solo N estados en T s"), y con eso se distingue "el
 * anillo no se mueve" de "el entorno no rinde".
 */
async function sampleRingUntil(page, want = 8, budgetMs = 20000) {
  const states = await page.evaluate(async ({ objetivo, techo }) => {
    const read = () => {
      const arc = document.querySelector('.zring-mod');
      const span = Number((arc?.getAttribute('stroke-dasharray') ?? '0').split(' ')[0] ?? 0);

      return { start: Number(arc?.getAttribute('stroke-dashoffset') ?? 0), span };
    };

    const vistos = new Map();
    const t0 = performance.now();

    while (performance.now() - t0 < techo) {
      await new Promise((resolve) => requestAnimationFrame(resolve));

      const muestra = read();
      vistos.set(`${muestra.start}|${muestra.span}`, muestra);

      if (vistos.size >= objetivo) break;
    }

    return [...vistos.values()];
  }, { objetivo: want, techo: budgetMs });

  return { states, enough: states.length };
}

/**
 * Muestrea el arco durante un numero FIJO de frames de dibujo, para comprobar
 * que NO se mueve. Aqui la cuenta de frames es lo que da determinismo: si la
 * maquina va lenta, cada frame tarda mas y la muestra dura mas de reloj de
 * pared —que es lo correcto para "el anillo sigue quieto"—, pero siempre se
 * miran los mismos 90 frames en vez de "los que quepan en 1,5 s".
 */
async function sampleRingFrames(page, frames = 90) {
  return page.evaluate(async (ticks) => {
    const muestras = [];

    for (let i = 0; i < ticks; i += 1) {
      await new Promise((resolve) => requestAnimationFrame(resolve));

      const arc = document.querySelector('.zring-mod');
      const span = Number((arc?.getAttribute('stroke-dasharray') ?? '0').split(' ')[0] ?? 0);

      muestras.push({ start: Number(arc?.getAttribute('stroke-dashoffset') ?? 0), span });
    }

    return muestras;
  }, frames);
}

/** El pad del lienzo (el unico `.xy-pad` montado: el del cajon solo existe al abrirlo). */
const canvasPad = (page) => page.locator('.xy-pad').first();

let pageErrors = [];

test.beforeEach(async ({ page }) => {
  pageErrors = [];
  page.on('pageerror', (error) => pageErrors.push(error.message));

  await page.addInitScript(workletProbe);
  await page.goto('/');
  await page.waitForFunction(() => window.__pilotReady === true);
});

test.afterEach(() => {
  // Un error de JS en la pagina es un fallo del smoke aunque las aserciones pasen.
  expect(pageErrors, 'la pagina no debe lanzar errores').toEqual([]);
});

test('SOUND ON arranca el motor WASM y el estado de la pagina LLEGA al worklet', async ({ page }) => {
  await expect(page.locator('.audio-mode__label'))
    .toHaveText('AUDIO: motor local del navegador (WASM)');
  await expect(page.locator('.audio-mode__detail')).toHaveText('· sin arrancar');

  await startLocalEngine(page);

  // El boton desaparece cuando el motor manda (si fallara, volveria como REINTENTAR).
  await expect(page.locator('.audio-start')).toBeHidden();

  // La ruta de la MATRIZ que la pagina siembra en local se VE en el resumen. Cada
  // fila del resumen es un boton: el texto suelto del div va sin espacios.
  await expect(page.getByRole('button', { name: '3 LFO 2 → Morph Z 1.00' })).toBeVisible();

  // ...y LLEGA al motor: es el unico camino (no hay APVTS), asi que sin este mensaje
  // el worklet sonaria con los defaults del struct aunque el badge diga ON.
  await expect.poll(() => page.evaluate(() => {
    const params = (window.__neuronikSent ?? []).filter((message) => message?.type === 'neuronik:params');
    const last = params.at(-1);

    if (!last) return null;

    // La fila 3 de la matriz: fuente 2 (LFO 2), destino 28 (Morph Z), amount 1.
    return [28, 29, 30].map((field) => last.fields.find(([id]) => id === field) ?? null);
  })).toEqual([[28, 2], [29, 28], [30, 1]]);

  // El motor tambien recibe el modelo de motor y el morph del pad: el re-sync completo.
  const types = await page.evaluate(() => [...new Set((window.__neuronikSent ?? []).map((message) => message?.type))]);

  expect(types).toEqual(expect.arrayContaining(['neuronik:engine', 'neuronik:morph', 'neuronik:params']));
});

test('el anillo del pad BAILA con la ruta sembrada (LFO 2 -> Morph Z)', async ({ page }) => {
  const attempts = await ensureAudioClock(page);

  test.skip(!attempts, 'el reloj de audio de Chromium no avanza en este entorno (sin salida de audio)');

  await startLocalEngine(page);

  const { states: samples } = await sampleRingUntil(page, 12);
  const spans = samples.map((sample) => sample.span);
  const distinct = new Set(samples.map((sample) => `${sample.start}|${sample.span}`));

  // Baila: el arco cambia de un frame a otro (meter ~85 ms) y se abre de verdad.
  // El mensaje lleva la cuenta para que un fallo diga si el anillo no se movio
  // o si el entorno no dio los frames.
  expect(distinct.size,
    `el arco solo dio ${distinct.size} estado(s) distinto(s) en el techo de muestreo`)
    .toBeGreaterThan(4);
  expect(Math.max(...spans)).toBeGreaterThan(20);

  // Y muestra las DOS semiondas: horario nace en la base (offset 0), antihorario
  // TERMINA en ella (offset negativo). Es el contrato del arco con signo.
  expect(samples.some((sample) => sample.start === 0)).toBe(true);
  expect(samples.some((sample) => sample.start <= -20)).toBe(true);

  // La modulacion tambien viaja en el meter (la fuente del anillo en local).
  const mods = await page.evaluate(() => (window.__neuronikMeter ?? []).map((frame) => frame.morphZMod));
  expect(Math.max(...mods)).toBeGreaterThan(0.5);
  expect(Math.min(...mods)).toBeLessThan(-0.5);
});

test('una nota enciende la voz y la apaga al soltar', async ({ page }) => {
  const attempts = await ensureAudioClock(page);

  test.skip(!attempts, 'el reloj de audio de Chromium no avanza en este entorno (sin salida de audio)');

  await startLocalEngine(page);

  const meter = page.locator('.voice-meter');

  // Sin nota el medidor esta quieto (atributo `hidden`, y sin leds encendidos).
  await expect(meter).toHaveAttribute('hidden', '');
  await expect(page.locator('.voice-meter__led[data-active="true"]')).toHaveCount(0);

  const key = page.locator('[data-note="48"]').first();
  const box = await key.boundingBox();

  expect(box, 'la tecla 48 debe estar en el teclado del lienzo').not.toBeNull();

  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();

  // Sostenida: UNA voz activa, y el medidor fuera de `hidden`.
  await expect(meter).toHaveAttribute('aria-label', 'PANIC: parar 1 voz activa');
  await expect(page.locator('.voice-meter__led[data-active="true"]')).toHaveCount(1);

  await page.mouse.up();

  // Soltada: la voz se apaga (la release de la envolvente tarda un poco; el timeout
  // generoso es del expect, no un sleep).
  await expect(meter).toHaveAttribute('aria-label', /parar 0 voces activas/, { timeout: 15_000 });
  await expect(meter).toHaveAttribute('hidden', '');
});

test('la ranura A carga un modelo real, llega al motor y sobrevive al F5', async ({ page }) => {
  await startLocalEngine(page);

  // Las cuatro ranuras viven en el cajon de MODELOS (en el lienzo solo queda el pad).
  await openDrawer(page, 'models');

  // El dialogo lo abre el input oculto de la pagina: Playwright lo intercepta.
  const chooser = page.waitForEvent('filechooser');

  await page.locator('[data-slot="0"] .model-slots__load').click();
  await (await chooser).setFiles(MODEL);

  await expect(page.locator('[data-slot="0"] .model-slots__name')).toHaveText('CZ-BASS1');
  await expect(page.locator('.model-slots__status')).toHaveText('1/4 cargados');

  // Y el modelo cruza al worklet (la ficha pintada no basta: tiene que sonar).
  await expect.poll(() => page.evaluate(() => {
    const messages = (window.__neuronikSent ?? []).filter((message) => message?.type === 'neuronik:models');
    const slot = messages.at(-1)?.slots?.find((entry) => entry?.slot === 0);

    return slot ? { name: slot.name, isValid: slot.isValid, first: slot.amplitudes[0] } : null;
  })).toEqual({ name: 'CZ-BASS1', isValid: true, first: 1 });

  // F5: el navegador no tiene preset que devuelva las ranuras, solo su memoria local.
  await page.reload();
  await page.waitForFunction(() => window.__pilotReady === true);

  await expect(page.locator('[data-slot="0"] .model-slots__name')).toHaveText('CZ-BASS1');
  await expect(page.locator('.model-slots__status')).toHaveText('1/4 cargados');
});

test('OLVIDAR una ranura la vacia en el motor y la saca de la memoria (y no vuelve)', async ({ page }) => {
  await startLocalEngine(page);
  await openDrawer(page, 'models');

  // Carga real: el mismo camino que el test de la ranura A, con el dialogo de
  // verdad interceptado por Playwright.
  const chooser = page.waitForEvent('filechooser');

  await page.locator('[data-slot="0"] .model-slots__load').click();
  await (await chooser).setFiles(MODEL);

  await expect(page.locator('[data-slot="0"] .model-slots__name')).toHaveText('CZ-BASS1');

  // OLVIDAR solo existe donde hay algo que olvidar.
  await expect(page.locator('[data-slot="0"] .model-slots__forget')).toBeVisible();
  await expect(page.locator('[data-slot="1"] .model-slots__forget')).toBeHidden();

  await page.locator('[data-slot="0"] .model-slots__forget').click();

  // La ficha: la ranura vacia, y el aviso por la MISMA linea de estado que usaria
  // un fallo de carga (con su tono, no el rojo del error).
  await expect(page.locator('[data-slot="0"] .model-slots__name')).toHaveText('—');
  await expect(page.locator('[data-slot="0"] .model-slots__forget')).toBeHidden();
  await expect(page.locator('.model-slots__status')).toHaveText(/^\u2713 /);
  await expect(page.locator('.model-slots__status')).toHaveAttribute('data-state', 'ok');
  await expect(page.locator('.model-slots__status')).toHaveText(/no sobrevive al F5/);

  // El distintivo vivo de la ficha baja solo: cuenta ranuras con nombre.
  await expect(page.locator('[data-live-badge="models"]')).toHaveText('0/4');

  // EL MOTOR: la ranura viaja al worklet como la entrada EMPTY de fabrica
  // (isValid false, 64 amplitudes a cero), que es como se descarga de verdad.
  await expect.poll(() => page.evaluate(() => {
    const messages = (window.__neuronikSent ?? []).filter((message) => message?.type === 'neuronik:models');
    const slot = messages.at(-1)?.slots?.find((entry) => entry?.slot === 0);

    if (!slot) return null;

    return {
      name: slot.name,
      isValid: slot.isValid,
      amps: slot.amplitudes,
    };
  })).toEqual({ name: 'EMPTY', isValid: false, amps: new Array(64).fill(0) });

  // Y la memoria del navegador: la clave ya no lleva esa ranura.
  expect(await page.evaluate(() => {
    const payload = JSON.parse(window.localStorage.getItem('neuronik.localModels') ?? 'null');

    return payload === null ? null : Object.keys(payload.slots ?? {});
  })).toBeNull();

  // F5: lo unico que podria devolverla era la memoria. No vuelve.
  await page.reload();
  await page.waitForFunction(() => window.__pilotReady === true);
  await openDrawer(page, 'models');

  await expect(page.locator('[data-slot="0"] .model-slots__name')).toHaveText('—');
  await expect(page.locator('.model-slots__status')).toHaveText('0/4 cargados');
});

test('el conmutador de la ruta del pad apaga y cambia el LFO (y el anillo responde)', async ({ page }) => {
  const attempts = await ensureAudioClock(page);

  test.skip(!attempts, 'el reloj de audio de Chromium no avanza en este entorno (sin salida de audio)');

  await startLocalEngine(page);

  const toggle = page.locator('[data-local-route-toggle="modMatrix"]');
  const source = page.locator('[data-local-route-source="modMatrix"]');
  const fila3 = page.getByRole('button', { name: /3 LFO 2 → Morph Z 1\.00/ });

  // El valor inicial es la siembra: encendida y con LFO 2, que es lo que hacia
  // que el anillo bailara desde el primer SOUND ON.
  await expect(toggle).toHaveAttribute('aria-pressed', 'true');
  await expect(source).toHaveValue('LFO 2');
  await expect(fila3).toBeVisible();

  // La fila 3 CRUZA al worklet encendida (2 = LFO 2, 28 = Morph Z, 1.0).
  const filaEnMotor = () => page.evaluate(() => {
    const params = (window.__neuronikSent ?? []).filter((message) => message?.type === 'neuronik:params');
    const last = params.at(-1);

    if (!last) return null;

    return [28, 29, 30].map((field) => last.fields.find(([id]) => id === field) ?? null);
  });

  await expect.poll(filaEnMotor).toEqual([[28, 2], [29, 28], [30, 1]]);

  // APAGAR: la fila vuelve a Off en el resumen Y en el motor, y el anillo se
  // queda quieto (el arco sin abrir: sin ruta, GetMod(28) es 0 exacto).
  await toggle.click();

  await expect(toggle).toHaveAttribute('aria-pressed', 'false');
  await expect(page.getByRole('button', { name: /3 Off → Off/ })).toBeVisible();
  await expect(source).toBeDisabled();
  await expect.poll(filaEnMotor).toEqual([[28, 0], [29, 0], [30, 0]]);

  const quieto = await sampleRingFrames(page, 90);

  expect(new Set(quieto.map((sample) => `${sample.start}|${sample.span}`)).size).toBe(1);
  expect(Math.max(...quieto.map((sample) => sample.span))).toBe(0);

  // Volver a encenderla: el anillo vuelve a bailar y el LFO elegido se recuerda
  // (el selector se rehabilita con el que habia).
  await toggle.click();

  await expect(toggle).toHaveAttribute('aria-pressed', 'true');
  await expect(source).toBeEnabled();
  await expect(page.getByRole('button', { name: /3 LFO 2 → Morph Z 1\.00/ })).toBeVisible();
  await expect.poll(filaEnMotor).toEqual([[28, 2], [29, 28], [30, 1]]);

  const { states: bailando } = await sampleRingUntil(page, 12);

  expect(new Set(bailando.map((sample) => `${sample.start}|${sample.span}`)).size,
    `el anillo solo dio ${bailando.length} estado(s) distinto(s): la ruta no esta moviendo el arco`)
    .toBeGreaterThan(4);

  // ELEGIR OTRO LFO: la MISMA fila pasa a LFO 1 (indice 1 de la tabla) y el motor
  // lo recibe; el anillo sigue bailando porque la ruta sigue viva.
  await source.selectOption('LFO 1');

  await expect(page.getByRole('button', { name: /3 LFO 1 → Morph Z 1\.00/ })).toBeVisible();
  await expect.poll(filaEnMotor).toEqual([[28, 1], [29, 28], [30, 1]]);

  const { states: conLfo1 } = await sampleRingUntil(page, 8);

  expect(new Set(conLfo1.map((sample) => `${sample.start}|${sample.span}`)).size,
    `tras cambiar a LFO 1 el anillo solo dio ${conLfo1.length} estado(s) distinto(s)`)
    .toBeGreaterThan(2);

  // Y volver a LFO 2 deja la pagina como estaba (el valor inicial de la siembra).
  await source.selectOption('LFO 2');
  await expect.poll(filaEnMotor).toEqual([[28, 2], [29, 28], [30, 1]]);
});

test('el distintivo vivo del lienzo abre SU cajon (MODELOS, GLOBAL y ENVOLVENTES)', async ({ page }) => {
  // El chip cuelga de la cabecera de la ficha, al lado del EDIT, y lleva la
  // fraccion del dato vivo: la cabecera de MODELOS es una fila fija y justa, asi
  // que el rotulo entero vive en el title y en la etiqueta accesible.
  const modelsChip = page.locator('[data-live-badge="models"]');
  const globalChip = page.locator('[data-live-badge="globalFull"]');
  const envelopesChip = page.locator('[data-live-badge="envelopes"]');

  await expect(modelsChip).toHaveText('0/4');
  await expect(globalChip).toHaveText('0/8');
  // ENVOLVENTES entro el 2026-09-28: con NEURONiK suena su ADSR entera, o sea
  // las ocho celdas de la ficha (las dos envelopes por sus cuatro knobs).
  await expect(envelopesChip).toHaveText('8/8');
  await expect(modelsChip).toHaveAttribute('aria-label', /^0\/4 RANURAS: abrir el caj/);
  // MODELOS, GLOBAL, LFO, ENVOLVENTES y EFECTOS (este ultimo entro el
  // 2026-09-29 con el cajon de los modulos de hueco). La MATRIZ conserva su
  // resumen y no cuelga chip, y las cuatro fichas sin cajon no pueden colgarlo.
  await expect(page.locator('.card__badge')).toHaveCount(5);

  // El texto visible no es decoracion: el EDIT de la MISMA ficha tiene que
  // seguir dentro de la cabecera (un chip que empuja el EDIT fuera dejaria a la
  // ficha sin su abridor, que es justo lo que el chip viene a sustituir).
  const encaje = await page.evaluate(() => {
    const heading = document.querySelector('[data-section-id="models"] .card__heading');

    return {
      desbordado: heading.scrollWidth - heading.clientWidth,
      editDentro: heading.querySelector('[data-drawer-trigger="models"]').getBoundingClientRect().right
        <= heading.getBoundingClientRect().right + 0.5,
    };
  });

  expect(encaje).toEqual({ desbordado: 0, editDentro: true });

  // El gesto: pulsarlo ABRE el cajon (el patron de la franja de GLOBAL).
  await modelsChip.click();
  await expect(page.locator('#drawer-models')).toHaveAttribute('aria-hidden', 'false');
  await expect(page.locator('#drawer-models .model-slots__status')).toHaveText('0/4 cargados');

  // Y el distintivo del cajon y el del lienzo son el MISMO dato.
  await expect(page.locator('#drawer-models .drawer__badge')).toHaveText('0/4 RANURAS');

  await page.keyboard.press('Escape');
  await expect(page.locator('#drawer-models')).toHaveAttribute('aria-hidden', 'true');

  // GLOBAL: su chip abre SU cajon, con las ocho celdas de su columna.
  await globalChip.click();
  await expect(page.locator('#drawer-globalFull')).toHaveAttribute('aria-hidden', 'false');
  await expect(page.locator('#drawer-globalFull .drawer-slot')).toHaveCount(8);
  await expect(page.locator('#drawer-globalFull .drawer__badge')).toHaveText('0/8 GLOBAL');

  await page.keyboard.press('Escape');
  await expect(page.locator('#drawer-globalFull')).toHaveAttribute('aria-hidden', 'true');

  // El chip SIGUE VIVO con el motor en marcha: cargar una ranura lo sube a 1/4.
  await startLocalEngine(page);
  await page.locator('[data-live-badge="models"]').click();

  const chooser = page.waitForEvent('filechooser');

  await page.locator('[data-slot="0"] .model-slots__load').click();
  await (await chooser).setFiles(MODEL);

  await page.keyboard.press('Escape');

  await expect(page.locator('[data-live-badge="models"]')).toHaveText('1/4');
  await expect(page.locator('#drawer-models .drawer__badge')).toHaveText('1/4 RANURAS');

  // ENVOLVENTES: su chip abre SU cajon y el dato es el MISMO en los dos sitios.
  await envelopesChip.click();
  await expect(page.locator('#drawer-envelopes')).toHaveAttribute('aria-hidden', 'false');
  await expect(page.locator('#drawer-envelopes .drawer__badge')).toHaveText('8/8 ACTIVAS');

  await page.keyboard.press('Escape');
  await expect(page.locator('#drawer-envelopes')).toHaveAttribute('aria-hidden', 'true');

  // Y el dato SE MUEVE, que es lo que justificaba declararlo: con NEUROTIK solo
  // viven las celdas del filtro, asi que el chip baja a 4/8 sin abrir nada.
  await page.getByRole('radio', { name: 'Neurotik' }).click();
  await expect(envelopesChip).toHaveText('4/8');

  // De vuelta a NEURONiK recupera las ocho: el chip lee el snapshot, no guarda
  // un historial de lo que se vio.
  await page.getByRole('radio', { name: 'Neuronik' }).click();
  await expect(envelopesChip).toHaveText('8/8');
});

test('el distintivo ACTIVE lee el MISMO gating que la celda pinta', async ({ page }) => {
  // El LFO es DSP COMPARTIDO (`engines: 'both'` en el contrato): su caja esta
  // entera con los dos motores, y el distintivo lo dice sin abrir el cajon.
  const lfoChip = page.locator('[data-live-badge="lfo"]');

  await expect(lfoChip).toHaveText('10/10');
  await expect(lfoChip).toHaveAttribute('aria-label', /^10\/10 LFO: abrir el caj/);

  // La verdad del modo 'active' es el gating de las celdas, asi que se mide en
  // la celda: la lista de destinos de la MATRIZ se apaga por motor, y ese es el
  // mismo `optionEngines` que lee el distintivo.
  const destinosApagados = () => page.evaluate(() => {
    const options = [...document.querySelectorAll('#drawer-modMatrix [data-parameter-id="mod3Destination"] option')];

    return options
      .filter((option) => option.disabled)
      .map((option) => option.textContent.trim());
  });

  await openDrawer(page, 'modMatrix');

  const conNeuronik = await destinosApagados();

  // NEURONiK: se apagan los destinos de NEUROTIK (Impulse Mix, Res Bank Res...).
  expect(conNeuronik).toEqual(expect.arrayContaining(['Excite Noise', 'Res Bank Res']));
  expect(conNeuronik).not.toContain('Inharmonicity');

  await page.keyboard.press('Escape');
  await page.getByRole('radio', { name: 'Neurotik' }).click();

  // El chip del LFO no se mueve: sus celdas las consumen los dos motores.
  await expect(lfoChip).toHaveText('10/10');

  await openDrawer(page, 'modMatrix');

  const conNeurotik = await destinosApagados();

  // NEUROTIK: ahora la lista apaga los destinos de NEURONiK.
  expect(conNeurotik).toEqual(expect.arrayContaining(['Inharmonicity', 'Filter Cutoff']));
  expect(conNeurotik).not.toContain('Excite Noise');
  expect(conNeurotik.length).toBeGreaterThan(conNeuronik.length);
});

test('arrastrar el pad mueve morphX/morphY', async ({ page }) => {
  await startLocalEngine(page);

  const pad = canvasPad(page);
  const readout = page.locator('.xy-pad__readout-row').first();
  const box = await pad.boundingBox();

  expect(box, 'el pad del lienzo debe tener caja').not.toBeNull();
  await expect(readout).toContainText('X 0% / Y 0%');

  const drag = async (fx, fy) => {
    await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
    await page.mouse.down();
    await page.mouse.move(box.x + box.width * fx, box.y + box.height * fy, { steps: 8 });
    await page.mouse.up();
    await page.waitForTimeout(200);

    const text = await readout.innerText();
    const [x, y] = [...text.matchAll(/(\d+)%/g)].map((match) => Number(match[1]));

    return { x, y };
  };

  // Arriba a la derecha: X y Y grandes (el eje Y del pad crece hacia ARRIBA).
  const high = await drag(0.85, 0.15);
  expect(high.x).toBeGreaterThan(60);
  expect(high.y).toBeGreaterThan(60);

  // Abajo a la izquierda: los dos al otro extremo (no basta con que algo cambie).
  const low = await drag(0.15, 0.85);
  expect(low.x).toBeLessThan(40);
  expect(low.y).toBeLessThan(40);

  // El valor accesible del pad acompana al readout.
  await expect(pad.locator('[role="slider"]').first()).toHaveAttribute('aria-valuetext', /X \d+% \/ Y \d+%/);
});

test('el knob del master se mueve con el dedo Y el puente del arnés lee lo que se ve', async ({ page }) => {
  // El control base es un Knob del paquete compartido (un `div[role=slider]` sin
  // input dentro) desde 2026-09-28, y el arnés del selftest lo maneja por el
  // puente `baselineControl.{value,setFromSnapshot}` de la celda que lleva el
  // ancla. Por eso esto vive en el smoke y no en vitest: hace falta un GESTO de
  // verdad sobre el dial, y un dial no se puede tocar desde jsdom.
  //
  // Lo que se fija es la mitad que se rompio: el puente se actualizaba solo con
  // el `paint`, asi que tras un arrastre el store iba a 0.55, el dial marcaba
  // 0.55 y el puente seguia en el ULTIMO pintado — el arnes leeria un valor que
  // la pagina ya no mostraba. El puente y la pagina tienen que decir lo mismo.
  const cell = page.locator('[data-baseline-control]');
  const dial = cell.locator('[role="slider"]');

  await expect(cell).toHaveAttribute('data-baseline-control', 'masterLevel');
  const antes = await cell.evaluate((el) => el.baselineControl.value);
  const box = await dial.boundingBox();

  await page.mouse.move(box.x + box.width / 2, box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width / 2 + 34, box.y + box.height / 2 - 30, { steps: 6 });
  await page.mouse.up();

  // El ESTADO NATIVO se movio de verdad (no un setValue programatico del arnés):
  // el gesto cruza la frontera en un `neuronik:params`, igual que haria un dedo.
  await expect
    .poll(async () => page.evaluate(() => {
      const messages = (window.__neuronikSent ?? []).filter((m) => m?.type === 'neuronik:params');
      return Number(messages.at(-1)?.masterLevel ?? NaN);
    }))
    .not.toBe(antes);

  // Y las TRES lecturas de la pagina coinciden: el valor accesible del dial, el
  // readout y el puente del arnés. Si el puente se queda atras, la direccion
  // NATIVO->JS empuja un valor que la pagina ya no muestra.
  const visto = await cell.evaluate((el) => ({
    puente: el.baselineControl.value,
    dial: Number(el.querySelector('[role="slider"]').getAttribute('aria-valuenow')),
    readout: el.querySelector('[data-parameter-readout]').textContent,
  }));

  expect(visto.dial, 'el dial debe mostrar el valor del gesto').toBeCloseTo(visto.puente, 5);
  expect(visto.puente, 'el puente debe decir lo mismo que el dial').toBeCloseTo(visto.dial, 5);
  expect(visto.readout, 'el readout debe acompañar al gesto').toBe(`${Math.round(visto.dial * 100)}%`);
});

test('la ayuda de gestos cabe en la cabecera sin empujar nada y su lista se lee', async ({ page }) => {
  // La ayuda de gestos vive en la cabecera, que tiene ALTO FIJO: cualquier
  // fila nueva se nota. Se midio el fallo dos veces antes de fijar esto — el
  // resumen caia en una fila implicita (y=44..52 con la cabecera en 0..35) y, al
  // meterlo en la fila 3, la linea de contrato bajaba 10 px. Ahora comparte
  // celda con ella, cada una a su extremo.
  const box = (locator) => locator.evaluate((el) => {
    const r = el.getBoundingClientRect();
    return { x: Math.round(r.x), top: Math.round(r.top), bottom: Math.round(r.bottom), right: Math.round(r.right) };
  });

  const help = page.locator('details.gesture-help');
  const contract = page.locator('.contract-line');

  await expect(help, 'la ayuda tiene que estar en la pagina, no solo en el codigo').toHaveCount(1);
  expect(await help.evaluate((el) => el.open), 'nace plegada').toBe(false);

  const helpBox = await box(help);
  const contractBox = await box(contract);

  // Misma banda, sin pisarse. OJO: la referencia es la linea de contrato y NO
  // la caja de la cabecera, que se queda corta con esa fila ya antes de existir
  // la ayuda (medido: cabecera 5..35, linea de contrato 33..41).
  expect(Math.abs(helpBox.top - contractBox.top), 'la ayuda deberia ir en la misma banda que la linea de contrato')
    .toBeLessThanOrEqual(2);
  expect(helpBox.right, 'la ayuda invade la linea de contrato').toBeLessThanOrEqual(contractBox.x);

  // Y lo que dice: los gestos del boton redondo, que es lo que no estaba
  // documentado en ninguna parte antes de esto.
  await help.locator('summary').click();
  const items = await help.locator('li').allTextContents();
  const text = items.join(' | ');

  expect(items.length, 'la ayuda deberia listar los gestos de la pagina').toBeGreaterThanOrEqual(4);
  expect(text).toContain('doble clic');
  expect(text).toContain('Shift');
  expect(text).toContain('flechas');
  // Y el popover se lee entero: dentro de la ventana, con fondo (si fuese
  // transparente, las bandas de debajo se transparentarian a traves del texto).
  const list = help.locator('ul');
  const listBox = await box(list);
  const styles = await list.evaluate((el) => {
    const style = getComputedStyle(el);
    return { position: style.position, zIndex: style.zIndex, background: style.backgroundColor };
  });

  expect(listBox.top, 'la lista cae por debajo del resumen').toBeGreaterThanOrEqual(helpBox.bottom - 1);
  expect(listBox.x, 'la lista se sale por la izquierda').toBeGreaterThanOrEqual(0);
  expect(listBox.bottom, 'la lista se sale por abajo').toBeLessThanOrEqual(page.viewportSize().height);
  expect(styles.position).toBe('absolute');
  expect(Number(styles.zIndex)).toBeGreaterThan(0);
  expect(styles.background).not.toBe('rgba(0, 0, 0, 0)');
});
