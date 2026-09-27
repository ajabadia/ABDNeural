/**
 * Memoria LOCAL de modelos (src/audio/localModelCache.js).
 *
 * El plugin vuelve a sus ranuras porque el PRESET lleva la ruta del fichero; en el
 * navegador no hay preset, asi que lo unico que sobrevive a un F5 es este almacen.
 * Lo que fija el test:
 *
 *   - se guarda el TEXTO crudo del .neuronikmodel, por ranura, con el payload
 *     VERSIONADO (una version futura se descarta, no se mal- lee);
 *   - todo es best-effort: sin almacen, con un almacen que LANZA al tocarlo (modo
 *     privado) o con el cupo lleno, se lee vacio y se escribe false — nunca revienta;
 *   - un payload corrupto, de otra version o con entradas basura se descarta entero
 *     (recuperar la memoria no puede romper el arranque de la pagina);
 *   - fuera de las cuatro ranuras del motor no se guarda nada;
 *   - OLVIDAR una ranura (el boton de la ficha RANURAS) quita solo esa, y lo
 *     DEVUELVE LEIDO: un almacen que acepta la escritura y la pierde sale como
 *     `remembered`, que es lo que el store pinta como aviso de "al recargar
 *     volvera".
 */

import { afterEach, describe, expect, it } from 'vitest';

import {
  LOCAL_MODEL_CACHE_KEY,
  LOCAL_MODEL_CACHE_VERSION,
  clearLocalModelCache,
  forgetCachedModelText,
  readCachedModelTexts,
  saveCachedModelText,
} from '../src/audio/localModelCache.js';

/** Almacen de mentira: un Map con la cara de Storage (para no tocar el de jsdom). */
function memoryStorage() {
  const map = new Map();

  return {
    map,
    getItem: (key) => (map.has(key) ? map.get(key) : null),
    setItem: (key, value) => { map.set(key, String(value)); },
    removeItem: (key) => { map.delete(key); },
    raw: () => map.get(LOCAL_MODEL_CACHE_KEY) ?? null,
  };
}

/** Almacen que acepta leer y LANZA al escribir (cupo lleno / negado). */
function fullStorage() {
  const map = new Map();

  return {
    map,
    getItem: (key) => (map.has(key) ? map.get(key) : null),
    setItem: () => { throw new Error('QuotaExceededError'); },
    removeItem: (key) => { map.delete(key); },
  };
}

/** Almacen que ACEPTA la escritura y la pierde: el setItem no lanza, no guarda. */
function liarStorage() {
  const inner = memoryStorage();
  const stored = new Map();

  return {
    getItem: (key) => (stored.has(key) ? stored.get(key) : null),
    setItem: () => {},
    removeItem: () => {},
    seed: (key, value) => { stored.set(key, value); },
    inner,
  };
}

const MODEL = '{"name":"CZ-BASS1","format":2,"amplitudes":[1,0]}';

