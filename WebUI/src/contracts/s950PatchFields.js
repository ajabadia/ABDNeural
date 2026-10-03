/**
 * El catálogo de patches del Akai S950, listo para un panel de esta WebUI.
 *
 * POR QUÉ EXISTE ESTE FICHERO Y NO UN `fetch`. El catálogo se GENERA desde
 * `ABDSharedCode/SynthCore/S950PatchFields.h` —el C++ que gobierna el importador
 * y los tests del motor— y se expone como
 * `@abdsynths/shared/contracts/s950_patch_fields.json`. Se IMPORTA, no se
 * descarga: un `fetch` haría que el panel fuera asíncrono, y con eso un control
 * que nace con el rango sin saberlo. Un mando que empieza en 0..1 y se
 * corrige un frame después es un mando que se ve saltar.
 *
 * Lo que esto resuelve es la razón de ser del catálogo compartido: un panel que
 * escribe los nombres y los rangos a mano tiene dos copias, y las copias se
 * separan sin ruido. Alguien añade un campo al motor, el panel sigue
 * enseñando 38, y el desfase se descubre el día que un patch importado suena
 * raro. Importando la tabla, el panel y el motor no pueden discrepar sobre el
 * mismo byte: si discrepan, es que el JSON no se regeneró, y hay un test que lo
 * dice.
 *
 * LO QUE NO HACE. No lee discos, no traduce un byte a un valor, no recorta. Eso
 * es de `S950Disk.h`, en C++. Aquí vive lo que un panel necesita —el nombre, el
 * rango, la pestaña, a qué trim responde— y ni una regla más.
 *
 * @example
 *   import { S950_PATCH_CATALOGUE } from '../contracts/s950PatchFields.js';
 *
 *   for (const field of S950_PATCH_CATALOGUE.page('FILTER')) {
 *     addControl({ key: field.code, label: field.name, min: field.lo, max: field.hi });
 *   }
 */

import {
  buildS950Catalogue,
  formatS950Name,
  isS950Bipolar,
  S950_ENCODINGS,
  S950_GROUPS,
} from '@abdsynths/shared/components';
import contract from '@abdsynths/shared/contracts/s950_patch_fields.json';

/**
 * El catálogo ya indexado, listo para usar.
 *
 * Sale `null` si el contrato no tuviera forma de catálogo, que no es una
 * instancia vacía sino un error de generación. Devolver un objeto vacío
 * dejaría al panel con cero controles y sin ninguna pista de por qué, que es
 * la peor forma de fallar.
 */
export const S950_PATCH_CATALOGUE = buildS950Catalogue(contract);

/** El contrato tal cual, para quien quiera leer una nota o la procedencia. */
export { contract as S950_PATCH_CONTRACT, formatS950Name, isS950Bipolar, S950_ENCODINGS, S950_GROUPS };

/**
 * Un campo del catálogo por su `code`, o `undefined`.
 *
 * @param {string} code por ejemplo `'softFine'`
 */
export function s950Field (code) {
  return S950_PATCH_CATALOGUE?.field(code);
}

/**
 * Los campos de una pestaña del panel, ya con el nombre formateado para pintar.
 *
 * El contrato guarda el nombre CRUDO de la máquina —`ALL`, `MONO1`— y la
 * tipografía va aquí, en el sitio que la elige. Un contrato que maqueta se
 * queda viejo el día que el panel cambie su estilo, y entonces el desfase
 * parece del panel cuando es del dato.
 *
 * @param {string} groupName una de `S950_GROUPS`
 * @returns {{code: string, label: string, lo: number, hi: number, bipolar: boolean}[]}
 */
export function s950Page (groupName) {
  if (!S950_PATCH_CATALOGUE) return [];

  return S950_PATCH_CATALOGUE.page(groupName).map((f) => ({
    code: f.code,
    label: formatS950Name(f.name),
    lo: f.lo,
    hi: f.hi,
    // Derivada del rango y no una columna del JSON: si el rango cambia, esto
    // cambia con él y no puede quedarse viejo.
    bipolar: isS950Bipolar(f),
  }));
}
