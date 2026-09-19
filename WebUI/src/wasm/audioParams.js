/**
 * Contract -> WASM GlobalParams field mapping (web audio path).
 *
 * PORTADO de `WebPilot/lib/audioParams.js` SIN cambios de comportamiento; sólo
 * cambia la ruta del import (el adaptador vive ahora en ../contracts/parameters.js).
 *
 * The bridge tab already maps contract IDs to the JUCE native host over the
 * WebView2 channel; this module maps the SAME contract IDs to the
 * GlobalParams struct of the REAL DSP running in the AudioWorklet
 * (field indices follow the documented order in NeuronikWasmBridge.cpp).
 *
 * Everything lives here — not in the worklet — so a contract change is
 * one edit in one place and the worklet stays dumb (it only knows indices).
 *
 * Real-units conversion reuses contracts/paramValue.js' math (via parameters.js):
 * contract values are normalised 0..1 on the wire and the DSP wants REAL units
 * (Hz, seconds...), the same direction the native ParameterPanel applies.
 *
 * NOTA (ticket 8.1): dentro del plugin el audio es NATIVO y la página sólo habla
 * por el bridge, así que este módulo es para la página FUERA del plugin (navegador
 * con el motor WASM en el worklet). El motor del worklet es lo que prueba
 * `neuronik_wasm_parity.mjs` muestra a muestra contra la referencia nativa.
 */

import { fromNormalized, getDescriptor } from '../contracts/parameters.js';

/**
 * contractId -> GlobalParams field index (order from neuronikGlobalParamsLayout).
 * Only the fields the UI can actually reach; bpm has no contract
 * parameter yet (it stays at the C++ default 120.0).
 */
export const CONTRACT_TO_GP_FIELD = {
  masterLevel: 0,
  fxSaturation: 1,
  // 2 = bpm (f64, contract-free for now)
  fxDelayTime: 3,
  fxDelayFeedback: 4,
  fxChorusRate: 5,
  fxChorusDepth: 6,
  fxChorusMix: 7,
  fxReverbSize: 8,
  fxReverbDamping: 9,
  fxReverbWidth: 10,
  fxReverbMix: 11,
  lfo1Waveform: 12,
  lfo1RateHz: 13,
  lfo1SyncMode: 14,
  lfo1RhythmicDivision: 15,
  lfo1Depth: 16,
  lfo2Waveform: 17,
  lfo2RateHz: 18,
  lfo2SyncMode: 19,
  lfo2RhythmicDivision: 20,
  lfo2Depth: 21,
};

/** GlobalParams C++ defaults (DspTypes.h) as REAL units, per field index. */
export const GP_FIELD_DEFAULTS = {
  0: 0.8, // masterLevel
  1: 0.0, // saturationAmt
  3: 0.3, // delayTime (s)
  4: 0.4, // delayFB
  5: 1.0, // chorusRate (Hz)
  6: 0.2, // chorusDepth
  7: 0.0, // chorusMix
  8: 0.5, // reverbSize
  9: 0.5, // reverbDamping
  10: 1.0, // reverbWidth
  11: 0.0, // reverbMix
  12: 0, 13: 1.0, 14: 0, 15: 0, 16: 1.0, // lfo1
  17: 0, 18: 1.0, 19: 0, 20: 0, 21: 1.0, // lfo2
};

/** Discrete fields (int slots): contract normalised -> index via (count-1). */
const DISCRETE_FIELD_IDS = new Set([
  'lfo1Waveform', 'lfo1SyncMode', 'lfo1RhythmicDivision',
  'lfo2Waveform', 'lfo2SyncMode', 'lfo2RhythmicDivision',
]);

/** engineType choice -> engine index (contract + bridge agree: 0 = NEURONiK, 1 = Neurotik). */
export function engineIndexFromNormalized(normalized, control) {
  const count = Math.max (control?.choices?.length ?? 2, 2);
  return clampInt (Math.round (normalized * (count - 1)), 0, count - 1);
}

/**
 * Build the full initial field payload for the worklet: every mapped field,
 * resolved from the contract's defaultNormalized into REAL units.
 */
export function defaultGpFields() {
  const fields = [];

  for (const [contractId, fieldIndex] of Object.entries (CONTRACT_TO_GP_FIELD)) {
    const descriptor = getDescriptor (contractId);
    const normalized = descriptor ? descriptor.defaultNormalized : 0;

    fields.push ([fieldIndex, fieldToReal (contractId, descriptor, normalized)]);
  }

  // bpm is not contract-mapped: C++ default (120) is already in the mirror
  // because we seed from GP_FIELD_DEFAULTS + C++ defaults in the worklet.
  return fields;
}

/** Build the field payload for the CURRENT page state (all mapped ids). */
export function gpFieldsFromState(parameters) {
  const fields = [];

  for (const [contractId, fieldIndex] of Object.entries (CONTRACT_TO_GP_FIELD)) {
    if (!(contractId in parameters)) continue;
    const descriptor = getDescriptor (contractId);

    fields.push ([fieldIndex, fieldToReal (contractId, descriptor, parameters[contractId])]);
  }

  return fields;
}

function clampInt(value, min, max) {
  return Math.min (max, Math.max (min, value));
}

/** One normalised contract value -> the REAL value the DSP field expects. */
function fieldToReal(contractId, descriptor, normalized) {
  if (DISCRETE_FIELD_IDS.has (contractId)) {
    const count = Math.max (descriptor.choices?.length ?? 1, 1);
    return clampInt (Math.round (normalized * (count - 1)), 0, count - 1);
  }

  // Raw descriptor (minValue/maxValue/skew) is exactly what fromNormalized
  // expects — same math the native ParameterPanel applies.
  return fromNormalized (descriptor, normalized);
}
