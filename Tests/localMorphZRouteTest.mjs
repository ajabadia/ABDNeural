/**
 * ABDNeural - el anillo del pad BAILA sin host (ruta local LFO 2 -> Morph Z).
 *
 * Fuera del plugin el motor WASM arranca con la matriz por defecto del contrato:
 * las dos rutas de las envolventes y los slots 3/4 en Off. Con eso
 * `_neuronikGetMod(28)` (Morph Z) vale 0 y el anillo del pad se queda quieto —
 * el pad mueve morphZ a mano, pero nada lo modula, que es justo el gesto Neuron
 * del panel nativo.
 *
 * La pagina SIEMBRA esa ruta en MODO LOCAL (paramStore::seedLocalMorphZRoute):
 * LFO 2 -> Morph Z al 100%, en el primer slot LIBRE. Este test es la otra mitad:
 * comprueba que esa ruta, por el MISMO camino que usa la pagina (el store real +
 * el mapeo contrato -> campos de GlobalParams del worklet), llega al MOTOR REAL
 * y el arco se mueve.
 *
 * Que se prueba (y por que no lo cubre el vitest):
 *   0. el CONMUTADOR de la ficha MATRIZ (2026-09-27): apagado deja el anillo en
 *      0 EXACTO y la siembra deja de re-plantar la ruta al recargar; encendido
 *      con LFO 1 la fila viaja con el indice 1 y el anillo vuelve a barrer los
 *      dos signos (el mismo camino que el boton y su desplegable);
 *   1. el store siembra la ruta y NO pisa un slot ya asignado (eso tambien esta
 *      en vitest, fijado aqui por el camino completo);
 *   2. los fields que la pagina manda son la fila 3 de la matriz (28/29/30) con
 *      los indices de la tabla del contrato (2 = LFO 2, 28 = Morph Z, 1.0);
 *   3. el WASM de verdad modula: |GetMod(28)| barre hasta 1.0 en los dos signos y
 *      el periodo medido entre crestas es ~1 s (LFO 2 a 1 Hz por defecto);
 *   4. control negativo: con el slot 3 en Off el anillo mide 0 exacto (lo medido
 *      es la ruta sembrada, no un resto de estado).
 * El vitest prueba el cable de la pagina; esto prueba que el motor responde.
 *
 * Skip (mismo criterio que NEURONiK_WorkletSync): sin build-wasm no hay DSP que
 * medir -clone limpio sin haber compilado WASM todavia-, asi que avisa y pasa.
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const thisDir = path.dirname(fileURLToPath(import.meta.url));
const neuralRoot = path.resolve(thisDir, '..');
const webuiRoot = path.join(neuralRoot, 'WebUI');

const jsPath = path.join(neuralRoot, 'build-wasm', 'neuronik_dsp.js');
const wasmPath = path.join(neuralRoot, 'build-wasm', 'neuronik_dsp.wasm');

if (!fs.existsSync(jsPath) || !fs.existsSync(wasmPath))
{
    console.log("NEURONiK_LocalMorphZRoute: sin build-wasm/neuronik_dsp.{js,wasm}");
    console.log("  [skip] el DSP del worklet no esta compilado todavia (via build_wasm.bat)");
    process.exit(0);
}

const asUrl = (...segments) => `file://${path.join(...segments).split(path.sep).join('/')}`;

const { createParameterStore } = await import(asUrl(webuiRoot, 'src/contracts/paramStore.js'));
const { gpFieldsFromState } = await import(asUrl(webuiRoot, 'src/wasm/audioParams.js'));
const { SCREEN_PARAMETER_IDS } = await import(asUrl(webuiRoot, 'src/contracts/screens.js'));

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
Module._neuronikInit(SR, BLOCK);

// --- 1. La pagina, en MODO LOCAL (sin host) ---------------------------------
// El MISMO store de la pagina: contrato generado + los ids del lienzo.
const store = createParameterStore({ ids: SCREEN_PARAMETER_IDS });
store.start();

check(store.getState().bridgeAvailable === false,
      "la pagina corre en MODO LOCAL (sin host): la matriz la siembra ella");

store.seedLocalModels([]);
store.setLocalModelReady(true);

const seeded = store.seedLocalMorphZRoute();
const parameters = store.getState().parameters;

check(seeded === true, "el store siembra la ruta local a Morph Z");
// Los dos primeros slots llevan las rutas de las envolventes: la siembra no las toca.
check(parameters.mod1Source < parameters.mod2Source && parameters.mod1Source > 0,
      "las rutas de las envolventes (slots 1 y 2) siguen intactas");
check(parameters.mod3Source > 0 && parameters.mod3Destination > 0 && parameters.mod3Amount === 1,
      "la ruta queda en el slot 3: LFO 2 (0.2857) -> Morph Z (0.9333), amount 1.0");

// Una ruta ya asignada no se pisa (aqui, un gesto del usuario).
const store2 = createParameterStore({ ids: SCREEN_PARAMETER_IDS });
store2.start();
store2.pushParameter('mod3Source', parameters.mod1Source, 'end');
check(store2.seedLocalMorphZRoute() === false,
      "un slot ya asignado NO se pisa (la siembra se rechaza entera)");

// --- 2. El worklet: mirror de GlobalParams + los fields de la pagina --------
const gpSize = Module._neuronikGlobalParamsSize();
const gpPtr = Module._malloc(gpSize);

const baseFields = Module._neuronikGlobalParamsLayout(0, 0);
const modFields = Module._neuronikModMatrixLayout(0, 0);

const basePtr = Module._malloc(4 * baseFields);
Module._neuronikGlobalParamsLayout(basePtr, baseFields);

const modPtr = Module._malloc(4 * modFields);
Module._neuronikModMatrixLayout(modPtr, modFields);

const byteOffsets = Array.from(Module.HEAP32.subarray(basePtr >> 2, (basePtr >> 2) + baseFields))
    .concat(Array.from(Module.HEAP32.subarray(modPtr >> 2, (modPtr >> 2) + modFields)));

Module._free(basePtr);
Module._free(modPtr);

// Misma disciplina que el worklet: bpm es f64 y los choice/int se escriben enteros.
const BPM_FIELD = 2;
const INT_FIELDS = new Set([12, 14, 15, 17, 19, 20, 22, 23, 25, 26, 28, 29, 31, 32]);

const mirror = new ArrayBuffer(gpSize);
const f32 = new Float32Array(mirror);
const f64 = new Float64Array(mirror);
const i32 = new Int32Array(mirror);

function writeField(fieldIndex, value)
{
    const byteOffset = byteOffsets[fieldIndex];
    if (byteOffset === undefined) return;

    if (fieldIndex === BPM_FIELD) { f64[byteOffset / 8] = Number(value); return; }

    const offset = byteOffset / 4;
    if (INT_FIELDS.has(fieldIndex)) i32[offset] = Math.round(Number(value));
    else f32[offset] = Number(value);
}

const fields = gpFieldsFromState(parameters);
for (const [fieldIndex, value] of fields) writeField(fieldIndex, value);

const heapU8 = Module.HEAPU8 ?? new Uint8Array(Module.HEAP32.buffer);
const push = () =>
{
    heapU8.set(new Uint8Array(mirror), gpPtr);
    Module._neuronikSetGlobalParams(gpPtr, gpSize);
};

push();

const sent = new Map(fields);
check(sent.get(28) === 2 && sent.get(29) === 28 && sent.get(30) === 1,
      `la ruta viaja como la fila 3 de la matriz: source ${sent.get(28)}, destino ${sent.get(29)}, amount ${sent.get(30)}`);

// --- 3. El motor de verdad: el arco se mueve --------------------------------
const blockBytes = BLOCK * 4;
const leftPtr = Module._malloc(blockBytes);
const rightPtr = Module._malloc(blockBytes);

const blocksPerSecond = SR / BLOCK;
const blockMs = (BLOCK / SR) * 1000;
const totalBlocks = Math.round(blocksPerSecond * 3);   // 3 s = 3 periodos a 1 Hz

let maxMod = 0;
let minMod = 0;
let crossings = 0;
let previous = 0;
let previousPeakBlock = -1;
const periods = [];
const peakTimes = [];

// Crestas: subidas a traves de +0.9 (una por periodo del LFO). Se cuentan umbrales
// con histeresis, no maximos locales: la senoide se muestrea cada 2.67 ms y su
// maximo real cae entre muestras.
const PEAK_GATE = 0.9;

for (let block = 0; block < totalBlocks; ++block)
{
    Module._neuronikProcess(leftPtr, rightPtr, BLOCK, 0, 0);

    const mod = Module._neuronikGetMod(28);

    maxMod = Math.max(maxMod, mod);
    minMod = Math.min(minMod, mod);

    if (block > 0 && Math.sign(mod) !== Math.sign(previous)) ++crossings;

    if (block > 0 && previous < PEAK_GATE && mod >= PEAK_GATE)
    {
        if (previousPeakBlock >= 0) periods.push((block - previousPeakBlock) * blockMs);
        previousPeakBlock = block;
        peakTimes.push(block * blockMs);
    }

    previous = mod;
}

const periodMs = periods.length > 0
    ? periods.reduce((sum, value) => sum + value, 0) / periods.length
    : -1;

check(maxMod > 0.9 && minMod < -0.9,
      `GetMod(28) barre los DOS signos: ${minMod.toFixed(4)} .. ${maxMod.toFixed(4)}`);
check(crossings >= 5, `y cruza por cero ${crossings} veces en 3 s (signo pintado, no rectificado)`);
check(periodMs > 950 && periodMs < 1050,
      `el periodo medido entre crestas es ~1 s (LFO 2 a 1 Hz por defecto): ${periodMs.toFixed(1)} ms`);

// --- 4. El conmutador: apagar deja el anillo en 0 y elegir LFO lo cambia -----
// Mismo camino que el dedo del usuario en la ficha MATRIZ: el store escribe la
// fila 3, la pagina manda los fields, y aqui se mira lo que contesta el motor.
const withRoute = (seconds) =>
{
    let peak = 0;
    let trough = 0;

    for (let block = 0; block < Math.round(blocksPerSecond * seconds); ++block)
    {
        Module._neuronikProcess(leftPtr, rightPtr, BLOCK, 0, 0);
        const mod = Module._neuronikGetMod(28);
        peak = Math.max(peak, mod);
        trough = Math.min(trough, mod);
    }

    return { peak, trough };
};

const send = () =>
{
    for (const [fieldIndex, value] of gpFieldsFromState(store.getState().parameters))
        writeField(fieldIndex, value);
    push();
};

// APAGADO: la fila vuelve a VIRGEN y el motor deja de modular (0 exacto, no
// "casi cero"): es lo que ve el usuario en el anillo quieto.
check(store.setLocalMorphRoute({ enabled: false }) === true,
      "el conmutador APAGA la ruta local (mismo camino que el boton de la ficha)");
send();

const apagado = withRoute(1);
check(apagado.peak === 0 && apagado.trough === 0,
      `apagada, el anillo mide 0 exacto en los dos signos (${apagado.peak} / ${apagado.trough})`);

// Y con el conmutador apagado, la siembra ya no hace nada al recargar.
check(store.seedLocalMorphZRoute() === false,
      "recargando con el conmutador apagado, la siembra NO vuelve a plantar la ruta");

// ENCENDIDO con otro LFO: la MISMA fila pasa a LFO 1 (indice 1 de la tabla) y el
// anillo vuelve a barrer los dos signos.
check(store.setLocalMorphRoute({ enabled: true, source: 'LFO 1' }) === true,
      "el conmutador ENCIENDE la ruta con LFO 1");
send();

const lfo1Fields = new Map(gpFieldsFromState(store.getState().parameters));
check(lfo1Fields.get(28) === 1 && lfo1Fields.get(29) === 28 && lfo1Fields.get(30) === 1,
      `la fila 3 viaja con LFO 1: source ${lfo1Fields.get(28)}, destino ${lfo1Fields.get(29)}, amount ${lfo1Fields.get(30)}`);

const conLfo1 = withRoute(2);
check(conLfo1.peak > 0.9 && conLfo1.trough < -0.9,
      `con LFO 1 el anillo tambien barre los dos signos: ${conLfo1.trough.toFixed(4)} .. ${conLfo1.peak.toFixed(4)}`);

// Dejar el store como arranco (encendido, LFO 2) para que el resto del test
// siga describiendo el estado de fabrica.
check(store.setLocalMorphRoute({ source: 'LFO 2' }) === true,
      "vuelta a LFO 2 (el valor inicial de la siembra)");
send();

// --- 5. Control negativo: sin la ruta el anillo mide 0 ----------------------
store.pushParameter('mod3Source', 0, 'end');
send();

const control = withRoute(1);
check(Math.max(Math.abs(control.peak), Math.abs(control.trough)) === 0,
      `con el slot 3 en Off el anillo mide 0 exacto (${control.peak} / ${control.trough})`);

console.log('');

if (failures > 0)
{
    console.log(`Checks failed: ${failures}`);
    process.exit(1);
}

console.log("El anillo del pad baila sin host (LFO 2 -> Morph Z por la ruta local).");
process.exit(0);
