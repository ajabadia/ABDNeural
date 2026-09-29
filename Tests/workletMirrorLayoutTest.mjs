/**
 * ABDNeural - EL ESPEJO DE PARAMETROS LLEGA ENTERO AL MOTOR DEL NAVEGADOR.
 *
 * La pagina escribe el espejo de `GlobalParams` por INDICE DE CAMPO, no por
 * offset (`WebUI/src/wasm/audioParams.js` -> `CONTRACT_TO_GP_FIELD`), y el
 * worklet traduce ese indice con el layout que le publico el motor. Si el campo
 * no esta en el layout, el worklet no tiene donde escribir y se lo come: el knob
 * se mueve en la pagina, el motor no oye nada y no salta ningun error.
 *
 * Eso es exactamente lo que pasaba con el `.wasm` versionado de
 * `WebUI/public/worklet`: era ANTERIOR al layout, asi que `neuronikModMatrixLayout`
 * publicaba doce campos (la matriz) y no los veinticuatro del bus. Con 34 campos
 * publicados y la pagina escribiendo hasta el 39, los seis mandos del hueco 1
 * (params[0..3], gain, mix) se perdian en silencio.
 *
 * QUE COMPRUEBA (y por que no lo cubren los otros):
 *   1. el motor REAL de este `.wasm` publica TODOS los campos que la pagina
 *      escribe — el numero sale del binario, no de una constante del test;
 *   2. un push completo de la pagina no produce ni un aviso de campo ausente;
 *   3. un campo de mas SI produce el aviso `neuronik:layout`, con su indice:
 *      la red de seguridad que hace visible lo que antes se perdia en silencio.
 *
 * El puente tiene su propio test (`Tests/WasmLayoutOrderTest.cpp`) y la pagina
 * el suyo (`WebUI/tests/audioParams.test.js`, `highestGpField() === 39`). Este
 * es el UNICO que junta las tres mitades: el mapa de la pagina, el layout del
 * puente y el BINARIO que se sirve al navegador, que es donde se separan.
 *
 * Skip (mismo criterio que NEURONiK_WorkletSync): sin el `.wasm` no hay motor
 * que arrancar, asi que avisa y pasa.
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const thisDir = path.dirname(fileURLToPath(import.meta.url));
const neuralRoot = path.resolve(thisDir, '..');
const webuiRoot = path.join(neuralRoot, 'WebUI');

const workletDir = path.join(webuiRoot, 'public', 'worklet');
const jsPath = path.join(workletDir, 'neuronik-worklet.js');
const wasmPath = path.join(workletDir, 'neuronik_dsp.wasm');

if (!fs.existsSync(jsPath) || !fs.existsSync(wasmPath))
{
    console.log('NEURONiK_WorkletMirrorLayout: sin WebUI/public/worklet/neuronik_dsp.{js,wasm}');
    console.log('  [skip] el DSP del worklet no esta compilado todavia (via build_wasm.bat)');
    process.exit(0);
}

const asUrl = (...segments) => `file://${path.join(...segments).split(path.sep).join('/')}`;

const { CONTRACT_TO_GP_FIELD, highestGpField } =
    await import(asUrl(webuiRoot, 'src/wasm/audioParams.js'));

const fallos = [];

function check (ok, message)
{
    console.log(`  ${ok ? 'ok  ' : 'FALLA'} ${message}`);
    if (!ok) fallos.push(message);
}

// ── El worklet de verdad, con el .wasm que se sirve al navegador ─────────────
let Processor = null;

globalThis.sampleRate = 48000;
globalThis.currentRenderQuantum = 128;
globalThis.AudioWorkletProcessor = class {
    constructor () {
        const posted = [];
        this.port = {
            onmessage: null,
            posted,
            postMessage (message) { posted.push(message); },
        };
    }
};
globalThis.registerProcessor = (name, cls) => { Processor = cls; };

await import(asUrl(workletDir, 'neuronik-worklet.js'));

const processor = new Processor({
    processorOptions: {
        wasmBinary: fs.readFileSync(wasmPath),
        sampleRate: 48000,
    },
});

for (let i = 0; i < 400 && !processor.ready && !processor.initError; i += 1)
    await new Promise((resolve) => setTimeout(resolve, 25));

if (!processor.ready)
{
    console.log(`NEURONiK_WorkletMirrorLayout: el motor no arranco (${processor.initError})`);
    process.exit(1);
}

const layout_messages = () => processor.port.posted.filter((m) => m.type === 'neuronik:layout');
const push = (fields) => processor.port.onmessage({ data: { type: 'neuronik:params', fields } });

// ── 1) Todo lo que la pagina escribe existe en el layout de este motor ──────
const ultimoCampo = highestGpField();
const camposQueFaltan = Object.entries(CONTRACT_TO_GP_FIELD)
    .filter(([, index]) => index >= processor.paramsFieldCount)
    .map(([id]) => id);

console.log(`NEURONiK_WorkletMirrorLayout: el motor publica ${processor.paramsFieldCount} campos;`
    + ` la pagina escribe hasta el ${ultimoCampo}`);

check(camposQueFaltan.length === 0,
    `el motor publica los ${ultimoCampo + 1} campos que la pagina escribe`
    + (camposQueFaltan.length > 0 ? ` (fuera: ${camposQueFaltan.join(', ')})` : ''));

// ── 2) Un push completo de la pagina no genera ni un aviso ──────────────────
const snapshot = Object.entries(CONTRACT_TO_GP_FIELD).map(([contractId, index]) => [index, 0.5]);
push(snapshot);
check(layout_messages().length === 0,
    'un push completo de la pagina no produce el aviso de campos ausentes');

// ── 3) Y un campo de mas si lo produce, con su indice ───────────────────────
// El fantasma se mide contra el layout REAL (`paramsFieldCount`), no contra
// el campo mas alto que escribe la pagina: el motor publica cuatro huecos
// y la pagina solo tiene el primero migrado, asi que un indice entre 40 y 57
// es un hueco que el motor SI tiene y la pagina todavia no escribe.
const fantasma = processor.paramsFieldCount + 3;
push([[fantasma, 0.25]]);

const avisos = layout_messages();
check(avisos.length === 1 && avisos[0].missingFields.includes(fantasma),
    `un campo que el motor no publica (${fantasma}) llega avisado por su indice`
    + ` (${JSON.stringify(avisos.map((m) => m.missingFields))})`);

console.log(fallos.length === 0
    ? 'NEURONiK_WorkletMirrorLayout: OK'
    : `NEURONiK_WorkletMirrorLayout: ${fallos.length} fallo(s)`);
process.exit(fallos.length === 0 ? 0 : 1);
