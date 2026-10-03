/**
 * Las curvas de calibración del Akai S950, listas para pintar los ejes de un
 * panel de esta WebUI.
 *
 * POR QUÉ EXISTE ESTE FICHERO Y POR QUÉ NO ES EL CATÁLOGO DE PATCHES.
 *
 * `s950PatchFields.js` de esta misma carpeta contesta "qué byte es este campo y
 * entre qué valores se mueve". Esto contesta OTRA pregunta: "cuánto vale", o sea
 * en qué unidad está cada magnitud y sobre qué rango real. Son dominios
 * distintos y no se mezclan: un mando de attack va de 0 a 99 porque es lo que
 * el byte da, y el eje que lo acompaña va en segundos —que es lo que el motor
 * oye— y ese segundo número no está en ninguna parte de este repo.
 *
 * POR QUÉ SE IMPORTA Y NO SE PIDE. Igual que el catálogo: el JSON se GENERA
 * desde `ABDSharedCode/SynthCore/S950Calibration.h`, que es el C++ que
 * gobierna el motor, y se importa para que el panel sea síncrono. Un panel que
 *Calculase sus ejes con un `setTimeout` dibujaría el mando con un rango y un
 * frame después con otro.
 *
 * LO MÁS IMPORTANTE DE ESTE FICHERO, Y LO QUE NO HACE.
 *
 * No hay NI UNA curva medida, y `s950AxisFor()` no intenta disimularlo: de las
 * seis, cuatro devuelven `needsMeasuredMinimum: true` porque su eje es
 * logarítmico y un eje en log necesita un mínimo REAL, y ese mínimo es
 * justamente un valor medido. Es decir: de las seis se puede pintar el eje
 * horizontal y solo dos el vertical.
 *
 * El módulo delega esa regla en `s950AxisFor()` del paquete compartido en vez
 * de repetirla aquí, y el motivo es que la regla interesa a todos los paneles.
 * Duplicada, la lista de cuatro se pondría vieja en un panel y no en el otro, y
 * un panel que dibujara un mínimo inventado tendría un eje de ataque con un
 * número que nadie midió — y se vería bonito, que es lo peor.
 *
 * @example
 *   import { S950_AXES, s950Axis, S950_COVERAGE } from '../contracts/s950Calibration.js';
 *
 *   for (const axis of S950_AXES) {
 *     addControl({ key: axis.code, min: axis.lo, max: axis.hi });
 *     if (axis.needsMeasuredMinimum) axis.hatch('sin medir');
 *   }
 *   header.textContent = S950_COVERAGE.label;
 */

import {
  buildS950Calibration,
  s950AxisFor,
  s950Coverage,
  S950_UNITS,
} from '@abdsynths/shared/components';
import contract from '@abdsynths/shared/contracts/s950_calibration.json';

/**
 * El contrato ya indexado, o `null` si el JSON no tuviera forma de catálogo.
 *
 * `null` y no un objeto vacío, por el mismo motivo que en el catálogo de
 * patches: un índice vacío haría que un `for` no pintara nada y el panel
 * pareciera funcionar.
 */
export const S950_CALIBRATION = buildS950Calibration(contract);

/** El contrato tal cual, para leer una nota o la procedencia. */
export { contract as S950_CALIBRATION_CONTRACT, S950_UNITS };

/** Un resumen de qué sabe el repo, para encabezar la sección. */
export const S950_COVERAGE = s950Coverage(S950_CALIBRATION);

/**
 * Una curva por su `code`, o `undefined`.
 *
 * @param {string} code por ejemplo `'envelopeTime'`
 */
export function s950Curve (code) {
  return S950_CALIBRATION?.curve(code);
}

/**
 * La geometría del eje de una curva, con su `code` puesto para poder pintar.
 *
 * Devuelve `null` si la curva no existe, y no un eje de mentira: un `{}` con
 * `lo: undefined` daría un `span` de NaN y un panel que parece funcionar y no
 * enseña nada.
 *
 * @param {string} code
 * @returns {object|null}
 */
export function s950Axis (code) {
  const curve = s950Curve(code);
  if (!curve) return null;

  const axis = s950AxisFor(curve);
  if (!axis) return null;

  // El `code` entra AQUÍ y no en el contrato: el JSON dice de qué curva habla,
  // y el código de la curva es de la propia curva. Añadirlo al contrato sería
  // duplicarlo en dos sitios para que se puedan separar sin ruido.
  return { code: curve.code, ...axis };
}

/**
 * Los seis ejes, ya con la forma que un panel pinta, en el orden del enum de
 * C++.
 *
 * Es el orden del motor y no un orden de"Se ve mejor en el panel": el motor
 * indexa los puntos medidos con el entero del enum, así que una tabla reordenada
 * no da ningún fallo en C++ y lo único que pasa es que cada curva dibuja los
 * puntos de otra. El orden es parte del contrato.
 */
export const S950_AXES = (S950_CALIBRATION?.curves ?? [])
  // El `code` se empareja con SU eje antes de filtrar, y no por indice
  // despues: un `map` que tira nulos y luego un `map` que vuelve a mirar la
  // posicion empareja el codigo de una curva con el eje de otra en cuanto
  // falta una. El fallo seria un panel con la etiqueta de un ataque y el rango
  // de un sustain, y no daria ningun error.
  .map((curve) => {
    const axis = s950AxisFor(curve);
    return axis === null ? null : { code: curve.code, ...axis };
  })
  .filter((axis) => axis !== null);

/**
 * Los ejes cuyo VERTICAL no se puede dibujar todavía, para marcar en vez de
 * inventar.
 *
 * Un panel lo recorre para rayarlos. Es la lista que responde a "qué me
 * falta", y sale del paquete compartido para que dos paneles no puedan
 * responder cosas distintas.
 */
export function s950AxesWithoutVertical () {
  return S950_CALIBRATION?.curves.filter((c) => s950AxisFor(c)?.needsMeasuredMinimum) ?? [];
}

/**
 * El valor de una curva en un valor de panel, o `null`.
 *
 * SIEMPRE `null` hoy, y se reexporta en vez de reimplementarse para que quede a
 * la vista de quien pinta: 0 no es lo mismo que "no lo sé", y un panel que
 * hiciera `valueAt(...) || 0` estaría pintando un ataque instantáneo, que es un
 * click, con toda la pinta de un dato medido.
 *
 * @param {string} code
 * @param {number} stored
 * @returns {number|null}
 */
export function s950ValueAt (code, stored) {
  return S950_CALIBRATION?.valueAt(code, stored) ?? null;
}
