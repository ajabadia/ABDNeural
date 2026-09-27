/**
 * ABDNeural - la pagina RECUERDA sus ranuras de modelo sin host (memoria local).
 *
 * El plugin vuelve a sus cuatro ranuras porque el PRESET lleva la ruta del fichero
 * (`modelPath<slot>`): al reabrir el proyecto el procesador recarga y publica
 * `modelsState`. La pagina del navegador no tiene preset ni sistema de ficheros -lo
 * unico que tiene es el `<input type="file">` que el usuario elige a mano-, asi que
 * sin memoria cada recarga empezaba con las cuatro ranuras EMPTY y habia que volver a
 * buscar los mismos ficheros.
 *
 * Este test es el camino COMPLETO contra los artefactos reales: el store de la pagina
 * (MODO LOCAL, con un localStorage de mentira porque Node no tiene) y el binario REAL
 * del worklet. Lo que comprueba, y por que no lo cubre el vitest:
 *
 *   1. lo guardado es el TEXTO crudo del .neuronikmodel, no el objeto parseado (el
 *      parser es UNO: recuperar vuelve a cruzar el mismo lector, sin dialectos);
 *   2. al "recargar" (store nuevo, misma memoria) la ranura vuelve con el MISMO
 *      nombre, la misma validez y las 64 amplitudes BIT A BIT contra el fichero;
 *   3. solo esa ranura: la memoria es por ranura y las otras tres siguen EMPTY;
 *   4. control negativo: sin memoria, no hay nada que recuperar (0 y cuatro EMPTY);
 *   5. una entrada corrupta se descarta y NO se lleva por delante las buenas;
 *   6. EL MOTOR: las amplitudes de la ranura recordada, por el mismo camino que usa el
 *      worklet (`neuronik:models` -> neuronikLoadModel), hacen SONAR distinto la misma
 *      nota que una ranura vacia - la memoria no es solo pintura de la ficha;
 *   7. OLVIDAR una ranura la descarga de verdad: lo que entra al motor despues es la
 *      entrada EMPTY de fabrica, y la MISMA nota suena EXACTAMENTE igual que con una
 *      ranura que nunca se cargo (mismo timbre, sample a sample).
 *
 * El vitest prueba el cable de la pagina (localStorage, el orden del arranque); esto
 * prueba que lo recordado llega al DSP y suena.
 *
 * Skip (mismo criterio que NEURONiK_WorkletSync y NEURONiK_LocalMorphZRoute): sin
 * build-wasm no hay DSP que medir -clone limpio sin haber compilado WASM todavia-, asi
 * que avisa y pasa.
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const thisDir = path.dirname(fileURLToPath(import.meta.url));
const neuralRoot = path.resolve(thisDir, '..');
const webuiRoot = path.join(neuralRoot, 'WebUI');

const jsPath = path.join(neuralRoot, 'build-wasm', 'neuronik_dsp.js');
const wasmPath = path.join(neuralRoot, 'build-wasm', 'neuronik_dsp.wasm');
const modelPath = path.join(neuralRoot, 'Assets', 'Models', 'CZ-BASS1.neuronikmodel');

if (!fs.existsSync(jsPath) || !fs.existsSync(wasmPath))
{
    console.log("NEURONiK_LocalModelCache: sin build-wasm/neuronik_dsp.{js,wasm}");
    console.log("  [skip] el DSP del worklet no esta compilado todavia (via build_wasm.bat)");
    process.exit(0);
}

if (!fs.existsSync(modelPath))
{
    console.log("NEURONiK_LocalModelCache: sin Assets/Models/CZ-BASS1.neuronikmodel");
    console.log("  [skip] el modelo de fabrica no esta en el arbol");
    process.exit(0);
}

// El navegador tiene localStorage; Node no. Se instala uno de mentira ANTES de usar el
// store -que no recibe almacen por parametro-, asi el test recorre el camino REAL. El
// Map SOBREVIVE a la "recarga": eso es exactamente lo que se quiere probar.
const memory = new Map();
globalThis.localStorage = {
    getItem: (key) => (memory.has(key) ? memory.get(key) : null),
    setItem: (key, value) => { memory.set(key, String(value)); },
    removeItem: (key) => { memory.delete(key); },
};

const asUrl = (...segments) => `file://${path.join(...segments).split(path.sep).join('/')}`;

const { createParameterStore } = await import(asUrl(webuiRoot, 'src/contracts/paramStore.js'));
const { SCREEN_PARAMETER_IDS } = await import(asUrl(webuiRoot, 'src/contracts/screens.js'));
const { emptyLocalModels, parseModelText } = await import(asUrl(webuiRoot, 'src/audio/localModels.js'));
const { LOCAL_MODEL_CACHE_KEY, readCachedModelTexts, saveCachedModelText } =
    await import(asUrl(webuiRoot, 'src/audio/localModelCache.js'));
const { emptyLocalModelSlot } = await import(asUrl(webuiRoot, 'src/audio/localModels.js'));

const wasmBinary = fs.readFileSync(wasmPath);
const { default: createModule } = await import(asUrl(jsPath));

const Module = await createModule
({
    instantiateWasm(info, receiveInstance)
    {
        WebAssembly.instantiate(wasmBinary, info)
            .then((out) => receiveInstance(out.instance))
            .catch((e) => { console.error('instantiateWasm fallo:', e); });

        return {};
    },
});

let failures = 0;

function check(condition, description)
{
    console.log(`  [${condition ? 'ok' : 'FAIL'}] ${description}`);
    if (!condition) ++failures;
}

const SR = 48000;
const BLOCK = 128;

// --- La pagina y su "recarga" ------------------------------------------------
// Mismo arranque local que app.js: store sin host + las cuatro ranuras vacias (el
// shape que pinta la ficha RANURAS) y, encima, la memoria.
function page()
{
    const store = createParameterStore({ ids: SCREEN_PARAMETER_IDS });
    store.start();
    store.seedLocalModels(emptyLocalModels());

    return store;
}

const rawText = fs.readFileSync(modelPath, 'utf8');
const file = { name: 'CZ-BASS1.neuronikmodel', text: async () => rawText };

// --- 1. La pagina carga el modelo (MODO LOCAL) y lo recuerda -----------------
const first = page();

check(first.getState().bridgeAvailable === false,
      "la pagina corre en MODO LOCAL (sin host): la memoria es lo unico que queda");

check(await first.loadLocalModel(file, 1) === true,
      "la pagina carga el .neuronikmodel real (Assets/Models/CZ-BASS1) en la ranura B");

check(readCachedModelTexts()[1] === rawText,
      "la memoria guarda el TEXTO crudo del fichero, no el objeto parseado");

// --- 2. "Recargar" (F5): store nuevo, la MISMA memoria -----------------------
const second = page();
const restored = second.restoreLocalModels();
const models = second.getState().models;

check(restored === 1, `al recargar la pagina recupera ${restored} ranura de la memoria`);
check(models[1].name === 'CZ-BASS1' && models[1].isValid === true && models[1].slot === 1,
      `la ranura B vuelve a su sitio: "${models[1].name}", valida, slot ${models[1].slot}`);

// Bit a bit: lo que vuelve del almacen es el fichero, no una copia a medias.
const expected = parseModelText(rawText);
let exact = expected.amplitudes.length === models[1].amplitudes.length;

for (let i = 0; exact && i < expected.amplitudes.length; ++i)
    exact = Object.is(expected.amplitudes[i], models[1].amplitudes[i]);

check(exact, 'y sus 64 amplitudes vuelven BIT A BIT contra el fichero (mismo parser)');
check(models.filter((entry) => entry.name === 'EMPTY').length === 3,
      'solo esa ranura: las otras tres siguen EMPTY (la memoria es por ranura)');
check(second.getState().modelError === null, 'y la recarga no enciende ningun aviso');

// --- 3. Control negativo: sin memoria no hay nada que recordar ---------------
memory.delete(LOCAL_MODEL_CACHE_KEY);

const blank = page();
check(blank.restoreLocalModels() === 0
      && blank.getState().models.every((entry) => entry.name === 'EMPTY'),
      'sin memoria la pagina arranca igual de limpia: 0 recuperadas y cuatro EMPTY');

// --- 4. Una entrada corrupta no se lleva por delante las buenas --------------
saveCachedModelText(0, 'esto no es un modelo');
saveCachedModelText(2, rawText);

const mixed = page();
const mixedRestored = mixed.restoreLocalModels();

check(mixedRestored === 1 && mixed.getState().models[2].name === 'CZ-BASS1',
      `de una memoria con una entrada rota y una buena, se recupera ${mixedRestored}`);
check(mixed.getState().models[0].name === 'EMPTY'
      && mixed.getState().modelError?.slot === 0,
      'la rota se descarta (EMPTY) y el motivo se pinta en la ficha, no se finge');

// --- 5. El MOTOR de verdad: la ranura recordada SUENA ------------------------
// Runtime::Event: 24 bytes [i32 type, i32 channel, i32 note, i32 value14, f32 value, i32 offset]
const EVENT_SIZE = 24;
const EVENT_TYPE_NOTE_ON = 0;
const BLOCKS = Math.round((SR / BLOCK) * 1);   // 1 s de nota larga

const eventPtr = Module._malloc(EVENT_SIZE);
const blockBytes = BLOCK * 4;
const leftPtr = Module._malloc(blockBytes);
const rightPtr = Module._malloc(blockBytes);

const heap32 = Module.HEAP32;
const heapF32 = Module.HEAPF32;

/**
 * Una nota con la ranura 0 en un estado concreto. El modelo entra por el MISMO camino
 * que usa el worklet (`neuronik:models` -> neuronikLoadModel: 128 floats, amplitudes
 * y luego frequencyOffsets); `slotEntry` null es una ranura VACIA de verdad.
 */
