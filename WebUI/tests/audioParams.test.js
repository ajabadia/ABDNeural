/**
 * Contract guard for the web audio path: the contract IDs the UI can reach
 * must keep mapping to the GlobalParams field indices the WASM bridge
 * documents (NeuronikWasmBridge.cpp::neuronikGlobalParamsLayout). If the C++
 * layout changes order, this test and the C++ bridge disagree loudly instead
 * of the web engine drifting silently.
 *
 * PORTADO de `WebPilot/tests/audioParams.test.js`; sólo cambian las rutas de import.
 */

import { describe, expect, it } from 'vitest';

import {
  CONTRACT_TO_GP_FIELD,
  engineIndexFromNormalized,
  gpFieldsFromState,
  defaultGpFields,
} from '../src/wasm/audioParams.js';
import { getDescriptor } from '../src/contracts/parameters.js';
import { fromNormalized } from '../src/contracts/parameters.js';

/** Mirrors of the C++ constants — keep in sync with DspTypes.h/bridge. */
const CXX_DEFAULTS = {
  masterLevel: 0.8,
  fxSaturation: 0,
  fxDelayTime: 0.3,
  fxDelayFeedback: 0.4,
  fxChorusRate: 1.0,
  fxChorusDepth: 0.2,
  fxChorusMix: 0,
  fxReverbSize: 0.5,
  fxReverbDamping: 0.5,
  fxReverbWidth: 1,
  fxReverbMix: 0,
};

describe('CONTRACT_TO_GP_FIELD', () => {
  it('cubre exactamente los 21 campos contract-reachables (sin bpm)', () => {
    expect(Object.keys (CONTRACT_TO_GP_FIELD).length).toBe (21);
    expect(Object.values (CONTRACT_TO_GP_FIELD)).not.toContain (2); // bpm f64 slot
  });

  it('cada id mapeado existe en el contrato generado', () => {
    for (const contractId of Object.keys (CONTRACT_TO_GP_FIELD)) {
      const descriptor = getDescriptor (contractId);
      expect (descriptor, `contrato no contiene "${contractId}"`).not.toBeNull();
    }
  });

  it('los defaults del contrato resueltos a real units coinciden con DspTypes.h', () => {
    for (const [contractId, cxxDefault] of Object.entries (CXX_DEFAULTS)) {
      const descriptor = getDescriptor (contractId);
      const resolved = fromNormalized (descriptor, descriptor.defaultNormalized);

      expect (resolved, contractId).toBeCloseTo (cxxDefault, 4);
    }
  });
});

describe('engineIndexFromNormalized', () => {
  const control = { choices: ['NEURONiK', 'Neurotik'] };

  it('mapea el choice de engineType a los dos engines del bridge', () => {
    expect (engineIndexFromNormalized (0, control)).toBe (0);
    expect (engineIndexFromNormalized (1, control)).toBe (1);
    expect (engineIndexFromNormalized (0.75, control)).toBe (1);
  });
});

describe('gpFieldsFromState', () => {
  it('resuelve valores real-units por campo (masterLevel 1.0 normalised -> 1.0 real)', () => {
    const fields = gpFieldsFromState ({ masterLevel: 1 });

    const entry = fields.find (([fieldIndex]) => fieldIndex === 0);
    expect (entry[1]).toBeCloseTo (1.0, 4);
  });

  it('resuelve fxDelayTime con el skew logaritmico del contrato', () => {
    const descriptor = getDescriptor ('fxDelayTime');
    // defaultNormalized 0.3817442 -> 0.3 s exactos (default del C++)
    const fields = gpFieldsFromState ({ fxDelayTime: descriptor.defaultNormalized });

    expect (fields.find (([fieldIndex]) => fieldIndex === 3)[1]).toBeCloseTo (0.3, 3);
  });

  it('los discretos LFO resuelven a enteros de indice', () => {
    const fields = gpFieldsFromState ({ lfo1Waveform: 0, lfo1SyncMode: 1 });
    const waveformChoices = getDescriptor ('lfo1Waveform').choices.length;

    expect (fields.find (([fieldIndex]) => fieldIndex === 12)[1]).toBe (0);
    expect (fields.find (([fieldIndex]) => fieldIndex === 14)[1]).toBe (1);
    // centro del rango -> indice medio (7 formas de onda: 0.5 -> 3)
    const mid = gpFieldsFromState ({ lfo1Waveform: 0.5 });
    expect (mid.find (([fieldIndex]) => fieldIndex === 12)[1])
      .toBe (Math.round (0.5 * (waveformChoices - 1)));
  });

  it('ignora ids desconocidos sin fallar', () => {
    const fields = gpFieldsFromState ({ idQueNoExiste: 0.5, masterLevel: 0.5 });

    expect (fields.some (([fieldIndex]) => fieldIndex === 0)).toBe (true);
    expect (fields.length).toBe (1);
  });

  it('defaultGpFields entrega los 21 campos con los defaults del contrato', () => {
    const fields = defaultGpFields();
    expect (fields.length).toBe (21);

    const byField = new Map (fields);
    expect (byField.get (0)).toBeCloseTo (CXX_DEFAULTS.masterLevel, 4);
    expect (byField.get (3)).toBeCloseTo (CXX_DEFAULTS.fxDelayTime, 3);
  });
});
