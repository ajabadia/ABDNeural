/**
 * La frontera entre el catálogo del S950 y los parámetros de NEURONiK.
 *
 * ───────────────────────────────────────────────────────────────────────────
 * POR QUÉ ESTE FICHERO, Y POR QUÉ NO HACE NADA.
 *
 * Como NEURONiK no tiene importador de patches del S950 —no hay ni uno
 * en todo el repo—, este fichero NO es un convertidor. Es lo contrario: es el
 * sitio donde queda escrito por qué no se puede escribir uno todavía, de forma
 * que nadie lo escriba por accidente dentro de tres meses.
 *
 * LA RAZÓN, QUE ES LO QUE ESTE FICHERO ENSEÑA.
 *
 * Los dos contratos hablan el mismo concepto en DOS DOMINIOS DISTINTOS:
 *
 *     S950 (units de panel)      NEURONiK (units reales)
 *     vcaAttack   0 .. 99        envAttack     0.001 .. 5     s
 *     softFilter  0 .. 99        filterCutoff  20 .. 20000    Hz
 *     lfoRate     0 .. 99        lfo1RateHz    0.01 .. 20     Hz
 *     vcaSustain  0 .. 99        envSustain    0 .. 1
 *
 * "Coinciden los rangos" NO TIENE SENTIDO entre esos dos: 0..99 y 20..20000 no
 * van a coincidir nunca, y no porque uno esté mal, sino porque son dominios
 * distintos. Compararlos directamente es comparar dos cosas que no se
 * corresponden, y un test así o bien pasa siempre o bien falla siempre: en los
 * dos casos no mira nada.
 *
 * EL PUENTE ENTRE LOS DOMINIOS ES LA TABLA DE CALIBRACIÓN, Y ESTÁ VACÍA.
 *
 * Saber qué vale "attack 80" en segundos no se deduce de los rangos: se mide,
 * y esa medición no se ha hecho. `S950Calibration.h` declara la FORMA de las
 * seis curvas y no tiene ni un punto. Hasta que las tenga, la conversión de
 * este fichero es `null` EN TODAS LAS FILAS, y `convert()` devuelve
 * `UNKNOWN` —nunca un número— igual que el `std::nullopt` de C++.
 *
 * ───────────────────────────────────────────────────────────────────────────
 * POR QUÉ EL SENTIDO DE LA CONVERSIÓN NO ES SIMÉTRICO.
 *
 * De unidades de panel a unidades reales hay un puente, y ese puente es la
 * calibración. Al revés no lo hay: un attack de 0.002 s no se puede volver a
 * escribir como un número de panel, porque un panel no tiene decimales. La
 * conversión que falta es siempre en un sentido, y por eso la fila declara su
 * dirección en vez de asumirla.
 *
 * ───────────────────────────────────────────────────────────────────────────
 * LO QUE HAY QUE HACER PARA LLENAR ESTO.
 *
 * 1. Una sesión de medición propia, con `S950CalibrationHarness.h` (que corrige
 *    el sesgo de la ventana de análisis, que es la parte que no es trivial).
 * 2. Volcar esa tabla a JSON, igual que el catálogo.
 * 3. Poner aquí el `panelValue` de cada fila, y cambiar la conversión de `null`
 *    a la curva. Un test de este repo ya comprueba que las conversiones siguen
 *    siendo `null`, así que al hacerlo se pondrán en rojo los que hagan falta
 *    revisar: que es exactamente lo que se quiere.
 *
 * @example
 *   import { s950ToNeuronik, CONCEPTS, UNKNOWN } from '../contracts/s950Bridge.js';
 *
 *   const hz = s950ToNeuronik('softFilter', 74);
 *   if (hz === UNKNOWN) {
 *     // No lo sabemos. Dejar el mando como estaba es correcto;
 *     // inventar 1000 Hz no lo es.
 *   }
 */

import { s950Field } from './s950PatchFields.js';
import { getDescriptor } from './parameters.js';

/**
 * Lo que se devuelve cuando no se sabe. Un simbolo y no un numero, por el
 * mismo motivo que `std::nullopt` en C++: un 0 seria un valor legitimo en
 * algunos dominios y nadie tendria nada de quoi sospechar.
 *
 * @type {symbol}
 */
export const UNKNOWN = Symbol('s950:conversion-desconocida');