function renderSlotZero(slotEntry)
{
    Module._neuronikSetEngine(0);
    Module._neuronikInit(SR, BLOCK);

    const modelPtr = Module._malloc(128 * 4);
    const view = Module.HEAPF32.subarray(modelPtr >> 2, (modelPtr >> 2) + 128);
    view.fill(0);

    if (slotEntry)
    {
        for (let i = 0; i < 64; ++i)
        {
            view[i] = slotEntry.amplitudes[i];
            view[64 + i] = slotEntry.frequencyOffsets[i];
        }
    }

    Module._neuronikLoadModel(0, 0, modelPtr, slotEntry?.isValid ? 1 : 0);
    Module._free(modelPtr);

    const out = new Float32Array(BLOCKS * BLOCK);

    for (let block = 0; block < BLOCKS; ++block)
    {
        if (block === 0)
        {
            heap32[eventPtr >> 2] = EVENT_TYPE_NOTE_ON;
            heap32[(eventPtr >> 2) + 1] = 1;
            heap32[(eventPtr >> 2) + 2] = 69;
            heap32[(eventPtr >> 2) + 3] = 8192;
            heapF32[(eventPtr >> 2) + 4] = 1.0;
            heap32[(eventPtr >> 2) + 5] = 0;
        }

        Module._neuronikProcess(leftPtr, rightPtr, BLOCK, block === 0 ? eventPtr : 0, block === 0 ? 1 : 0);
        out.set(heapF32.subarray(leftPtr >> 2, (leftPtr >> 2) + BLOCK), block * BLOCK);
    }

    return out;
}

