/**
 * Test de paridad WASM <-> nativo (Fase 5, hito 1/2).
 * Uso: node Tests/neuronik_wasm_parity.mjs build-wasm/neuronik_dsp.js [build-wasm/parity-native.json] [--strict]
 *
 * El test nativo NEURONiK_WasmParityTest ejecuta los MISMOS escenarios sobre
 * la misma frontera (DspEngineFacade) que consume el puente WASM y vuelca el
 * canal izquierdo a JSON. Este test instancia el módulo WASM real y compara
 * muestra a muestra.
 *
 * Comparación: distancia en ulps (unidades del último lugar de float32) con
 * presupuesto POR ESCENARIO. Resultado medido con MSVC vs emscripten/musl:
 * A/B/D son bit-exactos (0 ulps) — el núcleo (osciladores, envolventes,
 * modMatrix) coincide bit a bit entre toolchains. C (cola de delay/reverb
 * tras el pánico) difiere en <= 16 ulps: una diferencia de 1 ulp de libm
 * (sin/exp) se amplifica por el feedback del delay; 16 ulps en muestras de
 * ~1e-3 son ~3e-8 absolutos (≈ -150 dBFS, inaudible). Presupuesto 0 en
 * A/B/D = alarma inmediata ante CUALQUIER cambio de algoritmo o
 * determinismo; el presupuesto de C solo tolera ruido de libm de la cola.
 * Guard absoluto adicional: maxDiffAbs <= 1e-6 en todos los escenarios.
 *
 * Si cambias un escenario aquí, cambia su gemelo en Tests/WasmParityTest.cpp.
 */

import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const [, , jsPathArg, jsonPathArg] = process.argv;
if (!jsPathArg) {
  console.error('[parity] uso: node neuronik_wasm_parity.mjs <ruta/neuronik_dsp.js> [ruta/parity-native.json]');
  process.exit(2);
}

const MAX_DIFF_ABS = 1e-6;
/** Presupuesto de ulps por escenario (ver cabecera): 0 = bit-exacta exigida. */
const ULP_BUDGET = { A_neuronik_default: 0, B_neurotik_default: 0, C_fx_panico: 16, D_modmatrix: 0, E_modelo_espectral: 0 };

const jsPath = path.resolve(process.cwd(), jsPathArg);
const jsonPath = path.resolve(process.cwd(), jsonPathArg ?? 'build-wasm/parity-native.json');

const wasmBinary = await readFile(jsPath.replace(/\.js$/, '.wasm'));
const reference = JSON.parse(await readFile(jsonPath, 'utf8'));

const { default: createModule } = await import(
  `file://${jsPath.split(path.sep).join('/')}`
);

console.log('[parity] instanciando modulo (presupuesto de ulps por escenario)...');
const Module = await createModule({
  instantiateWasm (info, receiveInstance) {
    WebAssembly.instantiate (wasmBinary, info)
      .then ((out) => receiveInstance (out.instance))
      .catch ((e) => { console.error ('[parity] instantiateWasm fallo:', e); process.exit (1); });
    return {};
  },
});

const SAMPLE_RATE = reference.sampleRate;
const BLOCK = reference.blockSize;

// --- Runtime::Event: 24 bytes [i32 type, i32 channel, i32 note, i32 value14, f32 value, i32 sampleOffset]
const EVENT_SIZE = 24;
const EVENT_TYPE_NOTE_ON = 0;

// Duplicado 1:1 de buildScenarios() en Tests/WasmParityTest.cpp
function buildScenarios() {
  const defaultParams = () => ({
    masterLevel: 0.8, saturationAmt: 0.0, bpm: 120.0,
    delayTime: 0.3, delayFB: 0.4,
    chorusRate: 1.0, chorusDepth: 0.2, chorusMix: 0.0,
    reverbSize: 0.5, reverbDamping: 0.5, reverbWidth: 1.0, reverbMix: 0.0,
    lfo1: { waveform: 0, rateHz: 1.0, syncMode: 0, rhythmicDivision: 0, depth: 1.0 },
    lfo2: { waveform: 0, rateHz: 1.0, syncMode: 0, rhythmicDivision: 0, depth: 1.0 },
    modMatrix: [
      { source: 0, destination: 0, amount: 0.0 }, { source: 0, destination: 0, amount: 0.0 },
      { source: 0, destination: 0, amount: 0.0 }, { source: 0, destination: 0, amount: 0.0 },
    ],
  });

  return [
    {
      name: 'A_neuronik_default', engineType: 0, params: defaultParams(),
      events: [{ block: 0, note: 60, velocity: 100 / 127 }],
      blocks: 32, panicAtBlock: -1,
    },
    {
      name: 'B_neurotik_default', engineType: 1, params: defaultParams(),
      events: [{ block: 0, note: 48, velocity: 1.0 }],
      blocks: 32, panicAtBlock: -1,
    },
    {
      name: 'C_fx_panico', engineType: 0, ulpBudget: ULP_BUDGET.C_fx_panico,
      params: { ...defaultParams(), masterLevel: 0.5, chorusRate: 0.8, chorusMix: 0.35, reverbSize: 0.7, reverbDamping: 0.6, reverbMix: 0.25 },
      events: [{ block: 0, note: 64, velocity: 100 / 127 }],
      blocks: 48, panicAtBlock: 16,
    },
    {
      name: 'D_modmatrix', engineType: 0,
      params: { ...defaultParams(), modMatrix: [{ source: 1, destination: 4, amount: 0.5 }, { source: 0, destination: 0, amount: 0.0 }, { source: 0, destination: 0, amount: 0.0 }, { source: 0, destination: 0, amount: 0.0 }] },
      events: [{ block: 0, note: 60, velocity: 100 / 127 }],
      blocks: 24, panicAtBlock: -1,
    },
    {
      // Modelo espectral en el slot 0 (camino neuronikLoadModel): el timbre
      // que los presets publican por el puente cruza aquí bit a bit.
      name: 'E_modelo_espectral', engineType: 0, loadModel: true,
      params: defaultParams(),
      events: [{ block: 0, note: 69, velocity: 1.0 }],
      blocks: 24, panicAtBlock: -1,
    },
  ];
}

