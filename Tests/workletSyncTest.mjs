/**
 * ABDNeural — guard de sincronia del worklet WASM (anti-drift build-wasm -> public/worklet).
 *
 * Compara por HASH (SHA256) los artefactos WASM que genera build-wasm y los que sirve
 * la WebUI. Hace fallar el build y el ctest si quedan desincronizados: un DSP viejo
 * en WebUI/public/worklet (y por tanto en WebUI/dist/worklet y embebido en el plugin)
 * suena y no deja rastro en el codigo — el drift ya ocurrio dos veces.
 *
 * LA HUELLA LA PONE EL SINCRONIZADOR, no este test (cambiado el 2026-10-02, medido).
 * Este fichero comparaba los bytes CRUDOS con su propio sha256, y eso declaraba
 * desincronizado un DSP que no habia cambiado en nada: `build-wasm/` NO esta
 * versionado (es salida de Emscripten, y en Windows llega con CRLF) mientras que
 * `WebUI/public/worklet/neuronik_dsp.js` lo esta, con `*.js text eol=lf` en
 * `.gitattributes`. Los dos ficheros eran el MISMO codigo y se declaraban
 * distintos por dos bytes de salto de linea:
 *
 *   build-wasm/neuronik_dsp.js   14454 bytes, 2 CRLF,  sha ec8222f67231
 *   public/worklet/neuronik_dsp.js 14452 bytes, LF,    sha c6b5a5b2ec52
 *
 * El rojo era para siempre: la unica manera de callarlo era copiar, y copiar mete
 * CRLF en un fichero que el repo exige en LF. Y el aviso de la cabecera del
 * sincronizador ("a stale copy here would silently ship an outdated DSP") dejaba
 * de ser creible, porque el unico rojo que producia no era ninguno de los dos que
 * ese aviso describe.
 *
 * El arreglo no es una copia de la regla: es IMPORTAR la regla. `sync-wasm.mjs`
 * exporta `huella()`/`contenido()` justo para esto, asi que la pregunta de "son
 * el mismo fichero" la responde el que sincroniza y no una segunda opinion que
 * puede apartarse (que es como se declara un rojo eterno). El `.wasm` se sigue
 * comparando byte a byte, que es lo que corresponde a un binario.
 *
 * Origenes:
 *   - build-wasm/neuronik_dsp.{js,wasm}  (salida de build_wasm.bat / wasm/CMakeLists.txt)
 *   - WebUI/public/worklet/neuronik_dsp.{js,wasm}  (SSOT versionada, copiada por sync-wasm.mjs)
 *   - WebUI/dist/worklet/neuronik_dsp.{js,wasm}    (copia de public/ que hace `pnpm build`)
 *
 * Reglas (lo que hace fallar):
 *   - Si build-wasm no existe: no hay nada que comparar — avisa y pasa (clone limpio
 *     sin haber compilado WASM todavia). Es el mismo skip que hacen otros guards con node.
 *   - Si build-wasm existe y public/worklet falta o su hash difiere: FAIL.
 *   - Si public/worklet y dist/worklet existen y difieren: FAIL (dist desactualizado
 *     respecto a public: falta `pnpm build`).
 *   - Si build-wasm y dist/worklet existen y difieren: FAIL (dist desactualizado
 *     respecto al DSP).
 *
 * El hash es el de `sync-wasm.mjs`: los `.js` se comparan con los finales de linea
 * normalizados y los `.wasm` byte a byte. Que coincidan los hashes NO implica que
 * los ficheros tengan los mismos bytes, asi que cuando los tamanos crudos se
 * distinguen y el hash coincide, el guard lo dice en vez de callarlo.
 *
 * Se lanza de tres sitios, con la misma comparacion:
 *   - ctest:  NEURONiK_WorkletSync (Tests/workletSyncTest.mjs via node)
 *   - build.bat: tras compilar WASM (public vs build-wasm) y tras `pnpm build` (dist vs ambos)
 *   - build_wasm.bat: tras sync-wasm.mjs (public vs build-wasm)
 * Todos comparten este fichero — un solo hash, un solo mensaje de error.
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// La regla de "son el mismo fichero" viene del sincronizador, que es quien
// escribe el destino y por tanto quien tiene que decir si lo que copia es lo
// mismo que lo que copia. Importarlo es la decision que hace que este guard no
// pueda declarar un rojo eterno por dos bytes de salto de linea.
import { huella } from '../WebUI/scripts/sync-wasm.mjs';

const thisDir = path.dirname(fileURLToPath(import.meta.url));
const neuralRoot = path.resolve(thisDir, '..');

const sourceDir = path.join(neuralRoot, 'build-wasm');
const publicDir = path.join(neuralRoot, 'WebUI', 'public', 'worklet');
const distDir = path.join(neuralRoot, 'WebUI', 'dist', 'worklet');

const ARTIFACTS = ['neuronik_dsp.js', 'neuronik_dsp.wasm'];

// El segundo argumento es el NOMBRE del artefacto, y no es decorativo: es lo que
// decide si se normalizan los finales de linea (`neuronik_dsp.js` si, `.wasm` no).
// Pasarlo mal no falla, y devuelve la huella de byte a byte de un texto: el mismo
// fallo de antes, pero sin que se note.
function sha256(filePath, name) {
  return huella(filePath, name);
}

// El aviso de "los bytes difieren pero el codigo es el mismo". Sale solo cuando
// pasa eso, y es lo que evita que el "ok" esconda la razon por la que un dia
// alguien se pregunta por que los ficheros no pesan lo mismo.
function notaDeBytes (nombre, tamanoA, tamanoB) {
  if (tamanoA === tamanoB) return '';
  const delta = Math.abs (tamanoB - tamanoA);

  return ` (los bytes crudos se distinguen en ${delta}: finales de linea, que si se normalizan)`;
}

function shortHash(hash) {
  return hash.slice(0, 12);
}

let failures = [];
let compared = 0;
let skippedBecauseNoBuildWasm = 0;

console.log('[worklet-sync] comparando hashes SHA256 (build-wasm <-> public/worklet <-> dist/worklet)');
console.log('[worklet-sync] la regla es la de sync-wasm.mjs: los .js con finales de linea normalizados, los .wasm byte a byte');

for (const name of ARTIFACTS) {
  const sourcePath = path.join(sourceDir, name);
  const publicPath = path.join(publicDir, name);
  const distPath = path.join(distDir, name);

  const hasSource = fs.existsSync(sourcePath);
  const hasPublic = fs.existsSync(publicPath);
  const hasDist = fs.existsSync(distPath);

  // Sin build-wasm: clone limpio sin WASM compilado — no hay origen con el que comparar.
  // No se falla: se avisa y se sigue (el guard de build.bat ya advierte que nowasm deja
  // el worklet viejo; el ctest sin WASM no puede juzgar drift).
  if (!hasSource) {
    skippedBecauseNoBuildWasm += 1;
    if (hasPublic && hasDist) {
      // Aun asi, public vs dist si se puede juzgar: un `pnpm build` desactualizado.
      const publicHash = sha256(publicPath, name);
      const distHash = sha256(distPath, name);
      compared += 1;
      if (publicHash !== distHash) {
        const msg = `${name}: dist/worklet desincronizado de public/worklet (falta pnpm build) — public ${shortHash(publicHash)} != dist ${shortHash(distHash)}`;
        console.error(`  [FAIL] ${msg}`);
        console.error(`         public: ${publicPath}`);
        console.error(`         dist:   ${distPath}`);
        failures.push(msg);
      } else {
        console.log(`  [ok]   ${name}: public/worklet == dist/worklet (${shortHash(publicHash)}${notaDeBytes(name, fs.statSync(publicPath).size, fs.statSync(distPath).size)})`);
      }
    } else if (hasPublic && !hasDist) {
      console.log(`  [skip] ${name}: sin dist/worklet — aun no se ha hecho pnpm build (public: ${shortHash(sha256(publicPath, name))})`);
    } else {
      console.log(`  [skip] ${name}: sin build-wasm/${name} — ejecuta build_wasm.bat para generar el origen`);
    }
    continue;
  }

  const sourceHash = sha256(sourcePath, name);
  const sourceSize = fs.statSync(sourcePath).size;

  if (!hasPublic) {
    const msg = `${name}: falta en public/worklet pero existe en build-wasm (ejecuta sync-wasm.mjs) — build-wasm ${shortHash(sourceHash)} (${sourceSize} bytes)`;
    console.error(`  [FAIL] ${msg}`);
    console.error(`         build-wasm: ${sourcePath}`);
    console.error(`         public:     ${publicPath} (no existe)`);
    failures.push(msg);
    continue;
  }

  const publicHash = sha256(publicPath, name);
  const publicSize = fs.statSync(publicPath).size;

  compared += 1;
  if (sourceHash !== publicHash) {
    const msg = `${name}: public/worklet desincronizado de build-wasm — build-wasm ${shortHash(sourceHash)} (${sourceSize} bytes) != public ${shortHash(publicHash)} (${publicSize} bytes) (ejecuta WebUI/scripts/sync-wasm.mjs o build_wasm.bat)`;
    console.error(`  [FAIL] ${msg}`);
    console.error(`         build-wasm: ${sourcePath}`);
    console.error(`         public:     ${publicPath}`);
    failures.push(msg);
  } else {
    console.log(`  [ok]   ${name}: build-wasm == public/worklet (${shortHash(sourceHash)}${notaDeBytes(name, sourceSize, publicSize)})`);
  }

  if (!hasDist) {
    console.log(`  [skip] ${name}: sin dist/worklet — aun no se ha hecho pnpm build`);
    continue;
  }

  const distHash = sha256(distPath, name);
  const distSize = fs.statSync(distPath).size;

  if (sourceHash !== distHash) {
    const msg = `${name}: dist/worklet desincronizado de build-wasm — build-wasm ${shortHash(sourceHash)} != dist ${shortHash(distHash)} (${distSize} bytes) (ejecuta pnpm build en WebUI/)`;
    console.error(`  [FAIL] ${msg}`);
    console.error(`         build-wasm: ${sourcePath}`);
    console.error(`         dist:       ${distPath}`);
    failures.push(msg);
  } else {
    console.log(`  [ok]   ${name}: build-wasm == dist/worklet (${shortHash(distHash)}${notaDeBytes(name, sourceSize, distSize)})`);
  }

  if (publicHash !== distHash) {
    const msg = `${name}: dist/worklet desincronizado de public/worklet — public ${shortHash(publicHash)} != dist ${shortHash(distHash)} (falta pnpm build)`;
    console.error(`  [FAIL] ${msg}`);
    failures.push(msg);
  }
}

if (failures.length > 0) {
  console.error(`\n[worklet-sync] FAIL: ${failures.length} desincronizacion${failures.length > 1 ? 'es' : ''} detectada${compared > 0 ? ` (${compared} comparaciones)` : ''}.`);
  console.error('[worklet-sync] Accion: ejecuta build_wasm.bat (compila + sync) y, si WebUI/dist existe, pnpm build en WebUI/.');
  process.exit(1);
}

if (skippedBecauseNoBuildWasm === ARTIFACTS.length && compared === 0) {
  console.log('[worklet-sync] SKIP: sin build-wasm — nada que comparar (ejecuta build_wasm.bat para generar el origen).');
} else {
  console.log(`[worklet-sync] OK: ${compared} comparacion${compared !== 1 ? 'es' : ''} sin drift.`);
}
