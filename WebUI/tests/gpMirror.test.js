/**
 * EL ESPEJO DE GlobalParams, su parte pura (`public/worklet/gpMirror.js`).
 *
 * Lo que se prueba aqui es una TRUCCION, no una clase: donde cae cada campo y
 * con que vista se escribe. Es la operacion que el worklet hace por cada
 * `neuronik:params` y la que hacia a mano con dos listas escritas (`INT_FIELDS`,
 * `BPM_FIELD`) que eran una COPIA del struct de C++.
 *
 * Por que hace falta un modulo de mentira: el worklet de verdad es un
 * `AudioWorkletProcessor` que se registra al importarse y no exporta nada, asi
 * que esta traduccion era intestable. Ahora es una funcion, y se le puede dar
 * un `Module` falso con un heap de verdad encima.
 *
 * El caso que mas importa es el 3: un campo `int` escrito con `Float32Array`
 * deja el patron de bits de IEEE (28 -> 0x41C00000) y el motor lee basura. Con
 * el numero del puente, ese error no se puede cometer.
 */

import { describe, expect, it } from 'vitest';

import {
  FIELD_CLASS,
  readGpLayout,
  writeGpField,
} from '../public/worklet/gpMirror.js';

/** Un Module de mentira: heap real, exports con la misma forma que el glue. */
function fakeModule({ offsets, kinds, kindCount = null, fingerprint = 0xc548f50d, noFingerprint = false }) {
  const HEAP32 = new Int32Array(64);
  const ptr = 16; // alineado a 4, con sitio de sobra para unas pocas entradas

  const fill = (array, values, outPtr, max) => {
    if (outPtr === 0) return values.length;   // el contrato: asking for the count
    const n = Math.min(values.length, max);
    for (let i = 0; i < n; i += 1) HEAP32[(outPtr >> 2) + i] = values[i];
    return n;
  };

  const mod = {
    HEAP32,
    _malloc: () => ptr,
    _free: () => {},
    _neuronikGlobalParamsLayout: (outPtr, max) => fill(offsets, offsets, outPtr, max),
    _neuronikGlobalParamsFieldKinds: (outPtr, max) =>
      fill(kinds, kindCount === null ? kinds : kindCount, outPtr, max),
  };

  // La firma la publica el puente. Por defecto es la de este `wasm` de
  // mentira; `noFingerprint` la quita, que es el binario viejo. Se hace
  // con una bandera y no con `fingerprint: undefined` porque un valor por
  // defecto no distingue lo que no se ha pasado de lo que se pasa a undefined,
  // y aqui lo que se quiere es justo poder quitar el export.
  if (!noFingerprint)
    mod._neuronikGlobalParamsLayoutFingerprint = () => fingerprint;

  return mod;
}

/** El espejo: tres vistas sobre el MISMO ArrayBuffer, como en el worklet. */
function makeViews(byteLength = 32) {
  const mirror = new ArrayBuffer(byteLength);
  return { mirror, f32: new Float32Array(mirror), i32: new Int32Array(mirror), f64: new Float64Array(mirror) };
}

