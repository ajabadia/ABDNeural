/**
 * Carga LOCAL de modelos .neuronikmodel (modo navegador sin host).
 *
 * El host nativo carga ficheros por dialogo propio (bridge `loadModel` →
 * modelsState); la página no tiene sistema de ficheros pero SÍ tiene
 * `<input type="file">`. Esta via es para el motor WASM del worklet, no para
 * el plugin: el flujo es leer → validar → parsear → meterlo en el store como
 * `models` (el MISMO estado que pinta la ficha RANURAS) → `neuronik:models`
 * hacia el worklet.
 *
 * El parser acepta el dialecto del escritor único (Common::modelToJson):
 *   { name, format: 2 | 2.1, amplitudes[64], frequencyOffsets[64],
 *     frameSpanHz, frames[...], layers { layerCount, layers[...] } }
 * FASE 11.3 en la WebUI: la capa 1 (frames + frameF0 + pesos) viaja COMPLETA
 * en `parsed.layers` y el worklet la cruza a `neuronikLoadModelLayers` — el
 * motor del navegador TAMBIEN suma las capas (antes solo sonaba la raiz).
 *
 * Los 64 amplitudes del JSON son la RAIZ (capa 0): el bloque v1 del puente.
 * Las capas 2+ (kMaxLayers = 3) se truncan con el mismo trato del lector de
 * presets; hoy el clustering produce como mucho 3.
 */

export const MODEL_SLOT_COUNT = 4;

/**
 * Extrae la capa 1 del bloque v2.1 al shape del worklet
 * ({ layerCount, weight, frames[{amplitudes, frequencyOffsets, frameF0}],
 *    frameWeights }), o null si el modelo no la lleva (v2 puro).
 */
function extractExtraLayer(json) {
  if (json.format !== 2.1 || !json.layers || Number(json.layers.layerCount) < 2)
    return null;

  const layer = json.layers.layers?.[1];
  const frames = layer?.frames;

  if (!Array.isArray(frames) || frames.length === 0) return null;

  const cleanFrames = frames.slice(0, 16).map((frame) => ({
    amplitudes: Array.isArray(frame?.amplitudes) && frame.amplitudes.length === 64
      ? frame.amplitudes.map((v) => (Number.isFinite(v) ? v : 0))
      : null,
    frequencyOffsets: Array.isArray(frame?.frequencyOffsets) && frame.frequencyOffsets.length === 64
      ? frame.frequencyOffsets.map((v) => (Number.isFinite(v) ? v : 0))
      : new Array(64).fill(0),
    frameF0: Number.isFinite(frame?.frameF0) ? frame.frameF0 : 0,
  }));

  if (cleanFrames.some((frame) => frame.amplitudes === null)) return null;

  const weights = Array.isArray(layer.frameWeights)
    ? layer.frameWeights.slice(0, cleanFrames.length).map((v) => (Number.isFinite(v) ? v : 1))
    : new Array(cleanFrames.length).fill(1);

  return {
    layerCount: 2,
    weight: Number.isFinite(layer.weight) ? layer.weight : 1,
    frames: cleanFrames,
    frameWeights: weights,
  };
}

/**
 * Parsea el texto de un .neuronikmodel al shape de `modelsState`
 * ({ slot, name, isValid, amplitudes[64], frequencyOffsets[64] }).
 * Lanza con un mensaje de usuario (no de programador) si el fichero no sirve.
 */
export function parseModelText(text) {
  let json;
  try {
    json = JSON.parse(text);
  } catch {
    throw new Error('el fichero no es JSON');
  }

  if (!json || typeof json !== 'object' || Array.isArray(json))
    throw new Error('el fichero no contiene un modelo');

  const amps = json.amplitudes;
  const freqs = json.frequencyOffsets;

  if (!Array.isArray(amps) || amps.length !== 64)
    throw new Error('el modelo no lleva 64 amplitudes');

  const amplitudes = amps.map((value) => (Number.isFinite(value) ? value : 0));
  const frequencyOffsets = Array.isArray(freqs) && freqs.length === 64
    ? freqs.map((value) => (Number.isFinite(value) ? value : 0))
    : new Array(64).fill(0);

  const name = typeof json.name === 'string' && json.name.trim() !== ''
    ? json.name.trim()
    : 'modelo';

  // FASE 11.3: la capa 1 viaja completa (el motor del navegador la suma).
  // Solo se recorta la capa 2 (el clustering puede producir 3; el export
  // de la frontera hoy lleva la raiz + UNA capa extra).
  const extraLayer = extractExtraLayer(json);
  const layered = extraLayer !== null;

  const peak = Math.max(...amplitudes);
  const isValid = peak > 0;

  return {
    slot: -1,               // lo fija el llamador (la ranura destino)
    name,
    isValid,
    amplitudes,
    frequencyOffsets,
    layers: extraLayer,     // capa 1 completa, o null (v2 puro)
  };
}

/**
 * Lee un File del input y devuelve el modelo parseado. Rejects con el mensaje
 * de usuario; el llamador decide dónde pintarlo (el estado de la ficha).
 */
export async function readModelFile(file) {
  if (!file) throw new Error('no hay fichero');

  let text;
  if (typeof file.text === 'function') {
    text = await file.text();
  } else {
    // Fallback (jsdom no implementa Blob.text()): FileReader, el camino
    // que tambien sirve en runtimes viejos.
    text = await new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(String(reader.result));
      reader.onerror = () => reject(new Error('no se pudo leer el fichero'));
      reader.readAsText(file);
    });
  }

  return parseModelText(text);
}

/**
 * Estado `models` inicial en modo local: cuatro ranuras vacías (el mismo shape
 * que manda el host en modelsState, con isValid false). El paint de la ficha
 * RANURAS no distingue de dónde vengo.
 */
export function emptyLocalModels() {
  return Array.from({ length: MODEL_SLOT_COUNT }, (_, slot) => ({
    slot,
    name: 'EMPTY',
    isValid: false,
    amplitudes: new Array(64).fill(0),
    frequencyOffsets: new Array(64).fill(0),
  }));
}