const rms = (samples) =>
{
    let sum = 0;
    for (const sample of samples) sum += sample * sample;
    return Math.sqrt(sum / samples.length);
};

// Lo que entra al motor es EXACTAMENTE la ranura que recupero la memoria en el paso 2:
// el motor no sabe de donde vino (mismo serializer que el worklet en 'neuronik:models').
const remembered = renderSlotZero(models[1]);
const empty = renderSlotZero(null);

const rmsRemembered = rms(remembered);
const rmsEmpty = rms(empty);
let maxDiff = 0;

for (let i = 0; i < remembered.length; ++i)
    maxDiff = Math.max(maxDiff, Math.abs(remembered[i] - empty[i]));

const relative = rmsEmpty > 0 ? (rmsRemembered - rmsEmpty) / rmsEmpty : Infinity;

check(rmsEmpty > 0 && rmsRemembered > 0,
      `la nota suena en los dos casos (RMS ${rmsEmpty.toFixed(5)} vacia / ${rmsRemembered.toFixed(5)} con modelo)`);
check(maxDiff > 1e-3 && Math.abs(relative) > 0.01,
      `la ranura recordada cambia el timbre de la MISMA nota: maxDiff ${maxDiff.toFixed(5)} (relativo ${(relative * 100).toFixed(1)}%)`);

