/**
 * Carga LOCAL de modelos (modo navegador sin host): src/audio/localModels.js.
 *
 * Lo que fija el test:
 *   - el parser acepta el dialecto del escritor unico (modelToJson) y produce
 *     el MISMO shape que manda el host en modelsState (slot, name, isValid,
 *     amplitudes[64], frequencyOffsets[64]);
 *   - los errores son mensajes de usuario, no stacks;
 *   - un modelo v2.1 lleva la capa 1 COMPLETA en `parsed.layers` (el motor del
 *     puente WASM) y un modelo vacio sale isValid false;
 *   - emptyLocalModels() da cuatro ranuras con el shape del bridge.
 */

import { describe, expect, it } from 'vitest';

import { emptyLocalModels, parseModelText, readModelFile } from '../src/audio/localModels.js';

function modelJson(overrides = {}) {
  return JSON.stringify({
    name: 'CZ-BASS1',
    format: 2,
    amplitudes: Array.from({ length: 64 }, (_, i) => (i === 0 ? 1 : 0.5 / (i + 1))),
    frequencyOffsets: new Array(64).fill(0),
    frameSpanHz: 124.3,
    frames: [],
    ...overrides,
  });
}

describe('localModels / parser del dialecto v2', () => {
  it('parsea un modelo v2 al shape de modelsState', () => {
    const model = parseModelText(modelJson());

    expect(model.name).toBe('CZ-BASS1');
    expect(model.isValid).toBe(true);
    expect(model.slot).toBe(-1);                       // la ranura la pone el llamador
    expect(model.amplitudes).toHaveLength(64);
    expect(model.amplitudes[0]).toBe(1);
    expect(model.frequencyOffsets).toHaveLength(64);
    expect(model.layers).toBe(null);   // v2 puro: sin capa extra
  });

  it('un modelo de capas (v2.1) lleva la capa 1 COMPLETA (fase 11.3 local)', () => {
    const layer = {
      name: 'voz', weight: 0.8,
      frames: [
        { amplitudes: Array.from({ length: 64 }, (_, i) => (i === 6 ? 0.9 : 0)),
          frequencyOffsets: new Array(64).fill(0), frameF0: 124.5 },
        { amplitudes: Array.from({ length: 64 }, (_, i) => (i === 7 ? 0.7 : 0)),
          frequencyOffsets: new Array(64).fill(0), frameF0: 130.2 },
      ],
      frameWeights: [1, 0.5],
    };
    const model = parseModelText(modelJson({
      format: 2.1,
      layers: { layerCount: 2, layers: [{ name: 'drone', weight: 1 }, layer] },
    }));

    expect(model.layers).not.toBe(null);
    expect(model.layers.layerCount).toBe(2);
    expect(model.layers.weight).toBe(0.8);
    expect(model.layers.frames).toHaveLength(2);
    expect(model.layers.frames[0].amplitudes[6]).toBe(0.9);
    expect(model.layers.frames[0].frameF0).toBe(124.5);
    expect(model.layers.frames[1].frameF0).toBe(130.2);
    expect(model.layers.frameWeights).toEqual([1, 0.5]);
    expect(model.isValid).toBe(true);
  });

  it('una capa declarada sin frames (o rota) no se anuncia: el v1 manda', () => {
    const sinFrames = parseModelText(modelJson({
      format: 2.1,
      layers: { layerCount: 2, layers: [{ name: 'a' }, { name: 'b' }] },
    }));
    expect(sinFrames.layers).toBe(null);

    const rota = parseModelText(modelJson({
      format: 2.1,
      layers: { layerCount: 2, layers: [{ name: 'a' },
        { name: 'b', frames: [{ amplitudes: new Array(10).fill(0) }] }] },
    }));
    expect(rota.layers).toBe(null);
  });

  it('un modelo vacio (todo ceros) sale isValid false', () => {
    const model = parseModelText(modelJson({ amplitudes: new Array(64).fill(0) }));

    expect(model.isValid).toBe(false);
  });

  it('frequencyOffsets ausente sale a cero (no revienta)', () => {
    const json = JSON.parse(modelJson());
    delete json.frequencyOffsets;

    const model = parseModelText(JSON.stringify(json));

    expect(model.frequencyOffsets).toEqual(new Array(64).fill(0));
    expect(model.isValid).toBe(true);
  });

  it('los errores son de usuario: no-JSON, no-modelo, sin 64 amplitudes', () => {
    expect(() => parseModelText('esto no es json')).toThrow(/no es JSON/);
    expect(() => parseModelText('[]')).toThrow(/no contiene un modelo/);
    expect(() => parseModelText(JSON.stringify({ name: 'x' }))).toThrow(/64 amplitudes/);
  });

  it('readModelFile lee el File del input (el dialogo local)', async () => {
    const file = new File([modelJson()], 'CZ-BASS1.neuronikmodel');
    const model = await readModelFile(file);

    expect(model.name).toBe('CZ-BASS1');
    expect(model.isValid).toBe(true);
  });

  it('readModelFile sin fichero rechaza con mensaje de usuario', async () => {
    await expect(readModelFile(null)).rejects.toThrow(/no hay fichero/);
  });
});

describe('localModels / estado inicial local', () => {
  it('cuatro ranuras vacias con el shape del bridge', () => {
    const models = emptyLocalModels();

    expect(models).toHaveLength(4);
    expect(models.map((m) => m.slot)).toEqual([0, 1, 2, 3]);
    for (const model of models) {
      expect(model.name).toBe('EMPTY');
      expect(model.isValid).toBe(false);
      expect(model.amplitudes).toHaveLength(64);
      expect(model.frequencyOffsets).toHaveLength(64);
    }
  });
});
