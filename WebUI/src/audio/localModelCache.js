/**
 * Memoria LOCAL de los modelos cargados en el navegador (modo sin host).
 *
 * El plugin sabe volver a sus ranuras porque el PRESET lleva la ruta del fichero
 * (`modelPath<slot>`): al reabrir el proyecto el procesador recarga y publica
 * `modelsState`. La pagina del navegador no tiene ni preset ni sistema de ficheros
 * — lo unico que tiene es el `<input type="file">` que el usuario elige a mano —, asi
 * que sin esta memoria cada recarga empezaba con las cuatro ranuras EMPTY y habia que
 * volver a buscar los mismos ficheros.
 *
 * Se guarda el TEXTO CRUDO del .neuronikmodel, no el objeto ya parseado: el lector del
 * modelo es UNO (`localModels::parseModelText`) y re-parsear al recuperar obliga a que
 * lo que vuelve del almacen pase por la MISMA validacion que un fichero recien elegido.
 * Guardar el objeto seria congelar un dialecto derivado — el dia que el parser cambie
 * (o el escritor publique v2.2), lo guardado seguiria siendo el shape viejo.
 *
 * Todo es best-effort y a prueba de balas: sin `localStorage` (Node, un WebView que lo
 * niega, un navegador en modo privado que lanza al tocarlo) se lee vacio y se escribe
 * false; con el cupo lleno, igual. La carga del modelo NO depende de esto: el modelo
 * suena en la sesion aunque el almacen no quiera recordarlo, y quien llama decide si
 * merece un aviso.
 *
 * Solo MODO LOCAL: con host manda `modelsState` y el store ni lee ni escribe aqui.
 */

import { MODEL_SLOT_COUNT } from './localModels.js';

/**
 * Clave del almacen. El formato va VERSIONADO dentro del payload (no en la clave) para
 * que una version futura se pueda distinguir y descartar sin dejar basura huerfana.
 */
export const LOCAL_MODEL_CACHE_KEY = 'neuronik.localModels';
export const LOCAL_MODEL_CACHE_VERSION = 1;

/**
 * El almacen de siempre, o null si este runtime no lo tiene (o lo niega al tocarlo:
 * acceder a `localStorage` puede LANZAR, y eso hay que tratarlo como "no hay").
 */
function ambientStorage() {
  try {
    const storage = globalThis.localStorage;
    return storage && typeof storage.getItem === 'function' ? storage : null;
  } catch {
    return null;
  }
}

/**
 * Los textos guardados, por ranura: `{ 0: '...', 2: '...' }`. Un almacen vacio, uno
 * que no existe, un payload de otra version o uno corrupto devuelven `{}` — recuperar
 * la memoria NUNCA puede romper el arranque de la pagina.
 *
 * @param {Storage|null} [storage]  inyectable (tests); por defecto, `localStorage`.
 * @returns {Object<number, string>}
 */
export function readCachedModelTexts(storage = ambientStorage()) {
  if (!storage || typeof storage.getItem !== 'function') return {};

  let payload;
  try {
    const raw = storage.getItem(LOCAL_MODEL_CACHE_KEY);
    if (typeof raw !== 'string' || raw === '') return {};
    payload = JSON.parse(raw);
  } catch {
    return {};
  }

  if (!payload || typeof payload !== 'object' || Array.isArray(payload)) return {};
  if (payload.version !== LOCAL_MODEL_CACHE_VERSION) return {};

  const slots = payload.slots;
  if (!slots || typeof slots !== 'object') return {};

  const remembered = {};

  for (const [key, text] of Object.entries(slots)) {
    const slot = Number(key);

    // Una entrada que no es de una ranura del motor (o que no trae texto) se ignora:
    // el shape de `models` es del motor y la ranura es su numeracion.
    if (!Number.isInteger(slot) || slot < 0 || slot >= MODEL_SLOT_COUNT) continue;
    if (typeof text !== 'string' || text.trim() === '') continue;

    remembered[slot] = text;
  }

  return remembered;
}

/**
 * Guarda (o reemplaza) el texto crudo del modelo de una ranura.
 *
 * @param {number} slot  0..3, la numeracion del motor (A..D).
 * @param {string} text  el fichero tal cual salio del `<input type="file">`.
 * @param {Storage|null} [storage]
 * @returns {boolean} true solo si quedo guardado. `false` no es un error de la carga:
 *   es la memoria la que no ha podido tomar nota.
 */
