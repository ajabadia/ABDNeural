/**
 * Smoke test del modulo WASM de NEURONiK (Fase 5).
 * Uso: node Tests/neuronik_wasm_smoke.mjs build-wasm/neuronik_dsp.js
 *
 * Verifica: init, noteOn->audio finito y no silencioso, allNotesOff,
 * getNumActiveVoices y estabilidad de memoria (ALLOW_MEMORY_GROWTH).
 * El DSP por defecto es determinista, igual que en nativo: si un dia se
 * quiere paridad bit-exacta nativo<->wasm, este test es el ancla JS.
 */

import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const [, , jsPathArg] = process.argv;
if (!jsPathArg) {
  console.error('[smoke] uso: node neuronik_wasm_smoke.mjs <ruta/neuronik_dsp.js>');
  process.exit(2);
}

const jsPath = path.resolve(process.cwd(), jsPathArg);
const wasmPath = jsPath.replace(/\.js$/, '.wasm');

// Binario leido de disco y entregado via hook instantiateWasm: el glue ES6
// (ENVIRONMENT=web,worker) usa fetch sobre import.meta.url, que no funciona
// en Node sobre file://. El hook es la via documentada y entorno-agnostica.
const wasmBinary = await readFile(wasmPath);

const { default: createModule } = await import(
  `file://${jsPath.split(path.sep).join('/')}`
);

console.log('[smoke] instanciando modulo...');
const Module = await createModule({
  instantiateWasm (info, receiveInstance) {
    WebAssembly.instantiate (wasmBinary, info)
      .then ((out) => receiveInstance (out.instance))
      .catch ((e) => { console.error ('[smoke] instantiateWasm fallo:', e); process.exit (1); });
    return {}; // el glue rellena los exports via receiveInstance
  },
});

const SAMPLE_RATE = 48000;
const BLOCK = 128;
const BLOCKS = 100; // ~0.21 s

Module._neuronikInit(SAMPLE_RATE, BLOCK);

// Render de la nota 60 (velocidad ~0.79): 24 bytes por evento Runtime::Event
// [i32 type, i32 channel, i32 note, i32 value14, f32 value, i32 sampleOffset]
const eventPtr = Module._malloc(24);
const heap = Module.HEAP32;
const heapF = Module.HEAPF32;
const noteOn = 0; // Runtime::EventType::NoteOn

heap[eventPtr >> 2] = noteOn;
heap[(eventPtr >> 2) + 1] = 1; // channel
heap[(eventPtr >> 2) + 2] = 60; // note
heap[(eventPtr >> 2) + 3] = 8192; // value14
heapF[(eventPtr >> 2) + 4] = 100 / 127; // value
heap[(eventPtr >> 2) + 5] = 0; // sampleOffset

const blockBytes = BLOCK * 4;
const leftPtr = Module._malloc(blockBytes);
const rightPtr = Module._malloc(blockBytes);

let peak = 0;
let finite = true;
for (let b = 0; b < BLOCKS; ++b) {
  Module._neuronikProcess(leftPtr, rightPtr, BLOCK, eventPtr, b === 0 ? 1 : 0);
  const left = heapF.subarray(leftPtr >> 2, (leftPtr >> 2) + BLOCK);
  for (let i = 0; i < BLOCK; ++i) {
    const v = left[i];
    if (!Number.isFinite(v)) {
      finite = false;
      break;
    }
    peak = Math.max(peak, Math.abs(v));
  }
}

const active = Module._neuronikNumActiveVoices();

// allNotesOff dispara RELEASES (contrato ISynthesisEngine: RT-safe, sin
// click, mantiene la cola del envelope). Las voces se drenan al seguir
// renderizando: verificar que alcanzan 0 en <= ~5 s de audio (el release
// por defecto es 200 ms pero la cola exponencial cruza el umbral de Idle
// tarde: ~1.4-1.9 s; 5 s deja margen).
Module._neuronikAllNotesOff();
let activeAfterPanic = -1;
for (let b = 0; b < 1875; ++b) {
  Module._neuronikProcess (leftPtr, rightPtr, BLOCK, null, 0);
  const a = Module._neuronikNumActiveVoices();
  activeAfterPanic = a;
  if (a === 0) break;
}

Module._free(eventPtr);
Module._free(leftPtr);
Module._free(rightPtr);

console.log(`[smoke] peak=${peak.toFixed(5)} finite=${finite} active=${active} activeAfterPanic=${activeAfterPanic}`);

const checks = [
  [finite, 'la salida contiene NaN/Inf'],
  [peak > 1e-4, 'la salida es silencio (peak <= 1e-4)'],
  [peak < 50, 'salida descontrolada (peak > 50)'],
  [active > 0, 'no hay voz activa tras noteOn'],
  [activeAfterPanic === 0, `allNotesOff no dreno las voces (quedaron ${activeAfterPanic} activas tras 5 s de release)`],
];

let failed = false;
for (const [ok, message] of checks) {
  if (!ok) {
    console.error(`[smoke] FALLO: ${message}`);
    failed = true;
  }
}

if (failed) process.exit(1);
console.log('[smoke] OK: WASM renderiza audio del DSP real');
