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
 * Lo mismo para el segundo canal, el del ADSR de la voz (`neuronik:voice`,
 * `CONTRACT_TO_VOICE_FIELD`): son VoiceParams y viajan aparte de GlobalParams.
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
  // Field 1 is `saturationAmt`, which the plugin NO LONGER HANDLES ANYWHERE: the
  // first FX slot got its own bus on 2026-09-29 and the drive moved into it
  // (see `fx1Param1` below). The field stays at index 1 on purpose — the
  // layout is a documented order and removing it would shift every index below
  // it, so the page would start writing the wrong field with no warning. It is
  // simply never written from here.
  //
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
  // Matriz de modulacion (4 rutas x 3 campos): fields 22..33 en el layout
  // documentado del bridge — el orden EXACTO de GlobalParams.modMatrix[r].
  mod1Source: 22,
  mod1Destination: 23,
  mod1Amount: 24,
  mod2Source: 25,
  mod2Destination: 26,
  mod2Amount: 27,
  mod3Source: 28,
  mod3Destination: 29,
  mod3Amount: 30,
  mod4Source: 31,
  mod4Destination: 32,
  mod4Amount: 33,

  // El BUS DEL HUECO 1 (2026-09-29), que empieza en el field 34: los DOCE
  // mandos, luego la ganancia (46) y la mezcla (47); y a partir del 48 el hueco
  // 2, que esta pagina todavia NO mapea. Los numeros salen del orden que publica
  // `neuronikGlobalParamsLayout`, y ese orden es `params[0..bus-1], gain, mix`
  // por hueco — el mismo que en el puente.
  //
  // EL ANCHO DEL BUS SON DOCE desde 2026-09-29, no cuatro: los cuatro huecos
  // publican el bus entero porque el mando que el host automatiza tiene que ser
  // el mismo para cualquier efecto, y porque es la forma que tiene ABDEep. Los
  // ids planos de mas arriba (coro, retardo, reverb) se quedan donde estan, con
  // el field que les toca, por dos razones: son indices publicados y borrarlos
  // correria lo de detras; y `PresetMigrationFx.cpp` los lee al abrir un preset
  // viejo. PERO EL MOTOR YA NO LOS MIRA (los cuatro huecos se rellenan por su
  // bus), asi que en esta ruta --la pagina FUERA del plugin-- esos once knobs
  // mueven el espejo y no suenan. El contrato los marca `notRouted` y la pagina
  // los pinta como divergentes, que es la verdad. llevarlos al bus (con el
  // viaje de unidades que hace falta) es el siguiente paso.
  //
  // Y ESE ORDEN ESTA FIJADO, no es una convencion: `NEURONiK_WasmLayoutOrderTest`
  // (Tests/WasmLayoutOrderTest.cpp) confronta estos numeros con los `offsetof`
  // que construye el puente. Hasta el 2026-09-29 los bucles del puente ponian
  // todos los params de todos los huecos y despues todas las ganancias y
  // mezclas, con lo que el 38 y el 39 de aqui eran los dos primeros mandos del
  // hueco 2: la ganancia y la mezcla del hueco 1 no llegaban a nada y ningun
  // knob se quejaba. Los params si cuadraban, porque los del hueco 1 van los
  // primeros en las dos formas, que es por eso que el fallo estaba donde no se
  // miraba. Si ese test se pone rojo, el que hay que arreglar es el puente.
  //
  // `fx1Type` NO APARECE, y no es un olvido: el tipo de un hueco lo decide el
  // hilo de mensajes (crea y destruye la instancia del efecto) y no viaja por
  // el espejo del hilo de audio. Es el unico parametro del bus que no se mapea.
  fx1Param1: 34,
  fx1Param2: 35,
  fx1Param3: 36,
  fx1Param4: 37,
  fx1Param5: 38,
  fx1Param6: 39,
  fx1Param7: 40,
  fx1Param8: 41,
  fx1Param9: 42,
  fx1Param10: 43,
  fx1Param11: 44,
  fx1Param12: 45,
  fx1Gain: 46,
  fx1Mix: 47,
};

/** El campo mas alto que escribe esta pagina. */
export const highestGpField = () => Math.max (...Object.values (CONTRACT_TO_GP_FIELD));