describe('gpMirror / readGpLayout', () => {
  it('lee el offset y la clase de cada campo, en el MISMO orden', () => {
    const Module = fakeModule({
      offsets: [0, 4, 8, 12],
      kinds: [FIELD_CLASS.FLOAT32, FIELD_CLASS.INT32, FIELD_CLASS.FLOAT64, FIELD_CLASS.FLOAT32],
    });

    const layout = readGpLayout(Module);

    expect(layout.fieldCount).toBe(4);
    expect(layout.byteOffsets).toEqual([0, 4, 8, 12]);
    expect(layout.fieldKinds).toEqual([0, 1, 2, 0]);
  });

  it('LANZA si el motor publica tantos offsets como clases NO', () => {
    // El .wasm viejo: sin `neuronikGlobalParamsFieldKinds` la cuenta sale vacia
    // (0), y seguir con una tabla de clases inventada seria volver a la copia
    // que se acaba de quitar. Aqui se dice y se para.
    const Module = fakeModule({ offsets: [0, 4, 8], kinds: [0, 0, 0], kindCount: 0 });

    expect(() => readGpLayout(Module)).toThrow(/anterior a la tabla unica/);
  });

  it('LANZA, y con el remedio, si el export de clases NO EXISTE', () => {
    // El caso ANTERIOR es un .wasm que tiene el export pero con el recuento
    // descuadrado. Este es el otro, y el mas probable: clonar el repo, no
    // recompilar, y que el binario servido sea el de antes de la tabla unica.
    // Sin guardia, `Module._neuronikGlobalParamsFieldKinds(0, 0)` revienta con
    // un "is not a function" de JavaScript que no dice que hay que recompilar
    // nada: el worklet lo captura y lo manda por el port, pero la pagina ve
    // un error crudo en vez de su linea de "sin motor".
    const Module = fakeModule({ offsets: [0, 4, 8], kinds: [0, 0, 0] });
    delete Module._neuronikGlobalParamsFieldKinds;

    expect(() => readGpLayout(Module))
      .toThrow(/neuronikGlobalParamsFieldKinds/);
    expect(() => readGpLayout(Module))
      .toThrow(/Recompila el \.wasm/);
  });

  it('REPORTA la firma del binario, y no la espera a nadie', () => {
    // El worklet no sabe de que motor se trata: solo puede decir de que
    // motor ES. Comparar es cosa de la pagina, que es quien tiene la firma
    // esperada en su fichero generado. Por eso esto devuelve el numero y
    // no un si o no.
    const Module = fakeModule({ offsets: [0, 4], kinds: [0, 1] });

    expect(readGpLayout(Module).fingerprint).toBe(0xc548f50d);
  });

  it('la firma es null si el export NO existe, no un 0 cualquiera', () => {
    // Un 0 seria un numero valido que la pagina compararia y declararia
    // 'binario ajeno' sin saber que no sabe. El null se
    // distingue de cualquier valor, que es lo que permite que el aviso diga
    // 'no publica la firma' y no 'la firma no coincide'.
    const Module = fakeModule({
      offsets: [0, 4],
      kinds: [0, 1],
      noFingerprint: true,
    });

    expect(readGpLayout(Module).fingerprint).toBeNull();
  });

  it('una firma DISTINTA se devuelve tal cual, sin juzgarla aqui', () => {
    // Si el modulo la marcara como mala, la comparacion estaria en los dos
    // lados y volveriamos a tener dos verdades. Solo hay una: la pagina.
    const Module = fakeModule({ offsets: [0, 4], kinds: [0, 1], fingerprint: 999 });

    expect(readGpLayout(Module).fingerprint).toBe(999);
  });
});

describe('gpMirror / writeGpField', () => {
  const layout = {
    fieldCount: 4,
    byteOffsets: [0, 4, 8, 12],
    fieldKinds: [FIELD_CLASS.FLOAT32, FIELD_CLASS.INT32, FIELD_CLASS.FLOAT64, FIELD_CLASS.FLOAT32],
  };

  it('un float va a Float32Array', () => {
    const views = makeViews();

    expect(writeGpField(layout, 0, 0.25, views)).toBe(true);
    expect(views.f32[0]).toBe(0.25);
  });

  it('un INT va a Int32Array, no a su patron de bits de float', () => {
    // EL CASO. Con `Float32Array`, 28 se guarda como 0x41C00000 y el motor lee
    // basura; el destino 28 de la matriz es Morph Z, y un destino de 1.1e9 no
    // lo jlimitea a nada util. Con la vista correcta, es un 28.
    const views = makeViews();

    expect(writeGpField(layout, 1, 28, views)).toBe(true);
    expect(views.i32[1]).toBe(28);
    expect(views.f32[1]).not.toBe(28);          // ahi NO esta
    expect(new Uint32Array(views.mirror, 4, 1)[0]).toBe(28);
  });

  it('un INT se redondea, como hace el miembro de C++ al leerlo', () => {
    const views = makeViews();

    writeGpField(layout, 1, 2.6, views);

    expect(views.i32[1]).toBe(3);
  });

  it('un double va a Float64Array (el bpm), con las 8 bytes', () => {
    const views = makeViews(64);

    expect(writeGpField(layout, 2, 132.5, views)).toBe(true);
    expect(views.f64[1]).toBe(132.5);
    expect(views.f32[2]).toBe(0);   // ni una palabra deprecision se ha escrito
  });

  it('un campo que el layout no publica devuelve false y NO escribe', () => {
    const views = makeViews();

    expect(writeGpField(layout, 99, 0.5, views)).toBe(false);
    expect(views.f32.some((value) => value !== 0)).toBe(false);
  });
});