/**
 * El mapa de conceptos: un campo del S950 y el parametro de NEURONiK que
 * representa el mismo magnitudes, con la conversion DECLARADA NULA.
 *
 * @typedef {object} ConceptBridge
 * @property {string} concept      que magnitud es
 * @property {string} s950Field    el `code` del campo en el catalogo
 * @property {string} neuronikId   el id del parametro en el panel
 * @property {'panelToReal'} direction  siempre panel -> unidades reales, y por
 *                                     eso no es opcional: la vuelta no existe
 * @property {string} panelUnit    en que esta el panel
 * @property {string} realUnit     en que esta el motor
 * @property {null} conversion     la curva que falta. `null` hasta que haya
 *                                 medicion; ver la cabecera.
 */

/** @type {readonly ConceptBridge[]} */
export const CONCEPTS = Object.freeze([
  {
    concept: 'tiempo de ataque de la envolvente',
    s950Field: 'vcaAttack', neuronikId: 'envAttack',
    direction: 'panelToReal', panelUnit: '0..99', realUnit: 's', conversion: null,
  },
  {
    concept: 'tiempo de caida de la envolvente',
    s950Field: 'vcaDecay', neuronikId: 'envDecay',
    direction: 'panelToReal', panelUnit: '0..99', realUnit: 's', conversion: null,
  },
  {
    concept: 'nivel de sustain',
    s950Field: 'vcaSustain', neuronikId: 'envSustain',
    direction: 'panelToReal', panelUnit: '0..99', realUnit: '0..1', conversion: null,
  },
  {
    concept: 'tiempo de release de la envolvente',
    s950Field: 'vcaRelease', neuronikId: 'envRelease',
    direction: 'panelToReal', panelUnit: '0..99', realUnit: 's', conversion: null,
  },
  {
    concept: 'frecuencia de corte del filtro',
    s950Field: 'softFilter', neuronikId: 'filterCutoff',
    direction: 'panelToReal', panelUnit: '0..99', realUnit: 'Hz', conversion: null,
  },
  {
    concept: 'resonancia del filtro',
    s950Field: 'vcfAmount', neuronikId: 'filterRes',
    direction: 'panelToReal', panelUnit: '-50..+50', realUnit: '0..1', conversion: null,
  },
  {
    concept: 'velocidad del LFO',
    s950Field: 'lfoRate', neuronikId: 'lfo1RateHz',
    direction: 'panelToReal', panelUnit: '0..99', realUnit: 'Hz', conversion: null,
  },
  {
    concept: 'profundidad del LFO',
    s950Field: 'lfoDepth', neuronikId: 'lfo1Depth',
    direction: 'panelToReal', panelUnit: '0..99', realUnit: '0..1', conversion: null,
  },
]);

/**
 * La fila de un concepto, por el `code` del campo del S950.
 *
 * @param {string} s950FieldCode por ejemplo `'softFilter'`
 * @returns {ConceptBridge|undefined}
 */
export function bridgeFor (s950FieldCode) {
  return CONCEPTS.find((c) => c.s950Field === s950FieldCode);
}

/**
 * Convierte un valor de PANEL del S950 a las unidades reales del motor.
 *
 * Devuelve `UNKNOWN` —no un numero, no un 0, no un `null`— mientras la tabla de
 * calibración no tenga el punto correspondiente. Que sea un simbolo y no un
 * `null` es lo que impide que un `?? 0` o un `|| 1` en el llamante convierta
 * "no lo sé" en un valor: los dos fallan con un simbolo.
 *
 * @param {string} s950FieldCode el campo del catalogo
 * @param {number} panelValue    el valor de panel, 0..99
 * @returns {number|typeof UNKNOWN}
 */
export function s950ToNeuronik (s950FieldCode, panelValue) {
  const row = bridgeFor (s950FieldCode);

  if (!row) return UNKNOWN;
  if (row.conversion === null) return UNKNOWN;

  // Hoy no se llega aqui, y esta es la linea que habra que cambiar cuando haya
  // medicion. Se deja escrita para que quede claro DONDE va el puente y para
  // que el camino sea explicito cuando alguien lo rellene.
  return row.conversion(panelValue);
}

/**
 * Las dos unidades de un concepto, para que un panel pueda ENSEÑAR las dos
 * sin convertir entre ellas.
 *
 * @param {string} s950FieldCode
 * @returns {{panelUnit: string, realUnit: string, realMin: number, realMax: number}|null}
 */
export function unitsFor (s950FieldCode) {
  const row = bridgeFor (s950FieldCode);
  if (!row) return null;

  const d = getDescriptor(row.neuronikId);
  if (!d) return null;

  return {
    panelUnit: row.panelUnit,
    realUnit: row.realUnit,
    realMin: d.minValue,
    realMax: d.maxValue,
  };
}