/** Gemelo 1:1 de fillTestModel() en Tests/WasmParityTest.cpp: constantes
 *  exactas (potencias de dos) y multiplicación por 2^-7 — idénticas bits en
 *  float32 y double, sin doble redondeo posible entre MSVC y V8. */
function fillTestModel(view) {
  for (let i = 0; i < 64; ++i) {
    const k = i % 4;
    view[i] = k === 0 ? 1.0 : k === 1 ? 0.5 : k === 2 ? 0.25 : 0.125;
    view[64 + i] = i * 0.0078125;
  }
}

// --- Escritura de GlobalParams por offsets del propio módulo (nunca hardcoded)
const gpSize = Module._neuronikGlobalParamsSize();
const gpPtr = Module._malloc(gpSize);
const layoutSize = Module._neuronikGlobalParamsLayout(0, 0);
const layoutPtr = Module._malloc(layoutSize * 4);
const nLayout = Module._neuronikGlobalParamsLayout(layoutPtr, layoutSize);
const modSize = Module._neuronikModMatrixLayout(0, 0);
const modPtr = Module._malloc(modSize * 4);
const nMod = Module._neuronikModMatrixLayout(modPtr, modSize);

const heap32 = Module.HEAP32;
const heapF32 = Module.HEAPF32;

// El módulo no exporta HEAPF64: el único double (bpm) se escribe como dos
// mitades i32 little-endian via un buffer puente.
const f64Bridge = new Float64Array(1);
const i32Bridge = new Int32Array(f64Bridge.buffer);
function setF64At(byteOffset, v) {
  f64Bridge[0] = v;
  heap32[byteOffset >> 2] = i32Bridge[0];
  heap32[(byteOffset >> 2) + 1] = i32Bridge[1];
}

function writeParams(p) {
  heap32.fill(0, gpPtr >> 2, (gpPtr + gpSize) >> 2);
  const off = (i) => heap32[(layoutPtr >> 2) + i];
  const modOff = (i) => heap32[(modPtr >> 2) + i];

  const setF32 = (i, v) => { heapF32[(gpPtr + off(i)) >> 2] = v; };
  const setI32 = (i, v) => { heap32[(gpPtr + off(i)) >> 2] = v; };

  // Orden documentado en NeuronikWasmBridge.cpp (globalParamsLayout):
  // 0 masterLevel, 1 saturationAmt, 2 bpm, 3 delayTime, 4 delayFB,
  // 5 chorusRate, 6 chorusDepth, 7 chorusMix, 8 reverbSize, 9 reverbDamping,
  // 10 reverbWidth, 11 reverbMix, 12-16 lfo1, 17-21 lfo2, luego modMatrix (12 slots)
  setF32(0, p.masterLevel); setF32(1, p.saturationAmt); setF64At(gpPtr + off(2), p.bpm);
  setF32(3, p.delayTime);   setF32(4, p.delayFB);
  setF32(5, p.chorusRate);  setF32(6, p.chorusDepth); setF32(7, p.chorusMix);
  setF32(8, p.reverbSize);  setF32(9, p.reverbDamping); setF32(10, p.reverbWidth); setF32(11, p.reverbMix);
  for (const [idx, lfo] of [[12, p.lfo1], [17, p.lfo2]]) {
    setI32(idx + 0, lfo.waveform); setF32(idx + 1, lfo.rateHz);
    setI32(idx + 2, lfo.syncMode); setI32(idx + 3, lfo.rhythmicDivision);
    setF32(idx + 4, lfo.depth);
  }
  for (let r = 0; r < 4; ++r) {
    heap32[(gpPtr + modOff(r * 3 + 0)) >> 2] = p.modMatrix[r].source;
    heap32[(gpPtr + modOff(r * 3 + 1)) >> 2] = p.modMatrix[r].destination;
    heapF32[(gpPtr + modOff(r * 3 + 2)) >> 2] = p.modMatrix[r].amount;
  }
  Module._neuronikSetGlobalParams(gpPtr, gpSize);
}