describe('localModelCache / memoria de las ranuras locales', () => {
  afterEach(() => {
    // Por si algun test cae en el localStorage de jsdom (el camino por defecto).
    clearLocalModelCache();
  });

  it('guarda y recupera el texto crudo, por ranura', () => {
    const storage = memoryStorage();

    expect(saveCachedModelText(1, MODEL, storage)).toBe(true);
    expect(readCachedModelTexts(storage)).toEqual({ 1: MODEL });
  });

  it('volver a cargar la MISMA ranura reemplaza (no acumula)', () => {
    const storage = memoryStorage();

    saveCachedModelText(0, MODEL, storage);
    saveCachedModelText(0, '{"name":"OTRO","amplitudes":[0]}', storage);

    expect(readCachedModelTexts(storage)).toEqual({ 0: '{"name":"OTRO","amplitudes":[0]}' });
    expect(Object.keys(JSON.parse(storage.raw()).slots)).toEqual(['0']);
  });

  it('las ranuras son independientes: llenar una no toca a las demas', () => {
    const storage = memoryStorage();

    saveCachedModelText(3, MODEL, storage);
    saveCachedModelText(1, '{"name":"B"}', storage);

    expect(readCachedModelTexts(storage)).toEqual({ 1: '{"name":"B"}', 3: MODEL });
  });

  it('el payload va VERSIONADO (una version futura se descarta, no se mal-lee)', () => {
    const storage = memoryStorage();

    saveCachedModelText(2, MODEL, storage);

    const payload = JSON.parse(storage.raw());
    expect(payload.version).toBe(LOCAL_MODEL_CACHE_VERSION);
    expect(Object.keys(payload.slots)).toEqual(['2']);

    storage.setItem(LOCAL_MODEL_CACHE_KEY, JSON.stringify({ version: 99, slots: { 2: MODEL } }));
    expect(readCachedModelTexts(storage)).toEqual({});
  });

  it('sin almacen (null) se lee vacio y no se escribe', () => {
    expect(readCachedModelTexts(null)).toEqual({});
    expect(saveCachedModelText(0, MODEL, null)).toBe(false);
    expect(clearLocalModelCache(null)).toBe(false);
  });

  it('un almacen que LANZA al tocarlo (modo privado) no revienta la pagina', () => {
    const descriptor = Object.getOwnPropertyDescriptor(globalThis, 'localStorage');

    Object.defineProperty(globalThis, 'localStorage', {
      configurable: true,
      get() { throw new Error('almacen negado'); },
    });

    try {
      // Construir el payload pasa por leer el actual: con el almacen negado, vacio.
      expect(readCachedModelTexts()).toEqual({});
      expect(saveCachedModelText(0, MODEL)).toBe(false);
    } finally {
      if (descriptor) Object.defineProperty(globalThis, 'localStorage', descriptor);
      else delete globalThis.localStorage;
    }
  });

  it('un payload corrupto o con basura dentro se descarta sin romper el arranque', () => {
    const storage = memoryStorage();

    storage.setItem(LOCAL_MODEL_CACHE_KEY, 'esto no es json');
    expect(readCachedModelTexts(storage)).toEqual({});

    storage.setItem(LOCAL_MODEL_CACHE_KEY, JSON.stringify([1, 2, 3]));
    expect(readCachedModelTexts(storage)).toEqual({});

    storage.setItem(LOCAL_MODEL_CACHE_KEY, JSON.stringify({ version: 1 }));
    expect(readCachedModelTexts(storage)).toEqual({});

    // Entradas basura: fuera de rango, no numericas, vacias o que no son texto.
    storage.setItem(LOCAL_MODEL_CACHE_KEY, JSON.stringify({
      version: 1,
      slots: { 0: MODEL, 4: MODEL, '-1': MODEL, nombre: MODEL, 2: '', 3: 42 },
    }));
    expect(readCachedModelTexts(storage)).toEqual({ 0: MODEL });
  });

  it('sin cupo devuelve false y NO rompe lo que ya estaba guardado', () => {
    const storage = memoryStorage();
    saveCachedModelText(0, MODEL, storage);

    const full = fullStorage();
    full.map.set(LOCAL_MODEL_CACHE_KEY, storage.raw());

    expect(saveCachedModelText(1, MODEL, full)).toBe(false);
    expect(readCachedModelTexts(full)).toEqual({ 0: MODEL });
  });

  it('fuera de las ranuras del motor no se guarda nada', () => {
    const storage = memoryStorage();

    for (const slot of [-1, 4, 1.5, NaN, '0'])
      expect(saveCachedModelText(slot, MODEL, storage)).toBe(false);

    expect(saveCachedModelText(0, '   ', storage)).toBe(false);
    expect(saveCachedModelText(0, 42, storage)).toBe(false);
    expect(readCachedModelTexts(storage)).toEqual({});
  });

  it('por defecto usa el localStorage del entorno (el navegador)', () => {
    expect(saveCachedModelText(3, MODEL)).toBe(true);
    expect(readCachedModelTexts()).toEqual({ 3: MODEL });

    expect(clearLocalModelCache()).toBe(true);
    expect(readCachedModelTexts()).toEqual({});
  });
});

