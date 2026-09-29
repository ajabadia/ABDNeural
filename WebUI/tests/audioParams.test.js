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
  CONTRACT_TO_VOICE_FIELD,
  engineIndexFromNormalized,
  gpFieldsFromState,
  defaultGpFields,
  defaultVoiceFields,
  voiceFieldsFromState,
} from '../src/wasm/audioParams.js';
import { getDescriptor } from '../src/contracts/parameters.js';
import { fromNormalized } from '../src/contracts/parameters.js';

/** Mirrors of the C++ constants — keep in sync with DspTypes.h/bridge. */
const CXX_DEFAULTS = {
  masterLevel: 0.8,
  // `fxSaturation` ya no es un parametro del contrato (2026-09-29: el hueco 1
  // del rack de efectos lo absorbio), asi que no se mira. Lo que se mira ahora
  // es el drive del hueco, que es NORMALIZADO 0..1 igual que el hueco lo habla
  // (el `FxSlot` normaliza con la fila, y el APVTS publica 0..1).
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
  it('cubre exactamente los 38 campos contract-reachables (sin bpm)', () => {
    // 33 del bus global y la matriz, + 5 del hueco 1 (sus cuatro mandos, la
    // ganancia y la mezcla). El sexto parametro del hueco, `fx1Type`, NO se
    // cuenta: elige el efecto y lo decide el hilo de mensajes, no viaja por el
    // espejo del hilo de audio.
    expect(Object.keys (CONTRACT_TO_GP_FIELD).length).toBe (38);
    expect(Object.values (CONTRACT_TO_GP_FIELD)).not.toContain (2); // bpm f64 slot
  });

  it('el hueco 1 empieza en el field 34 y va mandos, ganancia y mezcla', () => {
    // El orden lo publica `neuronikGlobalParamsLayout`: tras los 34 campos
    // escalares (22 + 12 de la matriz) vienen `params[0..3]`, `gain` y `mix`
    // del hueco 0, y despues el hueco 1 entero. Estos numeros son el ABI: si
    // el puente cambia el orden, el motor web escribe el mando equivocado sin
    // quejarse, y esta es la asercion que lo dice.
    expect(CONTRACT_TO_GP_FIELD.fx1Param1).toBe (34);
    expect(CONTRACT_TO_GP_FIELD.fx1Param4).toBe (37);
    expect(CONTRACT_TO_GP_FIELD.fx1Gain).toBe (38);
    expect(CONTRACT_TO_GP_FIELD.fx1Mix).toBe (39);
    // El TIPO no viaja: no esta en el mapa, y esa ausencia es deliberada.
    expect(CONTRACT_TO_GP_FIELD.fx1Type).toBeUndefined();
  });

  it('ningun id mapeado pisa el field 1 (saturationAmt, ya sin dueño)', () => {
    // El field 1 sigue siendo `saturationAmt` en el layout por ABI, pero ya no
    // lo maneja nadie. Si un id nuevo acabara en el 1, la pagina estaria
    // escribiendo un campo muerto y el mando no se moveria.
    expect(Object.values (CONTRACT_TO_GP_FIELD)).not.toContain (1);
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

  it('defaultGpFields entrega los 38 campos con los defaults del contrato', () => {
    const fields = defaultGpFields();
    expect (fields.length).toBe (38);

    const byField = new Map (fields);
    expect (byField.get (0)).toBeCloseTo (CXX_DEFAULTS.masterLevel, 4);
    expect (byField.get (3)).toBeCloseTo (CXX_DEFAULTS.fxDelayTime, 3);

    // Y AHORA EL HUECO 1, que es el bus nuevo (2026-09-29). Se compara cada
    // campo con el default del contrato YA EN UNIDADES REALES, que es lo que
    // viaja por el espejo, y no con un numero escrito aqui.
    //
    // LA COMPARACION TIENE QUE SER EN REAL UNITS Y NO EN NORMALIZADO, y la
    // primera version de esta asercion se escribio en normalizado y fallo por
    // la ganancia: el espejo lleva 1.0 (la ganancia fisica, que es como la
    // habla `FxSlot::setGain`) y el normalizado de un 0..2 con default 1.0 es
    // 0.5. Para los MANDOS del hueco no se nota, porque su rango es 0..1 y
    // fisico y normalizado coinciden; para la ganancia si, y por eso el bus
    // mezcla las dos cosas y el test tiene que distinguirlas.
    for (const contractId of ['fx1Param1', 'fx1Param2', 'fx1Param3', 'fx1Param4', 'fx1Gain', 'fx1Mix'])
    {
      const descriptor = getDescriptor (contractId);

      expect (byField.get (CONTRACT_TO_GP_FIELD[contractId]), contractId)
        .toBeCloseTo (fromNormalized (descriptor, descriptor.defaultNormalized), 5);
    }

    // Y los dos que se parecen pero no son lo mismo, por si alguien los
    // cambiara de sitio: la ganancia es fisica (1.0) y la mezcla normalizada
    // (0.0 de bypass). Un bus que mezclara las dos unidades haria que subir
    // la ganancia se oyera en el sitio equivocado.
    expect (byField.get (CONTRACT_TO_GP_FIELD.fx1Gain)).toBeCloseTo (1.0, 5);
    expect (byField.get (CONTRACT_TO_GP_FIELD.fx1Mix)).toBeCloseTo (0.0, 5);
  });
});

describe('CONTRACT_TO_VOICE_FIELD (ADSR, canal neuronik:voice)', () => {
  it('cubre los ocho tramos de las DOS envolventes, en el orden del POD', () => {
    expect (CONTRACT_TO_VOICE_FIELD).toEqual ({
      envAttack: 0, envDecay: 1, envSustain: 2, envRelease: 3,
      filterAttack: 4, filterDecay: 5, filterSustain: 6, filterRelease: 7,
    });
  });

  it('los ocho ids existen en el contrato generado', () => {
    for (const contractId of Object.keys (CONTRACT_TO_VOICE_FIELD))
      expect (getDescriptor (contractId), contractId).not.toBeNull();
  });

  it('los seis tiempos viajan en MILISEGUNDOS y los dos sustains en 0..1', () => {
    // El contrato da segundos (envAttack: 0.001..5) y el POD del puente pide
    // milisegundos (las unidades de AdditiveVoice::Params). Sin el factor 1000
    // el attack llega al motor como 10 microsegundos: inaudible.
    const fields = new Map (voiceFieldsFromState ({ envAttack: 1, envSustain: 1 }));
    expect (fields.get (0)).toBeCloseTo (5.0 * 1000.0, 1);   // 5 s -> 5000 ms
    expect (fields.get (2)).toBeCloseTo (1.0, 4);            // sustain, sin factor

    const filter = new Map (voiceFieldsFromState ({ filterRelease: 1 }));
    expect (filter.get (7)).toBeCloseTo (5.0 * 1000.0, 1);
  });

  it('el snapshot es COMPLETO aunque el estado traiga solo un id', () => {
    // El worklet re-aplica el espejo entero tras un cambio de motor: un espejo
    // a medias congelaria las otras siete envolventes en cero (silencio).
    const fields = voiceFieldsFromState ({ envAttack: 0.5 });

    expect (fields.length).toBe (8);
    for (const [fieldIndex, value] of fields)
      expect (Number.isFinite (value), `campo ${fieldIndex}`).toBe (true);
  });

  it('los ids ausentes caen en el default del contrato, no en 0', () => {
    const byField = new Map (defaultVoiceFields());

    // Defaults del contrato = defaults de AdditiveVoice::Params (10/100/0.7/500).
    expect (byField.get (0)).toBeCloseTo (10.0, 3);
    expect (byField.get (1)).toBeCloseTo (100.0, 3);
    expect (byField.get (2)).toBeCloseTo (0.7, 4);
    expect (byField.get (3)).toBeCloseTo (500.0, 3);
    // La ENV 2 arranca igual que la 1: son los defaults del MISMO struct.
    for (const field of [4, 5, 6, 7])
      expect (byField.get (field)).toBeCloseTo (byField.get (field - 4), 6);
  });
});
