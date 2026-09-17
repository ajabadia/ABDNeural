/**
 * Copy the WASM DSP artifacts from build-wasm/ into WebPilot/public/worklet/.
 *
 * Run this after `build_wasm.bat` whenever the DSP changes — the AudioWorklet
 * path serves the binary from the static export (public/ -> out/), so a stale
 * copy here would silently ship an outdated DSP to the browser.
 *
 * Usage: node WebPilotVite/scripts/sync-wasm.mjs   (from ABDNeural/, or any cwd)
 */

import { copyFileSync, existsSync, mkdirSync, statSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const thisDir = path.dirname (fileURLToPath (import.meta.url));
const neuralRoot = path.resolve (thisDir, '..', '..');

const sourceDir = path.join (neuralRoot, 'build-wasm');
const targetDir = path.join (neuralRoot, 'WebPilot', 'public', 'worklet');

const ARTIFACTS = ['neuronik_dsp.js', 'neuronik_dsp.wasm'];

let failed = false;
for (const name of ARTIFACTS) {
  const source = path.join (sourceDir, name);

  if (!existsSync (source)) {
    console.error (`[sync-wasm] falta ${name} en ${sourceDir} — ejecuta build_wasm.bat primero`);
    failed = true;
    continue;
  }

  mkdirSync (targetDir, { recursive: true });
  copyFileSync (source, path.join (targetDir, name));
  console.log (`[sync-wasm] ${name} -> ${path.relative (neuralRoot, path.join (targetDir, name))} (${statSync (source).size} bytes)`);
}

process.exit (failed ? 1 : 0);