describe('localModelCache / olvidar una ranura', () => {
  afterEach(() => {
    clearLocalModelCache();
  });

  it('quita SOLO esa ranura y deja las demas donde estaban', () => {
    const storage = memoryStorage();

    saveCachedModelText(0, MODEL, storage);
    saveCachedModelText(2, '{"name":"Campana"}', storage);

    expect(forgetCachedModelText(0, storage)).toEqual({ forgotten: true, remembered: false });
    expect(readCachedModelTexts(storage)).toEqual({ 2: '{"name":"Campana"}' });
  });

  it('la ULTIMA ranura olvidada se lleva la clave entera (no queda cascara)', () => {
    const storage = memoryStorage();

    saveCachedModelText(1, MODEL, storage);
    forgetCachedModelText(1, storage);

    expect(storage.raw()).toBe(null);
  });

  it('olvidar lo que no estaba olvidado es un acierto, no un fallo', () => {
    const storage = memoryStorage();

    saveCachedModelText(0, MODEL, storage);
    expect(forgetCachedModelText(3, storage)).toEqual({ forgotten: true, remembered: false });
    expect(readCachedModelTexts(storage)).toEqual({ 0: MODEL });
  });

  it('un almacen que ACEPTA la escritura y la pierde sale como `remembered`', () => {
    // El caso que no puede fiarse del write: `setItem`/`removeItem` no lanzan y no
    // hacen nada. Verificar por lectura es lo que separa "olvidada" de "va a volver".
    const storage = liarStorage();

    saveCachedModelText(0, MODEL, { ...storage, setItem: storage.setItem });
    storage.seed(LOCAL_MODEL_CACHE_KEY, JSON.stringify({
      version: LOCAL_MODEL_CACHE_VERSION,
      slots: { 0: MODEL },
    }));

    expect(forgetCachedModelText(0, storage)).toEqual({ forgotten: false, remembered: true });
  });

  it('sin almacen no hay memoria que olvidar: se dice forgotten, no se inventa', () => {
    expect(forgetCachedModelText(0, null)).toEqual({ forgotten: false, remembered: true });
  });

  it('un almacen que se LEE y no se ESCRIBE sale como `remembered` (al F5 volvera)', () => {
    // El caso de verdad del aviso "al recargar volvera": la memoria se lee bien
    // (la ranura esta ahi) pero el navegador no la suelta al escribir.
    const map = new Map([[LOCAL_MODEL_CACHE_KEY, JSON.stringify({
      version: LOCAL_MODEL_CACHE_VERSION,
      slots: { 0: MODEL },
    })]]);

    const readonly = {
      getItem: (key) => (map.has(key) ? map.get(key) : null),
      setItem: () => { throw new Error('QuotaExceededError'); },
      removeItem: () => { throw new Error('SecurityError'); },
    };

    expect(forgetCachedModelText(0, readonly)).toEqual({ forgotten: false, remembered: true });
    expect(readCachedModelTexts(readonly)).toEqual({ 0: MODEL });
  });

  it('un almacen que LANZA al LEER no se rompe el arranque: no recuerda, luego no vuelve', () => {
    // Lo que el F5 hara con esta memoria es lo mismo que hace `restoreLocalModels`:
    // leer. Si no se puede leer, tampoco restaura, asi que la ranura no vuelve y
    // decir "olvidada" es verdad (y no se inventa un aviso de `remembered`).
    const angry = {
      getItem: () => { throw new Error('SecurityError'); },
      setItem: () => { throw new Error('SecurityError'); },
      removeItem: () => { throw new Error('SecurityError'); },
    };

    expect(() => forgetCachedModelText(0, angry)).not.toThrow();
    expect(forgetCachedModelText(0, angry)).toEqual({ forgotten: true, remembered: false });
  });

  it('fuera de las ranuras del motor no hay nada que olvidar', () => {
    const storage = memoryStorage();

    for (const slot of [-1, 4, 1.5, NaN, '0'])
      expect(forgetCachedModelText(slot, storage)).toEqual({ forgotten: false, remembered: false });
  });
});