/**
 * Los ids que la pagina escribe y el motor de este `.wasm` NO publica.
 *
 * `fieldCount` es lo que el worklet contesto al arrancar
 * (`neuronik:ready.paramsFieldCount`), que es el numero de campos que el
 * layout del motor publica de verdad.
 *
 * ESTA CUENTA ES LA QUE FALTABA, porque el fallo que aparece aqui es
 * SILENCIOSO por construccion: el worklet recibe el par [indice, valor],
 * busca su offset, no lo encuentra y se lo come. El knob se mueve, la
 * pagina no se queja y el motor no oye nada. Con el .wasm que hay hoy en
 * `public/worklet` (el de 2026-09-28) son los seis del bus: el puente ya
 * publica el tramo entero, pero el binario es anterior a ese cambio.
 *
 * Vive aqui, y no en `audioWorkletEngine.js`, porque el que sabe que
 * indices escribe esta pagina es este mapa.
 */
export function gpIdsBeyondFieldCount (fieldCount) {
  if (!Number.isFinite (fieldCount)) return [];

  return Object.entries (CONTRACT_TO_GP_FIELD)
    .filter (([, fieldIndex]) => fieldIndex >= fieldCount)
    .map (([contractId]) => contractId)
    .sort ();
}

/**
 * contractId -> ADSR field index (order from neuronikVoiceEnvelopeLayout).
 *
 * Los ocho tramos de las DOS envolventes. Van por un canal aparte
 * (`neuronik:voice`, no `neuronik:params`) porque son VoiceParams —los publica
 * la voz, no el motor global—, igual que el morph. Antes de este canal la
 * pagina movia los ocho knobs y el motor local seguia con los defaults de C++.
 *
 * UNIDADES: el layout del puente esta en MILISEGUNDOS (las unidades de
 * `AdditiveVoice::Params`) y el contrato las da en SEGUNDOS, que es lo que ve
 * el APVTS del plugin; el factor 1000 lo aplica `voiceFieldToReal` aqui, igual
 * que el `* 1000.0f` de `synchronizeEngineParameters`. Los dos sustains van
 * 0..1 sin factor.
 */
export const CONTRACT_TO_VOICE_FIELD = {
  envAttack: 0,
  envDecay: 1,
  envSustain: 2,
  envRelease: 3,
  filterAttack: 4,
  filterDecay: 5,
  filterSustain: 6,
  filterRelease: 7,
};

/** Los ids de ADSR cuyo layout es un SEGUNDO y viaja como milisegundos. */
const VOICE_TIME_FIELDS = new Set([
  'envAttack', 'envDecay', 'envRelease',
  'filterAttack', 'filterDecay', 'filterRelease',
]);

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
  // La matriz son ranuras enteras de GlobalParams: choice -> indice directo.
  'mod1Source', 'mod1Destination',
  'mod2Source', 'mod2Destination',
  'mod3Source', 'mod3Destination',
  'mod4Source', 'mod4Destination',
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

/** Un tramo de ADSR -> los milisegundos (o el nivel) del layout del puente. */
function voiceFieldToReal(contractId, descriptor, normalized) {
  const real = fromNormalized (descriptor, normalized);

  return VOICE_TIME_FIELDS.has (contractId) ? real * 1000.0 : real;
}

/**
 * The ADSR payload for the CURRENT page state, in the bridge's units.
 * Unlike gpFieldsFromState this does NOT skip ids missing from the state: the
 * mirror is pushed as a whole snapshot and the worklet re-applies it verbatim
 * after an engine switch, so a half-filled mirror would freeze the envelopes at
 * zero. Ids absent from the contract fall back to their own default.
 */
export function voiceFieldsFromState(parameters) {
  const fields = [];

  for (const [contractId, fieldIndex] of Object.entries (CONTRACT_TO_VOICE_FIELD)) {
    const descriptor = getDescriptor (contractId);
    if (!descriptor) continue;

    const normalized = contractId in parameters
      ? parameters[contractId]
      : descriptor.defaultNormalized;

    fields.push ([fieldIndex, voiceFieldToReal (contractId, descriptor, normalized)]);
  }

  return fields;
}

/** The full ADSR snapshot from the contract defaults (arranque del motor). */
export function defaultVoiceFields() {
  return voiceFieldsFromState ({});
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
