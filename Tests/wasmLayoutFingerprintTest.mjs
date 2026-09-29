/*
  ==============================================================================

    wasmLayoutFingerprintTest.mjs
    Comprueba el binario REAL que sirve la WebUI, no un doble de test:
    que publique la firma del layout y que coincida con la que espera la
    pagina. Uso:

                   node Tests/wasmLayoutFingerprintTest.mjs
                   (opcional) ruta/neuronik_dsp.js

    Por defecto mira WebUI\public\worklet\neuronik_dsp.js, que es el que
    carga el AudioWorklet. Es el sitio donde el mecanismo tiene que cerrar:
    un binario que no exporta la firma hace que la pagina avise, y un
    binario que la exporta con OTRO valor hace lo mismo -- pero solo uno
    de los dos es un fallo de compilacion.

    POR QUE ESTE TEST Y NO SOLO EL DE gpMirror. Los tests de gpMirror
    usan un modulo falso, asi que comprueban que la pagina COMPARA bien,
    no que el binario TENGA que comparar. Este es el unico que abre el
    `.wasm` de verdad; sin el, un binario antiguo --sin el export-- pasa
    todos los tests que hay y la pagina avisa en silencio.

    NINGUNA COMPROBACION SE SALE ANTES DE TIEMPO. La primera version
    hacia process.exit en cuanto faltaba el export de la firma, con lo
    que la del numero de campos --que va despues-- no llegaba a correr
    nunca. Un test con un tramo que solo se ejecuta cuando todo lo
    anterior va bien no es un test: es codigo sin probar, y asi se
    colaron aqui dos cosas, un nombre de export que no existe y un
    TypeError esperando a que alguien recompilara el binario.

    LAS RUTAS CUELGAN DEL FICHERO, NO DE process.cwd(). ctest arranca
    este test con el directorio de build como cwd, de modo que un
    caminho relativo ahi apunta al sitio equivocado y el fallo se lee
    como "falta el binario" cuando lo que falta es el punto de partida.
    Mismo criterio que workletSyncTest.mjs y workletMirrorLayoutTest.mjs.

  ==============================================================================
*/

import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const thisDir = path.dirname(fileURLToPath(import.meta.url));
const neuralRoot = path.resolve(thisDir, '..');

const DEFAULT_MODULE = 'WebUI/public/worklet/neuronik_dsp.js';
const GENERATED = 'WebUI/generated/gp-layout.generated.js';

const jsPathArg = process.argv[2];
const jsPath = path.resolve(neuralRoot, jsPathArg ?? DEFAULT_MODULE);
const wasmPath = jsPath.replace(/\.js$/, '.wasm');

let failures = 0;
function check (ok, label, detail) {
  console.log(`  ${ok ? 'SI ' : 'NO '} ${label}${detail ? ` -- ${detail}` : ''}`);
  if (!ok) failures++;
}

// --- 1. La firma que espera la pagina -----------------------------------------
const generatedPath = path.join(neuralRoot, GENERATED);
const generated = await readFile(generatedPath, 'utf8');
const match = generated.match(/LAYOUT_FINGERPRINT\s*=\s*(\d+)/);
if (!match) {
  console.error(`[fingerprint] ${generatedPath} no declara LAYOUT_FINGERPRINT`);
  process.exit(1);
}
const expected = Number(match[1]);
console.log(`[fingerprint] binario : ${path.relative(neuralRoot, jsPath)}`);
console.log(`[fingerprint] pagina  : ${expected}`);

// --- 2. Instanciar el binario real -------------------------------------------
// Mismo camino que neuronik_wasm_smoke.mjs: el glue ES6 usa fetch sobre
// import.meta.url, que no funciona en Node sobre file://, asi que se le
// entrega el binario por instantiateWasm.
const wasmBinary = await readFile(wasmPath);
const { default: createModule } = await import(
  `file://${jsPath.split(path.sep).join('/')}`
);

const Module = await createModule({
  instantiateWasm (info, receiveInstance) {
    WebAssembly.instantiate(wasmBinary, info)
      .then((out) => receiveInstance(out.instance))
      .catch((e) => {
        console.error('[fingerprint] instantiateWasm fallo:', e);
        process.exit(1);
      });
    return {};
  },
});

Module._neuronikInit(48000, 128);

// --- 3. La firma que publica el binario --------------------------------------
const hasFingerprint = typeof Module._neuronikGlobalParamsLayoutFingerprint === 'function';
check(
  hasFingerprint,
  'el .wasm publica neuronikGlobalParamsLayoutFingerprint',
  hasFingerprint ? '' : 'falta el export: recompila el .wasm con el exportador'
);

if (hasFingerprint) {
  const got = Module._neuronikGlobalParamsLayoutFingerprint();
  console.log(`[fingerprint] binario : ${got}`);
  check(
    got === expected,
    'la firma del binario es la que espera la pagina',
    got === expected ? '' : `binario ${got} != pagina ${expected}`
  );
} else {
  console.log('  NOTA: sin este export el worklet postea layoutFingerprint: null');
  console.log('        y la pagina avisa de que el binario no es el suyo.');
}

// --- 4. El numero de campos, que va en claro ---------------------------------
// La firma es un hash: si dos tablas distintas dieran el mismo valor seria un
// accidente, y el hash no lo delata hasta que colisiona. El numero de campos
// va en claro y se puede comparar sin esperar a eso.
//
// El recuento NO tiene export propio: se pide el de la funcion de layout
// pasándole punteros nulos, que es la via que usa gpMirror. Llamar a un
// neuronikGlobalParamsFieldCount() que no existe daria un TypeError que
// pareceria un fallo del binario cuando lo que fallaria seria este test.
const hasLayout = typeof Module._neuronikGlobalParamsLayout === 'function';
const fieldMatch = generated.match(/LAYOUT_FIELD_COUNT\s*=\s*(\d+)/);

if (!fieldMatch) {
  console.log('  --  el generado no declara LAYOUT_FIELD_COUNT: nada que comparar');
} else if (!hasLayout) {
  check(false, 'el .wasm publica neuronikGlobalParamsLayout', 'falta el export');
} else {
  const want = Number(fieldMatch[1]);
  const gotFields = Module._neuronikGlobalParamsLayout(0, 0);
  console.log(`[fingerprint] campos  : binario ${gotFields}, pagina ${want}`);
  check(
    gotFields === want,
    'el numero de campos cuadra con la pagina',
    gotFields === want ? '' : `binario ${gotFields} != pagina ${want}`
  );
}

console.log('');
if (failures === 0) {
  console.log('='.repeat(60));
  console.log('  [EXITO] el .wasm servido y la pagina hablan del mismo layout');
  console.log('='.repeat(60));
  process.exit(0);
}

console.log('='.repeat(60));
console.log(`  [FALLO] ${failures} comprobacion(es) distinta(s)`);
console.log('  El .wasm y gp-layout.generated.js tienen que salir del MISMO');
console.log('  commit: si no, hay que regenerar y recompilar a la vez.');
console.log('='.repeat(60));
process.exit(1);