// --- 6. OLVIDAR una ranura: el motor la descarga, y la memoria la suelta -------
// La pagina carga un modelo de verdad en la ranura A (mismo camino que el E2E: el
// input de fichero), lo olvida, y despues se mete al motor lo que quedo en el
// ESTADO - la entrada EMPTY de fabrica, no un hueco.
const forgetting = page();

check(await forgetting.loadLocalModel(file, 0) === true,
      "la pagina carga el modelo real en la ranura A otra vez (para olvidarla)");

check(forgetting.forgetLocalModel(0) === true,
      "OLVIDAR la ranura A devuelve true (el gesto ocurrio)");

check(forgetting.getState().models[0].name === 'EMPTY'
      && forgetting.getState().models[0].isValid === false,
      "la ranura A queda EMPTY en el estado: es la entrada vacia de fabrica");

check(forgetting.getState().modelNotice?.tone === 'ok'
      && /no sobrevive al F5/.test(forgetting.getState().modelNotice?.detail ?? ''),
      "y el aviso dice que no sobrevive al F5 (tono 'ok', no es un error)");

// OLVidar es POR RANURA: la otra que la memoria aun guarda (la del caso mixto de
// arriba) se queda donde esta. Un "olvidar" que limpiara la memoria entera seria
// otro boton, y por eso no se confunde con el.
const remaining = Object.keys(readCachedModelTexts());

check(remaining.length === 1 && remaining[0] === '2',
      `la memoria local ya no guarda la ranura A y sigue guardando la C (quedan: ${remaining.join(', ') || '—'})`);

// "Recargar": lo unico que puede devolver la A es su texto, y ya no esta.
const afterForget = page();
const restoredAfterForget = afterForget.restoreLocalModels();

check(restoredAfterForget === 1
      && afterForget.getState().models[0].name === 'EMPTY'
      && afterForget.getState().models[2].name === 'CZ-BASS1',
      `tras recargar vuelve solo la que quedo (${restoredAfterForget} recuperada) y la olvidada sigue EMPTY`);

// EL DSP: lo que entra por `neuronik:models` con la ranura olvidada es
// `neuronikLoadModel(0, engine, <64 ceros + 64 ceros>, isValid 0)`. Suena igual
// que una ranura que nunca se cargo? Si el olvido fuera solo de pintura (o si el
// motor guardara el modelo viejo por dentro), aqui se veria.
const forgottenSlot = emptyLocalModelSlot(0);

const renderedForgotten = renderSlotZero(forgottenSlot);
const renderedNeverLoaded = renderSlotZero(null);

let forgetDiff = 0;
let forgetRms = 0;

for (let i = 0; i < renderedForgotten.length; ++i)
{
    forgetDiff = Math.max(forgetDiff,
        Math.abs(renderedForgotten[i] - renderedNeverLoaded[i]));
    forgetRms += renderedForgotten[i] * renderedForgotten[i];
}

forgetRms = Math.sqrt(forgetRms / renderedForgotten.length);

check(forgetDiff === 0,
      `la nota tras olvidar es BIT A BIT la de una ranura nunca cargada (maxDiff ${forgetDiff})`);
check(forgetRms > 0,
      `y sigue sonando (RMS ${forgetRms.toFixed(5)}): vaciar una ranura no es silenciar el motor`);
check(Math.abs(rmsRemembered - forgetRms) > 0.01 * Math.max(rmsRemembered, forgetRms),
      `y suena DISTINTO a cuando el modelo estaba cargado (RMS ${rmsRemembered.toFixed(5)} con modelo)`);

afterForget.dispose();
forgetting.dispose();

Module._free(eventPtr);
Module._free(leftPtr);
Module._free(rightPtr);

console.log('');

if (failures > 0)
{
    console.log(`Checks failed: ${failures}`);
    process.exit(1);
}

console.log("La pagina recuerda sus ranuras sin host: lo cargado sobrevive al F5 y suena.");
process.exit(0);
