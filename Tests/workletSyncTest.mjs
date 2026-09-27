/**
 * ABDNeural — guard de sincronia del worklet WASM (anti-drift build-wasm -> public/worklet).
 *
 * Compara por HASH (SHA256) los artefactos WASM que genera build-wasm y los que sirve
 * la WebUI. Hace fallar el build y el ctest si quedan desincronizados: un DSP viejo
 * en WebUI/public/worklet (y por tanto en WebUI/dist/worklet y embebido en el plugin)
 * suena y no deja rastro en el codigo — el drift ya ocurrio dos veces.
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
 * Se lanza de tres sitios, con la misma comparacion:
 *   - ctest:  NEURONiK_WorkletSync (Tests/workletSyncTest.mjs via node)
 *   - build.bat: tras compilar WASM (public vs build-wasm) y tras `pnpm build` (dist vs ambos)
 *   - build_wasm.bat: tras sync-wasm.mjs (public vs build-wasm)
 * Todos comparten este fichero — un solo hash, un solo mensaje de error.
 */

import fs from 'node:fs';
import crypto from 'node:crypto';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const thisDir = path.dirname(fileURLToPath(import.meta.url));
const neuralRoot = path.resolve(thisDir, '..');

const sourceDir = path.join(neuralRoot, 'build-wasm');
const publicDir = path.join(neuralRoot, 'WebUI', 'public', 'worklet');
const distDir = path.join(neuralRoot, 'WebUI', 'dist', 'worklet');

const ARTIFACTS = ['neuronik_dsp.js', 'neuronik_dsp.wasm'];

function sha256(filePath) {
  const data = fs.readFileSync(filePath);
  return crypto.createHash('sha256').update(data).digest('hex');
}

function shortHash(hash) {
  return hash.slice(0, 12);
}

let failures = [];
let compared = 0;
let skippedBecauseNoBuildWasm = 0;

console.log('[worklet-sync] comparando hashes SHA256 (build-wasm <-> public/worklet <-> dist/worklet)');

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
      const publicHash = sha256(publicPath);
      const distHash = sha256(distPath);
      compared += 1;
      if (publicHash !== distHash) {
        const msg = `${name}: dist/worklet desincronizado de public/worklet (falta pnpm build) — public ${shortHash(publicHash)} != dist ${shortHash(distHash)}`;
        console.error(`  [FAIL] ${msg}`);
        console.error(`         public: ${publicPath}`);
        console.error(`         dist:   ${distPath}`);
        failures.push(msg);
      } else {
        console.log(`  [ok]   ${name}: public/worklet == dist/worklet (${shortHash(publicHash)})`);
      }
    } else if (hasPublic && !hasDist) {
      console.log(`  [skip] ${name}: sin dist/worklet — aun no se ha hecho pnpm build (public: ${shortHash(sha256(publicPath))})`);
    } else {
      console.log(`  [skip] ${name}: sin build-wasm/${name} — ejecuta build_wasm.bat para generar el origen`);
    }
    continue;
  }

  const sourceHash = sha256(sourcePath);
  const sourceSize = fs.statSync(sourcePath).size;

  if (!hasPublic) {
    const msg = `${name}: falta en public/worklet pero existe en build-wasm (ejecuta sync-wasm.mjs) — build-wasm ${shortHash(sourceHash)} (${sourceSize} bytes)`;
    console.error(`  [FAIL] ${msg}`);
    console.error(`         build-wasm: ${sourcePath}`);
    console.error(`         public:     ${publicPath} (no existe)`);
    failures.push(msg);
    continue;
  }

  const publicHash = sha256(publicPath);
  const publicSize = fs.statSync(publicPath).size;

  compared += 1;
  if (sourceHash !== publicHash) {
    const msg = `${name}: public/worklet desincronizado de build-wasm — build-wasm ${shortHash(sourceHash)} (${sourceSize} bytes) != public ${shortHash(publicHash)} (${publicSize} bytes) (ejecuta WebUI/scripts/sync-wasm.mjs o build_wasm.bat)`;
    console.error(`  [FAIL] ${msg}`);
    console.error(`         build-wasm: ${sourcePath}`);
    console.error(`         public:     ${publicPath}`);
    failures.push(msg);
  } else {
    console.log(`  [ok]   ${name}: build-wasm == public/worklet (${shortHash(sourceHash)}, ${sourceSize} bytes)`);
  }

  if (!hasDist) {
    console.log(`  [skip] ${name}: sin dist/worklet — aun no se ha hecho pnpm build`);
    continue;
  }

  const distHash = sha256(distPath);
  const distSize = fs.statSync(distPath).size;

  if (sourceHash !== distHash) {
    const msg = `${name}: dist/worklet desincronizado de build-wasm — build-wasm ${shortHash(sourceHash)} != dist ${shortHash(distHash)} (${distSize} bytes) (ejecuta pnpm build en WebUI/)`;
    console.error(`  [FAIL] ${msg}`);
    console.error(`         build-wasm: ${sourcePath}`);
    console.error(`         dist:       ${distPath}`);
    failures.push(msg);
  } else {
    console.log(`  [ok]   ${name}: build-wasm == dist/worklet (${shortHash(distHash)})`);
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