function runScenario(s) {
  Module._neuronikSetEngine(s.engineType);
  Module._neuronikInit(SAMPLE_RATE, BLOCK);

  if (s.loadModel) {
    // Mismo camino que el worklet (neuronik:models -> neuronikLoadModel):
    // 128 floats — amplitudes[0..63] luego frequencyOffsets[0..63].
    const modelPtr = Module._malloc(128 * 4);
    fillTestModel(Module.HEAPF32.subarray(modelPtr >> 2, (modelPtr >> 2) + 128));
    Module._neuronikLoadModel(0, s.engineType, modelPtr, 1);
    Module._free(modelPtr);
  }

  writeParams(s.params);

  const eventPtr = Module._malloc(EVENT_SIZE);
  const blockBytes = BLOCK * 4;
  const leftPtr = Module._malloc(blockBytes);
  const rightPtr = Module._malloc(blockBytes);

  const out = new Float32Array(s.blocks * BLOCK);
  for (let b = 0; b < s.blocks; ++b) {
    if (b === s.panicAtBlock) Module._neuronikAllNotesOff();

    const blockEvents = s.events.filter((e) => e.block === b);
    if (blockEvents.length > 0) {
      // Un evento por llamada es suficiente: los escenarios solo disparan nota en el bloque 0.
      const e = blockEvents[0];
      heap32[eventPtr >> 2] = EVENT_TYPE_NOTE_ON;
      heap32[(eventPtr >> 2) + 1] = 1;
      heap32[(eventPtr >> 2) + 2] = e.note;
      heap32[(eventPtr >> 2) + 3] = 8192;
      heapF32[(eventPtr >> 2) + 4] = e.velocity;
      heap32[(eventPtr >> 2) + 5] = 0;
    }
    Module._neuronikProcess(leftPtr, rightPtr, BLOCK, blockEvents.length > 0 ? eventPtr : 0, blockEvents.length);

    const left = heapF32.subarray(leftPtr >> 2, (leftPtr >> 2) + BLOCK);
    out.set(left, b * BLOCK);
  }

  Module._free(eventPtr);
  Module._free(leftPtr);
  Module._free(rightPtr);
  return out;
}

// --- Distancia en ulps entre dos float32 (bitwise)
function ulpDistance(a, b) {
  const ia = new Int32Array(new Float32Array([a]).buffer)[0];
  const ib = new Int32Array(new Float32Array([b]).buffer)[0];
  // Mapa monótono para floats con signo: si el bit de signo está puesto, invertir.
  const ta = ia < 0 ? 0x80000000 - ia : ia;
  const tb = ib < 0 ? 0x80000000 - ib : ib;
  return Math.abs(ta - tb);
}

let totalFailed = 0;
for (const scenario of buildScenarios()) {
  const ref = reference.scenarios.find((r) => r.name === scenario.name);
  if (!ref) { console.error(`[parity] FALLO: la referencia no contiene '${scenario.name}'`); ++totalFailed; continue; }

  const got = runScenario(scenario);
  let maxUlp = 0, maxDiff = 0, worstIdx = -1, mismatches = 0;

  if (got.length !== ref.samples.length) {
    console.error(`[parity] ${scenario.name}: longitud ${got.length} != referencia ${ref.samples.length}`);
    ++totalFailed;
    continue;
  }

  const budget = scenario.ulpBudget ?? 0;
  for (let i = 0; i < got.length; ++i) {
    const d = ulpDistance(got[i], ref.samples[i]);
    const diff = Math.abs(got[i] - ref.samples[i]);
    if (d > maxUlp) { maxUlp = d; worstIdx = i; }
    if (diff > maxDiff) maxDiff = diff;
    if (d > budget || diff > MAX_DIFF_ABS) ++mismatches;
  }

  const ok = mismatches === 0;
  if (!ok) ++totalFailed;
  console.log(
    `[parity] ${scenario.name.padEnd(20)} ${ok ? 'OK ' : 'FALLO'}  maxUlp=${maxUlp}${budget > 0 ? ` (budget ${budget})` : ' (bit-exacta)'}  maxDiff=${maxDiff.toExponential(3)}  mismatched=${mismatches}/${got.length}`
    + (ok ? '' : `  (peor muestra #${worstIdx}: wasm=${got[worstIdx]} nativo=${ref.samples[worstIdx]})`)
  );
}

Module._free(gpPtr); Module._free(layoutPtr); Module._free(modPtr);

if (totalFailed > 0) {
  console.error('[parity] FALLO: escenario(s) fuera de su presupuesto de ulps.');
  process.exit(1);
}
console.log('[parity] OK: paridad WASM<->nativo — bit-exacta en el núcleo (A/B/D/E), cola C dentro del presupuesto de libm.');