export function saveCachedModelText(slot, text, storage = ambientStorage()) {
  if (!Number.isInteger(slot) || slot < 0 || slot >= MODEL_SLOT_COUNT) return false;
  if (typeof text !== 'string' || text.trim() === '') return false;
  if (!storage || typeof storage.setItem !== 'function') return false;

  const slots = { ...readCachedModelTexts(storage), [slot]: text };

  try {
    storage.setItem(LOCAL_MODEL_CACHE_KEY, JSON.stringify({
      version: LOCAL_MODEL_CACHE_VERSION,
      slots,
    }));
    return true;
  } catch {
    // Cupo lleno (QuotaExceededError) o almacen negado a mitad: best-effort.
    return false;
  }
}

/**
 * Olvida UNA ranura: la saca de la memoria y deja las demas donde estaban.
 *
 * Es la puerta que usa el boton OLVIDAR de la ficha RANURAS, y por eso se
 * distingue de `clearLocalModelCache` en dos cosas:
 *
 *   - VERIFICA lo que quedo, en vez de fiarse de que `setItem` no fallo. Un
 *     almacen que acepta la escritura y luego la pierde (cuota nearly lleno,
 *     modo privado que lanza al tocar) devolvia un `true` de mentira: aqui se
 *     relee y se compara con lo pedido.
 *   - SI QUEDA OTRA RANURA se reescribe el payload entero; si esta era la
 *     ULTIMA se borra la clave, para que un almacen vacio no se quede con un
 *     `{"version":1,"slots":{}}` de cascara.
 *
 * @param {number} slot  0..3, la numeracion del motor (A..D).
 * @param {Storage|null} [storage]
 * @returns {{ forgotten: boolean, remembered: boolean }}
 *   `forgotten`: la memoria ya NO guarda ese texto (o no lo guardaba, que es lo
 *   mismo para el F5). `remembered`: la memoria lo conserva, sea porque el
 *   almacen no quiso o porque no habia quien lo borrara. Los dos false solo
 *   salen con un slot que no es del motor.
 */
export function forgetCachedModelText(slot, storage = ambientStorage()) {
  if (!Number.isInteger(slot) || slot < 0 || slot >= MODEL_SLOT_COUNT)
    return { forgotten: false, remembered: false };

  if (!storage || typeof storage.getItem !== 'function') return { forgotten: false, remembered: true };

  const remembered = readCachedModelTexts(storage);
  const had = Object.prototype.hasOwnProperty.call(remembered, slot);

  if (!had) return { forgotten: true, remembered: false };

  const rest = { ...remembered };
  delete rest[slot];

  const empty = Object.keys(rest).length === 0;

  try {
    // Sin ranuras que recordar, la clave ENTERA se va: una memoria vacia es
    // mejor que una clave con un payload sin contenido.
    if (empty && typeof storage.removeItem === 'function')
      storage.removeItem(LOCAL_MODEL_CACHE_KEY);
    else if (!empty)
      storage.setItem(LOCAL_MODEL_CACHE_KEY, JSON.stringify({
        version: LOCAL_MODEL_CACHE_VERSION,
        slots: rest,
      }));
  } catch {
    // Best-effort como el resto del modulo: quien llama decide si avisa.
  }

  // La verdad es lo que se LEE despues, no lo que se escribio antes.
  const still = readCachedModelTexts(storage);

  return {
    forgotten: !Object.prototype.hasOwnProperty.call(still, slot),
    remembered: Object.prototype.hasOwnProperty.call(still, slot),
  };
}

/**
 * Olvida TODA la memoria (las cuatro ranuras de golpe). Sin UI que lo pida: la
 * ficha RANURAS olvida de una en una, que es el gesto que el usuario puede
 * deshacer recargando el fichero. Queda para tests y para quien quiera el
 * borrado completo.
 */
export function clearLocalModelCache(storage = ambientStorage()) {
  if (!storage || typeof storage.removeItem !== 'function') return false;

  try {
    storage.removeItem(LOCAL_MODEL_CACHE_KEY);
    return true;
  } catch {
    return false;
  }
}
